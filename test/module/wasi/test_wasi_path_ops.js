'use strict';

const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

const { WASI } = require('node:wasi');

console.log('========================================');
console.log('WASI Path Operations Tests');
console.log('========================================');
console.log('Modules loaded');

const tmpRoot = path.join(process.cwd(), 'target', 'tmp');
const sandboxDir = path.join(tmpRoot, 'wasi-path-test');
fs.mkdirSync(tmpRoot, { recursive: true });

if (fs.existsSync(sandboxDir)) {
  fs.rmSync(sandboxDir, { recursive: true, force: true });
}
fs.mkdirSync(sandboxDir, { recursive: true });
console.log(`Sandbox prepared at ${sandboxDir}`);

const wasi = new WASI({
  args: [],
  env: [],
  preopens: {
    '/sandbox': sandboxDir,
  },
});
console.log('WASI instance created');

// Minimal WASI module:
// (module
//   (import "wasi_snapshot_preview1" "fd_write" (func (param i32 i32 i32 i32) (result i32)))
//   (memory (export "memory") 1)
//   (func (export "_initialize"))
// )
const wasmBytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00,
  0x00, 0x03, 0x02, 0x01, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x18, 0x02,
  0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x0b, 0x5f, 0x69, 0x6e,
  0x69, 0x74, 0x69, 0x61, 0x6c, 0x69, 0x7a, 0x65, 0x00, 0x00, 0x0a, 0x04, 0x01,
  0x02, 0x00, 0x0b,
]);

const importObject = wasi.getImportObject();
const wasiNamespace =
  importObject.wasi_snapshot_preview1 || importObject.wasi_unstable;
const module = new WebAssembly.Module(wasmBytes);
const instance = new WebAssembly.Instance(module, importObject);
wasi.initialize(instance);
console.log('WASM module instantiated and WASI initialized');

const memory = instance.exports.memory;
const mem = new Uint8Array(memory.buffer);
const view = new DataView(memory.buffer);

function writeString(offset, value) {
  for (let i = 0; i < value.length; i++) {
    mem[offset + i] = value.charCodeAt(i);
  }
}

function readUint32(offset) {
  return view.getUint32(offset, true);
}

function readUint64(offset) {
  return Number(view.getBigUint64(offset, true));
}

const PREOPEN_FD = 3;
const OFLAGS = {
  O_CREAT: 0x0001,
  O_TRUNC: 0x0008,
};
const RIGHTS_BASE = (1 << 1) | (1 << 2) | (1 << 5) | (1 << 6); // read, seek, tell, write
const FILETYPE_REGULAR = 4;
const ERRNO = {
  ESUCCESS: 0,
  ENOTCAPABLE: 76,
  ENOSYS: 52,
};

const PATH_PTR = 1024;
const SECOND_PATH_PTR = 1200;
const FD_OUT_PTR = 1400;
const STAT_PTR = 1600;
const POLL_EVENTS_PTR = 2000;

console.log('Test 1: path_open creates file and returns descriptor');
writeString(PATH_PTR, 'test.txt');
let result = wasiNamespace.path_open(
  PREOPEN_FD,
  0,
  PATH_PTR,
  'test.txt'.length,
  OFLAGS.O_CREAT | OFLAGS.O_TRUNC,
  RIGHTS_BASE,
  RIGHTS_BASE,
  0,
  FD_OUT_PTR
);
assert.strictEqual(result, ERRNO.ESUCCESS, 'path_open should succeed');
const createdFd = readUint32(FD_OUT_PTR);
assert.ok(createdFd >= 4, `expected fd >= 4, got ${createdFd}`);
result = wasiNamespace.fd_close(createdFd);
assert.strictEqual(result, ERRNO.ESUCCESS, 'fd_close should succeed');
assert.ok(
  fs.existsSync(path.join(sandboxDir, 'test.txt')),
  'test.txt should exist on host'
);
console.log('PASS: path_open created file and returned fd', createdFd);

console.log('Test 2: path_filestat_get reports regular file');
result = wasiNamespace.path_filestat_get(
  PREOPEN_FD,
  0,
  PATH_PTR,
  'test.txt'.length,
  STAT_PTR
);
assert.strictEqual(result, ERRNO.ESUCCESS, 'path_filestat_get should succeed');
const fileType = mem[STAT_PTR + 16];
assert.strictEqual(
  fileType,
  FILETYPE_REGULAR,
  `expected regular file type, got ${fileType}`
);
console.log('PASS: path_filestat_get reports regular file');

