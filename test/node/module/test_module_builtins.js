/**
 * Test suite for module.builtinModules and module.isBuiltin()
 */

// Test module.builtinModules
try {
  const module = require('node:module');
  console.assert(module !== undefined, 'module should be defined');
  console.assert(
    module.builtinModules !== undefined,
    'module.builtinModules should be defined'
  );
  console.assert(
    Array.isArray(module.builtinModules),
    'module.builtinModules should be an array'
  );
  console.log('✓ module.builtinModules exists and is an array');
} catch (e) {
  console.error('✗ Failed to load node:module:', e.message);
}

// Test module.isBuiltin()
try {
  const module = require('node:module');
  console.assert(
    typeof module.isBuiltin === 'function',
    'module.isBuiltin should be a function'
  );
  console.log('✓ module.isBuiltin exists and is a function');

  // Test with common built-in modules
  console.assert(module.isBuiltin('fs') === true, 'fs should be built-in');
  console.assert(module.isBuiltin('http') === true, 'http should be built-in');
  console.assert(module.isBuiltin('path') === true, 'path should be built-in');
  console.assert(
    module.isBuiltin('node:fs') === true,
    'node:fs should be built-in'
  );
  console.assert(
    module.isBuiltin('node:http') === true,
    'node:http should be built-in'
  );
  console.assert(
    module.isBuiltin('lodash') === false,
    'lodash should not be built-in'
  );
  console.assert(
    module.isBuiltin('./mymodule') === false,
    'relative paths should not be built-in'
  );
  console.assert(
    module.isBuiltin('') === false,
    'empty string should not be built-in'
  );
  console.log('✓ module.isBuiltin() works correctly');
} catch (e) {
  console.error('✗ module.isBuiltin test failed:', e.message);
}

// Test that builtinModules array is populated
try {
  const module = require('node:module');
  console.assert(
    module.builtinModules.length > 0,
    'builtinModules should not be empty'
  );
  console.assert(
    module.builtinModules.includes('fs'),
    'builtinModules should include fs'
  );
  console.assert(
    module.builtinModules.includes('node:fs'),
    'builtinModules should include node:fs'
  );
  console.assert(
    module.builtinModules.includes('http'),
    'builtinModules should include http'
  );
  console.assert(
    module.builtinModules.includes('path'),
    'builtinModules should include path'
  );
  console.log('✓ module.builtinModules is populated correctly');
  console.log(`  Found ${module.builtinModules.length} built-in modules`);
} catch (e) {
  console.error('✗ module.builtinModules population test failed:', e.message);
}

console.log('Module builtins tests completed');
