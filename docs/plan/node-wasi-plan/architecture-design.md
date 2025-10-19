# WASI Module Architecture Design
Created: 2025-10-19
Updated: 2025-10-19

## Task 1.5: WASI Class Architecture

### C Data Structures

#### Main WASI Instance Structure
```c
// Opaque WASI instance handle
typedef struct jsrt_wasi_instance jsrt_wasi_instance_t;

struct jsrt_wasi_instance {
  // JavaScript context
  JSContext* ctx;

  // WASM instance (JavaScript object)
  JSValue wasm_instance;  // WebAssembly.Instance

  // Configuration options
  char** args;                  // Command-line arguments
  size_t args_count;            // Number of arguments

  char** env;                   // Environment variables (key=value format)
  size_t env_count;             // Number of environment variables

  jsrt_wasi_preopen_t* preopens;  // Preopened directories
  size_t preopen_count;          // Number of preopens

  int stdin_fd;                  // Stdin file descriptor (default: 0)
  int stdout_fd;                 // Stdout file descriptor (default: 1)
  int stderr_fd;                 // Stderr file descriptor (default: 2)

  bool return_on_exit;           // Return instead of process.exit()
  const char* version;           // WASI version ("preview1" or "unstable")

  // State tracking
  bool initialized;              // Has initialize() been called?
  bool started;                  // Has start() been called?
  int exit_code;                 // Exit code from _start (if return_on_exit=true)

  // WAMR integration
  wasm_instance_t* wamr_instance;  // WAMR instance handle
  wasm_exec_env_t* exec_env;       // WAMR execution environment
};
```

#### Preopen Directory Structure
```c
typedef struct {
  char* virtual_path;    // Virtual path in WASM (e.g., "/sandbox")
  char* real_path;       // Real filesystem path (e.g., "/tmp/wasm")
  wasi_fd_t fd;          // Preopened file descriptor
} jsrt_wasi_preopen_t;
```

#### WASI Options (from JavaScript)
```c
typedef struct {
  JSValue args;          // Array<string>
  JSValue env;           // Object
  JSValue preopens;      // Object
  int stdin_fd;          // number (default: 0)
  int stdout_fd;         // number (default: 1)
  int stderr_fd;         // number (default: 2)
  bool return_on_exit;   // boolean (default: false)
  const char* version;   // string (default: "preview1")
} jsrt_wasi_options_t;
```

### Memory Management Rules

#### Allocation Ownership
1. **WASI Instance**: Owns all internal allocations
   - args array and strings
   - env array and strings
   - preopens array and paths
   - Freed in destructor

2. **JavaScript Objects**: Reference counted
   - wasm_instance: Use JS_DupValue/JS_FreeValue
   - Options objects: Temporary, no ownership

3. **WAMR Objects**: Managed by WAMR
   - wamr_instance: Freed via wasm_instance_delete()
   - exec_env: Freed via wasm_runtime_destroy_exec_env()

#### Lifetime Rules
```c
// Creation
jsrt_wasi_instance_t* wasi = jsrt_wasi_new(ctx, options);  // Allocates all

// Usage
jsrt_wasi_get_import_object(wasi);  // No allocations, returns JSValue
jsrt_wasi_start(wasi, instance);    // Borrows instance, no ownership transfer

// Destruction
jsrt_wasi_free(wasi);  // Frees all internal allocations
```

### State Machine

#### States
1. **CREATED**: Constructor called, options parsed
2. **BOUND**: getImportObject() called, instance provided to start()/initialize()
3. **INITIALIZED**: initialize() called successfully (for reactors)
4. **STARTED**: start() called successfully (for commands)
5. **EXITED**: WASM module exited (exit code available)
6. **ERROR**: Error occurred (exception pending)

