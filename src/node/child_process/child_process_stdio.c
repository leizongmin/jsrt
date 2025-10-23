#include "../../util/debug.h"
#include "child_process_internal.h"

// Simple stream wrapper for pipes
typedef struct {
  uv_pipe_t* pipe;
  JSContext* ctx;
  JSValue child_obj;
  bool is_stdout;  // true for stdout, false for stderr
} PipeStreamData;

// Setup stdio pipes for the child process
// For MVP: support 'pipe', 'ignore', 'inherit' modes
int setup_stdio_pipes(JSContext* ctx, JSChildProcess* child, const JSChildProcessOptions* options) {
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);

  // Initialize stdio containers
  for (int i = 0; i < options->stdio_count; i++) {
    uv_stdio_container_t* container = (uv_stdio_container_t*)&options->stdio[i];

    if (i < 3) {
      // stdin/stdout/stderr
      // Check if already configured by parse_spawn_options()
      if (container->flags == UV_INHERIT_FD) {
        // inherit mode - already configured with fd
        // No need to create pipes, just keep the UV_INHERIT_FD flag
        continue;
      } else if (container->flags == UV_IGNORE) {
        // ignore mode - already configured
        continue;
      } else {
        // pipe mode (UV_CREATE_PIPE or default) - create pipe
        uv_pipe_t* pipe = malloc(sizeof(uv_pipe_t));
        if (!pipe) {
          JSRT_Debug("Failed to allocate pipe for stdio %d", i);
          return -1;
        }

        int result = uv_pipe_init(rt->uv_loop, pipe, 0);  // 0 = not IPC
        if (result < 0) {
          JSRT_Debug("uv_pipe_init failed for stdio %d: %s", i, uv_strerror(result));
          free(pipe);
          return -1;
        }

        // Store pipe reference
        if (i == 0) {
          child->stdin_pipe = pipe;
          child->handles_to_close++;
        } else if (i == 1) {
          child->stdout_pipe = pipe;
          child->handles_to_close++;
        } else if (i == 2) {
          child->stderr_pipe = pipe;
          child->handles_to_close++;
        }

        // Configure stdio container
        if (i == 0) {
          // stdin: readable from parent perspective
          container->flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
        } else {
          // stdout/stderr: writable from parent perspective
          container->flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
        }
        container->data.stream = (uv_stream_t*)pipe;
      }
    } else if (i == 3) {
      // IPC channel (stdio[3])
      IPCChannelState* ipc = create_ipc_channel(ctx, child, rt->uv_loop);
      if (!ipc) {
        JSRT_Debug("Failed to create IPC channel");
        return -1;
      }

      child->ipc_channel = ipc;
      child->connected = true;
      child->handles_to_close++;

      // Configure stdio container for IPC (bidirectional pipe)
      container->flags = UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE;
      container->data.stream = (uv_stream_t*)ipc->pipe;
    }
  }

  return 0;
}

// Start reading from stdout/stderr pipes after spawn
int start_stdio_reading(JSContext* ctx, JSChildProcess* child) {
  int result;

  // Start reading from stdout
  if (child->stdout_pipe) {
    child->stdout_pipe->data = child;
    result = uv_read_start((uv_stream_t*)child->stdout_pipe, on_stdout_alloc, on_stdout_read);
    if (result < 0) {
      JSRT_Debug("Failed to start reading stdout: %s", uv_strerror(result));
      return -1;
    }
  }

  // Start reading from stderr
  if (child->stderr_pipe) {
    child->stderr_pipe->data = child;
    result = uv_read_start((uv_stream_t*)child->stderr_pipe, on_stderr_alloc, on_stderr_read);
    if (result < 0) {
      JSRT_Debug("Failed to start reading stderr: %s", uv_strerror(result));
      return -1;
    }
  }

  return 0;
}

// Close all stdio pipes
void close_stdio_pipes(JSChildProcess* child) {
  JSRT_Debug("Closing stdio pipes");

  // Close stdin
  if (child->stdin_pipe && !uv_is_closing((uv_handle_t*)child->stdin_pipe)) {
    uv_close((uv_handle_t*)child->stdin_pipe, child_process_close_callback);
  }

  // Close stdout
  if (child->stdout_pipe && !uv_is_closing((uv_handle_t*)child->stdout_pipe)) {
    // Stop reading first
    uv_read_stop((uv_stream_t*)child->stdout_pipe);
    uv_close((uv_handle_t*)child->stdout_pipe, child_process_close_callback);
  }

  // Close stderr
  if (child->stderr_pipe && !uv_is_closing((uv_handle_t*)child->stderr_pipe)) {
    // Stop reading first
    uv_read_stop((uv_stream_t*)child->stderr_pipe);
    uv_close((uv_handle_t*)child->stderr_pipe, child_process_close_callback);
  }

  // Close IPC channel
  if (child->ipc_channel) {
    disconnect_ipc_channel(child->ipc_channel);
    child->ipc_channel = NULL;
  }
}

// Create stdin stream (simple stub that emits data events)
JSValue create_stdin_stream(JSContext* ctx, uv_pipe_t* pipe) {
  // For MVP, just create a plain object with a write stub
  JSValue stream = JS_NewObject(ctx);

  // TODO: Implement proper writable stream
  // For now, stdin is not writable

  return stream;
}

// Create stdout stream that emits 'data' events
JSValue create_stdout_stream(JSContext* ctx, uv_pipe_t* pipe) {
  // For MVP, create a simple EventEmitter-like object
  JSValue stream = JS_NewObject(ctx);

  // Add EventEmitter methods
  add_event_emitter_methods(ctx, stream);

  return stream;
}

// Create stderr stream that emits 'data' events
JSValue create_stderr_stream(JSContext* ctx, uv_pipe_t* pipe) {
  // For MVP, create a simple EventEmitter-like object
  JSValue stream = JS_NewObject(ctx);

  // Add EventEmitter methods
  add_event_emitter_methods(ctx, stream);

  return stream;
}