console.log('Test 3: path_unlink_file removes file');
result = wasiNamespace.path_unlink_file(
  PREOPEN_FD,
  PATH_PTR,
  'test.txt'.length
);
assert.strictEqual(result, ERRNO.ESUCCESS, 'path_unlink_file should succeed');
assert.ok(
  !fs.existsSync(path.join(sandboxDir, 'test.txt')),
  'test.txt should be removed'
);
console.log('PASS: path_unlink_file removed file');

console.log('Test 4: path_create_directory and path_remove_directory');
writeString(PATH_PTR, 'subdir');
result = wasiNamespace.path_create_directory(
  PREOPEN_FD,
  PATH_PTR,
  'subdir'.length
);
assert.strictEqual(
  result,
  ERRNO.ESUCCESS,
  'path_create_directory should succeed'
);
assert.ok(
  fs.statSync(path.join(sandboxDir, 'subdir')).isDirectory(),
  'subdir should exist'
);

result = wasiNamespace.path_remove_directory(
  PREOPEN_FD,
  PATH_PTR,
  'subdir'.length
);
assert.strictEqual(
  result,
  ERRNO.ESUCCESS,
  'path_remove_directory should succeed'
);
assert.ok(
  !fs.existsSync(path.join(sandboxDir, 'subdir')),
  'subdir should be removed'
);
console.log('PASS: directory create/remove via WASI');

console.log('Test 5: path_rename renames file');
fs.writeFileSync(path.join(sandboxDir, 'old.txt'), 'hello');
writeString(PATH_PTR, 'old.txt');
writeString(SECOND_PATH_PTR, 'renamed.txt');
result = wasiNamespace.path_rename(
  PREOPEN_FD,
  PATH_PTR,
  'old.txt'.length,
  PREOPEN_FD,
  SECOND_PATH_PTR,
  'renamed.txt'.length
);
assert.strictEqual(result, ERRNO.ESUCCESS, 'path_rename should succeed');
assert.ok(
  !fs.existsSync(path.join(sandboxDir, 'old.txt')),
  'old.txt should be gone'
);
assert.ok(
  fs.existsSync(path.join(sandboxDir, 'renamed.txt')),
  'renamed.txt should exist'
);
fs.unlinkSync(path.join(sandboxDir, 'renamed.txt'));
console.log('PASS: path_rename renamed file');

console.log('Test 6: path_open rejects directory escape');
writeString(PATH_PTR, '../escape');
result = wasiNamespace.path_open(
  PREOPEN_FD,
  0,
  PATH_PTR,
  '../escape'.length,
  OFLAGS.O_CREAT,
  RIGHTS_BASE,
  RIGHTS_BASE,
  0,
  FD_OUT_PTR
);
assert.strictEqual(
  result,
  ERRNO.ENOTCAPABLE,
  'path_open with ../ should be rejected'
);
console.log('PASS: path_open prevents directory escape');

console.log('Test 7: poll_oneoff and socket functions return ENOSYS');
view.setUint32(POLL_EVENTS_PTR, 0, true);
result = wasiNamespace.poll_oneoff(0, 0, 0, POLL_EVENTS_PTR);
assert.strictEqual(result, ERRNO.ENOSYS, 'poll_oneoff should be stubbed');
result = wasiNamespace.sock_accept(PREOPEN_FD, 0, FD_OUT_PTR);
assert.strictEqual(result, ERRNO.ENOSYS, 'sock_accept should be stubbed');
result = wasiNamespace.sock_recv(
  PREOPEN_FD,
  0,
  0,
  0,
  POLL_EVENTS_PTR,
  POLL_EVENTS_PTR + 8
);
assert.strictEqual(result, ERRNO.ENOSYS, 'sock_recv should be stubbed');
result = wasiNamespace.sock_send(PREOPEN_FD, 0, 0, 0, POLL_EVENTS_PTR);
assert.strictEqual(result, ERRNO.ENOSYS, 'sock_send should be stubbed');
result = wasiNamespace.sock_shutdown(PREOPEN_FD, 0);
assert.strictEqual(result, ERRNO.ENOSYS, 'sock_shutdown should be stubbed');
console.log('PASS: poll_oneoff and socket functions return ENOSYS');

console.log('All WASI path operation tests passed.');

// Cleanup
fs.rmSync(sandboxDir, { recursive: true, force: true });
