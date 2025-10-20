'use strict';

const assert = require('node:assert');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { TextDecoder, TextEncoder } = require('node:util');
const { WASI } = require('node:wasi');

const encoder = new TextEncoder();
const decoder = new TextDecoder('utf8');

const WASI_ERRNO_SUCCESS = 0;
const WASI_ERRNO_ENOTCAPABLE = 76;

const WASI_O_CREAT = 0x0001;
const WASI_O_TRUNC = 0x0008;

const RIGHTS = {
  FD_DATASYNC: 2 ** 0,
  FD_READ: 2 ** 1,
  FD_SEEK: 2 ** 2,
  FD_FDSTAT_SET_FLAGS: 2 ** 3,
  FD_SYNC: 2 ** 4,
  FD_TELL: 2 ** 5,
  FD_WRITE: 2 ** 6,
  FD_FILESTAT_GET: 2 ** 21,
  FD_FILESTAT_SET_SIZE: 2 ** 22,
};

const FILE_RIGHTS =
  RIGHTS.FD_DATASYNC |
  RIGHTS.FD_READ |
  RIGHTS.FD_SEEK |
  RIGHTS.FD_FDSTAT_SET_FLAGS |
  RIGHTS.FD_SYNC |
  RIGHTS.FD_TELL |
  RIGHTS.FD_WRITE |
  RIGHTS.FD_FILESTAT_GET |
  RIGHTS.FD_FILESTAT_SET_SIZE;

function loadWasm(name) {
  const filePath = path.join(__dirname, 'wasm', name);
  return fs.readFileSync(filePath);
}

function instantiateWasi(options) {
  const wasi = new WASI(options);
  const importObject = wasi.getImportObject();
  const module = new WebAssembly.Module(loadWasm('hello_start.wasm'));
  const instance = new WebAssembly.Instance(module, importObject);
  const exitCode = wasi.start(instance);
  assert.strictEqual(exitCode, 0, 'hello_start.wasm should exit cleanly');
  const memory = instance.exports.memory;
  return {
    wasi,
    importObject,
    instance,
    memory,
    view: new DataView(memory.buffer),
  };
}

function writeBytes(view, ptr, bytes) {
  for (let i = 0; i < bytes.length; i += 1) {
    view.setUint8(ptr + i, bytes[i]);
  }
}

function writeString(view, ptr, value) {
  writeBytes(view, ptr, encoder.encode(value));
}

function zeroMemory(view, ptr, length) {
  for (let i = 0; i < length; i += 1) {
    view.setUint8(ptr + i, 0);
  }
}

function readUint64(view, ptr) {
  const low = view.getUint32(ptr, true);
  const high = view.getUint32(ptr + 4, true);
  return (BigInt(high) << 32n) | BigInt(low);
}

function arraysEqual(a, b, length) {
  for (let i = 0; i < length; i += 1) {
    if (a[i] !== b[i]) {
      return false;
    }
  }
  return true;
}

