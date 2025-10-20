'use strict';

const assert = require('node:assert');
const { WASI } = require('node:wasi');

console.log('========================================');
console.log('WASI Error Message Validation');
console.log('========================================');

const wasmStartModule = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00,
  0x00, 0x03, 0x02, 0x01, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x13, 0x02,
  0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x06, 0x5f, 0x73, 0x74,
  0x61, 0x72, 0x74, 0x00, 0x00, 0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
]);

function createInstance(wasi, bytes) {
  const module = new WebAssembly.Module(bytes);
  return new WebAssembly.Instance(module, wasi.getImportObject());
}

console.log('Test 1: start() invalid exports matches Node.js message');
const wasiInvalid = new WASI({ returnOnExit: true });
try {
  wasiInvalid.start({});
  assert.fail('Expected start() to throw when instance exports missing');
} catch (error) {
  assert.strictEqual(error.name, 'TypeError');
  assert.strictEqual(
    error.message,
    'The "instance.exports" property must be of type object. Received undefined'
  );
}
console.log('PASS: Missing exports error mirrors Node.js wording');

console.log('All WASI error message validation tests passed.');
