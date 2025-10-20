'use strict';

const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { TextDecoder, TextEncoder } = require('node:util');
const { WASI } = require('node:wasi');

const decoder = new TextDecoder('utf8');
const encoder = new TextEncoder();

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

function utf8ByteLength(value) {
  return encoder.encode(value).length;
}

console.log('========================================');
console.log('WASI Unicode Args & Env');
console.log('========================================');

console.log('Test 1: Unicode arguments roundtrip through args_get');
const unicodeArgs = ['--emoji=😀', 'тест', '路径', 'مرحبا'];
const argsHarness = instantiateWasi({ args: unicodeArgs, returnOnExit: true });
const argsNS = argsHarness.importObject.wasi_snapshot_preview1;
const argsMem = argsHarness.memoryView;

const argcPtr = 0;
const argvBufSizePtr = 4;
assert.strictEqual(argsNS.args_sizes_get(argcPtr, argvBufSizePtr), 0);
const argc = argsMem.getUint32(argcPtr, true);
const argvBufSize = argsMem.getUint32(argvBufSizePtr, true);
assert.strictEqual(argc, unicodeArgs.length);
const expectedArgSize = unicodeArgs.reduce(
  (acc, value) => acc + utf8ByteLength(value) + 1,
  0
);
assert.strictEqual(argvBufSize, expectedArgSize);

const argvArrayPtr = 32;
const argvBufPtr = 256;
assert.strictEqual(argsNS.args_get(argvArrayPtr, argvBufPtr), 0);
const decodedArgs = [];
for (let i = 0; i < argc; i += 1) {
  const argPtr = argsMem.getUint32(argvArrayPtr + i * 4, true);
  decodedArgs.push(readCString(argsMem, argPtr));
}
assert.deepStrictEqual(decodedArgs, unicodeArgs);

console.log('Test 2: Unicode environment values surface via environ_get');
const unicodeEnv = {
  GREETING: 'こんにちは世界',
  EMOJI: '🚀🌕✨',
  CYRILLIC: 'Привет мир',
};
const envHarness = instantiateWasi({ env: unicodeEnv, returnOnExit: true });
const envNS = envHarness.importObject.wasi_snapshot_preview1;
const envMem = envHarness.memoryView;

const envCountPtr = 0;
const envBufSizePtr = 4;
assert.strictEqual(envNS.environ_sizes_get(envCountPtr, envBufSizePtr), 0);
const envCount = envMem.getUint32(envCountPtr, true);
const envBufSize = envMem.getUint32(envBufSizePtr, true);
const expectedEnvEntries = Object.entries(unicodeEnv).map(
  ([key, value]) => `${key}=${value}`
);
const expectedEnvSize = expectedEnvEntries.reduce(
  (acc, entry) => acc + utf8ByteLength(entry) + 1,
  0
);
assert.strictEqual(envCount, expectedEnvEntries.length);
assert.strictEqual(envBufSize, expectedEnvSize);

const environArrayPtr = 64;
const environBufPtr = 320;
assert.strictEqual(envNS.environ_get(environArrayPtr, environBufPtr), 0);
const decodedEnv = [];
for (let i = 0; i < envCount; i += 1) {
  const entryPtr = envMem.getUint32(environArrayPtr + i * 4, true);
  decodedEnv.push(readCString(envMem, entryPtr));
}

for (const entry of expectedEnvEntries) {
  assert.ok(decodedEnv.includes(entry), `Missing environment entry: ${entry}`);
}

console.log('All WASI Unicode tests passed.');
