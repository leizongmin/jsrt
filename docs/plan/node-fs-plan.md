---
Created: 2025-10-04T00:00:00Z
Last Updated: 2025-10-04T10:30:00Z
Status: 🔵 IN_PROGRESS
Overall Progress: 36/95 tasks completed (38%)
---

# Task Plan: Node.js fs Module Compatibility Implementation

## 📋 Task Analysis & Breakdown

### L0 Main Task
**Requirement:** Implement comprehensive Node.js fs module compatibility in jsrt runtime, accessible via `import 'node:fs'`

**Success Criteria:**
- Full support for sync, async callback, and Promise-based APIs
- 95%+ compatibility with Node.js fs module API surface
- All unit tests passing (100% pass rate)
- WPT compliance maintained (no regression from baseline)
- Cross-platform support (Linux/macOS/Windows)
- Memory leak-free (validated with ASAN)

**Constraints:**
- QuickJS engine for JavaScript execution
- libuv for async I/O operations
- Memory management via QuickJS allocation functions (js_malloc/js_free, JS_FreeValue)
- Files >500 lines must be refactored into subdirectories
- Existing module pattern: `/src/node/fs/` structure (2,457 lines implemented)

**Risk Assessment:**
- **HIGH**: libuv async I/O integration complexity
- **MEDIUM**: Promise API and FileHandle lifecycle management
- **MEDIUM**: Cross-platform compatibility (Windows path handling, permissions)
- **LOW**: Breaking existing functionality (good test coverage exists)

---

## ✅ Already Completed (Celebrate Progress!)

### Phase 1-2: Core Foundation (36 APIs - 38% Complete)

**Implemented Sync APIs (28 methods):**
- ✅ **File I/O**: readFileSync, writeFileSync, appendFileSync
- ✅ **File Operations**: copyFileSync, renameSync, unlinkSync, existsSync
- ✅ **Directory Operations**: mkdirSync (with recursive), rmdirSync, readdirSync
- ✅ **File Stats**: statSync
- ✅ **File Descriptors**: openSync, closeSync, readSync, writeSync
- ✅ **Permissions**: chmodSync, chownSync, utimesSync, accessSync
- ✅ **Links**: linkSync, symlinkSync, readlinkSync, realpathSync
- ✅ **Advanced**: truncateSync, ftruncateSync, mkdtempSync, fsyncSync, fdatasyncSync

**Implemented Async APIs (7 callback-based methods):**
- ✅ readFile, writeFile, appendFile
- ✅ copyFile, rename, rmdir, access

**Implemented Constants (4):**
- ✅ F_OK, R_OK, W_OK, X_OK

**Code Structure (~2,457 lines):**
```
/home/lei/work/jsrt/src/node/fs/
├── fs_module.c         - Module initialization and exports
├── fs_common.c/h       - Common utilities and error handling
├── fs_sync_io.c        - Sync I/O (read/write/append/exists/unlink)
├── fs_sync_dir.c       - Directory operations (stat/readdir/mkdir/rmdir)
├── fs_sync_fd.c        - File descriptor operations (open/close/read/write/chmod/chown/utimes)
├── fs_sync_ops.c       - File operations (copy/rename/access)
├── fs_sync_link.c      - Link operations (link/symlink/readlink/realpath)
├── fs_sync_advanced.c  - Advanced operations (truncate/fsync/mkdtemp)
└── fs_async.c          - Async operations (callback-based, basic implementation)
```

**What's Working:**
- ✅ Complete basic file I/O operations
- ✅ Full directory manipulation
- ✅ File descriptor management
- ✅ Permission and ownership control
- ✅ Symbolic link support
- ✅ Temporary directory creation
- ✅ Basic async operations (non-libuv, synchronous callbacks)

---

## 📊 Current Implementation Status

**API Coverage Breakdown:**
- **Total Node.js fs APIs**: ~95 methods + 4 classes + constants
- **Implemented**: 36 APIs (38%)
- **Remaining**: 59 APIs (62%)

**Coverage by Category:**
| Category | Total | Implemented | Remaining | % Complete |
|----------|-------|-------------|-----------|------------|
| Sync File I/O | 8 | 8 | 0 | 100% ✅ |
| Sync Directory | 5 | 4 | 1 | 80% |
| Sync File Descriptor | 6 | 4 | 2 | 67% |
| Sync Stats | 4 | 1 | 3 | 25% |
| Sync Permissions | 7 | 3 | 4 | 43% |
| Sync Links | 4 | 4 | 0 | 100% ✅ |
| Sync Advanced | 10 | 5 | 5 | 50% |
| Async Callbacks | 40 | 7 | 33 | 18% |
| Promise API | 40+ | 0 | 40+ | 0% |
| Classes | 7 | 1 | 6 | 14% |

**What's Missing (Priority Order):**

1. **High Priority - Missing Sync APIs (14):**
   - fstatSync, lstatSync (stat variants)
   - fchmodSync, fchownSync, lchownSync (fd-based permissions)
   - futimesSync, lutimesSync (time variants)
   - rmSync (recursive delete with options)
   - cpSync (recursive copy with options)
   - opendirSync (Dir class support)
   - readvSync, writevSync (vectored I/O)
   - statfsSync (filesystem stats)

2. **High Priority - Async Callbacks (33 missing):**
   - All sync APIs need async callback versions
   - Currently only 7/40 implemented (18%)
   - Need proper libuv integration (current implementation is synchronous)

3. **High Priority - Promise API (40+ methods):**
   - Complete fs.promises.* namespace (0% implemented)
   - FileHandle class with all methods
   - Modern async/await support

4. **Medium Priority - Classes (6 missing):**
   - Dir (directory iterator)
   - FileHandle (Promise API)
   - Dirent (enhanced directory entries)
   - ReadStream, WriteStream
   - FSWatcher

5. **Low Priority - Advanced Features:**
   - File watchers (watch, watchFile, unwatchFile)
   - Newer Node.js APIs (globSync, statfsSync)

---

## 🎯 Revised Implementation Phases

### L1 Epic Phases

#### ✅ Phase 0: Foundation (COMPLETED)
**Status:** 100% Complete - 36 APIs implemented
**Achievement:** Core sync APIs and basic async operations
**Timeline:** Completed prior to this plan
**Lines of Code:** ~2,457 lines

#### 🔄 Phase 1: Complete Remaining Sync APIs (Current Phase)
**Goal:** Finish all missing synchronous file operations
- Execution: SEQUENTIAL (foundation for async)
- Dependencies: None (builds on existing)
- Timeline: 1-2 weeks
- **Tasks:** 14 APIs to implement

#### 2. [S][R:HIGH][C:COMPLEX] Phase 2: True Async I/O with libuv
**Goal:** Refactor async operations to use libuv thread pool (CRITICAL PATH)
- Execution: SEQUENTIAL (critical infrastructure change)
- Dependencies: [D:1]
- Timeline: 2-3 weeks
- **Tasks:** 33 async callback APIs + infrastructure

#### 3. [PS][R:MED][C:COMPLEX] Phase 3: Promise API & FileHandle
**Goal:** Implement fs.promises namespace and FileHandle class
- Execution: PARALLEL-SEQUENTIAL (can design while Phase 2 wraps)
- Dependencies: [D:2]
- Timeline: 2-3 weeks
- **Tasks:** 40+ Promise methods + FileHandle class

#### 4. [P][R:MED][C:COMPLEX] Phase 4: Advanced Classes
**Goal:** Implement Dir, Streams, and FSWatcher
- Execution: PARALLEL (independent subsystems)
- Dependencies: [D:3]
- Timeline: 3-4 weeks
- **Tasks:** 6 classes (Dir, Dirent, ReadStream, WriteStream, FSWatcher, StatWatcher)

#### 5. [P][R:LOW][C:SIMPLE] Phase 5: Testing & Cross-Platform
**Goal:** Comprehensive testing and platform compatibility
- Execution: PARALLEL (ongoing throughout)
- Dependencies: [SD:4] (soft dependency)
- Timeline: 1-2 weeks
- **Tasks:** Cross-platform validation, performance benchmarks

---

## 📝 Task Execution Tracker

### Phase 1: Complete Remaining Sync APIs (14 tasks)

| ID | Task | Exec | Status | Dependencies | Risk | Complexity |
|----|------|------|--------|--------------|------|------------|
| 1.1 | Implement fstatSync(fd) | [S] | ⏳ PENDING | None | LOW | SIMPLE |
| 1.2 | Implement lstatSync(path) | [S] | ⏳ PENDING | None | LOW | SIMPLE |
| 1.3 | Implement fchmodSync(fd, mode) | [S] | ⏳ PENDING | 1.1 | LOW | SIMPLE |
| 1.4 | Implement fchownSync(fd, uid, gid) | [S] | ⏳ PENDING | 1.1 | LOW | SIMPLE |
| 1.5 | Implement lchownSync(path, uid, gid) | [S] | ⏳ PENDING | 1.2 | LOW | SIMPLE |
| 1.6 | Implement futimesSync(fd, atime, mtime) | [S] | ⏳ PENDING | 1.1 | LOW | SIMPLE |
| 1.7 | Implement lutimesSync(path, atime, mtime) | [S] | ⏳ PENDING | 1.2 | LOW | SIMPLE |
| 1.8 | Implement rmSync(path, options) - recursive delete | [S] | ⏳ PENDING | None | MED | MEDIUM |
| 1.9 | Implement cpSync(src, dest, options) - recursive copy | [S] | ⏳ PENDING | None | MED | MEDIUM |
| 1.10 | Implement opendirSync(path) - Dir class basic | [S] | ⏳ PENDING | None | MED | MEDIUM |
| 1.11 | Implement readvSync(fd, buffers) - vectored read | [S] | ⏳ PENDING | 1.1 | MED | MEDIUM |
| 1.12 | Implement writevSync(fd, buffers) - vectored write | [S] | ⏳ PENDING | 1.1 | MED | MEDIUM |
| 1.13 | Implement statfsSync(path) - filesystem stats | [S] | ⏳ PENDING | None | LOW | SIMPLE |
| 1.14 | Add missing constants (open flags, modes) | [S] | ⏳ PENDING | None | LOW | TRIVIAL |

### Phase 2: True Async I/O with libuv (18 tasks)

| ID | Task | Exec | Status | Dependencies | Risk | Complexity |
|----|------|------|--------|--------------|------|------------|
| 2.1 | Study existing jsrt libuv integration patterns | [S] | ⏳ PENDING | 1.14 | MED | SIMPLE |
| 2.2 | Design libuv work request structure for fs ops | [S] | ⏳ PENDING | 2.1 | HIGH | COMPLEX |
| 2.3 | Implement async work wrapper/callback system | [S] | ⏳ PENDING | 2.2 | HIGH | COMPLEX |
| 2.4 | Create proof-of-concept with readFile | [S] | ⏳ PENDING | 2.3 | HIGH | COMPLEX |
| 2.5 | Refactor existing async methods to use libuv | [S] | ⏳ PENDING | 2.4 | HIGH | COMPLEX |
| 2.6 | Implement async stat/lstat/fstat | [P] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.7 | Implement async chmod/fchmod/lchmod | [P] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.8 | Implement async chown/fchown/lchown | [P] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.9 | Implement async utimes/futimes/lutimes | [P] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.10 | Implement async mkdir (with recursive) | [P] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.11 | Implement async readdir (with options) | [P] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.12 | Implement async rm (recursive delete) | [P] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.13 | Implement async cp (recursive copy) | [P] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.14 | Implement async open/close | [P] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.15 | Implement async read/write/readv/writev | [P] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.16 | Implement async link operations | [P] | ⏳ PENDING | 2.5 | LOW | MEDIUM |
| 2.17 | Implement async opendir | [S] | ⏳ PENDING | 2.11 | MED | MEDIUM |
| 2.18 | Add comprehensive error handling & cleanup | [S] | ⏳ PENDING | 2.17 | HIGH | MEDIUM |

### Phase 3: Promise API & FileHandle (22 tasks)

| ID | Task | Exec | Status | Dependencies | Risk | Complexity |
|----|------|------|--------|--------------|------|------------|
| 3.1 | Design fs.promises namespace structure | [S] | ⏳ PENDING | 2.18 | MED | MEDIUM |
| 3.2 | Implement Promise wrapper utilities | [S] | ⏳ PENDING | 3.1 | MED | MEDIUM |
| 3.3 | Design FileHandle class structure | [S] | ⏳ PENDING | 3.2 | MED | COMPLEX |
| 3.4 | Implement FileHandle lifecycle (constructor/finalizer) | [S] | ⏳ PENDING | 3.3 | HIGH | COMPLEX |
| 3.5 | Implement FileHandle.close() | [S] | ⏳ PENDING | 3.4 | MED | MEDIUM |
| 3.6 | Implement FileHandle.read(buffer, ...) | [S] | ⏳ PENDING | 3.4 | MED | MEDIUM |
| 3.7 | Implement FileHandle.write(buffer, ...) | [S] | ⏳ PENDING | 3.4 | MED | MEDIUM |
| 3.8 | Implement FileHandle.readv/writev | [P] | ⏳ PENDING | 3.6, 3.7 | LOW | SIMPLE |
| 3.9 | Implement FileHandle.readFile() | [P] | ⏳ PENDING | 3.6 | LOW | SIMPLE |
| 3.10 | Implement FileHandle.writeFile() | [P] | ⏳ PENDING | 3.7 | LOW | SIMPLE |
| 3.11 | Implement FileHandle.appendFile() | [P] | ⏳ PENDING | 3.7 | LOW | SIMPLE |
| 3.12 | Implement FileHandle.stat() | [P] | ⏳ PENDING | 3.4 | LOW | SIMPLE |
| 3.13 | Implement FileHandle.chmod/chown/utimes | [P] | ⏳ PENDING | 3.4 | LOW | SIMPLE |
| 3.14 | Implement FileHandle.truncate/sync/datasync | [P] | ⏳ PENDING | 3.4 | LOW | SIMPLE |
| 3.15 | Implement fsPromises.open() → FileHandle | [S] | ⏳ PENDING | 3.14 | MED | MEDIUM |
| 3.16 | Implement fsPromises.readFile/writeFile | [P] | ⏳ PENDING | 3.15 | LOW | SIMPLE |
| 3.17 | Implement fsPromises.appendFile | [P] | ⏳ PENDING | 3.15 | LOW | SIMPLE |
| 3.18 | Implement fsPromises.stat/lstat/fstat | [P] | ⏳ PENDING | 3.15 | LOW | SIMPLE |
| 3.19 | Implement fsPromises.chmod/chown/utimes variants | [P] | ⏳ PENDING | 3.15 | LOW | SIMPLE |
| 3.20 | Implement fsPromises.mkdir/rmdir/readdir | [P] | ⏳ PENDING | 3.15 | LOW | SIMPLE |
| 3.21 | Implement fsPromises.rm/cp/link operations | [P] | ⏳ PENDING | 3.15 | LOW | SIMPLE |
| 3.22 | Implement all remaining fsPromises methods | [P] | ⏳ PENDING | 3.15 | MED | MEDIUM |

