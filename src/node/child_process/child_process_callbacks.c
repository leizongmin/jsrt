#include "../../util/debug.h"
#include "child_process_internal.h"

// TODO: Implement process exit callback
void on_process_exit(uv_process_t* handle, int64_t exit_status, int term_signal) {
  JSRT_Debug("Process exited with status %lld, signal %d", exit_status, term_signal);
}

// TODO: Implement stdout callbacks
void on_stdout_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

void on_stdout_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  if (buf && buf->base) {
    free(buf->base);
  }
}

// TODO: Implement stderr callbacks
void on_stderr_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

void on_stderr_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  if (buf && buf->base) {
    free(buf->base);
  }
}

// TODO: Implement stdin write callback
void on_stdin_write(uv_write_t* req, int status) {
  // Placeholder
}

// TODO: Implement IPC callbacks
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
}

// TODO: Implement timeout callback
void on_timeout(uv_timer_t* timer) {
  JSRT_Debug("Process timeout");
}
