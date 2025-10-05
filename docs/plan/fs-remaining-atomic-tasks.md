---
Created: 2025-10-05T23:30:00Z
Last Updated: 2025-10-05T23:30:00Z
Status: 🟡 PLANNING
Overall Progress: 104/132 APIs (78.8% complete)
Remaining: 28 APIs
---

# Node.js fs Module - Atomic Task Breakdown for Remaining Work

## Executive Summary

**Current Status:**
- ✅ 104/132 APIs implemented (78.8% complete)
- ✅ Phase 0-3 completed (Foundation, Sync, Async, Promise core)
- ✅ Quality metrics: 113/113 tests (100%), 0 memory leaks, 90.6% WPT
- 🎯 Remaining: 28 APIs across 3 categories

**Recommended Approach:**
- **Phase A (HIGH PRIORITY)**: 8 high-value APIs → 84.8% complete (5-6 hours)
- **Phase B (RECOMMENDED)**: Additional 12 APIs → 90.9% complete (14-15 hours total)
- **Phase C (OPTIONAL)**: Additional 4 APIs → 93.2% complete (16-17 hours total)
- **Phase D (DEFER)**: Advanced features → 100% (40+ hours - not recommended)

**Critical Path Analysis:**
- ✅ NO BLOCKING DEPENDENCIES - All tasks are independent
- ✅ MAXIMUM PARALLELIZATION - Can execute all tasks in parallel within each session
- ✅ PROVEN PATTERNS - All implementation patterns established in existing 104 APIs
- ✅ LOW RISK - Very high confidence in estimates and success

**Key Insight:**
The remaining 21% of APIs account for only ~10% of actual real-world usage. Current 78.8% completion already covers 90%+ of common use cases. Phase A+B targets 90.9% API coverage with maximum value/effort ratio.

---

## Phase A: High-Value APIs (Priority 1)

**Target:** 104 → 112 APIs (84.8% complete)
**Effort:** 5-6 hours
**Risk:** VERY LOW
**Value:** HIGHEST
**Parallelization:** 100% - All tasks independent

### Session A1: FileHandle Convenience Methods

**Context:** FileHandle is the modern Promise-based file API. Missing convenience methods force users to manually manage open/close cycles. These are the MOST REQUESTED methods.

#### Task A1.1: FileHandle.appendFile()
**ID:** FH-APPEND-001
**Priority:** CRITICAL ⭐
**Execution Mode:** [P] Parallel
**Dependencies:** None (base FileHandle class exists)
**Risk:** LOW
**Complexity:** MEDIUM

**Description:**
Implement FileHandle.appendFile(data, options) method that appends data to the file associated with the FileHandle.

**Technical Details:**
- Pattern: Use existing FileHandle.write() method in append mode
- Implementation: Multi-step async (fstat to get size → write at end)
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +80 lines
- Memory: Allocate buffer for string/Buffer conversion, free in callback
- Error handling: Propagate write errors, handle closed FileHandle

**Acceptance Criteria:**
- [ ] Appends string/Buffer/TypedArray data to file
- [ ] Supports encoding option (default: 'utf8')
- [ ] Returns Promise<void>
- [ ] Throws if FileHandle is closed
- [ ] Handles empty data correctly
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: happy path + error cases (at least 3 tests)

**Test Plan:**
```javascript
// test/test_fs_filehandle_append.js
import { open } from 'node:fs/promises';

// Test 1: Basic append
const fh = await open('test.txt', 'w');
await fh.appendFile('line1\n');
await fh.appendFile('line2\n');
await fh.close();
const content = readFileSync('test.txt', 'utf8');
assert(content === 'line1\nline2\n');

// Test 2: Closed FileHandle error
await assert.rejects(fh.appendFile('data'), /closed/);

// Test 3: Buffer append
const fh2 = await open('test2.txt', 'w');
await fh2.appendFile(Buffer.from('binary data'));
await fh2.close();
```

**Estimated Effort:** 1 hour

---

#### Task A1.2: FileHandle.readFile()
**ID:** FH-READ-001
**Priority:** CRITICAL ⭐
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** MEDIUM

**Description:**
Implement FileHandle.readFile(options) method that reads the entire file contents.

**Technical Details:**
- Pattern: fstat to get size → allocate buffer → read → return Buffer/string
- Implementation: Multi-step async (fstat → read → convert)
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +100 lines
- Memory: Allocate buffer matching file size, handle empty files (malloc(0) issue)
- Error handling: Check file size, handle read errors, encoding errors

**Acceptance Criteria:**
- [ ] Reads entire file into Buffer (default)
- [ ] Supports encoding option to return string
- [ ] Returns Promise<Buffer> or Promise<string>
- [ ] Throws if FileHandle is closed
- [ ] Handles empty files correctly (0 bytes)
- [ ] ASAN clean (0 leaks)
- [ ] Cross-platform safe (malloc(0) handling)
- [ ] Test coverage: happy path + edge cases (at least 4 tests)

**Test Plan:**
```javascript
// Test 1: Read as Buffer
const fh = await open('test.txt', 'r');
const buffer = await fh.readFile();
assert(buffer instanceof Buffer);

// Test 2: Read as string
const content = await fh.readFile({ encoding: 'utf8' });
assert(typeof content === 'string');

// Test 3: Empty file
const emptyFh = await open('empty.txt', 'r');
const emptyContent = await emptyFh.readFile();
assert(emptyContent.length === 0);

// Test 4: Closed FileHandle
await fh.close();
await assert.rejects(fh.readFile(), /closed/);
```

**Estimated Effort:** 1 hour

---

#### Task A1.3: FileHandle.writeFile()
**ID:** FH-WRITE-001
**Priority:** CRITICAL ⭐
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** MEDIUM

**Description:**
Implement FileHandle.writeFile(data, options) method that writes data to the file, replacing existing content.

**Technical Details:**
- Pattern: truncate(0) → write(data)
- Implementation: Multi-step async (ftruncate → write)
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +90 lines
- Memory: Allocate buffer for string conversion, free in callback
- Error handling: Handle truncate errors, write errors, encoding errors

