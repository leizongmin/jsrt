const assert = require('jsrt:assert');

// Test process.env basic functionality
assert.ok(typeof process === 'object', 'process should be an object');
assert.ok(typeof process.env === 'object', 'process.env should be an object');

// Test that process.env contains common environment variables
// PATH should exist on most systems
if (process.env.PATH || process.env.Path) {
  assert.ok(
    typeof process.env.PATH === 'string' ||
      typeof process.env.Path === 'string',
    'PATH should be a string'
  );
  console.log('✓ PATH environment variable found');
} else {
  console.log(
    '⚠️ PATH not found in environment (unusual but not necessarily an error)'
  );
}

// Test that we can read multiple environment variables
let envCount = 0;
for (let key in process.env) {
  assert.ok(
    typeof key === 'string',
    'Environment variable key should be string'
  );
  assert.ok(
    typeof process.env[key] === 'string',
    'Environment variable value should be string'
  );
  envCount++;
}

assert.ok(envCount > 0, 'Should have at least one environment variable');
console.log(`✓ Found ${envCount} environment variables`);

// Test some platform-specific variables that are commonly available
const commonVars = [
  'HOME',
  'USER',
  'SHELL', // Unix/Linux/macOS
  'USERPROFILE',
  'USERNAME',
  'COMPUTERNAME', // Windows
  'PWD',
  'TERM', // General Unix-like
];

let foundVars = 0;
for (const varName of commonVars) {
  if (process.env[varName]) {
    foundVars++;
    console.log(
      `✓ Found ${varName} = ${process.env[varName].substring(0, 50)}${process.env[varName].length > 50 ? '...' : ''}`
    );
  }
}

if (foundVars > 0) {
  console.log(`✓ Found ${foundVars} common environment variables`);
} else {
  console.log('⚠️ No common environment variables found (unusual)');
}

// Test that process.env properties are enumerable
const keys = Object.keys(process.env);
assert.ok(
  Array.isArray(keys),
  'Object.keys(process.env) should return an array'
);
assert.strictEqual(
  keys.length,
  envCount,
  'Object.keys length should match iteration count'
);

console.log('=== Tests Completed ===');