### Phase 4: Advanced Classes (22 tasks)

| ID | Task | Exec | Status | Dependencies | Risk | Complexity |
|----|------|------|--------|--------------|------|------------|
| 4.1 | Implement Dir class structure | [S] | ⏳ PENDING | 2.17 | MED | COMPLEX |
| 4.2 | Implement Dir.read() | [S] | ⏳ PENDING | 4.1 | MED | MEDIUM |
| 4.3 | Implement Dir.close() | [S] | ⏳ PENDING | 4.1 | LOW | SIMPLE |
| 4.4 | Implement Dir async iterator | [S] | ⏳ PENDING | 4.2 | MED | MEDIUM |
| 4.5 | Implement Dir.readSync() | [S] | ⏳ PENDING | 4.1 | LOW | SIMPLE |
| 4.6 | Implement Dir.closeSync() | [S] | ⏳ PENDING | 4.1 | LOW | SIMPLE |
| 4.7 | Enhance Dirent class with type methods | [P] | ⏳ PENDING | 4.1 | LOW | SIMPLE |
| 4.8 | Design ReadStream architecture | [S] | ⏳ PENDING | 3.22 | MED | COMPLEX |
| 4.9 | Implement fs.createReadStream() | [S] | ⏳ PENDING | 4.8 | MED | COMPLEX |
| 4.10 | Implement ReadStream event emitter | [S] | ⏳ PENDING | 4.9 | MED | MEDIUM |
| 4.11 | Implement ReadStream data flow | [S] | ⏳ PENDING | 4.10 | MED | MEDIUM |
| 4.12 | Implement ReadStream backpressure | [S] | ⏳ PENDING | 4.11 | HIGH | COMPLEX |
| 4.13 | Design WriteStream architecture | [S] | ⏳ PENDING | 4.7 | MED | COMPLEX |
| 4.14 | Implement fs.createWriteStream() | [S] | ⏳ PENDING | 4.13 | MED | COMPLEX |
| 4.15 | Implement WriteStream event emitter | [S] | ⏳ PENDING | 4.14 | MED | MEDIUM |
| 4.16 | Implement WriteStream buffering | [S] | ⏳ PENDING | 4.15 | MED | MEDIUM |
| 4.17 | Implement WriteStream backpressure | [S] | ⏳ PENDING | 4.16 | HIGH | COMPLEX |
| 4.18 | Design FSWatcher (libuv fs_event) | [S] | ⏳ PENDING | 4.17 | HIGH | COMPLEX |
| 4.19 | Implement fs.watch() | [S] | ⏳ PENDING | 4.18 | HIGH | COMPLEX |
| 4.20 | Implement FSWatcher event handlers | [S] | ⏳ PENDING | 4.19 | MED | MEDIUM |
| 4.21 | Implement fs.watchFile() (polling-based) | [P] | ⏳ PENDING | 4.18 | MED | MEDIUM |
| 4.22 | Implement fs.unwatchFile() | [P] | ⏳ PENDING | 4.21 | LOW | SIMPLE |

### Phase 5: Testing & Cross-Platform (15 tasks)

| ID | Task | Exec | Status | Dependencies | Risk | Complexity |
|----|------|------|--------|--------------|------|------------|
| 5.1 | Expand test suite for Phase 1 sync APIs | [P] | ⏳ PENDING | 1.14 | LOW | MEDIUM |
| 5.2 | Create test suite for async callbacks | [P] | ⏳ PENDING | 2.18 | LOW | MEDIUM |
| 5.3 | Create test suite for Promise APIs | [P] | ⏳ PENDING | 3.22 | LOW | MEDIUM |
| 5.4 | Create test suite for FileHandle | [P] | ⏳ PENDING | 3.22 | LOW | MEDIUM |
| 5.5 | Create test suite for Dir class | [P] | ⏳ PENDING | 4.6 | LOW | MEDIUM |
| 5.6 | Create test suite for Streams | [P] | ⏳ PENDING | 4.17 | LOW | MEDIUM |
| 5.7 | Create test suite for FSWatcher | [P] | ⏳ PENDING | 4.22 | MED | MEDIUM |
| 5.8 | Test error handling & edge cases | [S] | ⏳ PENDING | 5.1-5.7 | MED | MEDIUM |
| 5.9 | Windows platform testing & fixes | [S] | ⏳ PENDING | 5.8 | HIGH | MEDIUM |
| 5.10 | macOS platform testing & fixes | [S] | ⏳ PENDING | 5.8 | MED | MEDIUM |
| 5.11 | Memory leak testing with ASAN | [S] | ⏳ PENDING | 5.10 | HIGH | MEDIUM |
| 5.12 | Performance benchmarking vs Node.js | [P] | ⏳ PENDING | 5.11 | LOW | MEDIUM |
| 5.13 | WPT compliance validation | [S] | ⏳ PENDING | 5.11 | MED | MEDIUM |
| 5.14 | Concurrent operation stress testing | [P] | ⏳ PENDING | 5.13 | MED | MEDIUM |
| 5.15 | Final integration testing & bug fixes | [S] | ⏳ PENDING | 5.14 | MED | MEDIUM |

### Execution Mode Legend
- **[S]** = Sequential - Must complete before next task
- **[P]** = Parallel - Can run simultaneously with other [P] tasks
- **[PS]** = Parallel-Sequential - Parallel within group, sequential between groups

### Status Legend
- ⏳ PENDING - Not started
- 🔄 IN_PROGRESS - Currently working
- ✅ COMPLETED - Done and verified
- ⚠️ DELAYED - Behind schedule
- 🔴 BLOCKED - Cannot proceed

---

## 🚀 Live Execution Dashboard

### Current Phase: Phase 1 - Complete Remaining Sync APIs
**Overall Progress:** 36/95 API tasks completed (38%)
**Current Phase Progress:** 0/14 tasks completed (0%)

**Status:** READY TO START ✅