function runFileIOTests() {
  console.log('========================================');
  console.log('WASI File I/O Tests');
  console.log('========================================');

  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'jsrt-wasi-fs-'));
  try {
    const harness = instantiateWasi({
      preopens: { '/sandbox': tempDir },
      returnOnExit: true,
    });

    const wasiNS = harness.importObject.wasi_snapshot_preview1;
    const memory = harness.memory;
    const view = harness.view;

    // Offsets within WASM memory used across tests
    const pathPtr = 0x400;
    const openedFdPtr = 0x500;
    const iovPtr = 0x600;
    const dataPtr = 0x800;
    const readPtr = 0x1000;
    const countPtr = 0x1200;
    const largeChunkPtr = 0x2000;
    const largeReadPtr = 0x4000;
    const filestatPtr = 0x6000;

    const PREOPEN_FD = 3;

    console.log('Test 1: Create, write, and read file within sandbox');
    zeroMemory(view, pathPtr, 256);
    zeroMemory(view, openedFdPtr, 8);
    zeroMemory(view, iovPtr, 16);
    zeroMemory(view, dataPtr, 512);
    zeroMemory(view, readPtr, 512);
    zeroMemory(view, countPtr, 8);

    const sandboxFile = 'output.txt';
    writeString(view, pathPtr, sandboxFile);

    const createResult = wasiNS.path_open(
      PREOPEN_FD,
      0,
      pathPtr,
      sandboxFile.length,
      WASI_O_CREAT | WASI_O_TRUNC,
      FILE_RIGHTS,
      FILE_RIGHTS,
      0,
      openedFdPtr
    );
    assert.strictEqual(
      createResult,
      WASI_ERRNO_SUCCESS,
      'path_open should succeed within sandbox'
    );

    const fileFd = view.getUint32(openedFdPtr, true);
    assert.ok(fileFd >= 4, 'New file descriptor should be allocated');

    const message = 'WASI file I/O ✅';
    const payload = encoder.encode(message);
    writeBytes(view, dataPtr, payload);

    view.setUint32(iovPtr, dataPtr, true);
    view.setUint32(iovPtr + 4, payload.length, true);

    const nwrittenPtr = countPtr;
    zeroMemory(view, nwrittenPtr, 4);
    const writeResult = wasiNS.fd_write(fileFd, iovPtr, 1, nwrittenPtr);
    assert.strictEqual(
      writeResult,
      WASI_ERRNO_SUCCESS,
      'fd_write should succeed for sandbox file'
    );

    const bytesWritten = view.getUint32(nwrittenPtr, true);
    assert.strictEqual(
      bytesWritten,
      payload.length,
      'fd_write should report all bytes written'
    );

    assert.strictEqual(
      wasiNS.fd_close(fileFd),
      WASI_ERRNO_SUCCESS,
      'fd_close should succeed for sandbox file'
    );

    zeroMemory(view, openedFdPtr, 8);
    const reopenResult = wasiNS.path_open(
      PREOPEN_FD,
      0,
      pathPtr,
      sandboxFile.length,
      0,
      RIGHTS.FD_READ,
      RIGHTS.FD_READ,
      0,
      openedFdPtr
    );
    assert.strictEqual(
      reopenResult,
      WASI_ERRNO_SUCCESS,
      'path_open should reopen file for reading'
    );

    const readFd = view.getUint32(openedFdPtr, true);
    view.setUint32(iovPtr, readPtr, true);
    view.setUint32(iovPtr + 4, payload.length, true);

    const nreadPtr = countPtr + 4;
    zeroMemory(view, nreadPtr, 4);
    const readResult = wasiNS.fd_read(readFd, iovPtr, 1, nreadPtr);
    assert.strictEqual(
      readResult,
      WASI_ERRNO_SUCCESS,
      'fd_read should succeed for sandbox file'
    );

    const bytesRead = view.getUint32(nreadPtr, true);
    assert.strictEqual(
      bytesRead,
      payload.length,
      'fd_read should read all bytes written'
    );

    const roundTrip = decoder.decode(
      new Uint8Array(memory.buffer, readPtr, bytesRead)
    );
    assert.strictEqual(
      roundTrip,
      message,
      'Round-trip read should match original message'
    );

    assert.strictEqual(
      wasiNS.fd_close(readFd),
      WASI_ERRNO_SUCCESS,
      'fd_close should succeed for reopened file'
    );

    const hostContents = fs.readFileSync(
      path.join(tempDir, sandboxFile),
      'utf8'
    );
    assert.strictEqual(
      hostContents,
      message,
      'Host filesystem should contain written content'
    );

    console.log('Test 1 passed.');

    console.log('Test 2: Sandbox prevents escaping preopen directory');
    zeroMemory(view, pathPtr, 256);
    writeString(view, pathPtr, '../escape.txt');
    const escapeResult = wasiNS.path_open(
      PREOPEN_FD,
      0,
      pathPtr,
      '../escape.txt'.length,
      WASI_O_CREAT,
      FILE_RIGHTS,
      FILE_RIGHTS,
      0,
      openedFdPtr
    );
    assert.strictEqual(
      escapeResult,
      WASI_ERRNO_ENOTCAPABLE,
      'path_open should reject attempts to escape sandbox directory'
    );
    assert.ok(
      !fs.existsSync(path.join(tempDir, '..', 'escape.txt')),
      'No file should be created outside sandbox'
    );

    zeroMemory(view, pathPtr, 256);
    writeString(view, pathPtr, '/absolute.txt');
    const absoluteResult = wasiNS.path_open(
      PREOPEN_FD,
      0,
      pathPtr,
      '/absolute.txt'.length,
      WASI_O_CREAT,
      FILE_RIGHTS,
      FILE_RIGHTS,
      0,
      openedFdPtr
    );
    assert.strictEqual(
      absoluteResult,
      WASI_ERRNO_ENOTCAPABLE,
      'path_open should reject absolute paths outside sandbox'
    );
    assert.ok(
      !fs.existsSync(path.join(tempDir, 'absolute.txt')),
      'Absolute path should not be resolved'
    );

    console.log('Test 2 passed.');

    console.log('Test 3: Large file read/write handles multi-MB payloads');
    const largeFile = 'large.bin';
    const chunkSize = 4096;
    const chunkCount = 512; // 2 MB total
    const totalBytes = chunkSize * chunkCount;

    zeroMemory(view, pathPtr, 256);
    writeString(view, pathPtr, largeFile);

    zeroMemory(view, openedFdPtr, 8);
    const largeCreateResult = wasiNS.path_open(
      PREOPEN_FD,
      0,
      pathPtr,
      largeFile.length,
      WASI_O_CREAT | WASI_O_TRUNC,
      FILE_RIGHTS,
      FILE_RIGHTS,
      0,
      openedFdPtr
    );
    assert.strictEqual(
      largeCreateResult,
      WASI_ERRNO_SUCCESS,
      'path_open should create large file within sandbox'
    );

    const largeFd = view.getUint32(openedFdPtr, true);
    assert.ok(largeFd >= 4, 'Large file descriptor should be allocated');

    const chunkData = new Uint8Array(chunkSize);
    for (let i = 0; i < chunkSize; i += 1) {
      chunkData[i] = i & 0xff;
    }
    zeroMemory(view, largeChunkPtr, chunkSize);
    writeBytes(view, largeChunkPtr, chunkData);

    view.setUint32(iovPtr, largeChunkPtr, true);
    view.setUint32(iovPtr + 4, chunkSize, true);

    const largeCountPtr = countPtr;
    zeroMemory(view, largeCountPtr, 4);

    let writtenTotal = 0;
    for (let i = 0; i < chunkCount; i += 1) {
      zeroMemory(view, largeCountPtr, 4);
      const writeResult = wasiNS.fd_write(largeFd, iovPtr, 1, largeCountPtr);
      assert.strictEqual(
        writeResult,
        WASI_ERRNO_SUCCESS,
        'fd_write should succeed for large file chunk'
      );
      const bytesWritten = view.getUint32(largeCountPtr, true);
      assert.strictEqual(
        bytesWritten,
        chunkSize,
        'Each fd_write should report full chunk written'
      );
      writtenTotal += bytesWritten;
    }
    assert.strictEqual(
      writtenTotal,
      totalBytes,
      'Total written bytes should match expected size'
    );

    assert.strictEqual(
      wasiNS.fd_close(largeFd),
      WASI_ERRNO_SUCCESS,
      'fd_close should succeed for large file'
    );

    zeroMemory(view, openedFdPtr, 8);
    const largeReopenResult = wasiNS.path_open(
      PREOPEN_FD,
      0,
      pathPtr,
      largeFile.length,
      0,
      RIGHTS.FD_READ | RIGHTS.FD_SEEK,
      RIGHTS.FD_READ | RIGHTS.FD_SEEK,
      0,
      openedFdPtr
    );
    assert.strictEqual(
      largeReopenResult,
      WASI_ERRNO_SUCCESS,
      'path_open should reopen large file for reading'
    );

    const largeReadFd = view.getUint32(openedFdPtr, true);
    view.setUint32(iovPtr, largeReadPtr, true);
    view.setUint32(iovPtr + 4, chunkSize, true);

    zeroMemory(view, largeReadPtr, chunkSize);
    zeroMemory(view, countPtr + 4, 4);

    let readTotal = 0;
    while (true) {
      const nreadPtr = countPtr + 4;
      zeroMemory(view, nreadPtr, 4);
      const readResult = wasiNS.fd_read(largeReadFd, iovPtr, 1, nreadPtr);
      assert.strictEqual(
        readResult,
        WASI_ERRNO_SUCCESS,
        'fd_read should succeed for large file chunk'
      );
      const bytesRead = view.getUint32(nreadPtr, true);
      if (bytesRead === 0) {
        break;
      }

      const chunkView = new Uint8Array(memory.buffer, largeReadPtr, bytesRead);
      assert.ok(
        arraysEqual(chunkData, chunkView, bytesRead),
        'Read bytes should match expected pattern'
      );

      readTotal += bytesRead;
    }

    assert.strictEqual(
      readTotal,
      totalBytes,
      'Total read bytes should match expected size'
    );

    assert.strictEqual(
      wasiNS.fd_close(largeReadFd),
      WASI_ERRNO_SUCCESS,
      'fd_close should succeed for large file read handle'
    );

    const hostLargeFile = path.join(tempDir, largeFile);
    const hostStats = fs.statSync(hostLargeFile);
    assert.strictEqual(
      hostStats.size,
      totalBytes,
      'Host filesystem should report full large file size'
    );

    zeroMemory(view, filestatPtr, 72);
    const filestatResult = wasiNS.path_filestat_get(
      PREOPEN_FD,
      0,
      pathPtr,
      largeFile.length,
      filestatPtr
    );
    assert.strictEqual(
      filestatResult,
      WASI_ERRNO_SUCCESS,
      'path_filestat_get should succeed for large file'
    );
    const wasmReportedSize = readUint64(view, filestatPtr + 32);
    assert.strictEqual(
      wasmReportedSize,
      BigInt(totalBytes),
      'WASI filestat size should match host size'
    );

    console.log('Test 3 passed.');
    console.log('All WASI file system tests passed.');
  } finally {
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
}
runFileIOTests();
