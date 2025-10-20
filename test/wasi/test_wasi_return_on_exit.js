'use strict';

const assert = require('node:assert');
const path = require('node:path');
const fs = require('node:fs');
const ffi = require('jsrt:ffi');
const { WASI } = require('node:wasi');

function loadWasm(name) {
  const filePath = path.join(__dirname, 'wasm', name);
  return fs.readFileSync(filePath);
}

function decodeExitStatus(status) {
  // Equivalent to WEXITSTATUS from <sys/wait.h>
  return (status >> 8) & 0xff;
}

function ensureLibc() {
  const candidates = ['libc.so.6', 'libSystem.B.dylib'];
  for (const name of candidates) {
    try {
      return ffi.Library(name, { system: ['int', ['string']] });
    } catch (err) {
      // Try next candidate
    }
  }
  assert.fail('Unable to load libc for system() call');
}

console.log('========================================');
console.log('WASI returnOnExit Behaviour Tests');
console.log('========================================');

console.log('Test 1: returnOnExit=true causes start() to return exit code');
const wasiReturn = new WASI({ returnOnExit: true });
const startModule = new WebAssembly.Module(loadWasm('hello_start.wasm'));
const startInstance = new WebAssembly.Instance(
  startModule,
  wasiReturn.getImportObject()
);
const startResult = wasiReturn.start(startInstance);
assert.strictEqual(
  startResult,
  0,
  'start() should return exit code when returnOnExit=true'
);
console.log('PASS: start() returned exit code 0');

console.log('Test 2: returnOnExit=true converts proc_exit into throws');
const wasiThrowing = new WASI({ returnOnExit: true });
const { wasi_snapshot_preview1: namespace } = wasiThrowing.getImportObject();
let threw = false;
try {
  namespace.proc_exit(33);
} catch (error) {
  threw = true;
  assert.match(
    String(error),
    /WASI proc_exit/,
    'proc_exit() should throw when returnOnExit=true'
  );
}
assert.strictEqual(
  threw,
  true,
  'proc_exit() should have thrown for returnOnExit=true'
);
console.log('PASS: proc_exit threw instead of exiting');

console.log('Test 3: returnOnExit=false terminates the host process (skipped)');
console.log(
  'SKIP: proc_exit invocation disabled to avoid terminating automated runners/ASAN harness.'
);

console.log('All returnOnExit behaviour tests passed.');
