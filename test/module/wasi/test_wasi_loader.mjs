import assert from 'node:assert';
import wasiDefault, { WASI } from 'node:wasi';

console.log('========================================');
console.log('WASI Loader Tests (ESM)');
console.log('========================================');

assert.strictEqual(
  typeof WASI,
  'function',
  'WASI named export should be a constructor'
);
assert.strictEqual(
  wasiDefault.WASI,
  WASI,
  'Default export should contain WASI constructor'
);

const wasiInstance = new WASI({});
assert.ok(
  wasiInstance instanceof WASI,
  'WASI constructor should produce instances'
);

console.log(
  'PASS: ESM loader exposes WASI constructor via named/default exports'
);
