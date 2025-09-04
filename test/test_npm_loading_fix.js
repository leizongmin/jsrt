// Test npm package loading fixes
const assert = require('jsrt:assert');
console.log('=== NPM Package Loading Fix Tests ===');

// Test 1: CommonJS require works
console.log('\nTest 1: CommonJS require("react")');
try {
  const react = require('react');
  assert.strictEqual(typeof react, 'object', 'React should be an object');
  assert.ok(react.createElement, 'React should have createElement method');
  console.log(react);
  console.log('✓ CommonJS require works correctly');
} catch (error) {
  console.log('✗ CommonJS require failed:', error.message);
  throw error;
}

// Test 2: ES module import works
console.log('\nTest 2: ES module import("react")');
import('react')
  .then((reactModule) => {
    try {
      assert.strictEqual(
        typeof reactModule.default,
        'object',
        'React default export should be an object'
      );
      assert.ok(
        reactModule.default.createElement,
        'React should have createElement method'
      );
      console.log(reactModule);
      console.log('✓ ES module import works correctly');
      console.log('\n=== All NPM package loading tests passed ===');
    } catch (error) {
      console.log('✗ ES module import failed:', error.message);
      throw error;
    }
  })
  .catch((error) => {
    console.log('✗ ES module import failed:', error.message);
    throw error;
  });
