/**
 * Test suite for module.createRequire()
 */

const { createRequire } = require('node:module');

// Test 1: Create require function with string path
try {
  const customRequire = createRequire('/test/fake/path.js');
  console.assert(
    typeof customRequire === 'function',
    'createRequire should return a function'
  );
  console.log('✓ createRequire returns function');
} catch (e) {
  console.error('✗ createRequire function creation failed:', e.message);
}

// Test 2: Require builtin module with custom require
try {
  const customRequire = createRequire('/test/fake/path.js');
  const fs = customRequire('fs');
  console.assert(
    typeof fs === 'object',
    'Should be able to require builtin modules'
  );
  console.log('✓ Custom require can load builtin modules');
} catch (e) {
  console.error('✗ Custom require builtin loading failed:', e.message);
}

// Test 3: require.resolve property exists
try {
  const customRequire = createRequire('/test/fake/path.js');
  console.assert(
    typeof customRequire.resolve === 'function',
    'require.resolve should be a function'
  );
  console.log('✓ require.resolve property exists');
} catch (e) {
  console.error('✗ require.resolve property check failed:', e.message);
}

// Test 4: require.cache property exists
try {
  const customRequire = createRequire('/test/fake/path.js');
  console.assert(
    typeof customRequire.cache === 'object',
    'require.cache should be an object'
  );
  console.log('✓ require.cache property exists');
} catch (e) {
  console.error('✗ require.cache property check failed:', e.message);
}

// Test 5: require.extensions property exists
try {
  const customRequire = createRequire('/test/fake/path.js');
  console.assert(
    typeof customRequire.extensions === 'object',
    'require.extensions should be an object'
  );
  console.log('✓ require.extensions property exists');
} catch (e) {
  console.error('✗ require.extensions property check failed:', e.message);
}

// Test 6: require.main property exists (initially undefined)
try {
  const customRequire = createRequire('/test/fake/path.js');
  console.assert('main' in customRequire, 'require.main property should exist');
  console.log('✓ require.main property exists');
} catch (e) {
  console.error('✗ require.main property check failed:', e.message);
}

// Test 7: Handle file:// URL
try {
  const customRequire = createRequire('file:///test/fake/path.js');
  console.assert(
    typeof customRequire === 'function',
    'Should handle file:// URLs'
  );
  console.log('✓ file:// URL handling');
} catch (e) {
  console.error('✗ file:// URL handling failed:', e.message);
}

// Test 8: Missing filename argument
try {
  createRequire();
  console.error('✗ Should throw error for missing filename');
} catch (e) {
  if (e.message.includes('Missing filename')) {
    console.log('✓ Error thrown for missing filename');
  } else {
    console.error('✗ Wrong error message:', e.message);
  }
}

console.log('\nmodule.createRequire tests completed');
