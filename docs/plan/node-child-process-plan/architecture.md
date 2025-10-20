# Node.js child_process Module - Architecture Design

## Overview

This document describes the architecture for implementing the Node.js `child_process` module in jsrt, based on existing patterns from dgram, events, and fs modules.

## Key Design Patterns from Existing Modules

### 1. Module Structure Pattern (from dgram)

**File Organization:**
```
src/node/child_process/
├── child_process_module.c       # Module registration (CommonJS/ESM)
├── child_process_internal.h     # Shared structures and declarations
├── child_process_spawn.c        # spawn() implementation
├── child_process_exec.c         # exec() and execFile() implementation
├── child_process_sync.c         # Sync APIs (spawnSync, execSync, execFileSync)
├── child_process_fork.c         # fork() implementation
├── child_process_ipc.c          # IPC channel implementation
├── child_process_stdio.c        # Stdio pipe management
├── child_process_options.c      # Options parsing
├── child_process_callbacks.c    # libuv callback handlers
├── child_process_finalizers.c   # Cleanup and finalizers
└── child_process_errors.c       # Error handling
```

### 2. Class Definition Pattern (from dgram)

**ChildProcess Class:**
```c
// Class definition
static JSClassDef js_child_process_class = {
    "ChildProcess",
    .finalizer = js_child_process_finalizer,
};

// Class ID
JSClassID js_child_process_class_id;

// Type tag for cleanup callback identification
#define CHILD_PROCESS_TYPE_TAG 0x43505243  // 'CPRC' in hex
```

### 3. State Management Pattern (from dgram)

**ChildProcess State Structure:**
```c
typedef struct {
    uint32_t type_tag;              // Must be first field for cleanup callback
    JSContext* ctx;
    JSValue child_obj;              // JavaScript ChildProcess object (EventEmitter)
    uv_process_t handle;            // libuv process handle

    // Process state
    int pid;
    bool spawned;
    bool exited;
    bool killed;
    bool connected;                 // IPC channel active
    bool in_callback;               // Prevent finalization during callback
    int exit_code;
    int signal_code;

    // Stdio pipes (uv_pipe_t*)
    uv_pipe_t* stdin_pipe;
    uv_pipe_t* stdout_pipe;
    uv_pipe_t* stderr_pipe;
    uv_pipe_t* ipc_pipe;            // For fork() IPC channel

    // Stream objects
    JSValue stdin_stream;           // Writable stream
    JSValue stdout_stream;          // Readable stream
    JSValue stderr_stream;          // Readable stream

    // Close tracking
    int close_count;                // Number of handles that need to close
    int handles_to_close;           // Expected number of handles to close

    // Options (for spawn)
    char* cwd;
    char** env;
    char** args;
    char* file;

} JSChildProcess;
```

### 4. EventEmitter Integration Pattern (from dgram)

**Adding EventEmitter Methods:**
```c
// In child_process_spawn.c - when creating ChildProcess instance
void add_event_emitter_methods(JSContext* ctx, JSValue obj);

// Emit events using:
JSValue argv[] = {JS_NewString(ctx, "exit"), exit_code, signal_code};
JSValue emit_func = JS_GetPropertyStr(ctx, child->child_obj, "emit");
if (JS_IsFunction(ctx, emit_func)) {
    JS_Call(ctx, emit_func, child->child_obj, 3, argv);
}
JS_FreeValue(ctx, emit_func);
// ... free argv
```

### 5. libuv Callback Pattern (from dgram)