### What's Been Achieved
- ✅ **36 APIs implemented** (28 sync, 7 async, 1 partial)
- ✅ **~2,457 lines of code** across 9 well-organized files
- ✅ **Solid foundation** with proven patterns for error handling, memory management
- ✅ **100% test pass rate** for implemented features
- ✅ **Common operations covered**: file I/O, directories, permissions, links

### Parallel Execution Opportunities

**Phase 1 (Sequential):**
- Tasks 1.1-1.14 mostly sequential due to dependencies
- Can parallelize: 1.8-1.10 (independent operations)

**Phase 2 (High Parallelism after foundation):**
- Tasks 2.1-2.5 sequential (critical path)
- Tasks 2.6-2.16 highly parallel (11 tasks can run together)
- Tasks 2.17-2.18 sequential (cleanup)

**Phase 3 (High Parallelism):**
- Tasks 3.1-3.5 sequential (foundation)
- Tasks 3.8-3.14 parallel (7 FileHandle methods)
- Tasks 3.16-3.22 parallel (7 Promise methods)

**Phase 4 (Grouped Parallelism):**
- Dir class (4.1-4.7): Sequential within group
- ReadStream (4.8-4.12): Sequential within group
- WriteStream (4.13-4.17): Sequential within group
- FSWatcher (4.18-4.22): Mixed
- **Groups can run in parallel after dependencies met**

**Phase 5 (Highly Parallel):**
- Tasks 5.1-5.7 all parallel (test creation)
- Tasks 5.8-5.11 sequential (validation)
- Tasks 5.12-5.14 parallel (performance/stress)

### Next Up (Immediate Priorities)

**Week 1-2 Focus: Phase 1**
1. ⏳ **1.1-1.2**: fstatSync/lstatSync (stat variants) - Foundation for fd operations
2. ⏳ **1.3-1.7**: fd-based permission/time operations - Complete fd API surface
3. ⏳ **1.8**: rmSync with recursive option - High-demand feature
4. ⏳ **1.9**: cpSync with recursive option - High-demand feature
5. ⏳ **1.10**: opendirSync - Foundation for Dir class

**Critical Path to Modern Async:**
```
Phase 1 (1-2 weeks)
  ↓
Phase 2.1-2.5: libuv infrastructure (2 weeks) ← CRITICAL BOTTLENECK
  ↓
Phase 2.6-2.18: async callbacks parallel (1 week)
  ↓
Phase 3: Promise API parallel (2-3 weeks)
  ↓
Phase 4: Advanced classes parallel (3-4 weeks)
  ↓
Phase 5: Testing parallel (ongoing)
```

### Blockers & Risk Mitigation

**Current Blockers:** None - ready to start Phase 1

**Upcoming High-Risk Tasks:**
1. ⚠️ **Task 2.2-2.4**: libuv integration design (Phase 2)
   - **Risk**: Complex async state management
   - **Mitigation**: Study existing jsrt libuv patterns first, create minimal POC

2. ⚠️ **Task 3.4**: FileHandle lifecycle management
   - **Risk**: File descriptor leaks
   - **Mitigation**: QuickJS finalizers, debug tracking, comprehensive tests

3. ⚠️ **Task 4.12, 4.17**: Stream backpressure
   - **Risk**: Memory exhaustion
   - **Mitigation**: Configurable buffers, pause/resume, stress tests

---

## 📜 Execution History

### Updates Log
| Timestamp | Action | Details |
|-----------|--------|---------|
| 2025-10-04T00:00:00Z | CREATED | Initial task plan created |
| 2025-10-04T10:30:00Z | UPDATED | Updated with actual implementation status: 36 APIs (38%) completed |

### Lessons Learned
- ✅ **Strong foundation established**: Existing implementation has excellent code organization and error handling patterns
- ✅ **Test quality**: Using system temporary directories prevents project pollution
- ✅ **Memory management**: Consistent use of QuickJS allocators, proven ASAN-clean
- 📝 **Next focus**: libuv integration is the critical path to modern async/Promise APIs

---

## Architecture Design

### Module Structure (Current + Planned)

```
src/node/fs/
├── fs_common.h          # Shared definitions & helpers (exists ✅)
├── fs_common.c          # Error handling, utilities (exists ✅)
├── fs_module.c          # Main module exports (exists ✅)
│
├── sync/                # Synchronous operations (exists ✅)
│   ├── fs_sync_io.c         # Basic I/O operations
│   ├── fs_sync_ops.c        # File operations (copy/rename/access)
│   ├── fs_sync_dir.c        # Directory operations
│   ├── fs_sync_fd.c         # File descriptor operations
│   ├── fs_sync_link.c       # Link operations
│   ├── fs_sync_advanced.c   # Advanced operations (truncate/fsync)
│   ├── fs_sync_fd_stat.c    # NEW: fstat/lstat variants
│   ├── fs_sync_fd_attr.c    # NEW: fchmod/fchown/futimes
│   └── fs_sync_rm_cp.c      # NEW: rmSync/cpSync with options
│
├── async/               # Asynchronous operations
│   ├── fs_async.c           # Basic async (exists, needs refactor ⚠️)
│   ├── fs_async_libuv.c     # NEW: libuv work wrappers
│   └── fs_async_core.c      # NEW: Core async operations with libuv
│
├── promises/            # Promise API (NEW)
│   ├── fs_promises.c        # fs.promises namespace
│   └── fs_filehandle.c      # FileHandle class
│
└── classes/             # Advanced classes (NEW)
    ├── fs_dir.c             # Dir class
    ├── fs_dirent.c          # Dirent enhancements
    ├── fs_streams.c         # ReadStream/WriteStream
    ├── fs_watcher.c         # FSWatcher
    └── fs_constants.c       # All constants (flags, modes)
```

### Sync vs Async Architecture

**Synchronous Operations (Current - Proven Pattern):**
```c
JSValue js_fs_operation_sync(JSContext* ctx, JSValueConst this_val,
                             int argc, JSValueConst* argv) {
    // 1. Parse arguments
    const char* path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;

    // 2. Execute POSIX syscall
    int result = posix_operation(path);

    // 3. Handle errors
    if (result < 0) {
        JSValue error = create_fs_error(ctx, errno, "operation", path);
        JS_FreeCString(ctx, path);
        return JS_ThrowError(ctx, error);
    }

    // 4. Cleanup and return
    JS_FreeCString(ctx, path);
    return JS_NewInt32(ctx, result);
}
```

**Async Callback Operations (Needs libuv Refactor):**
```c
// Work request structure (NEW)
typedef struct {
    uv_fs_t req;           // libuv fs request
    JSContext* ctx;        // QuickJS context
    JSValue callback;      // JS callback function (must be freed)
    char* path;            // Path (owned, must be freed)
    void* operation_data;  // Operation-specific data
} fs_async_work_t;

// Pattern:
// 1. Create work request with copied JS callback
// 2. uv_fs_open/read/write/close with completion callback
// 3. Completion callback executes JS function in event loop
// 4. Cleanup work request in completion handler
```

