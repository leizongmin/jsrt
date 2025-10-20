'use strict';

const assert = require('node:assert');
const { WASI } = require('node:wasi');

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
    assert.match(
      String(error),
      pattern,
      message || 'Error did not match pattern'
    );
  }
}

console.log('========================================');
console.log('WASI Lifecycle Tests');
console.log('========================================');

// Minimal WASM module exporting memory and a no-op _start function
const wasmStartModule = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00,
  0x00, 0x03, 0x02, 0x01, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x13, 0x02,
  0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x06, 0x5f, 0x73, 0x74,
  0x61, 0x72, 0x74, 0x00, 0x00, 0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
]);

// WASM module exporting memory and a no-op _initialize function
const wasmInitializeModule = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00,
  0x00, 0x03, 0x02, 0x01, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x18, 0x02,
  0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x0b, 0x5f, 0x69, 0x6e,
  0x69, 0x74, 0x69, 0x61, 0x6c, 0x69, 0x7a, 0x65, 0x00, 0x00, 0x0a, 0x04, 0x01,
  0x02, 0x00, 0x0b,
]);

// WASM module exporting both _start and _initialize (invalid for start())
const wasmStartWithInitializeModule = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00,
  0x00, 0x03, 0x02, 0x01, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x21, 0x03,
  0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x06, 0x5f, 0x73, 0x74,
  0x61, 0x72, 0x74, 0x00, 0x00, 0x0b, 0x5f, 0x69, 0x6e, 0x69, 0x74, 0x69, 0x61,
  0x6c, 0x69, 0x7a, 0x65, 0x00, 0x00, 0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
]);

// WASM module exporting only memory (no lifecycle functions)
const wasmMemoryOnlyModule = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01,
  0x07, 0x0a, 0x01, 0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00,
]);

function createInstance(wasi, bytes) {
  const module = new WebAssembly.Module(bytes);
  const instance = new WebAssembly.Instance(module, wasi.getImportObject());
  return { module, instance };
}

console.log('Test 1: start() succeeds exactly once');
const wasiStart = new WASI({});
const startMain = createInstance(wasiStart, wasmStartModule);
expectNoThrow(() => wasiStart.start(startMain.instance));
expectThrowMatching(
  () => wasiStart.start(startMain.instance),
  /already started/
);
console.log('PASS: start() enforces single invocation');

console.log('Test 2: start() rejects modules without _start and blocks reuse');
const wasiNoStart = new WASI({});
const noStartInstance = createInstance(wasiNoStart, wasmMemoryOnlyModule);
expectThrowMatching(
  () => wasiNoStart.start(noStartInstance.instance),
  /Missing required WASI entry export/
);
expectThrowMatching(
  () =>
    wasiNoStart.start(createInstance(wasiNoStart, wasmStartModule).instance),
  /cannot be reused/
);
console.log('PASS: start() failure locks WASI instance');

console.log('Test 3: start() rejects modules exporting _initialize');
const wasiDual = new WASI({});
const dualInstance = createInstance(wasiDual, wasmStartWithInitializeModule);
expectThrowMatching(
  () => wasiDual.start(dualInstance.instance),
  /_initialize export is incompatible/
);
expectThrowMatching(
  () => wasiDual.start(createInstance(wasiDual, wasmStartModule).instance),
  /cannot be reused/
);
console.log('PASS: start() enforces _initialize absence');

console.log(
  'Test 4: initialize() succeeds exactly once when _initialize is present'
);
const wasiInit = new WASI({});
const initInstance = createInstance(wasiInit, wasmInitializeModule);
expectNoThrow(() => wasiInit.initialize(initInstance.instance));
expectThrowMatching(
  () => wasiInit.initialize(initInstance.instance),
  /already initialized/
);
console.log('PASS: initialize() enforces single invocation');

console.log('Test 5: initialize() succeeds when _initialize export is missing');
const wasiOptionalInit = new WASI({});
const memoryOnly = createInstance(wasiOptionalInit, wasmMemoryOnlyModule);
expectNoThrow(() => wasiOptionalInit.initialize(memoryOnly.instance));
expectThrowMatching(
  () => wasiOptionalInit.initialize(memoryOnly.instance),
  /already initialized/
);
console.log('PASS: initialize() treats missing _initialize as no-op');

console.log('Test 6: initialize() rejects modules exporting _start');
const wasiInvalidInit = new WASI({});
const startExportInstance = createInstance(wasiInvalidInit, wasmStartModule);
expectThrowMatching(
  () => wasiInvalidInit.initialize(startExportInstance.instance),
  /_start export is incompatible/
);
console.log('PASS: initialize() enforces _start absence');

console.log('Test 7: start() blocked after initialize()');
const wasiMixed = new WASI({});
const reactor = createInstance(wasiMixed, wasmInitializeModule);
wasiMixed.initialize(reactor.instance);
expectThrowMatching(
  () => wasiMixed.start(createInstance(wasiMixed, wasmStartModule).instance),
  /already initialized/
);
console.log('PASS: start() rejects after initialize()');

console.log('Test 8: invalid arguments mark instance unusable');
const wasiInvalid = new WASI({});
expectThrowMatching(
  () => wasiInvalid.start({}),
  /The "instance\.exports" property must be of type object/
);
expectThrowMatching(
  () =>
    wasiInvalid.start(createInstance(wasiInvalid, wasmStartModule).instance),
  /cannot be reused/
);
const wasiInvalidInitArgs = new WASI({});
expectThrowMatching(
  () => wasiInvalidInitArgs.initialize({}),
  /The "instance\.exports" property must be of type object/
);
expectThrowMatching(
  () =>
    wasiInvalidInitArgs.initialize(
      createInstance(wasiInvalidInitArgs, wasmInitializeModule).instance
    ),
  /cannot be reused/
);
console.log('PASS: invalid arguments permanently lock instance');

console.log('All WASI lifecycle tests passed.');
