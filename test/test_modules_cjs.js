// Test CommonJS require
console.log('=== CommonJS Module Tests ===');

// Test 1: Basic require
console.log('Test 1: Basic require');
const { greet, data } = require('./test/commonjs_module.js');
console.log('greet("World"):', greet('World'));
console.log('data:', data);

// Test 2: Nested require (require the same module again)
console.log('\nTest 2: Nested require');
const mod2 = require('./test/commonjs_module.js');
console.log('mod2.greet("Again"):', mod2.greet('Again'));
console.log('mod2.data:', mod2.data);

console.log('\n=== All CommonJS tests completed ===');