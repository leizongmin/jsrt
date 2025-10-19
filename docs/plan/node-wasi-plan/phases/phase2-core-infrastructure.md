** DONE [#A] Phase 2: Core Infrastructure [S][R:MED][C:COMPLEX][D:phase-1] :implementation:
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: phase-2
:CREATED: 2025-10-16T22:45:00Z
:DEPS: phase-1
:PROGRESS: 25/25
:COMPLETION: 100%
:STATUS: ✅ COMPLETED
:COMPLETED: 2025-10-19T13:38:53Z
:END:

*** DONE [#A] Task 2.1: Create WASI module directory structure [S][R:LOW][C:TRIVIAL][D:1.5]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.1
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 1.5
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Set up directory structure for WASI module:
- Create src/node/wasi/ directory
- Create src/node/wasi/wasi.h header
- Create src/node/wasi/wasi_core.c
- Create src/node/wasi/wasi_imports.c
- Update CMakeLists.txt to include new files

**** Acceptance Criteria
- [X] Directory src/node/wasi/ created
- [X] All skeleton files created
- [X] CMakeLists.txt updated
- [X] Build succeeds: make jsrt_g

**** Testing Strategy
Build verification: make jsrt_g

*** DONE [#A] Task 2.2: Define WASI data structures [S][R:LOW][C:SIMPLE][D:2.1,1.5]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.2
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.1,1.5
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement core WASI data structures in wasi.h:

```c
typedef struct {
  char** args;              // Argument vector
  size_t args_count;
  char** env;               // Environment variables
  size_t env_count;
  // Preopens mapping
  char** preopen_names;     // Virtual paths
  char** preopen_paths;     // Real paths
  size_t preopen_count;
  // File descriptors
  int stdin_fd;
  int stdout_fd;
  int stderr_fd;
  // Options
  bool return_on_exit;
  char* version;            // "preview1" or "unstable"
} jsrt_wasi_options_t;

typedef struct {
  JSContext* ctx;
  jsrt_wasi_options_t options;
  wasm_module_inst_t instance;  // WAMR instance (set on start/initialize)
  bool started;
  bool initialized;
  JSValue import_object;         // Cached import object
} jsrt_wasi_t;
```

**** Acceptance Criteria
- [X] All structures defined in wasi.h
- [X] Memory layout documented
- [X] Lifecycle states documented
- [X] Build succeeds

**** Testing Strategy
Compilation test.

*** DONE [#A] Task 2.3: Implement WASI options parsing [S][R:MED][C:MEDIUM][D:2.2]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.3
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.2
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement function to parse JS options object into jsrt_wasi_options_t:

```c
int jsrt_wasi_parse_options(JSContext* ctx, JSValue options_obj,
                             jsrt_wasi_options_t* options);
```

Parse all fields:
- args (array of strings)
- env (object)
- preopens (object mapping virtual paths to real paths)
- stdin, stdout, stderr (numbers)
- returnOnExit (boolean)
- version (string)

**** Acceptance Criteria
- [X] Function implemented in wasi_core.c
- [X] All option fields parsed correctly
- [X] Type validation for each field
- [X] Default values applied when options missing
- [X] Memory allocated properly
- [X] Error handling for invalid types

**** Testing Strategy
Unit test with various options objects.

*** DONE [#A] Task 2.4: Implement WASI options cleanup [S][R:LOW][C:SIMPLE][D:2.2]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.4
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.2
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement cleanup function for jsrt_wasi_options_t:

```c
void jsrt_wasi_free_options(jsrt_wasi_options_t* options);
```

Free all allocated memory:
- args array and strings
- env array and strings
- preopen arrays and strings
- version string

**** Acceptance Criteria
- [X] Function implemented
- [X] All memory freed correctly
- [X] No double-free issues
- [X] NULL pointer safe
- [X] ASAN validation passes

**** Testing Strategy
Memory leak test with ASAN: make jsrt_m

*** DONE [#A] Task 2.5: Implement WASI constructor (C layer) [S][R:MED][C:MEDIUM][D:2.3,2.4]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.5
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.3,2.4
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement C function to create WASI instance:

```c
jsrt_wasi_t* jsrt_wasi_new(JSContext* ctx, const jsrt_wasi_options_t* options);
void jsrt_wasi_free(jsrt_wasi_t* wasi);
```

Initialize state:
- Copy options
- Set default values
- Initialize state flags (started=false, initialized=false)
- Allocate import_object slot

**** Acceptance Criteria
- [X] jsrt_wasi_new() implemented
- [X] jsrt_wasi_free() implemented
- [X] Memory management correct
- [X] Default values applied
- [X] Error handling for allocation failures

**** Testing Strategy
Memory leak test with various option combinations.

*** DONE [#A] Task 2.6: Define WASI class ID for QuickJS [S][R:LOW][C:SIMPLE][D:2.2]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.6
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.2
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Create QuickJS class ID and finalizer for WASI instances:

```c
static JSClassID jsrt_wasi_class_id;

static void jsrt_wasi_finalizer(JSRuntime* rt, JSValue val) {
  jsrt_wasi_t* wasi = JS_GetOpaque(val, jsrt_wasi_class_id);
  if (wasi) {
    jsrt_wasi_free(wasi);
  }
}

static JSClassDef jsrt_wasi_class = {
  "WASI",
  .finalizer = jsrt_wasi_finalizer,
};
```

**** Acceptance Criteria
- [X] Class ID registered
- [X] Finalizer implemented
- [X] Cleanup on garbage collection works
- [X] No memory leaks

**** Testing Strategy
GC test to verify finalizer called.

*** DONE [#A] Task 2.7: Implement WASI constructor (JS layer) [S][R:MED][C:MEDIUM][D:2.5,2.6]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.7
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.5,2.6
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement JS-visible WASI constructor:

```c
static JSValue js_wasi_constructor(JSContext* ctx, JSValueConst new_target,
                                    int argc, JSValueConst* argv) {
  // Parse options from argv[0]
  // Create C WASI instance
  // Wrap in JS object with class ID
  // Return JS object
}
```

**** Acceptance Criteria
- [X] Constructor function implemented
- [X] Options object parsed
- [X] C instance created and attached
- [X] Error handling for invalid options
- [X] Proper memory management

**** Testing Strategy
Unit test: new WASI({ options })

*** DONE [#A] Task 2.8: Implement args array conversion [S][R:LOW][C:SIMPLE][D:2.3]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.8
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.3
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement function to convert JS args array to C string array:

```c
int jsrt_wasi_convert_args(JSContext* ctx, JSValue args_array,
                            char*** out_args, size_t* out_count);
```

Handle:
- Array iteration
- String extraction and copying
- UTF-8 encoding
- Memory allocation

**** Acceptance Criteria
- [X] Function implemented
- [X] Array properly converted
- [X] UTF-8 handling correct
- [X] Memory allocated correctly
- [X] Error handling for non-strings

**** Testing Strategy
Test with Unicode arguments.

*** DONE [#A] Task 2.9: Implement env object conversion [S][R:MED][C:MEDIUM][D:2.3]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.9
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.3
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement function to convert JS env object to C string array:

```c
int jsrt_wasi_convert_env(JSContext* ctx, JSValue env_obj,
                           char*** out_env, size_t* out_count);
```

Convert to "KEY=VALUE" format:
- Iterate object properties
- Format as KEY=VALUE strings
- Handle UTF-8 encoding
- Allocate memory

**** Acceptance Criteria
- [X] Function implemented
- [X] Object properties enumerated
- [X] KEY=VALUE format correct
- [X] UTF-8 handling correct
- [X] Memory allocated correctly

**** Testing Strategy
Test with various env objects.

*** DONE [#A] Task 2.10: Implement preopens conversion [S][R:MED][C:COMPLEX][D:2.3,1.6]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.10
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.3,1.6
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement function to convert preopens object to C arrays:

```c
int jsrt_wasi_convert_preopens(JSContext* ctx, JSValue preopens_obj,
                                char*** out_names, char*** out_paths,
                                size_t* out_count);
```

Handle:
- Object property iteration
- Path validation (security)
- Real path resolution
- Memory allocation

**** Acceptance Criteria
- [X] Function implemented
- [X] Properties enumerated correctly
- [X] Path validation implemented
- [X] Path traversal attacks prevented
- [X] Memory allocated correctly

**** Testing Strategy
Security test with malicious paths (../, etc.)

*** DONE [#B] Task 2.11: Implement version string handling [P][R:LOW][C:SIMPLE][D:2.3]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.11
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.3
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Handle WASI version string ("preview1" or "unstable"):
- Validate version string
- Store in options
- Use to determine import object structure

**** Acceptance Criteria
- [X] Version validation implemented
- [X] Default to "preview1"
- [X] Error for invalid versions
- [X] Version stored correctly

**** Testing Strategy
Test with both valid versions and invalid strings.

*** DONE [#B] Task 2.12: Implement stdin/stdout/stderr FD handling [P][R:LOW][C:SIMPLE][D:2.3]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.12
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.3
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Handle stdio file descriptor options:
- Parse stdin, stdout, stderr numbers from options
- Validate FD values
- Set defaults (0, 1, 2)

**** Acceptance Criteria
- [X] FD parsing implemented
- [X] Validation for invalid FDs
- [X] Defaults applied correctly

**** Testing Strategy
Test with custom FD values.

*** DONE [#B] Task 2.13: Implement returnOnExit option handling [P][R:LOW][C:SIMPLE][D:2.3]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.13
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.3
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Handle returnOnExit boolean option:
- Parse boolean from options
- Default to true (Node.js default)
- Store in WASI instance

**** Acceptance Criteria
- [X] Boolean parsing implemented
- [X] Default value correct (true)
- [X] Value stored correctly

**** Testing Strategy
Test with true, false, and undefined.

*** DONE [#A] Task 2.14: Integrate WAMR WASI initialization [S][R:MED][C:COMPLEX][D:2.5,1.2]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.14
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.5,1.2
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Initialize WAMR WASI support when WASI instance is created:
- Call WAMR WASI init functions
- Set up WASI context in WAMR
- Register WASI imports with WAMR
- Handle WAMR WASI configuration

**** Acceptance Criteria
- [X] WAMR WASI initialized
- [X] WASI context created in WAMR
- [X] Configuration passed to WAMR
- [X] Error handling for init failures

**** Testing Strategy
Integration test with WAMR.

*** DONE [#A] Task 2.15: Implement WASI state validation [P][R:LOW][C:SIMPLE][D:2.5]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.15
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.5
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement state validation helpers:

```c
bool jsrt_wasi_is_started(const jsrt_wasi_t* wasi);
bool jsrt_wasi_is_initialized(const jsrt_wasi_t* wasi);
int jsrt_wasi_validate_state(const jsrt_wasi_t* wasi, const char* operation);
```

Check lifecycle state before operations.

**** Acceptance Criteria
- [X] State check functions implemented
- [X] Validation prevents invalid operations
- [X] Clear error messages

**** Testing Strategy
Test state transitions and invalid operations.

*** DONE [#B] Task 2.16: Add debug logging [P][R:LOW][C:TRIVIAL][D:2.5]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.16
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.5
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Add JSRT_Debug logging throughout WASI implementation:
- Constructor/destructor events
- Option parsing
- State transitions
- Error conditions

**** Acceptance Criteria
- [X] Debug logging added to key points
- [X] Informative log messages
- [X] Visible with make jsrt_g

**** Testing Strategy
Run with debug build and verify logs.

*** DONE [#A] Task 2.17: Implement error handling utilities [P][R:MED][C:SIMPLE][D:2.5]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.17
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.5
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Create WASI-specific error handling:

```c
JSValue jsrt_wasi_throw_error(JSContext* ctx, const char* message);
JSValue jsrt_wasi_throw_type_error(JSContext* ctx, const char* message);
```

Follow Node.js error patterns.

**** Acceptance Criteria
- [X] Error functions implemented
- [X] Error messages match Node.js style
- [X] Proper exception throwing

**** Testing Strategy
Test error messages match Node.js.

*** DONE [#B] Task 2.18: Handle memory allocation failures [P][R:MED][C:SIMPLE][D:2.5]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.18
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.5
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Add proper error handling for all memory allocations:
- Check all malloc/calloc returns
- Clean up partial allocations on failure
- Throw JS exceptions on OOM

**** Acceptance Criteria
- [X] All allocations checked
- [X] Cleanup on failure paths
- [X] No memory leaks on error

**** Testing Strategy
Memory leak test with ASAN.

*** DONE [#A] Task 2.19: Implement WASI instance validation [P][R:LOW][C:SIMPLE][D:2.6]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.19
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.6
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Add validation for WASI instance in JS methods:

```c
jsrt_wasi_t* jsrt_wasi_get_instance(JSContext* ctx, JSValueConst this_val) {
  jsrt_wasi_t* wasi = JS_GetOpaque(this_val, jsrt_wasi_class_id);
  if (!wasi) {
    JS_ThrowTypeError(ctx, "Expected WASI instance");
    return NULL;
  }
  return wasi;
}
```

**** Acceptance Criteria
- [X] Validation helper implemented
- [X] Used in all methods
- [X] Clear error messages

**** Testing Strategy
Test with invalid objects.

*** DONE [#B] Task 2.20: Handle default options [P][R:LOW][C:SIMPLE][D:2.3]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.20
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.3
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Apply Node.js default values for missing options:
- args: [] (empty array)
- env: {} (empty object)
- preopens: {} (empty object)
- stdin: 0
- stdout: 1
- stderr: 2
- returnOnExit: true
- version: "preview1"

**** Acceptance Criteria
- [X] All defaults applied correctly
- [X] Undefined options use defaults
- [X] Behavior matches Node.js

**** Testing Strategy
Test with minimal/empty options.

*** DONE [#B] Task 2.21: Validate options types [P][R:MED][C:SIMPLE][D:2.3]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.21
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.3
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Add type validation for all options:
- args must be array of strings
- env must be object
- preopens must be object with string values
- stdin/stdout/stderr must be numbers
- returnOnExit must be boolean
- version must be string

**** Acceptance Criteria
- [X] Type validation for all options
- [X] Clear error messages
- [X] Behavior matches Node.js

**** Testing Strategy
Test with invalid types for each option.

*** DONE [#A] Task 2.22: Memory ownership documentation [P][R:LOW][C:SIMPLE][D:2.4]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.22
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.4
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Document memory ownership rules in wasi.h:
- Who owns allocated memory
- When to free memory
- Lifecycle of JS vs C objects
- GC interaction

**** Acceptance Criteria
- [X] Memory rules documented in header
- [X] Ownership clear for all structures
- [X] Lifecycle documented

**** Testing Strategy
Documentation review.

*** DONE [#B] Task 2.23: Handle WASM instance attachment [P][R:MED][C:MEDIUM][D:2.5]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.23
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.5
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Design how to attach WASM instance to WASI instance:
- Store wasm_module_inst_t in jsrt_wasi_t
- Validate instance has required exports (memory)
- Handle instance lifecycle

**** Acceptance Criteria
- [X] Instance storage designed
- [X] Validation strategy defined
- [X] Lifecycle handling clear

**** Testing Strategy
Integration test with WASM instances.

*** DONE [#B] Task 2.24: Phase 2 code formatting [S][R:LOW][C:TRIVIAL][D:2.1-2.23]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.24
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.1,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9,2.10,2.11,2.12,2.13,2.14,2.15,2.16,2.17,2.18,2.19,2.20,2.21,2.22,2.23
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Format all Phase 2 code with make format.

**** Acceptance Criteria
- [X] make format runs successfully
- [X] All code properly formatted

**** Testing Strategy
make format

*** DONE [#A] Task 2.25: Phase 2 compilation validation [S][R:MED][C:SIMPLE][D:2.24]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 2.25
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.24
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Validate Phase 2 code compiles correctly:
- make clean && make jsrt_g
- make jsrt_m (ASAN build)
- Verify no warnings
- Verify no memory leaks in basic tests

**** Acceptance Criteria
- [X] Debug build succeeds
- [X] ASAN build succeeds
- [X] No compilation warnings
- [X] No memory leaks in constructor/destructor

**** Testing Strategy
make jsrt_g && make jsrt_m

** TODO [#A] Phase 3: WASI Import Implementation [S][R:HIGH][C:COMPLEX][D:phase-2] :implementation:
