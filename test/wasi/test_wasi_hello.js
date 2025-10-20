'use strict';

const assert = require('node:assert');
const path = require('node:path');
const fs = require('node:fs');
const { WASI } = require('node:wasi');

function loadWasm(name) {
  const filePath = path.join(__dirname, 'wasm', name);
  return fs.readFileSync(filePath);
}

console.log('========================================');
console.log('WASI Hello World Integration');
console.log('========================================');

console.log(
  'Test 1: wasi.start() executes _start export and returns exit code'
);
const startWasi = new WASI({ returnOnExit: true });
const startModule = new WebAssembly.Module(loadWasm('hello_start.wasm'));
const startInstance = new WebAssembly.Instance(
  startModule,
  startWasi.getImportObject()
);
const startResult = startWasi.start(startInstance);
assert.strictEqual(startResult, 0, 'hello_start.wasm should exit with code 0');

console.log('Test 2: wasi.initialize() executes optional _initialize export');
const initWasi = new WASI({ version: 'preview1' });
const initModule = new WebAssembly.Module(loadWasm('hello_initialize.wasm'));
const initInstance = new WebAssembly.Instance(
  initModule,
  initWasi.getImportObject()
);
const initResult = initWasi.initialize(initInstance);
assert.strictEqual(
  initResult,
  undefined,
  'initialize() should return undefined'
);

console.log('All WASI hello world integration tests passed.');