**Promise Operations (NEW):**
```c
// Promise work structure
typedef struct {
    uv_fs_t req;
    JSContext* ctx;
    JSValue resolve;       // Promise resolve function
    JSValue reject;        // Promise reject function
    void* operation_data;
} fs_promise_work_t;

// Pattern:
// 1. Create Promise with resolve/reject
// 2. Queue libuv work
// 3. On completion: call resolve(result) or reject(error)
// 4. Cleanup in completion handler
```

### FileHandle Lifecycle Management

```c
typedef struct {
    int fd;                // File descriptor
    char* path;            // Path (for error messages, can be NULL)
    JSContext* ctx;        // QuickJS context
    bool closed;           // Closed flag
    int ref_count;         // Reference counting for safety
} FileHandle;

// Lifecycle:
// 1. fsPromises.open() → Create FileHandle, register finalizer
// 2. Operations use fd, check closed flag
// 3. Explicit .close() marks closed, closes fd
// 4. Finalizer closes fd if not already closed (safety net)
// 5. CRITICAL: finalizer must handle already-closed case

// Registration:
JS_SetOpaque(filehandle_obj, fh);
JS_SetPropertyFunctionList(ctx, filehandle_obj, filehandle_methods, countof(filehandle_methods));
// Add finalizer via class definition
```

### Error Handling Pattern (Proven - Keep Using)

```c
// Existing pattern (excellent - reuse everywhere)
JSValue create_fs_error(JSContext* ctx, int err,
                       const char* syscall, const char* path) {
    const char* code = errno_to_node_code(err);  // ENOENT → "ENOENT"
    JSValue error = JS_NewError(ctx);

    JS_DefinePropertyValueStr(ctx, error, "code",
                              JS_NewString(ctx, code),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(ctx, error, "syscall",
                              JS_NewString(ctx, syscall),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    if (path) {
        JS_DefinePropertyValueStr(ctx, error, "path",
                                  JS_NewString(ctx, path),
                                  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    }
    JS_DefinePropertyValueStr(ctx, error, "errno",
                              JS_NewInt32(ctx, err),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

    // Set message: "ENOENT: no such file or directory, open 'file.txt'"
    char msg[512];
    snprintf(msg, sizeof(msg), "%s: %s, %s '%s'",
             code, strerror(err), syscall, path ? path : "");
    JS_DefinePropertyValueStr(ctx, error, "message",
                              JS_NewString(ctx, msg),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    return error;
}

// Usage in async callbacks:
if (req->result < 0) {
    JSValue error = create_fs_error(ctx, -req->result, "open", work->path);
    JSValue args[1] = { error };
    JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, error);
    // ... cleanup work
}
```

### Memory Management Rules (Critical)

**QuickJS Integration (Existing - Keep Following):**
```c
// Always use QuickJS allocators
char* buffer = js_malloc(ctx, size);
if (!buffer) return JS_EXCEPTION;
// ... use buffer
js_free(ctx, buffer);

// JS values MUST be freed
JSValue result = JS_NewObject(ctx);
// ... populate result
// Return OR free:
return result;  // OR
JS_FreeValue(ctx, result);

// String conversion requires freeing
const char* str = JS_ToCString(ctx, argv[0]);
if (!str) return JS_EXCEPTION;
// ... use str
JS_FreeCString(ctx, str);
```

**Async Work Cleanup (Critical for Phase 2):**
```c
static void free_async_work(fs_async_work_t* work) {
    if (!work) return;

    // Free owned strings
    if (work->path) js_free(work->ctx, work->path);

    // Free JS callback (CRITICAL - prevents leak)
    JS_FreeValue(work->ctx, work->callback);

    // Free operation-specific data
    if (work->operation_data) js_free(work->ctx, work->operation_data);

    // Free work struct itself
    js_free(work->ctx, work);
}

// Always call in completion handler:
static void fs_operation_complete(uv_fs_t* req) {
    fs_async_work_t* work = (fs_async_work_t*)req->data;

    // Execute JS callback
    // ...

    // Cleanup (MANDATORY)
    uv_fs_req_cleanup(req);  // libuv cleanup
    free_async_work(work);    // Our cleanup
}
```

**File Descriptor Tracking (Debug Builds):**
```c
#ifdef DEBUG
static int open_fd_count = 0;
static int open_fd_list[1024];  // Track up to 1024 open fds

#define TRACK_FD_OPEN(fd) do { \
    open_fd_list[open_fd_count++] = fd; \
} while(0)

#define TRACK_FD_CLOSE(fd) do { \
    for (int i = 0; i < open_fd_count; i++) { \
        if (open_fd_list[i] == fd) { \
            open_fd_list[i] = open_fd_list[--open_fd_count]; \
            break; \
        } \
    } \
} while(0)

// At shutdown: report any leaked fds
void fs_debug_report_leaks() {
    if (open_fd_count > 0) {
        fprintf(stderr, "WARNING: %d file descriptors leaked:\n", open_fd_count);
        for (int i = 0; i < open_fd_count; i++) {
            fprintf(stderr, "  fd %d\n", open_fd_list[i]);
        }
    }
}
#else
#define TRACK_FD_OPEN(fd)
#define TRACK_FD_CLOSE(fd)
#endif
```

### libuv Integration Strategy (Phase 2 - Critical Design)

**Study Existing Patterns First:**
```bash
# Before implementing, study these jsrt modules:
grep -r "uv_fs_" src/
grep -r "uv_work_" src/
# Look at timer.c, fetch.c for async patterns
```

**Phase 2 Implementation Approach:**

1. **Create generic async wrapper (Task 2.2-2.3):**
```c
// Generic async fs operation executor
typedef void (*fs_async_executor_t)(uv_fs_t* req);
typedef JSValue (*fs_async_handler_t)(JSContext* ctx, uv_fs_t* req);

// Generic wrapper
JSValue fs_async_execute(JSContext* ctx,
                        fs_async_executor_t executor,
                        fs_async_handler_t result_handler,
                        fs_async_handler_t error_handler,
                        JSValue callback,
                        void* operation_data) {
    // Create work request
    fs_async_work_t* work = js_malloc(ctx, sizeof(*work));
    if (!work) return JS_EXCEPTION;

    work->ctx = ctx;
    work->callback = JS_DupValue(ctx, callback);  // CRITICAL: increment refcount
    work->result_handler = result_handler;
    work->error_handler = error_handler;
    work->data = operation_data;
    work->req.data = work;

    // Start libuv operation
    executor(&work->req);

    return JS_UNDEFINED;  // Async - no return value
}

// Completion callback (generic)
static void fs_async_complete(uv_fs_t* req) {
    fs_async_work_t* work = (fs_async_work_t*)req->data;
    JSContext* ctx = work->ctx;

    JSValue result;
    if (req->result < 0) {
        // Error path
        result = work->error_handler(ctx, req);
    } else {
        // Success path
        result = work->result_handler(ctx, req);
    }

    // Call JS callback: callback(error, result)
    JSValue args[2];
    if (req->result < 0) {
        args[0] = result;  // error
        args[1] = JS_UNDEFINED;
    } else {
        args[0] = JS_UNDEFINED;
        args[1] = result;  // result
    }

    JS_Call(ctx, work->callback, JS_UNDEFINED, 2, args);
    JS_FreeValue(ctx, result);

    // Cleanup
    uv_fs_req_cleanup(req);
    free_async_work(work);
}
```