#### State Transitions
```
CREATED
  ├─> getImportObject() ─> CREATED (no state change)
  ├─> start(instance) ─> STARTED ─> EXITED
  └─> initialize(instance) ─> INITIALIZED

INITIALIZED
  └─> start(instance) ─> ERROR (invalid transition)

STARTED
  └─> initialize(instance) ─> ERROR (invalid transition)

EXITED
  ├─> start() ─> ERROR (already exited)
  └─> initialize() ─> ERROR (already exited)
```

#### State Validation
```c
// Before start()
if (wasi->started || wasi->initialized) {
  JS_ThrowTypeError(ctx, "WASI instance already started/initialized");
  return JS_EXCEPTION;
}

// Before initialize()
if (wasi->initialized || wasi->started) {
  JS_ThrowTypeError(ctx, "WASI instance already initialized/started");
  return JS_EXCEPTION;
}
```

### Error Handling Strategy

#### Error Categories
1. **Constructor Errors** (TypeError):
   - Invalid options object
   - Invalid types for args/env/preopens
   - Invalid file descriptors
   - Unknown WASI version

2. **Import Object Errors** (TypeError):
   - WASI instance already bound to different WASM instance

3. **Lifecycle Errors** (TypeError):
   - Invalid WebAssembly.Instance
   - Missing _start or _initialize export
   - Invalid state transitions
   - Instance already started/initialized

4. **Runtime Errors** (RuntimeError):
   - WASM execution errors
   - WASI syscall failures (propagated from WAMR)
   - Memory access violations

#### Error Propagation Pattern
```c
JSValue jsrt_wasi_start(JSContext* ctx, jsrt_wasi_instance_t* wasi, JSValue instance) {
  // Validation
  if (!JS_IsObject(instance)) {
    JS_ThrowTypeError(ctx, "Invalid WebAssembly.Instance");
    return JS_EXCEPTION;
  }

  // State check
  if (wasi->started) {
    JS_ThrowTypeError(ctx, "WASI instance already started");
    return JS_EXCEPTION;
  }

  // Get _start export
  JSValue exports = JS_GetPropertyStr(ctx, instance, "exports");
  JSValue start_fn = JS_GetPropertyStr(ctx, exports, "_start");

  if (!JS_IsFunction(ctx, start_fn)) {
    JS_FreeValue(ctx, start_fn);
    JS_FreeValue(ctx, exports);
    JS_ThrowTypeError(ctx, "WebAssembly.Instance missing _start export");
    return JS_EXCEPTION;
  }

  // Call _start
  JSValue result = JS_Call(ctx, start_fn, JS_UNDEFINED, 0, NULL);

  JS_FreeValue(ctx, start_fn);
  JS_FreeValue(ctx, exports);

  if (JS_IsException(result)) {
    return JS_EXCEPTION;  // Propagate WASM error
  }

  wasi->started = true;
  JS_FreeValue(ctx, result);

  // Handle exit
  if (!wasi->return_on_exit) {
    // Call process.exit(wasi->exit_code)
    // Not shown here - implementation in Phase 5
  }

  return wasi->return_on_exit ? JS_NewInt32(ctx, wasi->exit_code) : JS_UNDEFINED;
}
```

## Task 1.6: File System Preopen Strategy

### Preopen Concept
WASI preopens provide sandboxed filesystem access:
- Map virtual paths in WASM to real host paths
- WASM module can only access preopened directories
- Security boundary: path traversal prevention

### Data Structure Design
```c
typedef struct {
  char* virtual_path;    // Virtual path in WASM (e.g., "/sandbox")
  char* real_path;       // Real filesystem path (e.g., "/tmp/wasm")
  wasi_fd_t fd;          // Preopened file descriptor (>= 3)
  bool read_only;        // Read-only flag (future extension)
} jsrt_wasi_preopen_t;
```

### JavaScript to C Conversion

#### Input Format (JavaScript)
```javascript
{
  '/sandbox': '/tmp/wasm-sandbox',
  '/data': '/var/data/wasm',
  '/config': '/etc/wasm-config'
}
```