**Exit Callback:**
```c
// In child_process_callbacks.c
void on_process_exit(uv_process_t* handle, int64_t exit_status, int term_signal) {
    JSChildProcess* child = (JSChildProcess*)handle->data;

    if (!child || !child->ctx || child->exited) {
        return;
    }

    // Mark in callback to prevent finalization
    child->in_callback = true;

    JSContext* ctx = child->ctx;

    // Set exit information
    child->exited = true;
    child->exit_code = (int)exit_status;
    child->signal_code = term_signal;

    // Emit 'exit' event
    JSValue exit_code = JS_NewInt32(ctx, child->exit_code);
    JSValue signal_val = term_signal ? JS_NewString(ctx, signal_name(term_signal)) : JS_NULL;

    JSValue argv[] = {JS_NewString(ctx, "exit"), exit_code, signal_val};
    JSValue emit_func = JS_GetPropertyStr(ctx, child->child_obj, "emit");
    if (JS_IsFunction(ctx, emit_func)) {
        JS_Call(ctx, emit_func, child->child_obj, 3, argv);
    }
    JS_FreeValue(ctx, emit_func);
    JS_FreeValue(ctx, argv[0]);
    JS_FreeValue(ctx, argv[1]);
    JS_FreeValue(ctx, argv[2]);

    // Close stdio pipes
    close_all_pipes(child);

    // Clear callback flag
    child->in_callback = false;
}
```

### 6. Memory Management Pattern (from dgram)

**Finalizer:**
```c
void js_child_process_finalizer(JSRuntime* rt, JSValue val) {
    JSChildProcess* child = JS_GetOpaque(val, js_child_process_class_id);
    if (!child) {
        return;
    }

    // Don't finalize if we're in a callback
    if (child->in_callback) {
        return;
    }

    // Free resources
    if (child->cwd) free(child->cwd);
    if (child->env) free_string_array(child->env);
    if (child->args) free_string_array(child->args);
    if (child->file) free(child->file);

    // Free streams
    JS_FreeValue(child->ctx, child->stdin_stream);
    JS_FreeValue(child->ctx, child->stdout_stream);
    JS_FreeValue(child->ctx, child->stderr_stream);
    JS_FreeValue(child->ctx, child->child_obj);

    js_free_rt(rt, child);
}
```

### 7. Module Registration Pattern (from node_modules.c)

**In node_modules.c:**
```c
// Add to dependencies
static const char* child_process_deps[] = {"events", "stream", "buffer", NULL};

// Add to module registry
static NodeModuleEntry node_modules[] = {
    // ... existing modules
    {"child_process", JSRT_InitNodeChildProcess, js_node_child_process_init,
     child_process_deps, false, {0}},
    // ...
};
```

**In child_process_module.c:**
```c
// CommonJS initialization
JSValue JSRT_InitNodeChildProcess(JSContext* ctx) {
    JSValue cp = JS_NewObject(ctx);

    // Register class
    JS_NewClassID(&js_child_process_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_child_process_class_id, &js_child_process_class);

    // Export functions
    JS_SetPropertyStr(ctx, cp, "spawn", JS_NewCFunction(ctx, js_child_process_spawn, "spawn", 3));
    JS_SetPropertyStr(ctx, cp, "exec", JS_NewCFunction(ctx, js_child_process_exec, "exec", 3));
    JS_SetPropertyStr(ctx, cp, "execFile", JS_NewCFunction(ctx, js_child_process_exec_file, "execFile", 4));
    JS_SetPropertyStr(ctx, cp, "fork", JS_NewCFunction(ctx, js_child_process_fork, "fork", 3));
    JS_SetPropertyStr(ctx, cp, "spawnSync", JS_NewCFunction(ctx, js_child_process_spawn_sync, "spawnSync", 3));
    JS_SetPropertyStr(ctx, cp, "execSync", JS_NewCFunction(ctx, js_child_process_exec_sync, "execSync", 2));
    JS_SetPropertyStr(ctx, cp, "execFileSync", JS_NewCFunction(ctx, js_child_process_exec_file_sync, "execFileSync", 3));

    // Create ChildProcess prototype
    JSValue child_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, child_proto, "kill", JS_NewCFunction(ctx, js_child_process_kill, "kill", 1));
    JS_SetPropertyStr(ctx, child_proto, "send", JS_NewCFunction(ctx, js_child_process_send, "send", 2));
    JS_SetPropertyStr(ctx, child_proto, "disconnect", JS_NewCFunction(ctx, js_child_process_disconnect, "disconnect", 0));
    JS_SetPropertyStr(ctx, child_proto, "ref", JS_NewCFunction(ctx, js_child_process_ref, "ref", 0));
    JS_SetPropertyStr(ctx, child_proto, "unref", JS_NewCFunction(ctx, js_child_process_unref, "unref", 0));
    JS_SetClassProto(ctx, js_child_process_class_id, child_proto);

    return cp;
}

// ES Module initialization
int js_node_child_process_init(JSContext* ctx, JSModuleDef* m) {
    JSValue cp = JSRT_InitNodeChildProcess(ctx);

    JS_SetModuleExport(ctx, m, "spawn", JS_GetPropertyStr(ctx, cp, "spawn"));
    JS_SetModuleExport(ctx, m, "exec", JS_GetPropertyStr(ctx, cp, "exec"));
    JS_SetModuleExport(ctx, m, "execFile", JS_GetPropertyStr(ctx, cp, "execFile"));
    JS_SetModuleExport(ctx, m, "fork", JS_GetPropertyStr(ctx, cp, "fork"));
    JS_SetModuleExport(ctx, m, "spawnSync", JS_GetPropertyStr(ctx, cp, "spawnSync"));
    JS_SetModuleExport(ctx, m, "execSync", JS_GetPropertyStr(ctx, cp, "execSync"));
    JS_SetModuleExport(ctx, m, "execFileSync", JS_GetPropertyStr(ctx, cp, "execFileSync"));
    JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, cp));

    JS_FreeValue(ctx, cp);
    return 0;
}
```

