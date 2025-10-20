#ifndef JSRT_NODE_CHILD_PROCESS_INTERNAL_H
#define JSRT_NODE_CHILD_PROCESS_INTERNAL_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../runtime.h"
#include "../node_modules.h"

// Type tag for cleanup callback identification
#define CHILD_PROCESS_TYPE_TAG 0x43505243  // 'CPRC' in hex

// Forward declarations
typedef struct JSChildProcess JSChildProcess;
typedef struct JSChildProcessOptions JSChildProcessOptions;
typedef struct JSSendRequest JSSendRequest;

// Class ID for ChildProcess
extern JSClassID js_child_process_class_id;

// ChildProcess state structure
struct JSChildProcess {
  uint32_t type_tag;  // Must be first field for cleanup callback
  JSContext* ctx;
  JSValue child_obj;    // JavaScript ChildProcess object (EventEmitter)
  uv_process_t handle;  // libuv process handle

  // Process state
  int pid;
  bool spawned;
  bool exited;
  bool killed;
  bool connected;    // IPC channel active
  bool in_callback;  // Prevent finalization during callback
  int exit_code;
  int signal_code;

  // Stdio pipes
  uv_pipe_t* stdin_pipe;
  uv_pipe_t* stdout_pipe;
  uv_pipe_t* stderr_pipe;
  uv_pipe_t* ipc_pipe;  // For fork() IPC channel

  // Stream objects
  JSValue stdin_stream;   // Writable stream
  JSValue stdout_stream;  // Readable stream
  JSValue stderr_stream;  // Readable stream

  // Close tracking
  int close_count;       // Number of handles that need to close
  int handles_to_close;  // Expected number of handles to close

  // Buffering state (for exec/execFile)
  bool buffering;  // True if buffering output
  char* stdout_buffer;
  char* stderr_buffer;
  size_t stdout_size;
  size_t stderr_size;
  size_t stdout_capacity;
  size_t stderr_capacity;
  size_t max_buffer;      // maxBuffer option
  JSValue exec_callback;  // Callback for exec/execFile

  // Timeout tracking (for exec/execFile)
  uv_timer_t* timeout_timer;
  uint64_t timeout_ms;

  // Options (owned strings - must be freed)
  char* cwd;
  char** env;
  char** args;
  char* file;
  int uid;  // POSIX only (-1 if not set)
  int gid;  // POSIX only (-1 if not set)
};

// Spawn options (temporary structure used during spawn)
struct JSChildProcessOptions {
  const char* file;
  char** args;
  char** env;
  const char* cwd;
  int uid;  // POSIX only (-1 if not set)
  int gid;  // POSIX only (-1 if not set)
  bool detached;
  bool windows_hide;
  const char* shell;              // NULL or shell path
  uv_stdio_container_t stdio[4];  // stdin, stdout, stderr, optional IPC
  int stdio_count;
  uint64_t timeout;         // milliseconds (0 = no timeout)
  size_t max_buffer;        // for exec/execFile
  const char* kill_signal;  // for timeout
};

// Send request for IPC
struct JSSendRequest {
  uv_write_t req;
  JSContext* ctx;
  JSValue child_obj;
  JSValue callback;
  char* data;
  size_t len;
};

// ===== Module Functions (child_process_module.c) =====
JSValue JSRT_InitNodeChildProcess(JSContext* ctx);
int js_node_child_process_init(JSContext* ctx, JSModuleDef* m);

// ===== Spawn Functions (child_process_spawn.c) =====
JSValue js_child_process_spawn(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// ===== Exec Functions (child_process_exec.c) =====
JSValue js_child_process_exec(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_child_process_exec_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// ===== Sync Functions (child_process_sync.c) =====
JSValue js_child_process_spawn_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_child_process_exec_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_child_process_exec_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// ===== Fork Functions (child_process_fork.c) =====
JSValue js_child_process_fork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// ===== ChildProcess Methods =====
JSValue js_child_process_kill(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_child_process_send(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_child_process_disconnect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_child_process_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_child_process_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// ===== Options Parsing (child_process_options.c) =====
int parse_spawn_options(JSContext* ctx, JSValueConst options_obj, JSChildProcessOptions* options);
void free_spawn_options(JSChildProcessOptions* options);

// ===== Stdio Management (child_process_stdio.c) =====
int setup_stdio_pipes(JSContext* ctx, JSChildProcess* child, const JSChildProcessOptions* options);
void close_stdio_pipes(JSChildProcess* child);
JSValue create_stdin_stream(JSContext* ctx, uv_pipe_t* pipe);
JSValue create_stdout_stream(JSContext* ctx, uv_pipe_t* pipe);
JSValue create_stderr_stream(JSContext* ctx, uv_pipe_t* pipe);

// ===== IPC Functions (child_process_ipc.c) =====
int setup_ipc_channel(JSContext* ctx, JSChildProcess* child);
int send_ipc_message(JSContext* ctx, JSChildProcess* child, JSValue message, JSValue callback);
void close_ipc_channel(JSChildProcess* child);

// ===== Callbacks (child_process_callbacks.c) =====
void on_process_exit(uv_process_t* handle, int64_t exit_status, int term_signal);
void on_stdout_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
void on_stdout_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
void on_stderr_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
void on_stderr_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
void on_stdin_write(uv_write_t* req, int status);
void on_ipc_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
void on_ipc_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
void on_ipc_write(uv_write_t* req, int status);
void on_timeout(uv_timer_t* timer);

// ===== Finalizers (child_process_finalizers.c) =====
void js_child_process_finalizer(JSRuntime* rt, JSValue val);
void child_process_close_callback(uv_handle_t* handle);

// ===== Error Handling (child_process_errors.c) =====
JSValue create_spawn_error(JSContext* ctx, int uv_error, const char* path, const char* syscall);
JSValue create_exec_error(JSContext* ctx, int exit_code, const char* signal, const char* cmd);

// ===== Helper Functions =====
void add_event_emitter_methods(JSContext* ctx, JSValue obj);
const char* signal_name(int signal_num);
int signal_from_name(const char* name);
void emit_event(JSContext* ctx, JSValue obj, const char* event, int argc, JSValue* argv);
void free_string_array(char** arr);
char** copy_string_array(const char** arr);

#endif  // JSRT_NODE_CHILD_PROCESS_INTERNAL_H