#### Conversion Algorithm
```c
int jsrt_wasi_parse_preopens(JSContext* ctx, JSValue preopens_obj,
                              jsrt_wasi_preopen_t** out_preopens, size_t* out_count) {
  // Get property names
  JSPropertyEnum* props;
  uint32_t count;
  if (JS_GetOwnPropertyNames(ctx, &props, &count, preopens_obj,
                             JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
    return -1;
  }

  // Allocate preopens array
  jsrt_wasi_preopen_t* preopens = malloc(count * sizeof(jsrt_wasi_preopen_t));
  if (!preopens) {
    js_free(ctx, props);
    return -1;
  }

  // Parse each entry
  for (uint32_t i = 0; i < count; i++) {
    // Get virtual path (key)
    JSValue key = JS_AtomToString(ctx, props[i].atom);
    const char* virtual_path = JS_ToCString(ctx, key);
    JS_FreeValue(ctx, key);

    // Get real path (value)
    JSValue value = JS_GetProperty(ctx, preopens_obj, props[i].atom);
    const char* real_path = JS_ToCString(ctx, value);
    JS_FreeValue(ctx, value);

    // Validate paths
    if (!virtual_path || !real_path) {
      // Cleanup and return error
      jsrt_wasi_free_preopens(preopens, i);
      js_free(ctx, props);
      return -1;
    }

    // Validate real path exists and is directory
    if (!jsrt_wasi_validate_preopen_path(real_path)) {
      JS_ThrowTypeError(ctx, "Preopen path does not exist or is not a directory: %s", real_path);
      // Cleanup...
      return -1;
    }

    // Store preopen
    preopens[i].virtual_path = strdup(virtual_path);
    preopens[i].real_path = strdup(real_path);
    preopens[i].fd = 3 + i;  // FDs start at 3 (0=stdin, 1=stdout, 2=stderr)

    JS_FreeCString(ctx, virtual_path);
    JS_FreeCString(ctx, real_path);
  }

  js_free(ctx, props);

  *out_preopens = preopens;
  *out_count = count;
  return 0;
}
```

### Security Validation

#### Path Traversal Prevention
```c
bool jsrt_wasi_validate_preopen_path(const char* path) {
  // Check if path exists
  struct stat st;
  if (stat(path, &st) != 0) {
    return false;
  }

  // Check if directory
  if (!S_ISDIR(st.st_mode)) {
    return false;
  }

  // Resolve to canonical path (prevents symlink attacks)
  char resolved[PATH_MAX];
  if (!realpath(path, resolved)) {
    return false;
  }

  // Additional checks:
  // - No traversal outside allowed boundaries
  // - No special files (/dev, /proc, /sys on Linux)
  // - Configurable whitelist/blacklist (future)

  return true;
}
```

#### Runtime Access Control
```c
// Called by WASI fd_* functions
bool jsrt_wasi_validate_path_access(jsrt_wasi_instance_t* wasi,
                                     wasi_fd_t fd, const char* path) {
  // Find preopen for this FD
  jsrt_wasi_preopen_t* preopen = NULL;
  for (size_t i = 0; i < wasi->preopen_count; i++) {
    if (wasi->preopens[i].fd == fd) {
      preopen = &wasi->preopens[i];
      break;
    }
  }

  if (!preopen) {
    return false;  // Invalid FD
  }

  // Resolve requested path relative to preopen
  char full_path[PATH_MAX];
  snprintf(full_path, sizeof(full_path), "%s/%s", preopen->real_path, path);

  // Canonicalize
  char resolved[PATH_MAX];
  if (!realpath(full_path, resolved)) {
    return false;
  }

  // Check if resolved path is within preopen boundary
  size_t preopen_len = strlen(preopen->real_path);
  if (strncmp(resolved, preopen->real_path, preopen_len) != 0) {
    return false;  // Path traversal attempt!
  }

  return true;
}
```

### Memory Management
```c
void jsrt_wasi_free_preopens(jsrt_wasi_preopen_t* preopens, size_t count) {
  if (!preopens) return;

  for (size_t i = 0; i < count; i++) {
    free(preopens[i].virtual_path);
    free(preopens[i].real_path);
  }
  free(preopens);
}
```

