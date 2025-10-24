#include <stdlib.h>
#include <string.h>
#include "../../util/debug.h"
#include "child_process_internal.h"

// IPC message format:
// [4 bytes: message length (uint32_t, little-endian)]
// [N bytes: JSON-serialized message body]

// Forward declarations
static void on_ipc_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
static void on_ipc_write(uv_write_t* req, int status);
static void on_ipc_close(uv_handle_t* handle);
static void process_ipc_message(IPCChannelState* state, const char* data, size_t length);
static void flush_ipc_queue(IPCChannelState* state);

// Allocation callback for libuv
static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

// Create IPC channel
IPCChannelState* create_ipc_channel(JSContext* ctx, JSChildProcess* child, uv_loop_t* loop) {
  IPCChannelState* state = js_mallocz(ctx, sizeof(IPCChannelState));
  if (!state) {
    return NULL;
  }

  state->pipe = js_mallocz(ctx, sizeof(uv_pipe_t));
  if (!state->pipe) {
    js_free(ctx, state);
    return NULL;
  }

  // Initialize pipe in IPC mode (1 = IPC)
  int result = uv_pipe_init(loop, state->pipe, 1);
  if (result < 0) {
    js_free(ctx, state->pipe);
    js_free(ctx, state);
    return NULL;
  }

  state->pipe->data = state;
  state->child = child;
  state->connected = true;
  state->reading_header = true;
  state->expected_length = 0;

  // Allocate initial read buffer
  state->read_buffer_capacity = 8192;
  state->read_buffer = malloc(state->read_buffer_capacity);
  state->read_buffer_size = 0;

  return state;
}

// Start reading from IPC channel
int start_ipc_reading(IPCChannelState* state) {
  if (!state || !state->pipe || state->reading) {
    return -1;
  }

  // Get the underlying fd for debug
  uv_os_fd_t fd;
  int fd_result = uv_fileno((uv_handle_t*)state->pipe, &fd);
  JSRT_Debug("[PARENT] start_ipc_reading: pipe fd = %d (uv_fileno result: %d)", fd, fd_result);

  int result = uv_read_start((uv_stream_t*)state->pipe, alloc_buffer, on_ipc_read);
  if (result == 0) {
    state->reading = true;
  }

  return result;
}

