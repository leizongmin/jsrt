'use strict';

const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { WASI } = require('node:wasi');

const WASI_ERRNO_SUCCESS = 0;
const WASI_ERRNO_ENOSYS = 52;

const WASI_CLOCK_REALTIME = 0;
const WASI_CLOCK_MONOTONIC = 1;
const WASI_CLOCK_PROCESS_CPUTIME = 2;

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

function zeroMemory(view, offset, length) {
  const bytes = new Uint8Array(view.buffer, offset, length);
  bytes.fill(0);
}

function readUint64(view, offset) {
  const low = view.getUint32(offset, true);
  const high = view.getUint32(offset + 4, true);
  return (BigInt(high) << 32n) | BigInt(low);
}

console.log('========================================');
console.log('WASI Clock & Random Validation');
console.log('========================================');

const harness = instantiateWasi({ returnOnExit: true });
const wasiNS = harness.importObject.wasi_snapshot_preview1;
const view = harness.view;

const timePtr = 0x100;
const resolutionPtr = 0x200;
const randomPtr = 0x300;

console.log('Test 1: clock_time_get returns monotonic timestamps');
zeroMemory(view, timePtr, 8);
let result = wasiNS.clock_time_get(WASI_CLOCK_REALTIME, 0, timePtr);
assert.strictEqual(result, WASI_ERRNO_SUCCESS);
const realtime1 = readUint64(view, timePtr);
assert.ok(realtime1 > 0n, 'Realtime clock should be positive');

zeroMemory(view, timePtr, 8);
result = wasiNS.clock_time_get(WASI_CLOCK_REALTIME, 0, timePtr);
assert.strictEqual(result, WASI_ERRNO_SUCCESS);
const realtime2 = readUint64(view, timePtr);
assert.ok(
  realtime2 >= realtime1,
  'Second realtime timestamp should not go backwards'
);

zeroMemory(view, timePtr, 8);
result = wasiNS.clock_time_get(WASI_CLOCK_MONOTONIC, 0, timePtr);
assert.strictEqual(result, WASI_ERRNO_SUCCESS);
const monotonic1 = readUint64(view, timePtr);
assert.ok(monotonic1 > 0n, 'Monotonic clock should be positive');

zeroMemory(view, timePtr, 8);
result = wasiNS.clock_time_get(WASI_CLOCK_MONOTONIC, 0, timePtr);
assert.strictEqual(result, WASI_ERRNO_SUCCESS);
const monotonic2 = readUint64(view, timePtr);
assert.ok(monotonic2 >= monotonic1, 'Monotonic clock should not go backwards');

console.log('Test 2: clock_res_get reports expected resolutions');
zeroMemory(view, resolutionPtr, 8);
result = wasiNS.clock_res_get(WASI_CLOCK_REALTIME, resolutionPtr);
assert.strictEqual(result, WASI_ERRNO_SUCCESS);
const realtimeResolution = readUint64(view, resolutionPtr);
assert.strictEqual(
  realtimeResolution,
  1000n,
  'Realtime clock resolution should be 1 microsecond (1000 ns)'
);

zeroMemory(view, resolutionPtr, 8);
result = wasiNS.clock_res_get(WASI_CLOCK_MONOTONIC, resolutionPtr);
assert.strictEqual(result, WASI_ERRNO_SUCCESS);
const monotonicResolution = readUint64(view, resolutionPtr);
assert.strictEqual(
  monotonicResolution,
  1000n,
  'Monotonic clock resolution should be 1 microsecond (1000 ns)'
);

zeroMemory(view, resolutionPtr, 8);
result = wasiNS.clock_res_get(WASI_CLOCK_PROCESS_CPUTIME, resolutionPtr);
assert.strictEqual(
  result,
  WASI_ERRNO_ENOSYS,
  'Process CPU time resolution should report ENOSYS'
);

console.log('Test 3: random_get fills buffers with entropy');
const sentinel = 0xff;
const randomLength = 64;
const randomBuffer = new Uint8Array(view.buffer, randomPtr, randomLength);
randomBuffer.fill(sentinel);
result = wasiNS.random_get(randomPtr, randomLength);
assert.strictEqual(result, WASI_ERRNO_SUCCESS);
const hasEntropy = randomBuffer.some((value) => value !== sentinel);
assert.ok(hasEntropy, 'random_get should populate buffer with random data');

console.log('All WASI clock and random tests passed.');