**Acceptance Criteria:**
- [ ] Truncates file and writes new content
- [ ] Accepts string/Buffer/TypedArray
- [ ] Supports encoding option (default: 'utf8')
- [ ] Returns Promise<void>
- [ ] Throws if FileHandle is closed
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: happy path + error cases (at least 3 tests)

**Test Plan:**
```javascript
// Test 1: Replace file content
const fh = await open('test.txt', 'w');
await fh.writeFile('initial content');
await fh.writeFile('new content');
await fh.close();
const content = readFileSync('test.txt', 'utf8');
assert(content === 'new content');

// Test 2: Buffer write
const fh2 = await open('test2.txt', 'w');
await fh2.writeFile(Buffer.from('buffer data'));
await fh2.close();

// Test 3: Closed FileHandle
await assert.rejects(fh.writeFile('data'), /closed/);
```

**Estimated Effort:** 1 hour

---

#### Task A1.4: FileHandle.chmod()
**ID:** FH-CHMOD-001
**Priority:** HIGH
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement FileHandle.chmod(mode) method that changes file permissions.

**Technical Details:**
- Pattern: Direct uv_fs_fchmod() call
- Implementation: Single async operation
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +60 lines
- Memory: Simple work request structure
- Error handling: Permission errors, closed FileHandle

**Acceptance Criteria:**
- [ ] Changes file permissions via file descriptor
- [ ] Accepts numeric mode (e.g., 0o644)
- [ ] Returns Promise<void>
- [ ] Throws if FileHandle is closed
- [ ] Cross-platform (Windows limited support)
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: happy path + error case (at least 2 tests)

**Test Plan:**
```javascript
// Test 1: Change permissions
const fh = await open('test.txt', 'w');
await fh.chmod(0o644);
const stats = await fh.stat();
assert((stats.mode & 0o777) === 0o644);

// Test 2: Closed FileHandle
await fh.close();
await assert.rejects(fh.chmod(0o644), /closed/);
```

**Estimated Effort:** 30 minutes

---

### Session A2: Async Callback Low-Level Operations

**Context:** These are low-level file operations that currently only exist in sync form. Adding async versions completes the callback API coverage for file descriptor operations.

#### Task A2.1: fs.truncate(path, len, callback)
**ID:** ASYNC-TRUNC-001
**Priority:** MEDIUM
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement async callback version of truncate() that truncates a file to specified length.

**Technical Details:**
- Pattern: uv_fs_open() → uv_fs_ftruncate() → uv_fs_close()
- Implementation: Multi-step async operation
- Files to modify: src/node/fs/fs_async_core.c
- Estimated lines: +80 lines
- Memory: Standard async work request
- Error handling: File not found, permission errors

**Acceptance Criteria:**
- [ ] Truncates file to specified length
- [ ] Callback signature: (err) => void
- [ ] Handles path resolution
- [ ] Extends file with null bytes if len > current size
- [ ] Shrinks file if len < current size
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: shrink, extend, error cases (at least 3 tests)

**Test Plan:**
```javascript
// Test 1: Truncate to smaller size
fs.truncate('test.txt', 10, (err) => {
  assert(!err);
  assert(statSync('test.txt').size === 10);
});

// Test 2: Extend file
fs.truncate('test.txt', 100, (err) => {
  assert(!err);
  assert(statSync('test.txt').size === 100);
});
```

**Estimated Effort:** 30 minutes

---

#### Task A2.2: fs.ftruncate(fd, len, callback)
**ID:** ASYNC-FTRUNC-001
**Priority:** MEDIUM
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement async callback version of ftruncate() that truncates an open file descriptor.

**Technical Details:**
- Pattern: Direct uv_fs_ftruncate() call
- Implementation: Single async operation
- Files to modify: src/node/fs/fs_async_core.c
- Estimated lines: +60 lines
- Memory: Standard async work request
- Error handling: Invalid fd, permission errors

**Acceptance Criteria:**
- [ ] Truncates file via file descriptor
- [ ] Callback signature: (err) => void
- [ ] Works with open file descriptors
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: happy path + error case (at least 2 tests)

**Test Plan:**
```javascript
// Test 1: Truncate via fd
const fd = fs.openSync('test.txt', 'r+');
fs.ftruncate(fd, 50, (err) => {
  assert(!err);
  assert(fstatSync(fd).size === 50);
  fs.closeSync(fd);
});
```

**Estimated Effort:** 30 minutes

---

#### Task A2.3: fs.fsync(fd, callback)
**ID:** ASYNC-FSYNC-001
**Priority:** MEDIUM
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement async callback version of fsync() that synchronizes file data to disk.

**Technical Details:**
- Pattern: Direct uv_fs_fsync() call
- Implementation: Single async operation
- Files to modify: src/node/fs/fs_async_core.c
- Estimated lines: +60 lines
- Memory: Standard async work request
- Error handling: Invalid fd

**Acceptance Criteria:**
- [ ] Synchronizes file data and metadata to disk
- [ ] Callback signature: (err) => void
- [ ] Works with open file descriptors
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: happy path + error case (at least 2 tests)

**Test Plan:**
```javascript
// Test 1: Sync file
const fd = fs.openSync('test.txt', 'w');
fs.writeSync(fd, 'data');
fs.fsync(fd, (err) => {
  assert(!err);
  fs.closeSync(fd);
});
```

**Estimated Effort:** 30 minutes

---

#### Task A2.4: fs.fdatasync(fd, callback)
**ID:** ASYNC-FDSYNC-001
**Priority:** MEDIUM
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement async callback version of fdatasync() that synchronizes file data (not metadata) to disk.

**Technical Details:**
- Pattern: Direct uv_fs_fdatasync() call
- Implementation: Single async operation
- Files to modify: src/node/fs/fs_async_core.c
- Estimated lines: +60 lines
- Memory: Standard async work request
- Error handling: Invalid fd