## libuv Process API Integration

### Core libuv Structures

```c
// From libuv documentation
typedef struct uv_process_options_s {
    uv_exit_cb exit_cb;           // Callback when process exits
    const char* file;             // Path to program to execute
    char** args;                  // Command line arguments (NULL-terminated)
    char** env;                   // Environment variables (NULL-terminated)
    const char* cwd;              // Current working directory
    unsigned int flags;           // Process flags (UV_PROCESS_DETACHED, etc.)

    int stdio_count;              // Number of stdio containers
    uv_stdio_container_t* stdio;  // Stdio configuration

    uv_uid_t uid;                 // User ID (POSIX only)
    uv_gid_t gid;                 // Group ID (POSIX only)
} uv_process_options_t;

typedef struct uv_stdio_container_s {
    uv_stdio_flags flags;         // UV_IGNORE, UV_CREATE_PIPE, UV_INHERIT_FD, etc.
    union {
        uv_stream_t* stream;      // Pipe to use
        int fd;                   // File descriptor to inherit
    } data;
} uv_stdio_container_t;
```

### Spawn Flow

```c
// 1. Create ChildProcess instance
JSChildProcess* child = create_child_process(ctx);

// 2. Parse options and build uv_process_options_t
uv_process_options_t options;
memset(&options, 0, sizeof(options));
options.exit_cb = on_process_exit;
options.file = parsed_file;
options.args = parsed_args;
options.env = parsed_env;
options.cwd = parsed_cwd;

// 3. Setup stdio pipes
uv_stdio_container_t stdio[4];  // stdin, stdout, stderr, ipc (optional)
setup_stdio(child, stdio, &options);

// 4. Spawn process
int result = uv_spawn(loop, &child->handle, &options);
if (result < 0) {
    // Emit error event
    emit_error(ctx, child, result);
    return child_obj;
}

// 5. Set PID and emit 'spawn' event
child->pid = child->handle.pid;
emit_spawn(ctx, child);
```

## Error Handling

### Node.js Error Codes (from node_modules.h)

```c
typedef enum {
    NODE_ERR_INVALID_ARG_TYPE,
    NODE_ERR_MISSING_ARGS,
    NODE_ERR_OUT_OF_RANGE,
    NODE_ERR_INVALID_ARG_VALUE,
    NODE_ERR_INVALID_CALLBACK,
    NODE_ERR_SYSTEM_ERROR,
    // Add child_process specific:
    NODE_ERR_CHILD_PROCESS_STDIO_MAXBUFFER,
    NODE_ERR_CHILD_PROCESS_IPC_DISABLED,
} NodeErrorCode;
```

### Error Creation Pattern

