const assert = require('jsrt:assert');

// Testing jsrt:process module with CommonJS require:
const process = require('jsrt:process');
assert.strictEqual(
  typeof process.argv,
  'object',
  'process.argv should be an object'
);
assert.strictEqual(
  typeof process.argv0,
  'string',
  'process.argv0 should be a string'
);
assert.strictEqual(
  typeof process.pid,
  'number',
  'process.pid should be a number'
);
assert.strictEqual(
  typeof process.ppid,
  'number',
  'process.ppid should be a number'
);
assert.strictEqual(
  typeof process.version,
  'string',
  'process.version should be a string'
);
assert.strictEqual(
  typeof process.platform,
  'string',
  'process.platform should be a string'
);
assert.strictEqual(
  typeof process.arch,
  'string',
  'process.arch should be a string'
);
assert.strictEqual(
  typeof process.uptime(),
  'number',
  'process.uptime() should return a number'
);

// Testing jsrt:process module with CommonJS require:
const process2 = require('jsrt:process');

// Test that both imports refer to the same object
assert.strictEqual(
  process,
  process2,
  'ES import and CommonJS require should return the same object'
);

// Test basic properties
assert.strictEqual(
  Array.isArray(process2.argv),
  true,
  'argv should be an array'
);
assert.strictEqual(typeof process2.argv0, 'string', 'argv0 should be a string');
assert.strictEqual(typeof process2.pid, 'number', 'pid should be a number');
assert.strictEqual(process2.pid > 0, true, 'pid should be positive');
assert.strictEqual(typeof process2.ppid, 'number', 'ppid should be a number');
assert.strictEqual(process2.ppid > 0, true, 'ppid should be positive');
assert.strictEqual(
  typeof process2.version,
  'string',
  'version should be a string'
);
assert.strictEqual(
  process2.version.length > 0,
  true,
  'version should not be empty'
);
assert.strictEqual(
  typeof process2.platform,
  'string',
  'platform should be a string'
);
assert.strictEqual(
  process2.platform.length > 0,
  true,
  'platform should not be empty'
);
assert.strictEqual(typeof process2.arch, 'string', 'arch should be a string');
assert.strictEqual(process2.arch.length > 0, true, 'arch should not be empty');

// Test uptime function
const uptime1 = process2.uptime();
assert.strictEqual(typeof uptime1, 'number', 'uptime should return a number');
assert.strictEqual(uptime1 >= 0, true, 'uptime should be non-negative');

// Test versions object
assert.strictEqual(
  typeof process2.versions,
  'object',
  'versions should be an object'
);
assert.strictEqual(
  process2.versions !== null,
  true,
  'versions should not be null'
);

// Test that uptime increases over time
setTimeout(() => {
  const uptime2 = process.uptime();
  assert.strictEqual(
    uptime2 > uptime1,
    true,
    'uptime should increase over time'
  );
}, 100);
