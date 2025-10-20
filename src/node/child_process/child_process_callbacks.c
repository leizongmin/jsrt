#include "../../util/debug.h"
#include "child_process_internal.h"

// Timer close callback
static void on_timer_close(uv_handle_t* handle) {
  if (handle) {
    free(handle);  // Timer was allocated with js_malloc, but free directly here
  }
}

// Process exit callback
void on_process_exit(uv_process_t* handle, int64_t exit_status, int term_signal) {
  JSChildProcess* child = (JSChildProcess*)handle->data;

  if (!child || !child->ctx) {
    return;
  }

  JSRT_Debug("Process %d exited with status %ld, signal %d", child->pid, (long)exit_status, term_signal);

  // Prevent multiple exit callbacks
  if (child->exited) {
    return;
  }

  // Mark in callback to prevent finalization
  child->in_callback = true;

  JSContext* ctx = child->ctx;

  // Set exit information
  child->exited = true;
  child->exit_code = (int)exit_status;
  child->signal_code = term_signal;

  // Stop and close timeout timer if active
  if (child->timeout_timer) {
    uv_timer_stop(child->timeout_timer);
    // Close the timer handle properly
    if (!uv_is_closing((uv_handle_t*)child->timeout_timer)) {
      uv_close((uv_handle_t*)child->timeout_timer, on_timer_close);
    }
    child->timeout_timer = NULL;
  }

  // Handle exec/execFile callback if buffering
  if (child->buffering && !JS_IsUndefined(child->exec_callback)) {
    JSRT_Debug("Processing exec/execFile callback");

    // Convert buffers to Buffer objects
    JSValue stdout_val = JS_UNDEFINED;
    JSValue stderr_val = JS_UNDEFINED;

    if (child->stdout_size > 0) {
      JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
      if (!JS_IsException(buffer_module)) {
        JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
        if (!JS_IsException(buffer_class)) {
          JSValue from_func = JS_GetPropertyStr(ctx, buffer_class, "from");
          if (JS_IsFunction(ctx, from_func)) {
            JSValue array_buffer = JS_NewArrayBufferCopy(ctx, (const uint8_t*)child->stdout_buffer, child->stdout_size);
            JSValue argv_buf[] = {array_buffer};
            stdout_val = JS_Call(ctx, from_func, buffer_class, 1, argv_buf);
            JS_FreeValue(ctx, array_buffer);
          }
          JS_FreeValue(ctx, from_func);
        }
        JS_FreeValue(ctx, buffer_class);
        JS_FreeValue(ctx, buffer_module);
      }
    } else {
      // Empty stdout - create empty Buffer
      JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
      if (!JS_IsException(buffer_module)) {
        JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
        if (!JS_IsException(buffer_class)) {
          JSValue alloc_func = JS_GetPropertyStr(ctx, buffer_class, "alloc");
          if (JS_IsFunction(ctx, alloc_func)) {
            JSValue argv_alloc[] = {JS_NewInt32(ctx, 0)};
            stdout_val = JS_Call(ctx, alloc_func, buffer_class, 1, argv_alloc);
            JS_FreeValue(ctx, argv_alloc[0]);
          }
          JS_FreeValue(ctx, alloc_func);
        }
        JS_FreeValue(ctx, buffer_class);
        JS_FreeValue(ctx, buffer_module);
      }
    }

    if (child->stderr_size > 0) {
      JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
      if (!JS_IsException(buffer_module)) {
        JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
        if (!JS_IsException(buffer_class)) {
          JSValue from_func = JS_GetPropertyStr(ctx, buffer_class, "from");
          if (JS_IsFunction(ctx, from_func)) {
            JSValue array_buffer = JS_NewArrayBufferCopy(ctx, (const uint8_t*)child->stderr_buffer, child->stderr_size);
            JSValue argv_buf[] = {array_buffer};
            stderr_val = JS_Call(ctx, from_func, buffer_class, 1, argv_buf);
            JS_FreeValue(ctx, array_buffer);
          }
          JS_FreeValue(ctx, from_func);
        }
        JS_FreeValue(ctx, buffer_class);
        JS_FreeValue(ctx, buffer_module);
      }
    } else {
      // Empty stderr - create empty Buffer
      JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
      if (!JS_IsException(buffer_module)) {
        JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
        if (!JS_IsException(buffer_class)) {
          JSValue alloc_func = JS_GetPropertyStr(ctx, buffer_class, "alloc");
          if (JS_IsFunction(ctx, alloc_func)) {
            JSValue argv_alloc[] = {JS_NewInt32(ctx, 0)};
            stderr_val = JS_Call(ctx, alloc_func, buffer_class, 1, argv_alloc);
            JS_FreeValue(ctx, argv_alloc[0]);
          }
          JS_FreeValue(ctx, alloc_func);
        }
        JS_FreeValue(ctx, buffer_class);
        JS_FreeValue(ctx, buffer_module);
      }
    }

    // Create error if non-zero exit or signal
    JSValue error = JS_NULL;
    if (exit_status != 0 || term_signal != 0) {
      const char* signal_str = term_signal ? signal_name(term_signal) : NULL;
      error = create_exec_error(ctx, child->exit_code, signal_str, child->file ? child->file : "command");
    }

    // Call callback
    call_exec_callback(ctx, child, error, stdout_val, stderr_val);
  }

  // Emit 'exit' event: (code, signal)
  JSValue exit_code = JS_NewInt32(ctx, child->exit_code);
  JSValue signal_val = JS_NULL;
  if (term_signal != 0) {
    const char* sig_name = signal_name(term_signal);
    if (sig_name) {
      signal_val = JS_NewString(ctx, sig_name);
    }
  }

  JSValue argv[] = {exit_code, signal_val};
  emit_event(ctx, child->child_obj, "exit", 2, argv);
  JS_FreeValue(ctx, exit_code);
  JS_FreeValue(ctx, signal_val);

  // Close stdio pipes
  close_stdio_pipes(child);

  // The 'close' event will be emitted after all pipes are closed
  // For now, emit it immediately after exit
  emit_event(ctx, child->child_obj, "close", 2, argv);

  // Clear callback flag
  child->in_callback = false;
}