## Task 1.7: Environment Variable Handling

### Data Structure
```c
// Environment variables stored as "KEY=VALUE" strings
typedef struct {
  char** env;        // Array of "KEY=VALUE" strings
  size_t env_count;  // Number of environment variables
} jsrt_wasi_env_t;
```

### JavaScript to C Conversion

#### Input Format (JavaScript)
```javascript
{
  'HOME': '/home/user',
  'PATH': '/usr/bin:/bin',
  'LANG': 'en_US.UTF-8'
}
```

#### Conversion Algorithm
```c
int jsrt_wasi_parse_env(JSContext* ctx, JSValue env_obj,
                        char*** out_env, size_t* out_count) {
  // Get property names
  JSPropertyEnum* props;
  uint32_t count;
  if (JS_GetOwnPropertyNames(ctx, &props, &count, env_obj,
                             JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
    return -1;
  }

  // Allocate env array
  char** env = malloc((count + 1) * sizeof(char*));  // +1 for NULL terminator
  if (!env) {
    js_free(ctx, props);
    return -1;
  }

  // Parse each entry
  for (uint32_t i = 0; i < count; i++) {
    // Get key (environment variable name)
    JSValue key = JS_AtomToString(ctx, props[i].atom);
    const char* name = JS_ToCString(ctx, key);
    JS_FreeValue(ctx, key);

    // Get value
    JSValue value = JS_GetProperty(ctx, env_obj, props[i].atom);
    const char* val = JS_ToCString(ctx, value);
    JS_FreeValue(ctx, value);

    // Validate
    if (!name || !val) {
      jsrt_wasi_free_env(env, i);
      js_free(ctx, props);
      return -1;
    }

    // Format as "KEY=VALUE"
    size_t len = strlen(name) + 1 + strlen(val) + 1;
    env[i] = malloc(len);
    snprintf(env[i], len, "%s=%s", name, val);

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, val);
  }

  env[count] = NULL;  // NULL terminator

  js_free(ctx, props);

  *out_env = env;
  *out_count = count;
  return 0;
}
```

### Encoding Strategy

#### UTF-8 Everywhere
- JavaScript strings are UTF-16 internally
- QuickJS C API converts to UTF-8
- WASI expects UTF-8 strings
- No additional encoding needed

#### Special Character Handling
```c
// Validate environment variable
bool jsrt_wasi_validate_env_var(const char* name, const char* value) {
  // Name validation
  if (!name || !name[0]) {
    return false;  // Empty name
  }

  // No '=' in name
  if (strchr(name, '=')) {
    return false;
  }

  // No NULL bytes in name or value
  if (strlen(name) != strcspn(name, "\0") ||
      strlen(value) != strcspn(value, "\0")) {
    return false;
  }

  // Passed validation
  return true;
}
```

### Memory Management
```c
void jsrt_wasi_free_env(char** env, size_t count) {
  if (!env) return;

  for (size_t i = 0; i < count; i++) {
    free(env[i]);
  }
  free(env);
}
```

## Task 1.8: Argument Passing Mechanism

### Data Structure
```c
typedef struct {
  char** args;        // Array of argument strings
  size_t args_count;  // Number of arguments
} jsrt_wasi_args_t;
```

### JavaScript to C Conversion

#### Input Format (JavaScript)
```javascript
['program', 'arg1', 'arg2', '--flag', 'value']
```

