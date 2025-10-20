#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "../../util/debug.h"
#include "process.h"

// IPC state for the current process (when forked)
typedef struct {
  uv_pipe_t* pipe;
  JSContext* ctx;
  JSValue process_obj;
  bool connected;
  bool reading;

  // Read buffer for incoming messages
  char* read_buffer;
  size_t read_buffer_size;
  size_t read_buffer_capacity;
  uint32_t expected_length;
  bool reading_header;
} ProcessIPCState;

static ProcessIPCState* g_ipc_state = NULL;

// Forward declarations
static void on_ipc_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
static void on_ipc_write(uv_write_t* req, int status);
static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

// Check if IPC channel exists (stdio fd 3)
static bool has_ipc_channel() {
  // Check if fd 3 exists and is a pipe
  int fd = 3;
  int flags = fcntl(fd, F_GETFL);
  return flags != -1;
}

// Allocation callback
static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

// Process received IPC message
static void process_ipc_message(ProcessIPCState* state, const char* data, size_t length) {
  JSContext* ctx = state->ctx;

  // Parse JSON message
  JSValue message = JS_ParseJSON(ctx, data, length, "<ipc>");
  if (JS_IsException(message)) {
    JSRT_Debug("Failed to parse IPC message in child");
    JS_FreeValue(ctx, message);
    return;
  }

  // Emit 'message' event on process object
  JSValue emit = JS_GetPropertyStr(ctx, state->process_obj, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[2] = {JS_NewString(ctx, "message"), message};
    JSValue result = JS_Call(ctx, emit, state->process_obj, 2, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);
  JS_FreeValue(ctx, message);
}

// IPC read callback
static void on_ipc_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  ProcessIPCState* state = (ProcessIPCState*)stream->data;
  if (!state) {
    if (buf->base)
      free(buf->base);
    return;
  }

  if (nread < 0) {
    // EOF or error
    if (buf->base)
      free(buf->base);

    if (nread != UV_EOF) {
      JSRT_Debug("IPC read error in child: %s", uv_strerror(nread));
    }

    // Disconnect
    uv_read_stop(stream);
    state->reading = false;

    if (state->connected) {
      state->connected = false;

      // Emit 'disconnect' event
      JSValue emit = JS_GetPropertyStr(state->ctx, state->process_obj, "emit");
      if (JS_IsFunction(state->ctx, emit)) {
        JSValue args[1] = {JS_NewString(state->ctx, "disconnect")};
        JSValue result = JS_Call(state->ctx, emit, state->process_obj, 1, args);
        JS_FreeValue(state->ctx, result);
        JS_FreeValue(state->ctx, args[0]);
      }
      JS_FreeValue(state->ctx, emit);
    }

    return;
  }

  if (nread == 0) {
    if (buf->base)
      free(buf->base);
    return;
  }

  // Append to read buffer
  size_t needed = state->read_buffer_size + nread;
  if (needed > state->read_buffer_capacity) {
    size_t new_capacity = state->read_buffer_capacity * 2;
    while (new_capacity < needed)
      new_capacity *= 2;
    char* new_buffer = realloc(state->read_buffer, new_capacity);
    if (!new_buffer) {
      free(buf->base);
      return;
    }
    state->read_buffer = new_buffer;
    state->read_buffer_capacity = new_capacity;
  }

  memcpy(state->read_buffer + state->read_buffer_size, buf->base, nread);
  state->read_buffer_size += nread;
  free(buf->base);

  // Process complete messages
  while (state->read_buffer_size > 0) {
    if (state->reading_header) {
      if (state->read_buffer_size < 4)
        break;

      // Read length header
      uint32_t length;
      memcpy(&length, state->read_buffer, 4);
      state->expected_length = length;
      state->reading_header = false;

      // Remove header
      memmove(state->read_buffer, state->read_buffer + 4, state->read_buffer_size - 4);
      state->read_buffer_size -= 4;
    } else {
      // Reading body
      if (state->read_buffer_size < state->expected_length)
        break;

      // Process message
      process_ipc_message(state, state->read_buffer, state->expected_length);

      // Remove message
      size_t remaining = state->read_buffer_size - state->expected_length;
      if (remaining > 0) {
        memmove(state->read_buffer, state->read_buffer + state->expected_length, remaining);
      }
      state->read_buffer_size = remaining;
      state->reading_header = true;
      state->expected_length = 0;
    }
  }
}

// IPC write callback
static void on_ipc_write(uv_write_t* req, int status) {
  // Free the buffer
  uv_buf_t* buf = (uv_buf_t*)(req + 1);
  free(buf->base);
  free(req);

  if (status < 0) {
    JSRT_Debug("IPC write error in child: %s", uv_strerror(status));
  }
}

// process.send(message[, sendHandle][, options][, callback])
static JSValue js_process_send(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!g_ipc_state || !g_ipc_state->connected) {
    return JS_ThrowInternalError(ctx, "Channel closed");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "process.send() requires at least 1 argument");
  }

  JSValue message = argv[0];

  // Serialize message to JSON
  JSValue json_str = JS_JSONStringify(ctx, message, JS_UNDEFINED, JS_UNDEFINED);
  if (JS_IsException(json_str)) {
    return JS_EXCEPTION;
  }

  size_t length;
  const char* str = JS_ToCStringLen(ctx, &length, json_str);
  JS_FreeValue(ctx, json_str);

  if (!str) {
    return JS_EXCEPTION;
  }

  // Build message with length header
  size_t total_length = 4 + length;
  char* full_message = malloc(total_length);
  if (!full_message) {
    JS_FreeCString(ctx, str);
    return JS_ThrowOutOfMemory(ctx);
  }

  uint32_t length_header = (uint32_t)length;
  memcpy(full_message, &length_header, 4);
  memcpy(full_message + 4, str, length);
  JS_FreeCString(ctx, str);

  // Send message
  uv_write_t* req = malloc(sizeof(uv_write_t) + sizeof(uv_buf_t));
  uv_buf_t* buf = (uv_buf_t*)(req + 1);
  buf->base = full_message;
  buf->len = total_length;

  int result = uv_write(req, (uv_stream_t*)g_ipc_state->pipe, buf, 1, on_ipc_write);
  if (result < 0) {
    free(full_message);
    free(req);
    return JS_FALSE;
  }

  return JS_TRUE;
}

// process.disconnect()
static JSValue js_process_disconnect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!g_ipc_state || !g_ipc_state->connected) {
    return JS_UNDEFINED;
  }

  g_ipc_state->connected = false;

  // Stop reading
  if (g_ipc_state->reading) {
    uv_read_stop((uv_stream_t*)g_ipc_state->pipe);
    g_ipc_state->reading = false;
  }

  // Close pipe
  uv_close((uv_handle_t*)g_ipc_state->pipe, NULL);

  // Emit disconnect event
  JSValue emit = JS_GetPropertyStr(ctx, g_ipc_state->process_obj, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[1] = {JS_NewString(ctx, "disconnect")};
    JSValue result = JS_Call(ctx, emit, g_ipc_state->process_obj, 1, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);

  return JS_UNDEFINED;
}

// Get process.connected property
static JSValue js_process_get_connected(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!g_ipc_state) {
    return JS_FALSE;
  }
  return JS_NewBool(ctx, g_ipc_state->connected);
}

// Setup IPC for forked child process
void jsrt_process_setup_ipc(JSContext* ctx, JSValue process_obj, JSRT_Runtime* rt) {
  // Check if IPC channel exists
  if (!has_ipc_channel()) {
    return;
  }

  JSRT_Debug("Child process: IPC channel detected on fd 3");

  // Allocate IPC state
  g_ipc_state = malloc(sizeof(ProcessIPCState));
  if (!g_ipc_state) {
    return;
  }

  memset(g_ipc_state, 0, sizeof(ProcessIPCState));
  g_ipc_state->ctx = ctx;
  g_ipc_state->process_obj = JS_DupValue(ctx, process_obj);
  g_ipc_state->connected = true;
  g_ipc_state->reading_header = true;
  g_ipc_state->read_buffer_capacity = 8192;
  g_ipc_state->read_buffer = malloc(g_ipc_state->read_buffer_capacity);

  // Initialize pipe from existing fd 3
  // Note: fd 3 is already managed by libuv when spawned with IPC, so we just wrap it
  g_ipc_state->pipe = malloc(sizeof(uv_pipe_t));
  if (!g_ipc_state->pipe) {
    free(g_ipc_state->read_buffer);
    free(g_ipc_state);
    g_ipc_state = NULL;
    return;
  }

  // Initialize a new pipe in IPC mode
  int result = uv_pipe_init(rt->uv_loop, g_ipc_state->pipe, 1);
  if (result < 0) {
    JSRT_Debug("Failed to init IPC pipe: %s", uv_strerror(result));
    free(g_ipc_state->pipe);
    free(g_ipc_state->read_buffer);
    free(g_ipc_state);
    g_ipc_state = NULL;
    return;
  }

  // Open the existing fd 3 as a pipe
  // We need to dup the fd first to avoid conflicts with libuv's management
  int ipc_fd = dup(3);
  if (ipc_fd < 0) {
    JSRT_Debug("Failed to dup fd 3: %s", strerror(errno));
    uv_close((uv_handle_t*)g_ipc_state->pipe, NULL);
    free(g_ipc_state->read_buffer);
    free(g_ipc_state);
    g_ipc_state = NULL;
    return;
  }

  result = uv_pipe_open(g_ipc_state->pipe, ipc_fd);
  if (result < 0) {
    JSRT_Debug("Failed to open IPC pipe on duped fd: %s", uv_strerror(result));
    close(ipc_fd);
    uv_close((uv_handle_t*)g_ipc_state->pipe, NULL);
    free(g_ipc_state->read_buffer);
    free(g_ipc_state);
    g_ipc_state = NULL;
    return;
  }

  g_ipc_state->pipe->data = g_ipc_state;

  // Add IPC methods to process object
  JS_SetPropertyStr(ctx, process_obj, "send", JS_NewCFunction(ctx, js_process_send, "send", 2));
  JS_SetPropertyStr(ctx, process_obj, "disconnect", JS_NewCFunction(ctx, js_process_disconnect, "disconnect", 0));

  // Add connected property
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "connected"),
                          JS_NewCFunction(ctx, js_process_get_connected, "get connected", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  // Make process an EventEmitter (load events module and add methods)
  JSValue events_module = JS_Eval(ctx, "require('node:events')", 23, "<ipc>", JS_EVAL_TYPE_GLOBAL);
  if (!JS_IsException(events_module)) {
    JSValue EventEmitter = JS_GetPropertyStr(ctx, events_module, "EventEmitter");
    if (!JS_IsException(EventEmitter)) {
      JSValue proto = JS_GetPropertyStr(ctx, EventEmitter, "prototype");
      if (!JS_IsException(proto)) {
        // Copy EventEmitter methods to process
        JSValue on = JS_GetPropertyStr(ctx, proto, "on");
        JSValue emit = JS_GetPropertyStr(ctx, proto, "emit");
        JSValue once = JS_GetPropertyStr(ctx, proto, "once");
        JSValue off = JS_GetPropertyStr(ctx, proto, "off");

        if (!JS_IsException(on))
          JS_SetPropertyStr(ctx, process_obj, "on", on);
        if (!JS_IsException(emit))
          JS_SetPropertyStr(ctx, process_obj, "emit", emit);
        if (!JS_IsException(once))
          JS_SetPropertyStr(ctx, process_obj, "once", once);
        if (!JS_IsException(off))
          JS_SetPropertyStr(ctx, process_obj, "off", off);

        JS_FreeValue(ctx, proto);
      }
      JS_FreeValue(ctx, EventEmitter);
    }
    JS_FreeValue(ctx, events_module);
  }

  // Start reading from IPC channel
  result = uv_read_start((uv_stream_t*)g_ipc_state->pipe, alloc_buffer, on_ipc_read);
  if (result == 0) {
    g_ipc_state->reading = true;
    JSRT_Debug("Child process: IPC channel started successfully");
  } else {
    JSRT_Debug("Failed to start IPC reading: %s", uv_strerror(result));
  }
}

// Cleanup IPC state
void jsrt_process_cleanup_ipc(JSContext* ctx) {
  if (g_ipc_state) {
    if (g_ipc_state->read_buffer) {
      free(g_ipc_state->read_buffer);
    }
    if (g_ipc_state->pipe) {
      if (g_ipc_state->reading) {
        uv_read_stop((uv_stream_t*)g_ipc_state->pipe);
      }
      uv_close((uv_handle_t*)g_ipc_state->pipe, NULL);
      free(g_ipc_state->pipe);
    }
    JS_FreeValue(ctx, g_ipc_state->process_obj);
    free(g_ipc_state);
    g_ipc_state = NULL;
  }
}