```c
JSValue create_spawn_error(JSContext* ctx, int uv_error, const char* path) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, uv_err_name(uv_error)));
    JS_SetPropertyStr(ctx, error, "errno", JS_NewInt32(ctx, uv_error));
    JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, "spawn"));
    JS_SetPropertyStr(ctx, error, "path", JS_NewString(ctx, path));

    char message[512];
    snprintf(message, sizeof(message), "spawn %s %s", path, uv_strerror(uv_error));
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, message));

    return error;
}
```

## Stdio Stream Integration

### Stream Creation Pattern

```c
// For stdout (Readable stream)
JSValue create_readable_stream(JSContext* ctx, uv_pipe_t* pipe) {
    JSValue stream_module = JSRT_LoadNodeModuleCommonJS(ctx, "stream");
    JSValue Readable = JS_GetPropertyStr(ctx, stream_module, "Readable");

    // Create Readable instance
    JSValue stream = JS_CallConstructor(ctx, Readable, 0, NULL);

    // Attach pipe data handler
    // ... setup uv_read_start with callback that calls stream.push(data)

    JS_FreeValue(ctx, Readable);
    JS_FreeValue(ctx, stream_module);
    return stream;
}

// For stdin (Writable stream)
JSValue create_writable_stream(JSContext* ctx, uv_pipe_t* pipe) {
    JSValue stream_module = JSRT_LoadNodeModuleCommonJS(ctx, "stream");
    JSValue Writable = JS_GetPropertyStr(ctx, stream_module, "Writable");

    // Create Writable instance with _write method
    // ... setup that calls uv_write on the pipe

    JS_FreeValue(ctx, Writable);
    JS_FreeValue(ctx, stream_module);
    return stream;
}
```

## IPC Channel Design

### Message Serialization Format

```
[4 bytes: message length (uint32_t, little-endian)]
[N bytes: JSON-serialized message body]
```

### IPC Pipe Setup

```c
// In parent process (fork())
uv_pipe_t* ipc_pipe = js_malloc(ctx, sizeof(uv_pipe_t));
uv_pipe_init(loop, ipc_pipe, 1);  // 1 = IPC mode

// Add to stdio[3] (Node.js convention)
stdio[3].flags = UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE;
stdio[3].data.stream = (uv_stream_t*)ipc_pipe;

// In child process - detect IPC pipe via environment or stdio
// Setup process.send() and process.on('message')
```

## Testing Strategy

### Unit Tests Structure
```
test/node/child_process/
├── test_spawn_basic.js          # Basic spawn functionality
├── test_spawn_stdio.js          # Stdio pipe tests
├── test_spawn_errors.js         # Error handling
├── test_exec.js                 # exec() tests
├── test_exec_file.js            # execFile() tests
├── test_sync.js                 # Synchronous API tests
├── test_fork.js                 # fork() and IPC tests
├── test_signals.js              # Signal handling
└── fixtures/
    ├── echo.js                  # Echo script for testing
    ├── exit_code.js             # Script that exits with code
    ├── ipc_child.js             # Child process for IPC testing
    └── long_running.js          # For timeout tests
```

## Implementation Phases

1. **Phase 1: Research & Design** ✅ (This document)
2. **Phase 2: Core Infrastructure** - Build JSChildProcess class, stdio, options parsing
3. **Phase 3: spawn()** - Implement basic process spawning
4. **Phase 4: exec/execFile()** - Add buffering and shell execution
5. **Phase 5: Sync APIs** - Implement blocking variants
6. **Phase 6: IPC & fork()** - Add inter-process communication
7. **Phase 7: Advanced Features** - Platform-specific options, signals
8. **Phase 8: Integration & Documentation** - Final polish and docs

## Key Implementation Notes

1. **Memory Safety**: Use ASAN (`make jsrt_m`) throughout development
2. **Event Loop Integration**: All async operations must use JSRT_Runtime->uv_loop
3. **Stream Integration**: Reuse existing stream infrastructure, don't reinvent
4. **Error Compatibility**: Match Node.js error codes and messages exactly
5. **Cross-Platform**: Use libuv abstractions, test on Linux/macOS minimum

## References

- Node.js child_process documentation: https://nodejs.org/api/child_process.html
- libuv process documentation: http://docs.libuv.org/en/v1.x/process.html
- Existing jsrt modules: src/node/dgram/, src/node/events/, src/node/fs/