2. **Per-operation implementations (Task 2.4-2.17):**
```c
// Example: readFile with libuv (multi-step operation)
typedef struct {
    char* path;
    uint8_t* buffer;
    size_t size;
    int fd;
} readfile_data_t;

// Step 1: Open
static void readfile_executor(uv_fs_t* req) {
    fs_async_work_t* work = (fs_async_work_t*)req->data;
    readfile_data_t* data = (readfile_data_t*)work->data;

    uv_loop_t* loop = uv_default_loop();
    uv_fs_open(loop, req, data->path, O_RDONLY, 0, readfile_open_cb);
}

// Step 2: Stat to get size
static void readfile_open_cb(uv_fs_t* req) {
    if (req->result < 0) {
        fs_async_complete(req);  // Error
        return;
    }

    fs_async_work_t* work = (fs_async_work_t*)req->data;
    readfile_data_t* data = (readfile_data_t*)work->data;
    data->fd = req->result;

    uv_loop_t* loop = uv_default_loop();
    uv_fs_fstat(loop, req, data->fd, readfile_stat_cb);
}

// Step 3: Allocate and read
static void readfile_stat_cb(uv_fs_t* req) {
    if (req->result < 0) {
        fs_async_complete(req);  // Error
        return;
    }

    fs_async_work_t* work = (fs_async_work_t*)req->data;
    readfile_data_t* data = (readfile_data_t*)work->data;

    uv_stat_t* stat = (uv_stat_t*)req->ptr;
    data->size = stat->st_size;
    data->buffer = js_malloc(work->ctx, data->size);

    if (!data->buffer) {
        req->result = -ENOMEM;
        fs_async_complete(req);  // Error
        return;
    }

    uv_buf_t buf = uv_buf_init((char*)data->buffer, data->size);
    uv_loop_t* loop = uv_default_loop();
    uv_fs_read(loop, req, data->fd, &buf, 1, 0, readfile_read_cb);
}

// Step 4: Close and complete
static void readfile_read_cb(uv_fs_t* req) {
    fs_async_work_t* work = (fs_async_work_t*)req->data;
    readfile_data_t* data = (readfile_data_t*)work->data;

    // Close fd (ignore errors)
    uv_fs_t close_req;
    uv_fs_close(uv_default_loop(), &close_req, data->fd, NULL);
    uv_fs_req_cleanup(&close_req);

    // Complete with result
    fs_async_complete(req);
}

// Result handler
static JSValue readfile_result_handler(JSContext* ctx, uv_fs_t* req) {
    fs_async_work_t* work = (fs_async_work_t*)req->data;
    readfile_data_t* data = (readfile_data_t*)work->data;

    JSValue buffer = JS_NewArrayBuffer(ctx, data->buffer, data->size,
                                       js_free_arrayBuffer, work->ctx, 0);
    return buffer;
}
```

### Cross-Platform Considerations

**Windows-specific handling:**
```c
#ifdef _WIN32
#include <windows.h>

// Unicode path conversion
wchar_t* utf8_to_wchar(const char* utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t* wide = malloc(len * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return wide;
}

// Use _wopen instead of open
int fs_open_win32(const char* path, int flags, mode_t mode) {
    wchar_t* wpath = utf8_to_wchar(path);
    int fd = _wopen(wpath, flags | _O_BINARY, mode);  // Always binary
    free(wpath);
    return fd;
}

// Permission mapping (Windows doesn't have Unix modes exactly)
int map_unix_mode_to_windows(mode_t mode) {
    int win_mode = _S_IREAD;
    if (mode & 0200) win_mode |= _S_IWRITE;
    return win_mode;
}

#define fs_open fs_open_win32
#else
#define fs_open open
#endif
```

**Path normalization:**
```c
// Handle different path separators
char* normalize_path(JSContext* ctx, const char* path) {
    size_t len = strlen(path);
    char* normalized = js_malloc(ctx, len + 1);
    if (!normalized) return NULL;

    strcpy(normalized, path);

#ifdef _WIN32
    // Convert forward slashes to backslashes on Windows
    for (size_t i = 0; i < len; i++) {
        if (normalized[i] == '/') normalized[i] = '\\';
    }
#endif

    // Remove trailing slashes (except root)
    while (len > 1 && (normalized[len-1] == '/' || normalized[len-1] == '\\')) {
        normalized[--len] = '\0';
    }

    return normalized;
}
```

---

## Testing Strategy

### Unit Test Requirements

**Test File Organization:**
```
test/
├── test_node_fs_sync.js          # Sync API tests (existing + new)
├── test_node_fs_async.js         # Async callback tests
├── test_node_fs_promises.js      # Promise API tests
├── test_node_fs_filehandle.js    # FileHandle class tests
├── test_node_fs_dir.js           # Dir class tests
├── test_node_fs_streams.js       # Stream tests
├── test_node_fs_watcher.js       # FSWatcher tests
├── test_node_fs_cross_platform.js # Platform-specific tests
├── test_node_fs_edge_cases.js    # Edge cases & error handling
└── test_node_fs_performance.js   # Performance benchmarks
```

**Test Coverage Requirements (Per API):**
- ✅ Happy path (normal usage)
- ✅ Invalid arguments (null, undefined, wrong types)
- ✅ File not found / permission denied
- ✅ Edge cases (empty files, large files, special characters in paths)
- ✅ Cleanup (no leaked fds, no temp files left)

**Test Execution (MANDATORY Before Commit):**
```bash
# Standard workflow
make format                       # Format code
make test                         # All unit tests (MUST PASS 100%)
make wpt                          # WPT tests (MUST NOT REGRESS)

# Memory leak detection (Phase 2+)
./target/debug/jsrt_m test/test_node_fs_async.js
ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m test/test_node_fs_promises.js

# Platform-specific (use jsrt-cross-platform agent)
make test                         # Linux
make test                         # macOS
make test                         # Windows (WSL or native)
```

### WPT Compliance Targets

**Baseline Maintenance:**
- Current WPT pass rate: 90.6% (MUST MAINTAIN)
- No new failures in existing tests
- File API tests may improve pass rate

**Relevant WPT Test Suites:**
- `wpt/FileAPI/*` - File and Blob tests
- `wpt/streams/*` - Stream integration

### Performance Benchmarks (Phase 5)

**Metrics to Track:**
1. **Operation Latency:**
   - Sync operations: <1ms for small files (<1MB)
   - Async operations: overhead <5ms vs sync
   - Promise operations: overhead <2ms vs callback

2. **Throughput:**
   - ReadStream: >500 MB/s for large files
   - WriteStream: >300 MB/s for large files

3. **Memory Usage:**
   - FileHandle: <1KB per handle
   - Async work: <512 bytes per operation

---

## Risk Assessment & Mitigation

### Technical Risks