// IPC read callback
static void on_ipc_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  IPCChannelState* state = (IPCChannelState*)stream->data;
  if (!state || !state->child) {
    if (buf->base) {
      free(buf->base);
    }
    return;
  }

  if (nread < 0) {
    // EOF or error
    if (buf->base) {
      free(buf->base);
    }

    if (nread != UV_EOF) {
      JSRT_Debug("IPC read error: %s", uv_strerror(nread));
    }

    // Stop reading and emit disconnect
    uv_read_stop(stream);
    state->reading = false;

    if (state->connected) {
      state->connected = false;
      state->child->connected = false;

      // Emit 'disconnect' event (no additional arguments)
      JSContext* ctx = state->child->ctx;
      emit_event(ctx, state->child->child_obj, "disconnect", 0, NULL);
    }

    return;
  }

  if (nread == 0) {
    // EAGAIN or EWOULDBLOCK
    if (buf->base) {
      free(buf->base);
    }
    return;
  }

  // Append to read buffer
  size_t needed = state->read_buffer_size + nread;
  if (needed > state->read_buffer_capacity) {
    // Grow buffer
    size_t new_capacity = state->read_buffer_capacity * 2;
    while (new_capacity < needed) {
      new_capacity *= 2;
    }
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
      // Need at least 4 bytes for length header
      if (state->read_buffer_size < 4) {
        break;
      }

      // Read length (little-endian uint32_t)
      uint32_t length;
      memcpy(&length, state->read_buffer, 4);

      state->expected_length = length;
      state->reading_header = false;

      // Remove header from buffer
      memmove(state->read_buffer, state->read_buffer + 4, state->read_buffer_size - 4);
      state->read_buffer_size -= 4;
    } else {
      // Reading message body
      if (state->read_buffer_size < state->expected_length) {
        // Not enough data yet
        break;
      }

      // We have a complete message
      process_ipc_message(state, state->read_buffer, state->expected_length);

      // Remove message from buffer
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

// Process a complete IPC message
static void process_ipc_message(IPCChannelState* state, const char* data, size_t length) {
  JSContext* ctx = state->child->ctx;

  // Parse JSON message
  // JS_ParseJSON requires null-terminated string, so create one
  char* null_terminated = malloc(length + 1);
  if (!null_terminated) {
    JSRT_Debug("Failed to allocate memory for IPC message");
    return;
  }
  memcpy(null_terminated, data, length);
  null_terminated[length] = '\0';

  JSRT_Debug("Parsing IPC message: length=%zu, data='%s'", length, null_terminated);
  JSValue message = JS_ParseJSON(ctx, null_terminated, length, "<ipc>");
  free(null_terminated);

  if (JS_IsException(message)) {
    JSRT_Debug("Failed to parse IPC message");
    js_std_dump_error(ctx);
    JS_FreeValue(ctx, message);
    return;
  }

  // Emit 'message' event
  JSValue event_args[] = {message};
  emit_event(ctx, state->child->child_obj, "message", 1, event_args);

  JS_FreeValue(ctx, message);
}

// Serialize JSValue to JSON
static char* serialize_message(JSContext* ctx, JSValue message, size_t* out_length) {
  // Convert to JSON string
  JSValue json_str = JS_JSONStringify(ctx, message, JS_UNDEFINED, JS_UNDEFINED);
  if (JS_IsException(json_str)) {
    return NULL;
  }

  size_t length;
  const char* str = JS_ToCStringLen(ctx, &length, json_str);
  JS_FreeValue(ctx, json_str);

  if (!str) {
    return NULL;
  }

  // Duplicate string (JS_FreeCString will be called on str)
  char* result = malloc(length);
  if (!result) {
    JS_FreeCString(ctx, str);
    return NULL;
  }

  memcpy(result, str, length);
  *out_length = length;

  JS_FreeCString(ctx, str);
  return result;
}

// Write callback
static void on_ipc_write(uv_write_t* req, int status) {
  IPCChannelState* state = (IPCChannelState*)req->data;

  // Free write request and buffer
  uv_buf_t* buf = (uv_buf_t*)(req + 1);
  free(buf->base);
  free(req);

  if (!state) {
    return;
  }

  state->writing = false;

  if (status < 0) {
    JSRT_Debug("IPC write error: %s", uv_strerror(status));
    return;
  }

  // Flush queue if there are pending messages
  flush_ipc_queue(state);
}

// Flush write queue
static void flush_ipc_queue(IPCChannelState* state) {
  if (!state || state->writing || !state->queue_head) {
    return;
  }

  // Take first message from queue
  IPCQueueEntry* entry = state->queue_head;
  state->queue_head = entry->next;
  if (!state->queue_head) {
    state->queue_tail = NULL;
  }

  // Allocate write request with buffer embedded
  uv_write_t* req = malloc(sizeof(uv_write_t) + sizeof(uv_buf_t));
  uv_buf_t* buf = (uv_buf_t*)(req + 1);

  buf->base = entry->data;
  buf->len = entry->length;
  req->data = state;

  state->writing = true;

  int result = uv_write(req, (uv_stream_t*)state->pipe, buf, 1, on_ipc_write);
  if (result < 0) {
    JSRT_Debug("uv_write failed: %s", uv_strerror(result));
    free(entry->data);
    free(req);
    state->writing = false;
  }

  // Call callback if provided
  if (!JS_IsUndefined(entry->callback)) {
    JSContext* ctx = state->child->ctx;
    JSValue result_val = result == 0 ? JS_UNDEFINED : JS_NewInt32(ctx, result);
    JSValue cb_result = JS_Call(ctx, entry->callback, JS_UNDEFINED, 1, &result_val);
    JS_FreeValue(ctx, cb_result);
    JS_FreeValue(ctx, result_val);
    JS_FreeValue(ctx, entry->callback);
  }

  free(entry);
}

// Send message on IPC channel
int send_ipc_message(IPCChannelState* state, JSValue message, JSValue callback) {
  if (!state || !state->connected) {
    return -1;
  }

  JSContext* ctx = state->child->ctx;

  // Serialize message
  size_t body_length;
  char* body = serialize_message(ctx, message, &body_length);
  if (!body) {
    return -1;
  }

  // Build message with length header
  size_t total_length = 4 + body_length;
  char* full_message = malloc(total_length);
  if (!full_message) {
    free(body);
    return -1;
  }

  // Write length header (little-endian)
  uint32_t length_header = (uint32_t)body_length;
  memcpy(full_message, &length_header, 4);
  memcpy(full_message + 4, body, body_length);
  free(body);

  // Queue message
  IPCQueueEntry* entry = malloc(sizeof(IPCQueueEntry));
  if (!entry) {
    free(full_message);
    return -1;
  }

  entry->data = full_message;
  entry->length = total_length;
  entry->callback = JS_DupValue(ctx, callback);
  entry->next = NULL;

  // Add to queue
  if (state->queue_tail) {
    state->queue_tail->next = entry;
    state->queue_tail = entry;
  } else {
    state->queue_head = state->queue_tail = entry;
  }

  // Flush if not currently writing
  flush_ipc_queue(state);

  return 0;
}

// Close IPC channel
static void on_ipc_close(uv_handle_t* handle) {
  IPCChannelState* state = (IPCChannelState*)handle->data;
  if (!state) {
    return;
  }

  JSContext* ctx = state->child->ctx;
  JSChildProcess* child = state->child;

  // Free read buffer
  if (state->read_buffer) {
    free(state->read_buffer);
    state->read_buffer = NULL;
  }

  // Free write queue
  while (state->queue_head) {
    IPCQueueEntry* entry = state->queue_head;
    state->queue_head = entry->next;
    free(entry->data);
    if (!JS_IsUndefined(entry->callback)) {
      JS_FreeValue(ctx, entry->callback);
    }
    free(entry);
  }

  // Free pipe
  js_free(ctx, state->pipe);
  state->pipe = NULL;

  // Clear child's reference to prevent use-after-free
  if (child && child->ipc_channel == state) {
    child->ipc_channel = NULL;
  }

  // Free state
  js_free(ctx, state);
}

// Disconnect IPC channel
void disconnect_ipc_channel(IPCChannelState* state) {
  if (!state || !state->connected) {
    return;
  }

  // Check if pipe is already closing/closed to prevent use-after-free
  if (uv_is_closing((uv_handle_t*)state->pipe)) {
    return;
  }

  state->connected = false;
  state->child->connected = false;

  // Stop reading
  if (state->reading) {
    uv_read_stop((uv_stream_t*)state->pipe);
    state->reading = false;
  }

  // Close pipe
  uv_close((uv_handle_t*)state->pipe, on_ipc_close);
}
