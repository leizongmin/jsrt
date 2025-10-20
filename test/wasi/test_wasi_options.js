'use strict';

const assert = require('node:assert');
const path = require('node:path');
const fs = require('node:fs');
const { TextDecoder } = require('node:util');
const { WASI } = require('node:wasi');

const decoder = new TextDecoder('utf8');

function loadWasm(name) {
  const filePath = path.join(__dirname, 'wasm', name);
  return fs.readFileSync(filePath);
}

function readCString(view, offset) {
  const bytes = [];
  let cursor = offset;
  while (true) {
    const value = view.getUint8(cursor);
    if (value === 0) {
      break;
    }
    bytes.push(value);
    cursor += 1;
  }
  return decoder.decode(new Uint8Array(bytes));
}

function instantiateWasi(options) {
  const wasi = new WASI(options);
  const importObject = wasi.getImportObject();
  const module = new WebAssembly.Module(loadWasm('hello_start.wasm'));
  const instance = new WebAssembly.Instance(module, importObject);
  const exitCode = wasi.start(instance);
  return {
    wasi,
    importObject,
    instance,
    memoryView: new DataView(instance.exports.memory.buffer),
    exitCode,
  };
}

console.log('========================================');
console.log('WASI Options Validation');
console.log('========================================');

console.log('Test 1: args are exposed through args_sizes_get and args_get');
const args = ['--flag', 'value'];
const argsHarness = instantiateWasi({ args, returnOnExit: true });
const argsNS = argsHarness.importObject.wasi_snapshot_preview1;
const argsMem = argsHarness.memoryView;

const argcPtr = 0;
const argvBufSizePtr = 4;
assert.strictEqual(argsNS.args_sizes_get(argcPtr, argvBufSizePtr), 0);
const argc = argsMem.getUint32(argcPtr, true);
const argvBufSize = argsMem.getUint32(argvBufSizePtr, true);
assert.strictEqual(argc, args.length);
assert.strictEqual(
  argvBufSize,
  args.reduce((acc, value) => acc + value.length + 1, 0)
);

const argvArrayPtr = 16;
const argvBufPtr = 128;
assert.strictEqual(argsNS.args_get(argvArrayPtr, argvBufPtr), 0);
const readArgs = [];
for (let i = 0; i < argc; i += 1) {
  const argPtr = argsMem.getUint32(argvArrayPtr + i * 4, true);
  readArgs.push(readCString(argsMem, argPtr));
}
assert.deepStrictEqual(readArgs, args);

console.log('Test 2: environment variables are surfaced via environ_get');
const env = { FOO: 'bar', EMPTY: '' };
const envHarness = instantiateWasi({ env, returnOnExit: true });
const envNS = envHarness.importObject.wasi_snapshot_preview1;
const envMem = envHarness.memoryView;

const envCountPtr = 0;
const envBufSizePtr = 4;
assert.strictEqual(envNS.environ_sizes_get(envCountPtr, envBufSizePtr), 0);
const envCount = envMem.getUint32(envCountPtr, true);
const envBufSize = envMem.getUint32(envBufSizePtr, true);
assert.strictEqual(envCount, Object.keys(env).length);
const expectedEnvStrings = Object.entries(env).map(
  ([key, value]) => `${key}=${value}`
);
const expectedSize = expectedEnvStrings.reduce(
  (acc, value) => acc + value.length + 1,
  0
);
assert.strictEqual(envBufSize, expectedSize);

const environArrayPtr = 32;
const environBufPtr = 160;
assert.strictEqual(envNS.environ_get(environArrayPtr, environBufPtr), 0);
const readEnv = [];
for (let i = 0; i < envCount; i += 1) {
  const entryPtr = envMem.getUint32(environArrayPtr + i * 4, true);
  readEnv.push(readCString(envMem, entryPtr));
}
assert.strictEqual(readEnv.length, expectedEnvStrings.length);
for (const entry of expectedEnvStrings) {
  assert.ok(readEnv.includes(entry), `Missing environment entry: ${entry}`);
}

console.log('Test 3: preopen directories are reported through fd_prestat');
const preopenHarness = instantiateWasi({
  preopens: { '/sandbox': '.' },
  returnOnExit: true,
});
const preopenNS = preopenHarness.importObject.wasi_snapshot_preview1;
const preopenMem = preopenHarness.memoryView;

const prestatBufPtr = 0;
assert.strictEqual(preopenNS.fd_prestat_get(3, prestatBufPtr), 0);
const prestatType = preopenMem.getUint8(prestatBufPtr);
const __WASI_PREOPENTYPE_DIR = 0;
assert.strictEqual(
  prestatType,
  __WASI_PREOPENTYPE_DIR,
  'FD 3 should represent a directory preopen'
);

const pathPtr = 64;
const pathLen = 16;
assert.strictEqual(preopenNS.fd_prestat_dir_name(3, pathPtr, pathLen), 0);
const preopenPath = readCString(preopenMem, pathPtr);
assert.strictEqual(preopenPath, '/sandbox');

console.log('All WASI option validation tests passed.');
