// Test ESM module imports
console.log('=== ESM Module Import Tests ===');

// Test 1: Named imports
import { hello, value } from './test_module.mjs';
console.log('Test 1: Named imports');
console.log('hello("World"):', hello('World'));
console.log('value:', value);

// Test 2: Dynamic import
console.log('\nTest 2: Dynamic import');
import('./test_module.mjs')
  .then(mod => {
    console.log('Dynamic hello("Dynamic"):', mod.hello('Dynamic'));
    console.log('Dynamic value:', mod.value);
  })
  .catch(err => {
    console.error('Dynamic import failed:', err);
  });

// Test 3: Top-level await with dynamic import
console.log('\nTest 3: Top-level await with dynamic import');
const awaited_mod = await import('./test_module.mjs');
console.log('Awaited hello("Awaited"):', awaited_mod.hello('Awaited'));
console.log('Awaited value:', awaited_mod.value);

console.log('\n=== All ESM tests completed ===');