**1. libuv Integration Complexity (HIGH RISK) - Phase 2**
- **Risk:** Async operations may not integrate cleanly with QuickJS event loop
- **Impact:** Delays Phase 2-5, breaks async functionality
- **Mitigation:**
  - Study existing jsrt libuv integration (timer.c, fetch.c)
  - Create minimal proof-of-concept first (Task 2.4)
  - Incremental refactoring (one operation at a time)
  - Extensive testing with concurrent operations
  - Use jsrt-tester agent for memory analysis

**2. Memory Leaks in Async Operations (HIGH RISK) - Phase 2-3**
- **Risk:** Complex async workflows may leak JS values or memory
- **Impact:** Production stability issues, memory exhaustion
- **Mitigation:**
  - Mandatory ASAN testing for all async code
  - Reference counting for all JS values in async contexts
  - Cleanup handlers for ALL error paths (no shortcuts)
  - Debug fd tracking in development builds
  - Comprehensive leak tests in Phase 5

**3. FileHandle Lifecycle Management (MEDIUM RISK) - Phase 3**
- **Risk:** File descriptors may leak if not properly closed
- **Impact:** fd exhaustion, can't open more files
- **Mitigation:**
  - QuickJS finalizers for automatic cleanup
  - Debug mode fd tracking (#ifdef DEBUG)
  - Explicit close() requirements in documentation
  - Tests that verify fd cleanup (check /proc/self/fd on Linux)

**4. Cross-Platform Path Handling (MEDIUM RISK) - All Phases**
- **Risk:** Windows path behavior differs from POSIX (backslash vs slash, Unicode)
- **Impact:** Features work on Linux but fail on Windows
- **Mitigation:**
  - Centralized path normalization functions
  - Windows-specific test suite
  - Use jsrt-cross-platform agent for validation
  - Document platform-specific behavior clearly

**5. Stream Backpressure (MEDIUM RISK) - Phase 4**
- **Risk:** Memory exhaustion with fast producers/slow consumers
- **Impact:** OOM crashes with large files
- **Mitigation:**
  - Implement proper backpressure from the start
  - Configurable buffer sizes (default 64KB)
  - Pause/resume mechanisms
  - Stress tests with large files (>1GB)

**6. Breaking Existing Functionality (LOW RISK) - All Phases**
- **Risk:** New code breaks existing fs implementation
- **Impact:** Regression in working features
- **Mitigation:**
  - Keep existing tests passing at all times (100% pass rate)
  - Separate files for new features (avoid editing working code)
  - Incremental integration (one API at a time)
  - Regular regression testing (make test after every change)

### Compatibility Risks

**1. Node.js API Evolution (LOW RISK)**
- **Risk:** Node.js adds new APIs or changes behavior
- **Mitigation:**
  - Target stable LTS version (Node.js 20.x)
  - Document version compatibility in README
  - Track Node.js releases for breaking changes

**2. Platform-Specific Behavior (MEDIUM RISK)**
- **Risk:** Inconsistent behavior across Linux/macOS/Windows
- **Mitigation:**
  - Use jsrt-cross-platform agent for validation
  - CI/CD on all platforms (if available)
  - Document known differences (e.g., symlinks on Windows)
  - Graceful degradation where needed

---

## Success Metrics

### Functional Completeness

**Phase-wise targets:**
- ✅ Phase 0 (Foundation): 36 APIs - **COMPLETED**
- 🎯 Phase 1 (Sync Complete): +14 APIs → 50 total (week 2)
- 🎯 Phase 2 (Async libuv): +33 APIs → 83 total (week 5)
- 🎯 Phase 3 (Promises): +40 APIs → 123 total (week 8)
- 🎯 Phase 4 (Classes): +6 classes → Full feature set (week 12)
- 🎯 Phase 5 (Quality): 100% tests passing (week 14)

**API Coverage:**
| Milestone | Sync APIs | Async APIs | Promise APIs | Classes | Total | % Complete |
|-----------|-----------|------------|--------------|---------|-------|------------|
| **Phase 0 (Done)** | 28 | 7 | 0 | 1 | 36 | 38% ✅ |
| Phase 1 Target | 42 | 7 | 0 | 1 | 50 | 53% |
| Phase 2 Target | 42 | 40 | 0 | 1 | 83 | 87% |
| Phase 3 Target | 42 | 40 | 40 | 2 | 124 | 95% |
| Phase 4 Target | 42 | 40 | 40 | 7 | 129 | 98% |
| **Final Target** | 42 | 40 | 40 | 7 | **129** | **98%** |

**Compatibility Score:**
- Current: ~38% Node.js fs API compatibility
- Target: 95%+ Node.js fs API compatibility
- Tracking: Compare with Node.js v20.x LTS API surface

### Quality Metrics (MANDATORY)

**Test Coverage:**
- Unit test pass rate: 100% (mandatory before any commit)
- WPT pass rate: ≥90.6% (maintain baseline, no regression)
- Platform coverage: Linux (100%), macOS (90%), Windows (85%)
- Memory leak tests: 0 leaks in ASAN runs (mandatory Phase 2+)

**Performance Targets (Phase 5):**
- Sync operations: within 10% of Node.js
- Async operations: within 20% of Node.js (libuv overhead acceptable)
- Stream throughput: >50% of Node.js performance
- Memory overhead: <2MB for typical workloads

**Code Quality (Enforced by make format):**
- All code formatted (make format before commit)
- No compiler warnings (-Wall -Wextra)
- Files <500 lines (refactor if exceeded)
- Clear error messages matching Node.js format

---

## Implementation Timeline & Priorities

### Revised Timeline (From Current Status)

**Weeks 1-2: Phase 1 (Foundation Complete)**
- Focus: Complete missing sync APIs
- Deliverable: 14 new sync functions, all tested
- Risk: LOW - straightforward POSIX wrappers
- **Output:** 50 total APIs (53% coverage)

**Weeks 3-5: Phase 2 (CRITICAL PATH - Async Infrastructure)**
- Focus: libuv integration, refactor async
- Deliverable: True async I/O for all operations
- Risk: HIGH - core architecture change
- **Checkpoint:** All existing tests still pass, new async tests added
- **Output:** 83 total APIs (87% coverage)

**Weeks 6-8: Phase 3 (Modern API)**
- Focus: Promise API, FileHandle class
- Deliverable: fs.promises namespace fully functional
- Risk: MEDIUM - lifecycle management complexity
- **Output:** 124 total APIs (95% coverage) ✅ **TARGET MET**

**Weeks 9-12: Phase 4 (Advanced Features)**
- Focus: Dir, Streams, FSWatcher
- Deliverable: Full featured classes
- Risk: MEDIUM - each class is independent
- **Parallel work possible:** 3-4 classes simultaneously
- **Output:** 129 total APIs (98% coverage)

**Weeks 13-14: Phase 5 (Production Ready)**
- Focus: Testing, cross-platform, bug fixes
- Deliverable: Production-ready implementation
- Risk: LOW - refinement phase
- **Output:** All quality metrics met, documentation complete

**Total Timeline: 12-14 weeks** (vs original 16 weeks - 2-4 weeks ahead due to existing foundation!)

### Critical Path Analysis

```
CRITICAL PATH (Cannot Parallelize):
Phase 1 (2 weeks)
  ↓
Phase 2.1-2.5: libuv core (2 weeks) ← BOTTLENECK
  ↓
Phase 3.1-3.5: Promise core (1 week)
  ↓
Phase 4: Classes can parallelize (3 weeks)

TOTAL CRITICAL PATH: ~8 weeks

OFF CRITICAL PATH (Can Parallelize):
Phase 2.6-2.18: async APIs (1 week parallel)
Phase 3.6-3.22: Promise APIs (2 weeks parallel)
Phase 5: Testing (ongoing throughout)
```

**Acceleration Opportunities:**
- Phase 2.6-2.18: 11 tasks can run in parallel (save 2 weeks)
- Phase 3.6-3.22: 15 tasks can run in parallel (save 2 weeks)
- Phase 4: 4 class groups can run in parallel (save 6 weeks)
- **Potential savings: 6-8 weeks if fully parallel**

### Immediate Next Steps (Start Today!)

**Task 1.1: Implement fstatSync (Day 1)**
```c
// File: src/node/fs/fs_sync_fd_stat.c (NEW)
JSValue js_fs_fstat_sync(JSContext* ctx, JSValueConst this_val,
                        int argc, JSValueConst* argv) {
    // Get fd from argv[0]
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;

    // Call fstat
    struct stat st;
    if (fstat(fd, &st) < 0) {
        return JS_ThrowError(ctx, create_fs_error(ctx, errno, "fstat", NULL));
    }

    // Create Stats object (reuse existing create_stats_object)
    return create_stats_object(ctx, &st);
}
```

**Task 1.2: Implement lstatSync (Day 1)**
```c
// File: src/node/fs/fs_sync_fd_stat.c
JSValue js_fs_lstat_sync(JSContext* ctx, JSValueConst this_val,
                         int argc, JSValueConst* argv) {
    // Similar to statSync but use lstat instead of stat
    // (doesn't follow symlinks)
}
```

**Test Creation (Day 1):**
```javascript
// test/test_node_fs_sync.js (add to existing)
import * as fs from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

// Test fstatSync
const testFile = join(tmpdir(), 'test-fstat.txt');
fs.writeFileSync(testFile, 'test content');
const fd = fs.openSync(testFile, 'r');
const stats = fs.fstatSync(fd);
console.assert(stats.isFile(), 'fstatSync should return Stats object');
console.assert(stats.size === 12, 'File size should be 12');
fs.closeSync(fd);
fs.unlinkSync(testFile);

// Test lstatSync (with symlink)
// ...
```

---

## Appendix: Complete API Reference

### Node.js fs Module - Full Checklist

**Sync APIs (42 total, 28 done, 14 remaining):**

✅ **File I/O (8/8 - 100%):**
- readFileSync, writeFileSync, appendFileSync ✅
- copyFileSync, renameSync, unlinkSync ✅
- existsSync ✅
- truncateSync ✅

✅ **Links (4/4 - 100%):**
- linkSync, symlinkSync, readlinkSync, realpathSync ✅

⚠️ **Directory Operations (4/5 - 80%):**
- mkdirSync (with recursive), rmdirSync, readdirSync ✅
- ⏳ opendirSync, mkdtempSync ✅

⚠️ **File Stats (1/4 - 25%):**
- statSync ✅
- ⏳ fstatSync, lstatSync, statfsSync

⚠️ **File Descriptors (4/6 - 67%):**
- openSync, closeSync, readSync, writeSync ✅
- ⏳ readvSync, writevSync

⚠️ **Permissions (3/7 - 43%):**
- chmodSync, chownSync, accessSync ✅
- ⏳ fchmodSync, fchownSync, lchownSync

⚠️ **Time Attributes (1/3 - 33%):**
- utimesSync ✅
- ⏳ futimesSync, lutimesSync

⚠️ **Advanced (5/10 - 50%):**
- truncateSync, ftruncateSync, fsyncSync, fdatasyncSync, mkdtempSync ✅
- ⏳ rmSync (recursive), cpSync (recursive), statfsSync, globSync (Node 22+)

**Async Callback APIs (7/40 - 18%):**
- readFile, writeFile, appendFile, copyFile, rename, rmdir, access ✅
- ⏳ 33 remaining (all sync APIs need async versions)

**Promise APIs (0/40 - 0%):**
- ⏳ fs.promises.* (complete namespace)
- ⏳ FileHandle class with methods

**Classes (1/7 - 14%):**
- Stats ✅ (basic)
- ⏳ Dirent, FileHandle, Dir, ReadStream, WriteStream, FSWatcher

**Constants (4/20+ - partial):**
- F_OK, R_OK, W_OK, X_OK ✅
- ⏳ O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_EXCL, O_TRUNC, O_APPEND, etc.
- ⏳ S_IFMT, S_IFREG, S_IFDIR, S_IFLNK, S_IFSOCK, etc.
- ⏳ S_IRWXU, S_IRUSR, S_IWUSR, S_IXUSR, etc.
- ⏳ COPYFILE_EXCL, COPYFILE_FICLONE, COPYFILE_FICLONE_FORCE

---

## Conclusion

**Current Achievement: 38% Complete (36/95 APIs) ✅**

This revised plan reflects the **strong foundation** already in place:
- ✅ 28 sync APIs covering core file operations
- ✅ 7 async APIs with basic callback support
- ✅ Well-organized codebase (~2,457 lines across 9 files)
- ✅ Proven patterns for error handling and memory management
- ✅ 100% test pass rate for implemented features

**Key Milestones Ahead:**
1. **Phase 1 (2 weeks):** Complete remaining sync APIs → 53% coverage
2. **Phase 2 (3 weeks):** libuv async infrastructure → 87% coverage (CRITICAL)
3. **Phase 3 (3 weeks):** Promise API + FileHandle → 95% coverage ✅ **SUCCESS THRESHOLD**
4. **Phase 4 (4 weeks):** Advanced classes → 98% coverage
5. **Phase 5 (2 weeks):** Production hardening → Release ready

**Critical Success Factors:**
1. **Leverage existing code:** Reuse proven error handling, memory patterns
2. **Phase 2 is critical path:** libuv integration unblocks Phases 3-4
3. **Maximize parallelism:** 20-30 tasks can run in parallel after Phase 2
4. **Quality first:** Mandatory testing (make test + make wpt) before every commit
5. **Cross-platform:** Use jsrt-cross-platform agent for Windows/macOS validation

**Estimated Completion: 12-14 weeks** (ahead of original 16-week estimate due to solid foundation!)

**Immediate Next Action:**
```bash
cd /home/lei/work/jsrt
# Create new file for Phase 1 tasks
touch src/node/fs/fs_sync_fd_stat.c
# Start with Task 1.1: Implement fstatSync
```

---

*Document Version: 2.0*
*Created: 2025-10-04*
*Last Updated: 2025-10-04 (Revised with actual implementation status)*
*Current Status: Phase 0 Complete (38%), Phase 1 Ready to Start*
*Target Completion: Q2 2025*