**Acceptance Criteria:**
- [ ] Synchronizes file data (not metadata) to disk
- [ ] Faster than fsync (doesn't sync metadata)
- [ ] Callback signature: (err) => void
- [ ] Works with open file descriptors
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: happy path + error case (at least 2 tests)

**Test Plan:**
```javascript
// Test 1: Sync file data only
const fd = fs.openSync('test.txt', 'w');
fs.writeSync(fd, 'data');
fs.fdatasync(fd, (err) => {
  assert(!err);
  fs.closeSync(fd);
});
```

**Estimated Effort:** 30 minutes

---

### Phase A Summary

**Total Tasks:** 8 APIs
**Total Effort:** 5-6 hours
**Parallelization:** 100% - All tasks can run simultaneously
**Risk:** VERY LOW - All patterns proven
**Impact:** 104 → 112 APIs (84.8% complete)

**Completion Criteria:**
- [ ] All 8 APIs implemented and tested
- [ ] make format && make test && make wpt (all passing)
- [ ] ASAN verification (0 leaks)
- [ ] Update node-fs-plan.md with progress
- [ ] Git commit with clear message

**Testing Strategy:**
- Test each API individually during implementation
- Full regression test suite after all 8 complete
- ASAN run on all new tests
- Cross-platform smoke test (Linux minimum)

---

## Phase B: Medium-Value APIs (Priority 2)

**Target:** 112 → 120 APIs (90.9% complete)
**Effort:** 9 additional hours (14-15 hours total from start)
**Risk:** LOW
**Value:** MEDIUM-HIGH
**Parallelization:** 100% - All tasks independent

### Session B1: Promise API Wrappers

**Context:** These Promise APIs wrap existing sync/async implementations. They complete the fsPromises namespace for commonly-used operations.

#### Task B1.1: fsPromises.mkdtemp(prefix, options)
**ID:** PROM-MKDTEMP-001
**Priority:** MEDIUM
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** MEDIUM

**Description:**
Implement Promise version of mkdtemp() that creates a unique temporary directory.

**Technical Details:**
- Pattern: Generate unique suffix → uv_fs_mkdir()
- Implementation: Template generation + async mkdir
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +90 lines
- Memory: Allocate template string, free in callback
- Error handling: Template validation, mkdir errors

**Acceptance Criteria:**
- [ ] Creates unique temporary directory
- [ ] Accepts prefix string
- [ ] Supports encoding option (default: 'utf8')
- [ ] Returns Promise<string> with created path
- [ ] Uses 6 random characters for uniqueness
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: basic, encoding, error (at least 3 tests)

**Test Plan:**
```javascript
// Test 1: Basic mkdtemp
const dir = await fsPromises.mkdtemp('/tmp/test-');
assert(dir.startsWith('/tmp/test-'));
assert(existsSync(dir));

// Test 2: Multiple calls create unique dirs
const dir1 = await fsPromises.mkdtemp('/tmp/test-');
const dir2 = await fsPromises.mkdtemp('/tmp/test-');
assert(dir1 !== dir2);
```

**Estimated Effort:** 1 hour

---

#### Task B1.2: fsPromises.truncate(path, len)
**ID:** PROM-TRUNC-001
**Priority:** MEDIUM
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement Promise version of truncate() that truncates a file to specified length.

**Technical Details:**
- Pattern: uv_fs_open() → uv_fs_ftruncate() → uv_fs_close()
- Implementation: Multi-step async operation
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +80 lines
- Memory: Standard promise work request
- Error handling: File not found, permission errors

**Acceptance Criteria:**
- [ ] Truncates file to specified length
- [ ] Returns Promise<void>
- [ ] Handles path resolution
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: shrink, extend, error (at least 3 tests)

**Estimated Effort:** 30 minutes

---

#### Task B1.3: fsPromises.copyFile(src, dest, mode)
**ID:** PROM-COPY-001
**Priority:** MEDIUM
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** MEDIUM

**Description:**
Implement Promise version of copyFile() that copies a file.

**Technical Details:**
- Pattern: Use existing uv_fs_copyfile()
- Implementation: Single async operation with mode flags
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +70 lines
- Memory: Standard promise work request
- Error handling: Source not found, destination exists (if COPYFILE_EXCL)

**Acceptance Criteria:**
- [ ] Copies file from src to dest
- [ ] Supports mode flags (COPYFILE_EXCL, etc.)
- [ ] Returns Promise<void>
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: basic, excl flag, error (at least 3 tests)

**Test Plan:**
```javascript
// Test 1: Basic copy
await fsPromises.copyFile('source.txt', 'dest.txt');
assert(existsSync('dest.txt'));

// Test 2: COPYFILE_EXCL
await fsPromises.copyFile('source.txt', 'dest.txt'); // OK
await assert.rejects(
  fsPromises.copyFile('source.txt', 'dest.txt', COPYFILE_EXCL),
  /exists/
);
```

**Estimated Effort:** 1 hour

---

#### Task B1.4: fsPromises.lchmod(path, mode)
**ID:** PROM-LCHMOD-001
**Priority:** LOW
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement Promise version of lchmod() that changes permissions without following symlinks.

**Technical Details:**
- Pattern: Use existing lchmod() logic with Promise wrapper
- Implementation: Single async operation
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +60 lines
- Memory: Standard promise work request
- Error handling: Platform support check (macOS only), permission errors

**Acceptance Criteria:**
- [ ] Changes symlink permissions (macOS only)
- [ ] Returns Promise<void>
- [ ] Throws ENOSYS on unsupported platforms
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: platform check (at least 1 test)

**Estimated Effort:** 45 minutes

---

### Session B2: Async Callback Advanced Operations

**Context:** Complete the async callback API surface with less commonly used operations.

#### Task B2.1: fs.mkdtemp(prefix, options, callback)
**ID:** ASYNC-MKDTEMP-001
**Priority:** MEDIUM
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** MEDIUM

**Description:**
Implement async callback version of mkdtemp() that creates a unique temporary directory.

**Technical Details:**
- Pattern: Same as Promise version but with callback
- Implementation: Template generation + async mkdir
- Files to modify: src/node/fs/fs_async_core.c
- Estimated lines: +90 lines
- Memory: Standard async work request
- Error handling: Template validation, mkdir errors

**Acceptance Criteria:**
- [ ] Creates unique temporary directory
- [ ] Callback signature: (err, folder) => void
- [ ] Returns created path in callback
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: basic, encoding, error (at least 3 tests)

**Estimated Effort:** 1 hour

---

#### Task B2.2: fs.statfs(path, options, callback)
**ID:** ASYNC-STATFS-001
**Priority:** LOW
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** MEDIUM

**Description:**
Implement async callback version of statfs() that returns filesystem statistics.

**Technical Details:**
- Pattern: Use existing uv_fs_statfs() (libuv 1.31+)
- Implementation: Single async operation with result object creation
- Files to modify: src/node/fs/fs_async_core.c
- Estimated lines: +100 lines
- Memory: Standard async work request + stats object
- Error handling: Path not found, unsupported platform

**Acceptance Criteria:**
- [ ] Returns filesystem statistics object
- [ ] Callback signature: (err, stats) => void
- [ ] Stats includes: type, bsize, blocks, bfree, bavail, files, ffree
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: basic, error (at least 2 tests)

**Estimated Effort:** 1 hour

---

### Session B3: FileHandle Vectored I/O

**Context:** Vectored I/O operations (readv/writev) allow reading/writing to multiple buffers in a single syscall. These are advanced operations for performance-critical code.

#### Task B3.1: FileHandle.readv(buffers, position)
**ID:** FH-READV-001
**Priority:** MEDIUM
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** MEDIUM
**Complexity:** MEDIUM

**Description:**
Implement FileHandle.readv() method that reads into multiple buffers in a single operation.

**Technical Details:**
- Pattern: Use existing uv_fs_read() with iov array
- Implementation: Convert JS buffer array → uv_buf_t[] → async read
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +120 lines
- Memory: Allocate uv_buf_t array, manage buffer references
- Error handling: Invalid buffers, closed FileHandle, read errors

**Acceptance Criteria:**
- [ ] Reads into array of Buffer/TypedArray
- [ ] Accepts optional position parameter
- [ ] Returns Promise<{ bytesRead, buffers }>
- [ ] Throws if FileHandle is closed
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: basic, position, multiple buffers (at least 3 tests)

**Test Plan:**
```javascript
// Test 1: Read into multiple buffers
const fh = await open('test.txt', 'r');
const buf1 = Buffer.alloc(10);
const buf2 = Buffer.alloc(10);
const { bytesRead } = await fh.readv([buf1, buf2]);
assert(bytesRead <= 20);

// Test 2: Read with position
const { bytesRead: read2 } = await fh.readv([buf1], 100);
```

**Estimated Effort:** 2 hours

---

#### Task B3.2: FileHandle.writev(buffers, position)
**ID:** FH-WRITEV-001
**Priority:** MEDIUM
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** MEDIUM
**Complexity:** MEDIUM

**Description:**
Implement FileHandle.writev() method that writes from multiple buffers in a single operation.

**Technical Details:**
- Pattern: Use existing uv_fs_write() with iov array
- Implementation: Convert JS buffer array → uv_buf_t[] → async write
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +120 lines
- Memory: Allocate uv_buf_t array, manage buffer references
- Error handling: Invalid buffers, closed FileHandle, write errors

**Acceptance Criteria:**
- [ ] Writes from array of Buffer/TypedArray
- [ ] Accepts optional position parameter
- [ ] Returns Promise<{ bytesWritten, buffers }>
- [ ] Throws if FileHandle is closed
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: basic, position, multiple buffers (at least 3 tests)

**Test Plan:**
```javascript
// Test 1: Write from multiple buffers
const fh = await open('test.txt', 'w');
const buf1 = Buffer.from('hello ');
const buf2 = Buffer.from('world');
const { bytesWritten } = await fh.writev([buf1, buf2]);
assert(bytesWritten === 11);

// Test 2: Write with position
const { bytesWritten: written2 } = await fh.writev([buf1], 100);
```

**Estimated Effort:** 2 hours

---

### Phase B Summary

**Total Tasks:** 8 APIs
**Total Effort:** 9 hours
**Cumulative Effort:** 14-15 hours (from Phase A start)
**Parallelization:** 100% - All tasks can run simultaneously
**Risk:** LOW - Proven patterns, slightly higher complexity for vectored I/O
**Impact:** 112 → 120 APIs (90.9% complete)

**Completion Criteria:**
- [ ] All 8 APIs implemented and tested
- [ ] make format && make test && make wpt (all passing)
- [ ] ASAN verification (0 leaks)
- [ ] Update node-fs-plan.md with progress
- [ ] Git commit with clear message

**Testing Strategy:**
- Test each API individually during implementation
- Full regression test suite after all 8 complete
- ASAN run on all new tests
- Performance benchmarks for vectored I/O
- Cross-platform smoke test (Linux minimum)

---

## Phase C: Low-Priority Completeness (Optional)

**Target:** 120 → 123 APIs (93.2% complete)
**Effort:** 2 additional hours (16-17 hours total from start)
**Risk:** VERY LOW
**Value:** LOW (nice-to-have)
**Parallelization:** 100% - All tasks independent

### Session C1: FileHandle Low-Priority Methods

**Context:** These are rarely-used FileHandle methods that provide completeness for professional polish. Not critical for production use.

#### Task C1.1: FileHandle.sync()
**ID:** FH-SYNC-001
**Priority:** LOW
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement FileHandle.sync() method that synchronizes file data and metadata to disk.

**Technical Details:**
- Pattern: Direct uv_fs_fsync() call
- Implementation: Single async operation
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +60 lines
- Memory: Standard promise work request
- Error handling: Closed FileHandle

**Acceptance Criteria:**
- [ ] Synchronizes file data and metadata to disk
- [ ] Returns Promise<void>
- [ ] Throws if FileHandle is closed
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: happy path + error (at least 2 tests)

**Estimated Effort:** 30 minutes

---

#### Task C1.2: FileHandle.datasync()
**ID:** FH-DATASYNC-001
**Priority:** LOW
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement FileHandle.datasync() method that synchronizes file data (not metadata) to disk.

**Technical Details:**
- Pattern: Direct uv_fs_fdatasync() call
- Implementation: Single async operation
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +60 lines
- Memory: Standard promise work request
- Error handling: Closed FileHandle

**Acceptance Criteria:**
- [ ] Synchronizes file data (not metadata) to disk
- [ ] Faster than sync() (doesn't sync metadata)
- [ ] Returns Promise<void>
- [ ] Throws if FileHandle is closed
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: happy path + error (at least 2 tests)

**Estimated Effort:** 30 minutes

---

#### Task C1.3: FileHandle.utimes(atime, mtime)
**ID:** FH-UTIMES-001
**Priority:** LOW
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement FileHandle.utimes() method that changes file access and modification times.

**Technical Details:**
- Pattern: Direct uv_fs_futime() call
- Implementation: Single async operation
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +70 lines
- Memory: Standard promise work request
- Error handling: Time conversion, closed FileHandle

**Acceptance Criteria:**
- [ ] Changes file access and modification times via fd
- [ ] Accepts Date objects or numeric timestamps
- [ ] Returns Promise<void>
- [ ] Throws if FileHandle is closed
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: happy path + error (at least 2 tests)

**Estimated Effort:** 30 minutes

---

#### Task C1.4: FileHandle[Symbol.asyncDispose]()
**ID:** FH-DISPOSE-001
**Priority:** LOW
**Execution Mode:** [P] Parallel
**Dependencies:** None
**Risk:** LOW
**Complexity:** SIMPLE

**Description:**
Implement FileHandle async disposal for ECMAScript explicit resource management (using statement).

**Technical Details:**
- Pattern: Alias to close() method
- Implementation: Symbol property definition
- Files to modify: src/node/fs/fs_promises.c
- Estimated lines: +20 lines
- Memory: None (reuses close())
- Error handling: Same as close()

**Acceptance Criteria:**
- [ ] FileHandle works with `await using` statement
- [ ] Automatically closes on scope exit
- [ ] Returns Promise<void>
- [ ] ASAN clean (0 leaks)
- [ ] Test coverage: using statement (at least 1 test)

**Test Plan:**
```javascript
// Test 1: Explicit resource management
await using fh = await open('test.txt', 'r');
const content = await fh.readFile();
// fh automatically closed when exiting scope
```

**Estimated Effort:** 15 minutes

---

### Phase C Summary

**Total Tasks:** 4 APIs
**Total Effort:** 2 hours
**Cumulative Effort:** 16-17 hours (from Phase A start)
**Parallelization:** 100% - All tasks can run simultaneously
**Risk:** VERY LOW - Trivial implementations
**Impact:** 120 → 123 APIs (93.2% complete)

**Completion Criteria:**
- [ ] All 4 APIs implemented and tested
- [ ] make format && make test && make wpt (all passing)
- [ ] ASAN verification (0 leaks)
- [ ] Update node-fs-plan.md with progress
- [ ] Git commit with clear message

---

## Phase D: Advanced Features (DEFERRED)

**Target:** 123 → 132 APIs (100% complete)
**Effort:** 40+ hours
**Risk:** MEDIUM
**Value:** VERY LOW
**Recommendation:** ❌ DO NOT IMPLEMENT NOW - Wait for user demand

### Deferred Tasks

#### D1: globSync(pattern, options) - 8-12 hours
- **Why defer:** Node.js 22+ bleeding-edge feature, complex glob pattern matching
- **Complexity:** HIGH - Requires glob pattern parser and matcher
- **Alternative:** Users can use existing readdir + manual filtering

#### D2: fsPromises.glob(pattern, options) - 8-12 hours
- **Why defer:** Same as globSync, plus async iteration
- **Complexity:** HIGH - Requires async generator implementation
- **Alternative:** Users can use existing readdir + manual filtering

#### D3: fsPromises.watch(filename, options) - 8-12 hours
- **Why defer:** Complex file watching, FSWatcher integration
- **Complexity:** HIGH - Requires libuv fs_event integration
- **Alternative:** Users can poll with stat()

#### D4: fsPromises.opendir(path, options) - 4-6 hours
- **Why defer:** Async Dir class with iterator
- **Complexity:** MEDIUM - Requires async iterator protocol
- **Alternative:** Use existing readdir()

#### D5: FileHandle.createReadStream(options) - 8 hours
- **Why defer:** Stream integration complexity
- **Complexity:** HIGH - Requires ReadableStream implementation
- **Alternative:** Use readFile() or manual read() loops

#### D6: FileHandle.createWriteStream(options) - 8 hours
- **Why defer:** Stream integration complexity
- **Complexity:** HIGH - Requires WritableStream implementation
- **Alternative:** Use writeFile() or manual write() loops

#### D7: FileHandle.readLines(options) - 4 hours
- **Why defer:** Async line iterator
- **Complexity:** MEDIUM - Requires async iterator + line splitting
- **Alternative:** Use readFile() + manual split('\n')

#### D8: FileHandle.readableWebStream(options) - 8 hours
- **Why defer:** Web Streams API integration
- **Complexity:** HIGH - Requires ReadableStream web API
- **Alternative:** Use readFile()

### Phase D Summary

**Total Tasks:** 8 APIs
**Total Effort:** 40+ hours (3-4 weeks)
**Value:** VERY LOW (rarely used, bleeding-edge features)
**Recommendation:** DEFER until user demand

**Rationale:**
- These features account for <5% of real-world usage
- High implementation complexity vs low user benefit
- Better to wait for actual user requests
- Can implement on-demand if needed

---

## Dependency Graph

```
ALL REMAINING TASKS ARE INDEPENDENT - NO BLOCKING DEPENDENCIES

Session A1 (FileHandle Essentials)
├── A1.1 appendFile  ──┐
├── A1.2 readFile     ├── All parallel, no dependencies
├── A1.3 writeFile    │
└── A1.4 chmod        ┘

Session A2 (Async Callbacks Basic)
├── A2.1 truncate     ┐
├── A2.2 ftruncate    ├── All parallel, no dependencies
├── A2.3 fsync        │
└── A2.4 fdatasync    ┘

Session B1 (Promise Wrappers)
├── B1.1 mkdtemp      ┐
├── B1.2 truncate     ├── All parallel, no dependencies
├── B1.3 copyFile     │
└── B1.4 lchmod       ┘

Session B2 (Async Advanced)
├── B2.1 mkdtemp      ┐
└── B2.2 statfs       ┘── Both parallel, no dependencies

Session B3 (Vectored I/O)
├── B3.1 readv        ┐
└── B3.2 writev       ┘── Both parallel, no dependencies

Session C1 (Low-Priority)
├── C1.1 sync         ┐
├── C1.2 datasync     ├── All parallel, no dependencies
├── C1.3 utimes       │
└── C1.4 asyncDispose ┘

CRITICAL PATH: NONE (all independent)
BLOCKING TASKS: NONE
MAXIMUM PARALLELISM: 100%
```

---

## Parallel Execution Plan

### Maximum Parallelization Strategy

**Key Insight:** Since all remaining tasks are independent, we can execute multiple tasks simultaneously to minimize wall-clock time.

#### Optimistic Timeline (Parallel Execution)

```
Week 1: Phase A + Phase B
┌─────────────────────────────────────────────────────────────┐
│ Day 1  │ A1.1-A1.4 (4 tasks parallel)    │ 1 hour          │
│ Day 2  │ A2.1-A2.4 (4 tasks parallel)    │ 1 hour          │
│ Day 3  │ B1.1-B1.4 (4 tasks parallel)    │ 1 hour          │
│ Day 4  │ B2.1-B2.2 (2 tasks parallel)    │ 1 hour          │
│ Day 5  │ B3.1-B3.2 (2 tasks parallel)    │ 2 hours         │
│        │ Testing & Integration           │ 3 hours         │
│────────┴─────────────────────────────────┴─────────────────│
│ TOTAL: 9 hours development + 3 hours testing = 12 hours    │
│ RESULT: 104 → 120 APIs (90.9% complete) ✅                 │
└─────────────────────────────────────────────────────────────┘

Week 2 (Optional): Phase C + Polish
┌─────────────────────────────────────────────────────────────┐
│ Day 6  │ C1.1-C1.4 (4 tasks parallel)    │ 30 min          │
│ Day 7  │ Final testing & documentation   │ 2 hours         │
│────────┴─────────────────────────────────┴─────────────────│
│ TOTAL: 2.5 hours                                            │
│ RESULT: 120 → 123 APIs (93.2% complete) ✅                 │
└─────────────────────────────────────────────────────────────┘
```

#### Conservative Timeline (Sequential Execution)

```
Week 1-2: Phase A + Phase B
┌─────────────────────────────────────────────────────────────┐
│ Week 1 │ Phase A (8 APIs sequential)     │ 5-6 hours       │
│        │ Testing                         │ 2 hours         │
│────────┼─────────────────────────────────┼─────────────────│
│ Week 2 │ Phase B (8 APIs sequential)     │ 9 hours         │
│        │ Testing                         │ 3 hours         │
│────────┴─────────────────────────────────┴─────────────────│
│ TOTAL: 19-20 hours over 2 weeks                             │
│ RESULT: 104 → 120 APIs (90.9% complete) ✅                 │
└─────────────────────────────────────────────────────────────┘
```

---

## Testing Strategy

### Per-API Testing Checklist

For each implemented API:

```
1. Unit Tests (MANDATORY)
   ├── [ ] Happy path test (normal usage)
   ├── [ ] Error case test #1 (invalid arguments)
   ├── [ ] Error case test #2 (file not found / permission denied)
   ├── [ ] Edge case test (empty file, large file, special chars)
   └── [ ] Cleanup verification (no temp files, no leaked fds)

2. Quality Gates (MANDATORY)
   ├── [ ] make format (code formatting)
   ├── [ ] make test (100% pass rate)
   ├── [ ] make wpt (≥90.6% baseline)
   └── [ ] ASAN check: ./target/debug/jsrt_m test/test_fs_*.js

3. Documentation
   ├── [ ] Inline comments for complex logic
   ├── [ ] JSDoc comment for API signature
   └── [ ] Update node-fs-plan.md progress
```

### Phase-Level Testing

After each phase completion:

```
1. Regression Testing
   ├── [ ] All existing tests still pass (113/113 baseline)
   ├── [ ] New tests added and passing
   └── [ ] No test timeouts or hangs

2. Memory Safety
   ├── [ ] ASAN clean (0 leaks, 0 use-after-free)
   ├── [ ] Valgrind clean (if available)
   └── [ ] File descriptor accounting (no leaks)

3. Cross-Platform
   ├── [ ] Linux smoke test (minimum)
   ├── [ ] macOS test (if available)
   └── [ ] Windows/WSL test (if available)

4. Performance
   ├── [ ] No significant regression vs baseline
   ├── [ ] Async operations are non-blocking
   └── [ ] Memory usage within expected bounds
```

### Integration Testing

After all phases complete:

```
1. End-to-End Scenarios
   ├── [ ] Full file lifecycle (create, read, write, delete)
   ├── [ ] Concurrent operations (multiple files, multiple ops)
   ├── [ ] Error recovery (handle failures gracefully)
   └── [ ] Resource cleanup (all fds closed, all memory freed)

2. Stress Testing
   ├── [ ] Large file operations (>1GB)
   ├── [ ] Many small file operations (1000+ files)
   ├── [ ] Concurrent async operations (100+ simultaneous)
   └── [ ] Error injection (simulate disk full, permission errors)

3. Compatibility Testing
   ├── [ ] Node.js fs compatibility smoke test
   ├── [ ] WPT File API tests (if applicable)
   └── [ ] Real-world usage patterns
```

---

## Risk Assessment & Mitigation

### Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Implementation bugs** | VERY LOW | LOW | • All patterns proven in 104 existing APIs<br>• Incremental testing after each API<br>• Code review before commit |
| **Memory leaks** | VERY LOW | HIGH | • ASAN testing mandatory for all new code<br>• Existing code is ASAN clean<br>• Clear cleanup patterns established |
| **Test failures** | VERY LOW | MEDIUM | • 113 tests currently passing<br>• Test after each API<br>• Minimal changes to existing code |
| **Platform issues** | LOW | MEDIUM | • Cross-platform patterns established<br>• Platform-specific tests<br>• Graceful degradation where needed |
| **Performance regression** | VERY LOW | LOW | • Async operations use proven libuv patterns<br>• Performance benchmarks<br>• No changes to hot paths |

### Schedule Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Underestimated effort** | LOW | MEDIUM | • Conservative estimates (2x buffer)<br>• Simple tasks prioritized first<br>• Can adjust scope if needed |
| **Testing bottleneck** | VERY LOW | LOW | • Automated test suite exists<br>• Fast feedback (~30s per run)<br>• Incremental testing approach |
| **Blocked dependencies** | NONE | N/A | • All tasks are independent<br>• No critical path blockers |
| **Scope creep** | VERY LOW | LOW | • Clear scope defined<br>• Phase D explicitly deferred<br>• No new features beyond Node.js API |

### Overall Risk Profile

```
╔═══════════════════════════════════════════════════════════════╗
║                                                                ║
║  Overall Risk Level: VERY LOW ✅                              ║
║  Confidence in Success: VERY HIGH ✅                          ║
║                                                                ║
║  Key Success Factors:                                          ║
║  ├── ✅ All patterns proven in existing 104 APIs              ║
║  ├── ✅ No blocking dependencies                              ║
║  ├── ✅ Conservative estimates with buffer                    ║
║  ├── ✅ Incremental testing and validation                    ║
║  ├── ✅ Clear acceptance criteria                             ║
║  └── ✅ Excellent baseline quality (100% tests, 0 leaks)      ║
║                                                                ║
╚═══════════════════════════════════════════════════════════════╝
```

---

## Success Metrics

### Quantitative Metrics

**API Coverage Targets:**
```
Current:  104/132 APIs (78.8%) ✅
Phase A:  112/132 APIs (84.8%) 🎯
Phase B:  120/132 APIs (90.9%) 🎯 RECOMMENDED
Phase C:  123/132 APIs (93.2%) 🎯 OPTIONAL
Phase D:  132/132 APIs (100%)  ❌ DEFERRED
```

**Quality Metrics (MUST MAINTAIN):**
```
Test Pass Rate:    100% (113/113 tests) ✅ MANDATORY
WPT Pass Rate:     ≥90.6% (29/32 tests) ✅ MANDATORY
Memory Leaks:      0 leaks (ASAN)       ✅ MANDATORY
Platform Support:  Linux (100%)         ✅ MANDATORY
                   macOS (90%+)         🎯 TARGET
                   Windows (85%+)       🎯 TARGET
```

**Performance Targets:**
```
Async Operations:  Non-blocking (libuv)      ✅
Memory Overhead:   <2MB for typical workload 🎯
Throughput:        Within 20% of Node.js    🎯
```

### Qualitative Metrics

**Code Quality:**
- [ ] All code follows jsrt style guide
- [ ] Files <500 lines (refactor if exceeded)
- [ ] Clear error messages matching Node.js format
- [ ] Comprehensive inline documentation
- [ ] No compiler warnings (-Wall -Wextra)

**User Experience:**
- [ ] API signatures match Node.js exactly
- [ ] Error messages are helpful and actionable
- [ ] Edge cases handled gracefully
- [ ] Cross-platform behavior documented

**Maintainability:**
- [ ] Code organization is logical and clear
- [ ] Reusable patterns extracted to common functions
- [ ] Git history is clean with meaningful commits
- [ ] Documentation is up-to-date

---

## Implementation Guidelines

### Development Workflow

```bash
# For each API task:

1. Review task specification
   ├── Read task details in this document
   ├── Review Node.js documentation
   └── Study existing similar implementations

2. Implement the API
   ├── Create/modify source file
   ├── Follow established patterns
   ├── Add inline comments
   └── Handle all error cases

3. Create tests
   ├── Write at least 3 test cases
   ├── Cover happy path + error cases
   └── Add to appropriate test file

4. Run quality gates (MANDATORY)
   ├── make format
   ├── make test (must show 100%)
   ├── make wpt (must maintain ≥90.6%)
   └── ./target/debug/jsrt_m test/test_fs_*.js (ASAN)

5. Review and commit
   ├── Self-review code changes
   ├── Update node-fs-plan.md
   ├── git add . && git commit -m "feat(fs): add [API name]"
   └── Mark task complete in this document
```

### Code Patterns to Follow

**Promise API Pattern:**
```c
// 1. Create promise work structure
fs_promise_work_t* work = malloc(sizeof(fs_promise_work_t));
work->ctx = ctx;
work->resolve = JS_DupValue(ctx, resolving_funcs[0]);
work->reject = JS_DupValue(ctx, resolving_funcs[1]);
work->path = strdup(path_cstr);

// 2. Start libuv operation
uv_fs_<operation>(fs_get_uv_loop(ctx), &work->req,
                  args..., fs_promise_complete_<type>);

// 3. Return promise immediately
return promise;
```

**Async Callback Pattern:**
```c
// 1. Create async work structure
fs_async_work_t* work = create_async_work(ctx, callback);
work->path = strdup(path_cstr);

// 2. Start libuv operation
uv_fs_<operation>(fs_get_uv_loop(ctx), &work->req,
                  args..., fs_async_complete_<type>);

// 3. Return undefined immediately
return JS_UNDEFINED;
```

**FileHandle Method Pattern:**
```c
// 1. Get FileHandle from this
FileHandle* fh = JS_GetOpaque2(ctx, this_val, filehandle_class_id);
if (!fh) return JS_EXCEPTION;

// 2. Check if closed
if (fh->closed) {
  return JS_ThrowTypeError(ctx, "FileHandle is closed");
}

// 3. Perform operation using fh->fd
// ... (use Promise pattern above)
```

### Memory Management Rules

```c
// MANDATORY: Every malloc must have corresponding free

// QuickJS values
JSValue val = JS_NewString(ctx, "text");
// ... use val
JS_FreeValue(ctx, val);  // MUST free

// C strings from JS
const char* str = JS_ToCString(ctx, argv[0]);
if (!str) return JS_EXCEPTION;
// ... use str
JS_FreeCString(ctx, str);  // MUST free

// Allocated memory
char* buf = malloc(size);
if (!buf) return JS_ThrowOutOfMemory(ctx);
// ... use buf
free(buf);  // MUST free

// Work structures
fs_promise_work_t* work = malloc(sizeof(*work));
// ... use work
fs_promise_work_free(work);  // MUST free (handles all internal frees)
```

### Error Handling Pattern

```c
// Always use create_fs_error for consistency
if (req->result < 0) {
  int err = -req->result;
  JSValue error = create_fs_error(ctx, err, "operation", path);
  JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
  JS_FreeValue(ctx, error);
  JS_FreeValue(ctx, ret);
  fs_promise_work_free(work);
  return;
}
```

---

## Recommended Next Steps

### Immediate Actions (Today)

1. **Review this task breakdown**
   - [ ] Read through all Phase A tasks
   - [ ] Understand acceptance criteria
   - [ ] Review code patterns

2. **Set up tracking**
   - [ ] Update node-fs-plan.md with Phase A tasks
   - [ ] Create TodoWrite list if desired
   - [ ] Set target completion date

3. **Begin Phase A implementation**
   - [ ] Start with Task A1.1 (FileHandle.appendFile)
   - [ ] Follow development workflow
   - [ ] Test after each API

### Week 1 Goals (Phase A)

```
Day 1-2: FileHandle Essentials (Session A1)
├── Implement A1.1-A1.4 (4 APIs)
├── Test each API individually
└── Commit after each completion

Day 3-4: Async Callbacks Basic (Session A2)
├── Implement A2.1-A2.4 (4 APIs)
├── Test each API individually
└── Commit after each completion

Day 5: Testing & Integration
├── Full regression test suite
├── ASAN verification
├── Update documentation
└── Final Phase A commit

Result: 104 → 112 APIs (84.8% complete) ✅
```

### Week 2 Goals (Phase B - Optional)

```
Day 6-7: Promise Wrappers + Async Advanced (Sessions B1+B2)
├── Implement B1.1-B1.4 (4 APIs)
├── Implement B2.1-B2.2 (2 APIs)
└── Test and commit

Day 8-9: Vectored I/O (Session B3)
├── Implement B3.1-B3.2 (2 APIs)
├── Test with multiple buffer scenarios
└── Performance verification

Day 10: Testing & Integration
├── Full regression test suite
├── ASAN verification
├── Performance benchmarks
└── Final Phase B commit

Result: 112 → 120 APIs (90.9% complete) ✅ PRODUCTION READY
```

---

## Conclusion

### Current Achievement: EXCELLENT ✅

```
╔═══════════════════════════════════════════════════════════════╗
║                                                                ║
║  🎉 78.8% Complete (104/132 APIs)                             ║
║                                                                ║
║  ✅ Phase 0-3 Complete (Foundation, Sync, Async, Promises)    ║
║  ✅ 113/113 Tests Passing (100%)                              ║
║  ✅ 0 Memory Leaks (ASAN Verified)                            ║
║  ✅ 90.6% WPT Pass Rate                                       ║
║  ✅ Production-Ready Quality                                  ║
║                                                                ║
╚═══════════════════════════════════════════════════════════════╝
```

### Recommended Path: Phase A + B (90.9% Target)

```
╔═══════════════════════════════════════════════════════════════╗
║                                                                ║
║  ✅ RECOMMENDED: Implement Phase A + B                        ║
║                                                                ║
║  Effort:    14-15 hours (1-2 weeks)                           ║
║  Risk:      VERY LOW                                          ║
║  Value:     HIGH                                              ║
║  Result:    120/132 APIs (90.9% complete)                     ║
║                                                                ║
║  Why this is optimal:                                          ║
║  ├── ✅ Covers all commonly-used APIs (90%+ real usage)       ║
║  ├── ✅ Low risk (all patterns proven)                        ║
║  ├── ✅ Reasonable effort (2 weeks part-time)                 ║
║  ├── ✅ Production-ready at completion                        ║
║  ├── ✅ Maximum parallelization (no blockers)                 ║
║  └── ✅ Immediate value for users                             ║
║                                                                ║
╚═══════════════════════════════════════════════════════════════╝
```

### Key Success Factors

1. **Proven Patterns** - All implementation patterns established in existing 104 APIs
2. **No Dependencies** - All remaining tasks are independent (maximum parallelism)
3. **Low Risk** - Very high confidence in estimates and success
4. **High Value** - Phase A+B covers 90%+ of real-world usage
5. **Quality First** - Maintain 100% test pass rate, 0 leaks throughout

### What Makes This Achievable

- ✅ Strong foundation (104 APIs implemented, tested, production-ready)
- ✅ Clear patterns (Promise, async callback, FileHandle all proven)
- ✅ Excellent test coverage (113 tests, ASAN clean)
- ✅ Independent tasks (can parallelize within sessions)
- ✅ Conservative estimates (2x buffer built in)

---

**Document Version:** 1.0
**Created:** 2025-10-05T23:30:00Z
**Status:** Ready for implementation
**Recommended Start:** Phase A, Task A1.1 (FileHandle.appendFile)
**Target Completion:** Phase A+B in 1-2 weeks (90.9% coverage)

---

*This task breakdown provides atomic, executable specifications for all remaining work. Each task includes clear acceptance criteria, test plans, effort estimates, and risk assessment. All tasks are independent with no blocking dependencies, enabling maximum parallelization.*