#### Conversion Algorithm
```c
int jsrt_wasi_parse_args(JSContext* ctx, JSValue args_array,
                         char*** out_args, size_t* out_count) {
  // Check if array
  if (!JS_IsArray(ctx, args_array)) {
    JS_ThrowTypeError(ctx, "args must be an array");
    return -1;
  }

  // Get array length
  JSValue len_val = JS_GetPropertyStr(ctx, args_array, "length");
  uint32_t count;
  if (JS_ToUint32(ctx, &count, len_val) < 0) {
    JS_FreeValue(ctx, len_val);
    return -1;
  }
  JS_FreeValue(ctx, len_val);

  // Allocate args array
  char** args = malloc((count + 1) * sizeof(char*));  // +1 for NULL
  if (!args) {
    return -1;
  }

  // Parse each argument
  for (uint32_t i = 0; i < count; i++) {
    JSValue item = JS_GetPropertyUint32(ctx, args_array, i);

    // Convert to string
    const char* arg_str = JS_ToCString(ctx, item);
    JS_FreeValue(ctx, item);

    if (!arg_str) {
      jsrt_wasi_free_args(args, i);
      return -1;
    }

    // Duplicate string
    args[i] = strdup(arg_str);
    JS_FreeCString(ctx, arg_str);

    if (!args[i]) {
      jsrt_wasi_free_args(args, i);
      return -1;
    }
  }

  args[count] = NULL;  // NULL terminator

  *out_args = args;
  *out_count = count;
  return 0;
}
```

### Encoding Strategy

#### UTF-8 Support
- Same as environment variables
- QuickJS handles UTF-16 to UTF-8 conversion
- WASI receives UTF-8 strings

#### Unicode Argument Testing
```c
// Test with Unicode arguments
const char* test_args[] = {
  "program",
  "Hello, 世界",      // Chinese
  "Привет мир",       // Russian
  "مرحبا بالعالم",    // Arabic
  "こんにちは世界",    // Japanese
  "🚀 Emoji test",     // Emoji
  NULL
};
```

### Validation
```c
bool jsrt_wasi_validate_arg(const char* arg) {
  if (!arg) {
    return false;
  }

  // No NULL bytes in argument
  if (strlen(arg) != strcspn(arg, "\0")) {
    return false;
  }

  // Argument can be empty string (valid)
  return true;
}
```

### Memory Management
```c
void jsrt_wasi_free_args(char** args, size_t count) {
  if (!args) return;

  for (size_t i = 0; i < count; i++) {
    free(args[i]);
  }
  free(args);
}
```

## File Structure Plan

### Header Files
```
src/node/wasi/
├── wasi.h              # Public API and structures
├── wasi_internal.h     # Internal implementation details
├── wasi_imports.h      # WASI import object creation
└── wasi_syscalls.h     # WASI syscall implementations
```

### Implementation Files
```
src/node/wasi/
├── wasi_module.c       # Module registration and exports
├── wasi_constructor.c  # WASI class constructor
├── wasi_lifecycle.c    # start() and initialize() methods
├── wasi_imports.c      # getImportObject() implementation
├── wasi_syscalls.c     # WASI syscall implementations
└── wasi_options.c      # Options parsing (args, env, preopens)
```

## Integration Points

### WAMR Integration
- Use existing jsrt_wasm_* functions from src/wasm/runtime.c
- Leverage WAMR WASI built-in implementation
- No direct WAMR modifications needed

### Module System Integration
- Register in src/module/loaders/builtin_loader.c
- Add to src/node/node_modules.c registry
- Support both "node:wasi" and "jsrt:wasi"

### WebAssembly Integration
- Use existing WebAssembly.Instance constructor
- Import object passed via second argument
- Memory access via instance.exports.memory

## Summary

### Design Complete
✅ Task 1.5: WASI class architecture designed
✅ Task 1.6: Preopen strategy designed
✅ Task 1.7: Environment variable handling designed
✅ Task 1.8: Argument passing designed

### Key Design Decisions
1. **State machine**: Enforce single start/initialize per instance
2. **Memory ownership**: WASI instance owns all internal allocations
3. **Security**: Path validation and traversal prevention
4. **Encoding**: UTF-8 everywhere (handled by QuickJS)
5. **Error handling**: TypeError for API errors, RuntimeError for WASM errors

### Ready for Phase 2
Next: Implement core infrastructure (WASI class, constructors, data structures)