// Stdout allocation callback
void on_stdout_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

// Stdout read callback
void on_stdout_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  uv_pipe_t* pipe = (uv_pipe_t*)stream;
  JSChildProcess* child = (JSChildProcess*)pipe->data;

  if (!child || !child->ctx) {
    goto cleanup;
  }

  // Mark in callback
  child->in_callback = true;

  JSContext* ctx = child->ctx;

  // Handle EOF or error
  if (nread < 0) {
    if (nread != UV_EOF) {
      JSRT_Debug("stdout read error: %s", uv_strerror(nread));
    }
    child->in_callback = false;
    goto cleanup;
  }

  // Ignore empty reads
  if (nread == 0) {
    child->in_callback = false;
    goto cleanup;
  }

  JSRT_Debug("Read %zd bytes from stdout", nread);

  // Check if we're in buffering mode (exec/execFile)
  if (child->buffering) {
    // Append to stdout buffer
    size_t new_size = child->stdout_size + nread;
    if (new_size > child->max_buffer) {
      JSRT_Debug("maxBuffer exceeded on stdout (%zu > %zu)", new_size, child->max_buffer);
      // Kill process due to maxBuffer exceeded
      uv_process_kill(&child->handle, SIGKILL);
      child->killed = true;
      child->in_callback = false;
      goto cleanup;
    }

    // Resize buffer if needed
    if (new_size > child->stdout_capacity) {
      size_t new_capacity = (child->stdout_capacity == 0) ? 4096 : child->stdout_capacity * 2;
      while (new_capacity < new_size) {
        new_capacity *= 2;
      }

      char* new_buffer = js_realloc(ctx, child->stdout_buffer, new_capacity);
      if (!new_buffer) {
        JSRT_Debug("Failed to allocate stdout buffer");
        child->in_callback = false;
        goto cleanup;
      }

      child->stdout_buffer = new_buffer;
      child->stdout_capacity = new_capacity;
    }

    // Copy data to buffer
    memcpy(child->stdout_buffer + child->stdout_size, buf->base, nread);
    child->stdout_size += nread;
  } else {
    // Normal mode: emit 'data' event
    JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
    if (!JS_IsException(buffer_module)) {
      JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
      if (!JS_IsException(buffer_class)) {
        JSValue from_func = JS_GetPropertyStr(ctx, buffer_class, "from");
        if (JS_IsFunction(ctx, from_func)) {
          // Create ArrayBuffer copy from received data
          JSValue array_buffer = JS_NewArrayBufferCopy(ctx, (const uint8_t*)buf->base, nread);

          // Convert to Buffer
          JSValue argv_buf[] = {array_buffer};
          JSValue data_buffer = JS_Call(ctx, from_func, buffer_class, 1, argv_buf);

          // Emit 'data' event on stdout stream
          if (!JS_IsUndefined(child->stdout_stream)) {
            JSValue data_argv[] = {data_buffer};
            emit_event(ctx, child->stdout_stream, "data", 1, data_argv);
            JS_FreeValue(ctx, data_argv[0]);
          }

          JS_FreeValue(ctx, array_buffer);
        }
        JS_FreeValue(ctx, from_func);
      }
      JS_FreeValue(ctx, buffer_class);
      JS_FreeValue(ctx, buffer_module);
    }
  }

  // Clear callback flag
  child->in_callback = false;

