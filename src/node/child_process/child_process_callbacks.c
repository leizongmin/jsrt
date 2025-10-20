#include "../../util/debug.h"
#include "child_process_internal.h"

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

  // Create Buffer from received data
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

  // Create Buffer from received data
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
  JSRT_Debug("Process timeout");
  // TODO: Kill process and emit timeout error
}
