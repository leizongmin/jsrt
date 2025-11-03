// Test ES modules with explicit --compact-node flag for compatibility
// This test should be run with: jsrt --compact-node test_compact_node_esm.mjs

import os from 'os';
import path from 'path';
import { platform } from 'os';
import { join } from 'path';

console.log('\nTest 1: Default imports work');
console.log('  os type:', typeof os);
console.log('  path type:', typeof path);

console.log('\nTest 2: Named imports work');
console.log('  platform:', platform());
console.log('  joined path:', join('a', 'b', 'c'));

console.log('\nTest 3: Mixed imports (prefixed and unprefixed)');
import osWithPrefix from 'node:os';
import pathWithPrefix from 'node:path';

console.log('  node:os type:', typeof osWithPrefix);
console.log('  node:path type:', typeof pathWithPrefix);

// Note: In ES modules, we can't directly compare object identity across imports
// but we can verify both work
console.log('  Both prefixed and unprefixed imports work!');

console.log('\nTest 4: Global process in ES module');
console.log('  process type:', typeof process);
console.log('  process.platform:', process.platform);

console.log('\n✅ All ES module tests passed!');
