'use strict';

const assert = require('node:assert');

console.log('========================================');
console.log('WASI Loader Tests (CommonJS)');
console.log('========================================');

const wasiNodeFirst = require('node:wasi');
const wasiNodeSecond = require('node:wasi');
const wasiJsrt = require('jsrt:wasi');

assert.strictEqual(
  wasiNodeFirst,
  wasiNodeSecond,
  'node:wasi should return cached module on subsequent require() calls'
);

assert.strictEqual(
  wasiNodeFirst.WASI,
  wasiJsrt.WASI,
  'node:wasi and jsrt:wasi should expose the same WASI constructor'
);

const wasiInstance = new wasiNodeFirst.WASI({});
assert.ok(
  wasiInstance instanceof wasiNodeFirst.WASI,
  'WASI constructor should create instances'
);

console.log(
  'PASS: CommonJS loader exposes consistent WASI module across node:/jsrt: protocols'
);
