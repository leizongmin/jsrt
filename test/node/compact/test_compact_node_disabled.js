// Test that compact-node mode does NOT affect normal operation
// This test should be run with compact mode disabled: jsrt --no-compact-node test_compact_node_disabled.js

const assert = require('jsrt:assert');

console.log(
  '\nTest 1: Bare module names should NOT resolve when compact mode is disabled'
);
let osLoadFailed = false;
try {
  const os = require('os');
  console.log(
    '  ❌ ERROR: require("os") should have failed with compact mode disabled'
  );
  process.exit(1);
} catch (e) {
  osLoadFailed = true;
  assert(e.message.includes('os'), 'Error should mention module name "os"');
  console.log('  ✓ require("os") correctly failed:', e.message);
}

console.log('\nTest 2: Prefixed module names should still work');
const os = require('node:os');
const path = require('node:path');
assert(typeof os === 'object', 'node:os should work');
assert(typeof path === 'object', 'node:path should work');
console.log('  ✓ require("node:os") works');
console.log('  ✓ require("node:path") works');

console.log(
  '\nTest 3: Process should still be global (not affected by compact mode)'
);
assert(typeof process !== 'undefined', 'process should be globally available');
assert(
  typeof process.platform === 'string',
  'process.platform should be a string'
);
console.log('  ✓ process is global');

console.log('\n✅ All tests passed! Compact node mode is correctly disabled.');