cleanup:
  // Always free the buffer
  if (buf && buf->base) {
    free(buf->base);
  }
}

// Stderr allocation callback
void on_stderr_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

// Stderr read callback
void on_stderr_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  uv_pipe_t* pipe = (uv_pipe_t*)stream;
  JSChildProcess* child = (JSChildProcess*)pipe->data;

  if (!child || !child->ctx) {
    goto cleanup;
  }

  // Mark in callback
  child->in_callback = true;

  JSContext* ctx = child->ctx;

  // Handle EOF or error
  if (nread < 0) {
    if (nread != UV_EOF) {
      JSRT_Debug("stderr read error: %s", uv_strerror(nread));
    }
    child->in_callback = false;
    goto cleanup;
  }

  // Ignore empty reads
  if (nread == 0) {
    child->in_callback = false;
    goto cleanup;
  }

  JSRT_Debug("Read %zd bytes from stderr", nread);

  // Check if we're in buffering mode (exec/execFile)
  if (child->buffering) {
    // Append to stderr buffer
    size_t new_size = child->stderr_size + nread;
    if (new_size > child->max_buffer) {
      JSRT_Debug("maxBuffer exceeded on stderr (%zu > %zu)", new_size, child->max_buffer);
      // Kill process due to maxBuffer exceeded
      uv_process_kill(&child->handle, SIGKILL);
      child->killed = true;
      child->in_callback = false;
      goto cleanup;
    }

    // Resize buffer if needed
    if (new_size > child->stderr_capacity) {
      size_t new_capacity = (child->stderr_capacity == 0) ? 4096 : child->stderr_capacity * 2;
      while (new_capacity < new_size) {
        new_capacity *= 2;
      }

      char* new_buffer = js_realloc(ctx, child->stderr_buffer, new_capacity);
      if (!new_buffer) {
        JSRT_Debug("Failed to allocate stderr buffer");
        child->in_callback = false;
        goto cleanup;
      }

      child->stderr_buffer = new_buffer;
      child->stderr_capacity = new_capacity;
    }

    // Copy data to buffer
    memcpy(child->stderr_buffer + child->stderr_size, buf->base, nread);
    child->stderr_size += nread;
  } else {
    // Normal mode: emit 'data' event
    JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
    if (!JS_IsException(buffer_module)) {
      JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
      if (!JS_IsException(buffer_class)) {
        JSValue from_func = JS_GetPropertyStr(ctx, buffer_class, "from");
        if (JS_IsFunction(ctx, from_func)) {
          // Create ArrayBuffer copy from received data
          JSValue array_buffer = JS_NewArrayBufferCopy(ctx, (const uint8_t*)buf->base, nread);

          // Convert to Buffer
          JSValue argv_buf[] = {array_buffer};
          JSValue data_buffer = JS_Call(ctx, from_func, buffer_class, 1, argv_buf);

          // Emit 'data' event on stderr stream
          if (!JS_IsUndefined(child->stderr_stream)) {
            JSValue data_argv[] = {data_buffer};
            emit_event(ctx, child->stderr_stream, "data", 1, data_argv);
            JS_FreeValue(ctx, data_argv[0]);
          }

          JS_FreeValue(ctx, array_buffer);
        }
        JS_FreeValue(ctx, from_func);
      }
      JS_FreeValue(ctx, buffer_class);
      JS_FreeValue(ctx, buffer_module);
    }
  }

  // Clear callback flag
  child->in_callback = false;

cleanup:
  // Always free the buffer
  if (buf && buf->base) {
    free(buf->base);
  }
}

// Stdin write callback
void on_stdin_write(uv_write_t* req, int status) {
  // Placeholder for MVP
  if (status < 0) {
    JSRT_Debug("stdin write error: %s", uv_strerror(status));
  }
  free(req);
}

// IPC callbacks (placeholders for MVP)
void on_ipc_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

void on_ipc_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  if (buf && buf->base) {
    free(buf->base);
  }
}

void on_ipc_write(uv_write_t* req, int status) {
  // Placeholder
  free(req);
}

// Timeout callback (for exec/execFile)
void on_timeout(uv_timer_t* timer) {
  JSChildProcess* child = (JSChildProcess*)timer->data;

  if (!child || !child->ctx) {
    return;
  }

  JSRT_Debug("Process %d timeout after %llu ms", child->pid, (unsigned long long)child->timeout_ms);

  // Kill the process
  uv_process_kill(&child->handle, SIGKILL);
  child->killed = true;

  // The exit callback will handle calling the exec callback with timeout error
}
