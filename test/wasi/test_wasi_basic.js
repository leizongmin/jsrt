'use strict';

const assert = require('node:assert');
const path = require('node:path');
const fs = require('node:fs');
const { WASI } = require('node:wasi');

function loadFixture(name) {
  const filePath = path.join(__dirname, 'wasm', name);
  return fs.readFileSync(filePath);
}

console.log('========================================');
console.log('WASI Basic Constructor Tests');
console.log('========================================');

console.log('Test 1: default constructor provides lifecycle methods');
const wasiDefault = new WASI();
assert.strictEqual(typeof wasiDefault.getImportObject, 'function');
assert.strictEqual(typeof wasiDefault.start, 'function');
assert.strictEqual(typeof wasiDefault.initialize, 'function');
const defaultImport = wasiDefault.getImportObject();
assert.ok(
  defaultImport.wasi_snapshot_preview1,
  'Default import must expose wasi_snapshot_preview1 namespace'
);

console.log('Test 2: version selection chooses appropriate namespace');
const wasiPreview = new WASI({ version: 'preview1' });
const previewImports = wasiPreview.getImportObject();
assert.ok(previewImports.wasi_snapshot_preview1);

const wasiUnstable = new WASI({ version: 'unstable' });
const unstableImports = wasiUnstable.getImportObject();
assert.ok(unstableImports.wasi_unstable);

console.log('Test 3: constructor accepts args/env/preopens/stdio/returnOnExit');
const wasiOptions = new WASI({
  args: ['alpha', 'beta'],
  env: { FOO: 'bar', EMPTY: '' },
  preopens: { '/sandbox': '.' },
  stdin: 0,
  stdout: 1,
  stderr: 2,
  returnOnExit: true,
});
const optionsImport = wasiOptions.getImportObject();
assert.ok(optionsImport.wasi_snapshot_preview1);

// Smoke-test that the configured WASI instance can be used to instantiate a module
const bytes = loadFixture('hello_start.wasm');
const module = new WebAssembly.Module(bytes);
const instance = new WebAssembly.Instance(module, optionsImport);
assert.ok(instance.exports._start, 'hello_start.wasm must export _start');

console.log('Test 4: invalid versions and option types throw helpful errors');
assert.throws(
  () => new WASI({ version: 'jsrt-v1' }),
  /WASI version must be 'preview1' or 'unstable'/
);
assert.throws(() => new WASI({ env: null }), /env must be an object/);
assert.throws(() => new WASI({ preopens: 5 }), /preopens must be an object/);
assert.throws(
  () => new WASI('not-an-object'),
  /WASI options must be an object/
);

console.log('All WASI basic constructor tests passed.');
