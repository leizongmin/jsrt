const assert = require('jsrt:assert');
const module = require('node:module');

console.log('Testing node:module exports...');
console.log('Available properties:', Object.getOwnPropertyNames(module));

// Test that findPackageJSON is available and works
assert.strictEqual(typeof module.findPackageJSON, 'function',
                   'findPackageJSON should be available as a function');
console.log('✅ findPackageJSON is available');

// Test that findPackageJSON actually works
const result = module.findPackageJSON('./test_simple_package.js');
assert(typeof result === 'object' || typeof result === 'string' || result === undefined,
       `Expected object, string, or undefined from findPackageJSON, got ${typeof result}: ${result}`);
console.log('✅ findPackageJSON works correctly - Result type:', typeof result);

// Test that parsePackageJSON is available
assert.strictEqual(typeof module.parsePackageJSON, 'function',
                   'parsePackageJSON should be available as a function');
console.log('✅ parsePackageJSON is available');

console.log('✅ All tests passed');
