:PROPERTIES:
:ID: phase-3
:CREATED: 2025-10-16T22:45:00Z
:DEPS: phase-2
:PROGRESS: 20/38
:COMPLETION: 53%
:STATUS: 🔵 IN_PROGRESS
:END:

*** TODO [#A] Task 3.1: List all WASI preview1 functions [S][R:LOW][C:SIMPLE][D:1.1,1.2]
:PROPERTIES:
:ID: 3.1
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 1.1,1.2
:END:

**** Description
Create comprehensive list of all WASI preview1 functions to implement:
- fd_* functions (file operations)
- path_* functions (path operations)
- poll_* functions (polling)
- proc_* functions (process)
- random_* functions (random)
- sock_* functions (sockets)
- clock_* functions (time)

Reference: https://github.com/WebAssembly/WASI/blob/main/legacy/preview1/docs.md

**** Acceptance Criteria
- [ ] Complete list of ~50+ WASI functions
- [ ] Grouped by category
- [ ] Priority assigned (required vs optional)
- [ ] WAMR support status documented

**** Testing Strategy
Documentation review.

*** TODO [#A] Task 3.2: Design import object structure [S][R:MED][C:MEDIUM][D:3.1,1.1]
:PROPERTIES:
:ID: 3.2
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.1,1.1
:END:

**** Description
Design the import object structure returned by getImportObject():

For version "preview1":
```javascript
{
  wasi_snapshot_preview1: {
    fd_write: function() { ... },
    fd_read: function() { ... },
    // ... all WASI functions
  }
}
```

For version "unstable":
```javascript
{
  wasi_unstable: {
    // ... all WASI functions
  }
}
```

**** Acceptance Criteria
- [ ] Object structure defined
- [ ] Version-based namespacing designed
- [ ] Function signature strategy defined

**** Testing Strategy
Structure validation against Node.js.

*** DONE [#A] Task 3.3: Implement getImportObject() method [S][R:MED][C:MEDIUM][D:3.2,2.7]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.3
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.2,2.7
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement the getImportObject() JS method:

```c
static JSValue js_wasi_getImportObject(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
  jsrt_wasi_t* wasi = jsrt_wasi_get_instance(ctx, this_val);
  if (!wasi) return JS_EXCEPTION;

  // Create import object based on version
  // Populate with WASI function bindings
  // Cache and return
}
```

**** Acceptance Criteria
- [X] Method implemented
- [X] Returns correct structure for preview1
- [X] Returns correct structure for unstable
- [X] Object cached for reuse
- [X] WASI context passed to functions

**** Testing Strategy
Test object structure matches Node.js.

*** DONE [#A] Task 3.4: Implement WASI import function wrapper [S][R:MED][C:MEDIUM][D:3.3]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.4
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.3
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Create generic wrapper for WASI import functions:

```c
typedef int (*wasi_import_func_t)(jsrt_wasi_t* wasi, wasm_exec_env_t exec_env,
                                   uint32_t* args, uint32_t* results);

JSValue jsrt_wasi_create_import_func(JSContext* ctx, jsrt_wasi_t* wasi,
                                      const char* name,
                                      wasi_import_func_t native_func);
```

Wrapper should:
- Extract WASM execution environment
- Call WAMR native function
- Handle errors
- Return result

**** Acceptance Criteria
- [X] Wrapper function implemented
- [X] Arguments passed correctly to WAMR
- [X] Results extracted correctly
- [X] Error handling works

**** Testing Strategy
Test with simple WASI function.

*** DONE [#A] Task 3.5: Implement fd_write [S][R:HIGH][C:MEDIUM][D:3.4]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.5
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI fd_write function (most critical for "hello world"):

```c
int wasi_fd_write(jsrt_wasi_t* wasi, wasm_exec_env_t exec_env,
                  uint32_t fd, uint32_t iovs_ptr, uint32_t iovs_len,
                  uint32_t* nwritten_ptr);
```

Write to file descriptor from WASM memory.

**** Acceptance Criteria
- [X] Function implemented
- [X] Writes to stdout/stderr work
- [X] Returns correct byte count
- [X] Error handling for invalid FDs

**** Testing Strategy
Test "hello world" WASM module.

*** DONE [#A] Task 3.6: Implement fd_read [S][R:HIGH][C:MEDIUM][D:3.4]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.6
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI fd_read function:

```c
int wasi_fd_read(jsrt_wasi_t* wasi, wasm_exec_env_t exec_env,
                 uint32_t fd, uint32_t iovs_ptr, uint32_t iovs_len,
                 uint32_t* nread_ptr);
```

Read from file descriptor into WASM memory.

**** Acceptance Criteria
- [X] Function implemented
- [X] Reads from stdin work
- [X] Returns correct byte count
- [X] Error handling

**** Testing Strategy
Test reading from stdin.

*** DONE [#B] Task 3.7: Implement fd_close [P][R:MED][C:SIMPLE][D:3.4]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.7
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI fd_close function.

**** Acceptance Criteria
- [X] Function implemented
- [X] Closes FDs correctly
- [X] Error handling

**** Testing Strategy
Test FD lifecycle.

*** DONE [#B] Task 3.8: Implement fd_seek [P][R:MED][C:SIMPLE][D:3.4]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.8
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI fd_seek function for file positioning.

**** Acceptance Criteria
- [X] Function implemented
- [X] Seek operations work correctly

**** Testing Strategy
Test file positioning.

*** TODO [#B] Task 3.9: Implement fd_tell [P][R:MED][C:SIMPLE][D:3.4]
:PROPERTIES:
:ID: 3.9
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI fd_tell function.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Returns correct position

**** Testing Strategy
Test position tracking.

*** TODO [#B] Task 3.10: Implement fd_fdstat_get [P][R:MED][C:SIMPLE][D:3.4]
:PROPERTIES:
:ID: 3.10
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI fd_fdstat_get function to get FD metadata.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Returns correct FD stats

**** Testing Strategy
Test FD metadata retrieval.

*** TODO [#B] Task 3.11: Implement fd_fdstat_set_flags [P][R:LOW][C:SIMPLE][D:3.4]
:PROPERTIES:
:ID: 3.11
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI fd_fdstat_set_flags function.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Flags updated correctly

**** Testing Strategy
Test flag modification.

*** DONE [#B] Task 3.12: Implement fd_prestat_get [P][R:MED][C:MEDIUM][D:3.4,2.10]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.12
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4,2.10
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI fd_prestat_get function to query preopened directories.

**** Acceptance Criteria
- [X] Function implemented
- [X] Returns preopen info correctly

**** Testing Strategy
Test with preopened directories.

*** DONE [#B] Task 3.13: Implement fd_prestat_dir_name [P][R:MED][C:MEDIUM][D:3.4,2.10]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.13
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4,2.10
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI fd_prestat_dir_name function.

**** Acceptance Criteria
- [X] Function implemented
- [X] Returns directory names correctly

**** Testing Strategy
Test preopen directory names.

*** TODO [#B] Task 3.14: Implement path_open [P][R:HIGH][C:COMPLEX][D:3.4,2.10]
:PROPERTIES:
:ID: 3.14
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4,2.10
:END:

**** Description
Implement WASI path_open function for opening files.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Respects preopen sandboxing
- [ ] Path validation works
- [ ] Security checks in place

**** Testing Strategy
Test file opening with sandboxing.

*** TODO [#B] Task 3.15: Implement path_filestat_get [P][R:MED][C:MEDIUM][D:3.4]
:PROPERTIES:
:ID: 3.15
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI path_filestat_get function.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Returns file stats correctly

**** Testing Strategy
Test file stat retrieval.

*** TODO [#C] Task 3.16: Implement path_create_directory [P][R:MED][C:SIMPLE][D:3.4]
:PROPERTIES:
:ID: 3.16
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI path_create_directory function.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Creates directories correctly

**** Testing Strategy
Test directory creation.

*** TODO [#C] Task 3.17: Implement path_remove_directory [P][R:MED][C:SIMPLE][D:3.4]
:PROPERTIES:
:ID: 3.17
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI path_remove_directory function.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Removes directories correctly

**** Testing Strategy
Test directory removal.

*** TODO [#C] Task 3.18: Implement path_rename [P][R:MED][C:MEDIUM][D:3.4]
:PROPERTIES:
:ID: 3.18
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI path_rename function.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Renames files/dirs correctly

**** Testing Strategy
Test file/directory renaming.

*** TODO [#C] Task 3.19: Implement path_unlink_file [P][R:MED][C:SIMPLE][D:3.4]
:PROPERTIES:
:ID: 3.19
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI path_unlink_file function.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Deletes files correctly

**** Testing Strategy
Test file deletion.

*** TODO [#B] Task 3.20: Implement proc_exit [P][R:HIGH][C:MEDIUM][D:3.4,2.13]
:PROPERTIES:
:ID: 3.20
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4,2.13
:END:

**** Description
Implement WASI proc_exit function:
- Check returnOnExit option
- If true: return exit code without terminating process
- If false: terminate process with exit code

**** Acceptance Criteria
- [ ] Function implemented
- [ ] returnOnExit=true returns code
- [ ] returnOnExit=false exits process
- [ ] Exit code propagated correctly

**** Notes
- Current implementation stores exit code and throws internal error placeholder. Needs proper returnOnExit branching and process termination semantics.

**** Testing Strategy
Test both returnOnExit modes.

*** DONE [#B] Task 3.21: Implement environ_get [P][R:MED][C:MEDIUM][D:3.4,2.9]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.21
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4,2.9
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI environ_get function to expose environment variables.

**** Acceptance Criteria
- [X] Function implemented
- [X] Returns env variables correctly
- [X] Memory layout correct

**** Testing Strategy
Test environment variable access.

*** DONE [#B] Task 3.22: Implement environ_sizes_get [P][R:MED][C:SIMPLE][D:3.4,2.9]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.22
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4,2.9
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI environ_sizes_get function.

**** Acceptance Criteria
- [X] Function implemented
- [X] Returns correct sizes

**** Testing Strategy
Test size calculations.

*** DONE [#B] Task 3.23: Implement args_get [P][R:MED][C:MEDIUM][D:3.4,2.8]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.23
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4,2.8
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI args_get function to expose command-line arguments.

**** Acceptance Criteria
- [X] Function implemented
- [X] Returns args correctly
- [X] Memory layout correct

**** Testing Strategy
Test argument access.

*** DONE [#B] Task 3.24: Implement args_sizes_get [P][R:MED][C:SIMPLE][D:3.4,2.8]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.24
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4,2.8
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI args_sizes_get function.

**** Acceptance Criteria
- [X] Function implemented
- [X] Returns correct sizes

**** Testing Strategy
Test size calculations.

*** TODO [#B] Task 3.25: Implement clock_res_get [P][R:MED][C:SIMPLE][D:3.4]
:PROPERTIES:
:ID: 3.25
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI clock_res_get function for clock resolution.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Returns correct resolution

**** Testing Strategy
Test clock resolution query.

*** DONE [#B] Task 3.26: Implement clock_time_get [P][R:HIGH][C:MEDIUM][D:3.4]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.26
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI clock_time_get function for getting current time.

**** Acceptance Criteria
- [X] Function implemented
- [X] Returns correct time
- [X] Supports realtime and monotonic clocks

**** Testing Strategy
Test time retrieval.

*** DONE [#C] Task 3.27: Implement random_get [P][R:MED][C:SIMPLE][D:3.4]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.27
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement WASI random_get function for random number generation.

**** Acceptance Criteria
- [X] Function implemented
- [X] Uses secure random source

**** Testing Strategy
Test random number generation.

*** TODO [#C] Task 3.28: Implement poll_oneoff [P][R:LOW][C:COMPLEX][D:3.4]
:PROPERTIES:
:ID: 3.28
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI poll_oneoff function (optional, can return ENOSYS).

**** Acceptance Criteria
- [ ] Function implemented or stubbed
- [ ] Returns appropriate error if not supported

**** Testing Strategy
Test polling or stub behavior.

*** TODO [#C] Task 3.29: Implement sock_* functions [P][R:LOW][C:COMPLEX][D:3.4]
:PROPERTIES:
:ID: 3.29
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Implement WASI socket functions (optional, can stub):
- sock_accept
- sock_recv
- sock_send
- sock_shutdown

**** Acceptance Criteria
- [ ] Functions implemented or stubbed
- [ ] Returns appropriate errors if not supported

**** Testing Strategy
Test socket operations or stub behavior.

*** DONE [#A] Task 3.30: Implement WAMR memory access helpers [S][R:HIGH][C:MEDIUM][D:3.4]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.30
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement helpers to safely access WASM linear memory:

```c
void* jsrt_wasi_get_memory_ptr(wasm_exec_env_t exec_env,
                                uint32_t offset, uint32_t size);
int jsrt_wasi_validate_memory(wasm_exec_env_t exec_env,
                               uint32_t offset, uint32_t size);
```

Prevent buffer overflows.

**** Acceptance Criteria
- [X] Memory access helpers implemented
- [X] Bounds checking works
- [X] Prevents out-of-bounds access

**** Testing Strategy
Test with invalid memory offsets.

*** DONE [#A] Task 3.31: Implement iovec handling [S][R:MED][C:MEDIUM][D:3.30,3.5,3.6]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.31
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.30,3.5,3.6
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Implement helpers for WASI iovec (scatter-gather I/O):

```c
typedef struct {
  uint32_t buf_ptr;
  uint32_t buf_len;
} wasi_iovec_t;

int jsrt_wasi_read_iovecs(wasm_exec_env_t exec_env, uint32_t iovs_ptr,
                          uint32_t iovs_len, wasi_iovec_t** out_iovecs);
```

**** Acceptance Criteria
- [X] Iovec reading implemented
- [X] Memory validation works
- [X] Used in fd_read/fd_write

**** Testing Strategy
Test scatter-gather I/O.

*** DONE [#B] Task 3.32: Implement WASI error code mapping [P][R:MED][C:MEDIUM][D:3.4]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.32
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Map system errno to WASI error codes:

```c
uint32_t jsrt_wasi_errno_to_wasi(int errno_val);
```

WASI defines specific error codes (EBADF, EINVAL, etc.)

**** Acceptance Criteria
- [X] Error mapping function implemented
- [X] All common errors mapped
- [X] Returns correct WASI error codes

**** Testing Strategy
Test error code conversion.

*** TODO [#B] Task 3.33: Implement FD table management [P][R:HIGH][C:COMPLEX][D:3.4,2.10]
:PROPERTIES:
:ID: 3.33
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4,2.10
:END:

**** Description
Implement file descriptor table for WASI:

```c
typedef struct {
  int host_fd;           // Host OS file descriptor
  uint32_t rights_base;  // WASI rights
  uint32_t rights_inheriting;
  char* preopen_path;    // If this is a preopen
} wasi_fd_entry_t;

typedef struct {
  wasi_fd_entry_t* entries;
  size_t capacity;
  size_t count;
} wasi_fd_table_t;
```

Manage mapping of WASI FDs to host FDs.

**** Acceptance Criteria
- [ ] FD table data structure implemented
- [ ] FD allocation/deallocation works
- [ ] Preopens registered in FD table
- [ ] stdin/stdout/stderr registered

**** Testing Strategy
Test FD table operations.

*** DONE [#A] Task 3.34: Register all WASI functions in import object [S][R:HIGH][C:COMPLEX][D:3.5-3.29]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.34
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.5,3.6,3.7,3.8,3.9,3.10,3.11,3.12,3.13,3.14,3.15,3.16,3.17,3.18,3.19,3.20,3.21,3.22,3.23,3.24,3.25,3.26,3.27,3.28,3.29
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Register all implemented WASI functions in getImportObject():
- Create JS function for each WASI import
- Set correct names
- Attach to wasi_snapshot_preview1 or wasi_unstable namespace
- Cache import object

**** Acceptance Criteria
- [X] All functions registered
- [X] Correct namespace based on version
- [X] Function names match WASI spec
- [X] Import object cached

**** Testing Strategy
Verify import object structure.

*** TODO [#B] Task 3.35: Handle WASI memory export validation [P][R:MED][C:MEDIUM][D:3.4]
:PROPERTIES:
:ID: 3.35
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.4
:END:

**** Description
Validate WASM instance has required "memory" export:
- Check for memory export before start/initialize
- Throw error if missing
- Store memory reference

**** Acceptance Criteria
- [ ] Memory export validation implemented
- [ ] Clear error message if missing
- [ ] Behavior matches Node.js

**** Testing Strategy
Test with WASM modules with/without memory export.

*** DONE [#B] Task 3.36: Phase 3 code formatting [S][R:LOW][C:TRIVIAL][D:3.1-3.35]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.36
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.1,3.2,3.3,3.4,3.5,3.6,3.7,3.8,3.9,3.10,3.11,3.12,3.13,3.14,3.15,3.16,3.17,3.18,3.19,3.20,3.21,3.22,3.23,3.24,3.25,3.26,3.27,3.28,3.29,3.30,3.31,3.32,3.33,3.34,3.35
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Format all Phase 3 code.

**** Acceptance Criteria
- [X] make format runs successfully

**** Testing Strategy
make format

*** DONE [#A] Task 3.37: Phase 3 compilation validation [S][R:MED][C:SIMPLE][D:3.36]
CLOSED: [2025-10-19T13:38:53Z]
:PROPERTIES:
:ID: 3.37
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.36
:COMPLETED: 2025-10-19T13:38:53Z
:END:

**** Description
Validate Phase 3 compiles correctly.

**** Acceptance Criteria
- [X] Debug build succeeds
- [X] ASAN build succeeds
- [X] No warnings

**** Testing Strategy
make jsrt_g && make jsrt_m

*** TODO [#A] Task 3.38: Test basic WASI import usage [S][R:HIGH][C:MEDIUM][D:3.37]
:PROPERTIES:
:ID: 3.38
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.37
:END:

**** Description
Create basic test for WASI imports:
- Compile simple WASM module with fd_write
- Instantiate with WASI import object
- Verify functions callable
- Verify output works

**** Acceptance Criteria
- [ ] Test WASM module created
- [ ] Import object works with WebAssembly.instantiate
- [ ] WASI functions callable
- [ ] Output appears correctly

**** Testing Strategy
End-to-end test with WASM module.

** TODO [#A] Phase 4: Module Integration [S][R:MED][C:MEDIUM][D:phase-3] :implementation:
