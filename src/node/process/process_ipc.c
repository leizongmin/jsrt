#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../util/debug.h"
#include "process.h"

// Simple event listener storage for process object
typedef struct EventListener {
  char* event_name;
  JSValue callback;
  struct EventListener* next;
} EventListener;

// IPC state for the current process (when forked)
typedef struct {
  uv_pipe_t* pipe;
  JSContext* ctx;
  JSValue process_obj;
  bool connected;
  bool reading;

  // Event listeners for process object
  EventListener* listeners;

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

// Find the IPC socket fd (searches fds 3-20 for a socket)
static int find_ipc_fd() {
  for (int fd = 3; fd < 20; fd++) {
    struct stat st;
    if (fstat(fd, &st) != 0) {
      continue;  // fd doesn't exist
    }

    // Check if it's a socket
    if (S_ISSOCK(st.st_mode)) {
      JSRT_Debug("find_ipc_fd: found socket at fd %d", fd);
      return fd;
    }
  }

  JSRT_Debug("find_ipc_fd: no socket found in fds 3-19");
  return -1;
}

// Check if IPC channel exists
static bool has_ipc_channel() {
  int ipc_fd = find_ipc_fd();
  if (ipc_fd < 0) {
    JSRT_Debug("has_ipc_channel: no IPC socket found");
    return false;
  }

  JSRT_Debug("has_ipc_channel: IPC socket found at fd %d", ipc_fd);
  return true;
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
  // JS_ParseJSON requires null-terminated string
  char* null_terminated = malloc(length + 1);
  if (!null_terminated) {
    JSRT_Debug("Failed to allocate memory for IPC message in child");
    return;
  }
  memcpy(null_terminated, data, length);
  null_terminated[length] = '\0';

  JSValue message = JS_ParseJSON(ctx, null_terminated, length, "<ipc>");
  free(null_terminated);

  if (JS_IsException(message)) {
    JSRT_Debug("Failed to parse IPC message in child");
    JS_FreeValue(ctx, message);
    return;
  }

  // Emit 'message' event on process object using our simple emitter
  EventListener* listener = state->listeners;
  while (listener) {
    if (strcmp(listener->event_name, "message") == 0) {
      JSValue result = JS_Call(ctx, listener->callback, state->process_obj, 1, &message);
      JS_FreeValue(ctx, result);
    }
    listener = listener->next;
  }
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

      // Emit 'disconnect' event using our simple emitter
      EventListener* listener = state->listeners;
      while (listener) {
        if (strcmp(listener->event_name, "disconnect") == 0) {
          JSValue result = JS_Call(state->ctx, listener->callback, state->process_obj, 0, NULL);
          JS_FreeValue(state->ctx, result);
        }
        listener = listener->next;
      }
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

// Simple process.on() implementation for IPC events
static JSValue js_process_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!g_ipc_state || argc < 2) {
    return JS_UNDEFINED;
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, event_name);
    return JS_ThrowTypeError(ctx, "Callback must be a function");
  }

  // Add listener to list
  EventListener* listener = malloc(sizeof(EventListener));
  listener->event_name = strdup(event_name);
  listener->callback = JS_DupValue(ctx, callback);
  listener->next = g_ipc_state->listeners;
  g_ipc_state->listeners = listener;

  JS_FreeCString(ctx, event_name);
  return this_val;  // Return process for chaining
}

// Simple process.emit() implementation for IPC events
static JSValue js_process_emit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!g_ipc_state || argc < 1) {
    return JS_FALSE;
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name) {
    return JS_EXCEPTION;
  }

  // Find and call listeners for this event
  EventListener* listener = g_ipc_state->listeners;
  bool emitted = false;
  while (listener) {
    if (strcmp(listener->event_name, event_name) == 0) {
      // Call the callback with remaining arguments
      JSValue result = JS_Call(ctx, listener->callback, this_val, argc - 1, argv + 1);
      JS_FreeValue(ctx, result);
      emitted = true;
    }
    listener = listener->next;
  }

  JS_FreeCString(ctx, event_name);
  return JS_NewBool(ctx, emitted);
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

  // Emit disconnect event using our simple emitter
  EventListener* listener = g_ipc_state->listeners;
  while (listener) {
    if (strcmp(listener->event_name, "disconnect") == 0) {
      JSValue result = JS_Call(ctx, listener->callback, g_ipc_state->process_obj, 0, NULL);
      JS_FreeValue(ctx, result);
    }
    listener = listener->next;
  }

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
    JSRT_Debug("Child process: No IPC channel detected on fd 3");
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
  // The '1' flag enables IPC mode for passing handles (not just data)
  int result = uv_pipe_init(rt->uv_loop, g_ipc_state->pipe, 1);
  if (result < 0) {
    JSRT_Debug("Failed to init IPC pipe: %s", uv_strerror(result));
    free(g_ipc_state->pipe);
    free(g_ipc_state->read_buffer);
    free(g_ipc_state);
    g_ipc_state = NULL;
    return;
  }

  // Find the actual IPC socket fd
  int ipc_fd = find_ipc_fd();
  if (ipc_fd < 0) {
    JSRT_Debug("Cannot find IPC socket fd");
    uv_close((uv_handle_t*)g_ipc_state->pipe, NULL);
    free(g_ipc_state->read_buffer);
    free(g_ipc_state);
    g_ipc_state = NULL;
    return;
  }

  JSRT_Debug("Opening IPC socket fd %d as pipe...", ipc_fd);

  // Open the IPC socket fd as a pipe
  result = uv_pipe_open(g_ipc_state->pipe, ipc_fd);
  JSRT_Debug("uv_pipe_open result: %d (%s)", result, result < 0 ? uv_strerror(result) : "success");
  if (result < 0) {
    JSRT_Debug("Failed to open IPC pipe on fd 3: %s", uv_strerror(result));
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

  // Add simple event emitter methods to process object
  JS_SetPropertyStr(ctx, process_obj, "on", JS_NewCFunction(ctx, js_process_on, "on", 2));
  JS_SetPropertyStr(ctx, process_obj, "emit", JS_NewCFunction(ctx, js_process_emit, "emit", 1));

  JSRT_Debug("Child process: Event emitter methods added");

  // Start reading from IPC channel
  JSRT_Debug("About to call uv_read_start on pipe...");
  result = uv_read_start((uv_stream_t*)g_ipc_state->pipe, alloc_buffer, on_ipc_read);
  JSRT_Debug("uv_read_start returned: %d", result);
  if (result == 0) {
    g_ipc_state->reading = true;
    JSRT_Debug("Child process: IPC channel started successfully");
  } else {
    JSRT_Debug("Failed to start IPC reading: %s", uv_strerror(result));
  }
  JSRT_Debug("IPC setup complete, returning from jsrt_process_setup_ipc");
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
    // Free event listeners
    EventListener* listener = g_ipc_state->listeners;
    while (listener) {
      EventListener* next = listener->next;
      free(listener->event_name);
      JS_FreeValue(ctx, listener->callback);
      free(listener);
      listener = next;
    }

    JS_FreeValue(ctx, g_ipc_state->process_obj);
    free(g_ipc_state);
    g_ipc_state = NULL;
  }
}
