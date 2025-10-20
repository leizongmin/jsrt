'use strict';

const assert = require('node:assert');
const { WASI } = require('node:wasi');

console.log('========================================');
console.log('WASI start() Validation Tests');
console.log('========================================');

// WASM module exporting memory and a no-op _start function
const wasmStartModule = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00,
  0x00, 0x03, 0x02, 0x01, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x13, 0x02,
  0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x06, 0x5f, 0x73, 0x74,
  0x61, 0x72, 0x74, 0x00, 0x00, 0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
]);

// WASM module exporting memory but no _start function
const wasmMemoryOnlyModule = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01,
  0x07, 0x0a, 0x01, 0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00,
]);

// WASM module exporting _start but no memory
const wasmStartNoMemoryModule = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00,
  0x00, 0x03, 0x02, 0x01, 0x00, 0x07, 0x0a, 0x01, 0x06, 0x5f, 0x73, 0x74, 0x61,
  0x72, 0x74, 0x00, 0x00, 0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
]);

// WASM module exporting both _start and _initialize
const wasmStartWithInitializeModule = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00,
  0x00, 0x03, 0x02, 0x01, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x21, 0x03,
  0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x06, 0x5f, 0x73, 0x74,
  0x61, 0x72, 0x74, 0x00, 0x00, 0x0b, 0x5f, 0x69, 0x6e, 0x69, 0x74, 0x69, 0x61,
  0x6c, 0x69, 0x7a, 0x65, 0x00, 0x00, 0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
]);

function instantiate(wasi, bytes) {
  const module = new WebAssembly.Module(bytes);
  return new WebAssembly.Instance(module, wasi.getImportObject());
}

function expectNoThrow(fn, message) {
  let error;
  try {
    fn();
  } catch (err) {
    error = err;
  }
  assert.strictEqual(
    error,
    undefined,
    message || 'Expected function not to throw'
  );
}

function expectThrowMatching(fn, pattern, message) {
  let error;
  try {
    fn();
  } catch (err) {
    error = err;
  }
  assert.ok(error, message || 'Expected function to throw');
  if (pattern) {
    const match = pattern.test(String(error));
    assert.ok(match, message || 'Error did not match pattern');
  }
}

console.log('Test 1: start() rejects modules without _start export');
const wasiNoStart = new WASI({});
const noStartInstance = instantiate(wasiNoStart, wasmMemoryOnlyModule);
expectThrowMatching(
  () => wasiNoStart.start(noStartInstance),
  /Missing required WASI entry export/,
  'start() should reject when _start export is missing'
);
console.log('PASS: Missing _start export triggers validation error');

console.log('Test 2: start() rejects modules without memory export');
const wasiNoMemory = new WASI({});
const noMemoryInstance = instantiate(wasiNoMemory, wasmStartNoMemoryModule);
expectThrowMatching(
  () => wasiNoMemory.start(noMemoryInstance),
  /Missing WebAssembly memory export/,
  'start() should reject when memory export is missing'
);
console.log('PASS: Missing memory export triggers validation error');

console.log('Test 3: start() rejects modules exporting _initialize');
const wasiDualExports = new WASI({});
const dualInstance = instantiate(
  wasiDualExports,
  wasmStartWithInitializeModule
);
expectThrowMatching(
  () => wasiDualExports.start(dualInstance),
  /_initialize export is incompatible/,
  'start() should reject modules exporting _initialize'
);
console.log('PASS: start() enforces incompatibility with _initialize export');

console.log('All start() validation tests passed.');
