// Test Module.getStatistics() function
// This test verifies that module loading statistics are exposed correctly

const { getStatistics } = require('node:module');
const assert = require('assert');

console.log('Testing Module.getStatistics()...');

// Test 1: Function exists and is callable
assert.strictEqual(
  typeof getStatistics,
  'function',
  'getStatistics should be a function'
);

// Test 2: Calling the function should not throw
assert.doesNotThrow(() => {
  const stats = getStatistics();
  console.log('Statistics object:', JSON.stringify(stats, null, 2));
}, 'getStatistics() should not throw');

// Test 3: Return value should be an object with expected structure
const stats = getStatistics();
assert.strictEqual(
  typeof stats,
  'object',
  'getStatistics() should return an object'
);
assert(stats !== null, 'stats should not be null');

// Test 4: Check basic loading statistics properties
assert(typeof stats.loadsTotal === 'number', 'should have loadsTotal');
assert(typeof stats.loadsSuccess === 'number', 'should have loadsSuccess');
assert(typeof stats.loadsFailed === 'number', 'should have loadsFailed');
assert(typeof stats.successRate === 'number', 'should have successRate');

// Test 5: Check cache statistics properties
assert(typeof stats.cacheHits === 'number', 'should have cacheHits');
assert(typeof stats.cacheMisses === 'number', 'should have cacheMisses');
assert(typeof stats.cacheHitRate === 'number', 'should have cacheHitRate');

// Test 6: Check memory usage
assert(typeof stats.memoryUsed === 'number', 'should have memoryUsed');

// Test 7: Check configuration object
assert(
  typeof stats.configuration === 'object',
  'should have configuration object'
);
assert(
  typeof stats.configuration.cacheEnabled === 'boolean',
  'should have cacheEnabled'
);
assert(
  typeof stats.configuration.nodeCompatEnabled === 'boolean',
  'should have nodeCompatEnabled'
);
assert(
  typeof stats.configuration.maxCacheSize === 'number',
  'should have maxCacheSize'
);

// Test 8: Check module cache details (if available)
if (stats.moduleCache) {
  assert(
    typeof stats.moduleCache.hits === 'number',
    'moduleCache should have hits'
  );
  assert(
    typeof stats.moduleCache.misses === 'number',
    'moduleCache should have misses'
  );
  assert(
    typeof stats.moduleCache.size === 'number',
    'moduleCache should have size'
  );
  assert(
    typeof stats.moduleCache.utilization === 'number',
    'moduleCache should have utilization'
  );
}

// Test 9: Check compile cache details (if available)
if (stats.compileCache) {
  assert(
    typeof stats.compileCache.hits === 'number',
    'compileCache should have hits'
  );
  assert(
    typeof stats.compileCache.misses === 'number',
    'compileCache should have misses'
  );
  assert(
    typeof stats.compileCache.hitRate === 'number',
    'compileCache should have hitRate'
  );
}

// Test 10: Verify statistics are reasonable values
assert(stats.loadsTotal >= 0, 'loadsTotal should be non-negative');
assert(stats.loadsSuccess >= 0, 'loadsSuccess should be non-negative');
assert(stats.loadsFailed >= 0, 'loadsFailed should be non-negative');
assert(
  stats.successRate >= 0 && stats.successRate <= 100,
  'successRate should be between 0-100'
);
assert(
  stats.cacheHitRate >= 0 && stats.cacheHitRate <= 100,
  'cacheHitRate should be between 0-100'
);

// Test 11: Load some modules to see statistics change
const initialStats = getStatistics();
const initialTotal = initialStats.loadsTotal;

// Load a few modules
require('path');
require('os');
require('util');

const newStats = getStatistics();
console.log('Initial loads:', initialTotal, 'New loads:', newStats.loadsTotal);

// Should have loaded at least 3 more modules
assert(
  newStats.loadsTotal >= initialTotal + 3,
  'should have loaded more modules'
);

// Test 12: Test Module.getStatistics as static method as well
const Module = require('module');
assert.strictEqual(
  typeof Module.getStatistics,
  'function',
  'Module.getStatistics should be a function'
);

const staticStats = Module.getStatistics();
assert(
  typeof staticStats === 'object',
  'Module.getStatistics() should return an object'
);

console.log('✓ All tests passed!');
console.log('Module.getStatistics() test completed successfully');
console.log('Final statistics:', JSON.stringify(newStats, null, 2));
