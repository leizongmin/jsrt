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
  0x00,
  0x61,
  0x73,
  0x6d, // magic
  0x01,
  0x00,
  0x00,
  0x00, // version
  0x01,
  0x04,
  0x01,
  0x60,
  0x00,
  0x00, // type section
  0x03,
  0x02,
  0x01,
  0x00, // function section
  0x05,
  0x03,
  0x01,
  0x00,
  0x01, // memory section
  0x07,
  0x13,
  0x02,
  0x06,
  0x6d,
  0x65,
  0x6d,
  0x6f,
  0x72,
  0x79,
  0x02,
  0x00,
  0x06,
  0x5f,
  0x73,
  0x74,
  0x61,
  0x72,
  0x74,
  0x00,
  0x00, // export section (memory + _start)
  0x0a,
  0x04,
  0x01,
  0x02,
  0x00,
  0x0b, // code section
]);

// Minimal WASM module exporting memory and a no-op _initialize function
const wasmInitializeModule = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00,
  0x00, 0x03, 0x02, 0x01, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x18, 0x02,
  0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x0b, 0x5f, 0x69, 0x6e,
  0x69, 0x74, 0x69, 0x61, 0x6c, 0x69, 0x7a, 0x65, 0x00, 0x00, 0x0a, 0x04, 0x01,
  0x02, 0x00, 0x0b,
]);

function createInstance(wasi, bytes) {
  const module = new WebAssembly.Module(bytes);
  const instance = new WebAssembly.Instance(module, wasi.getImportObject());
  return { module, instance };
}

console.log('Test 1: start() succeeds exactly once');
const wasiStart = new WASI({});
const startMain = createInstance(wasiStart, wasmStartModule);
expectNoThrow(
  () => wasiStart.start(startMain.instance),
  'start() should succeed on first call'
);
expectThrowMatching(
  () => wasiStart.start(startMain.instance),
  /WASI instance already started/,
  'start() should reject repeated invocation'
);
console.log('PASS: start() enforces single invocation');
console.log('Checkpoint: test 1 completed');

console.log('Test 2: start() blocks reuse after missing _start export');
const wasiRetry = new WASI({});
const initOnly = createInstance(wasiRetry, wasmInitializeModule);
expectThrowMatching(
  () => wasiRetry.start(initOnly.instance),
  /Missing required WASI entry export/,
  'start() should fail when _start export is missing'
);
const retryInstance = createInstance(wasiRetry, wasmStartModule);
expectThrowMatching(
  () => wasiRetry.start(retryInstance.instance),
  /WASI instance cannot be reused after failure/,
  'start() should prevent reuse after a fatal failure'
);
console.log('PASS: start() marks instance unusable after fatal failure');
console.log('Checkpoint: test 2 completed');

console.log('Test 3: initialize() succeeds exactly once');
const wasiInit = new WASI({});
const initInstance = createInstance(wasiInit, wasmInitializeModule);
expectNoThrow(
  () => wasiInit.initialize(initInstance.instance),
  'initialize() should succeed on first call'
);
expectThrowMatching(
  () => wasiInit.initialize(initInstance.instance),
  /WASI instance already initialized/,
  'initialize() should reject repeated invocation'
);
console.log('PASS: initialize() enforces single invocation');
console.log('Checkpoint: test 3 completed');

console.log('Test 4: start() blocked after initialize()');
const wasiMixed = new WASI({});
const mixedInitInstance = createInstance(wasiMixed, wasmInitializeModule);
wasiMixed.initialize(mixedInitInstance.instance);
const mixedStartInstance = createInstance(wasiMixed, wasmStartModule);
expectThrowMatching(
  () => wasiMixed.start(mixedStartInstance.instance),
  /WASI instance already initialized/,
  'start() should not run after initialize()'
);
console.log('PASS: start() respects initialize() state');
console.log('Checkpoint: test 4 completed');

console.log('Test 5: invalid arguments rejected');
const wasiInvalid = new WASI({});
expectThrowMatching(
  () => wasiInvalid.start({}),
  /Invalid WASI argument/,
  'start() should require WebAssembly.Instance argument'
);
expectThrowMatching(
  () => wasiInvalid.initialize({}),
  /Invalid WASI argument/,
  'initialize() should require WebAssembly.Instance argument'
);
const validInstanceAfterError = createInstance(wasiInvalid, wasmStartModule);
expectNoThrow(
  () => wasiInvalid.start(validInstanceAfterError.instance),
  'start() should work after invalid argument rejection'
);
console.log('PASS: argument validation leaves WASI usable');
console.log('Checkpoint: test 5 completed');

console.log('All WASI lifecycle tests passed.');
