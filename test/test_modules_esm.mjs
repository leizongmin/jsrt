// Test ESM module imports

// Test 1: Named imports
import { hello, value } from './test_module.mjs';
const result1 = hello('World');
// Verify result exists
if (!result1) console.error('Named import failed');

// Test 2: Dynamic import
import('./test_module.mjs')
  .then((mod) => {
    const result2 = mod.hello('Dynamic');
    // Verify results exist
    if (!result2 || !mod.value) console.error('Dynamic import failed');
  })
  .catch((err) => {
    console.error('Dynamic import failed:', err);
  });

// Test 3: Top-level await with dynamic import
const awaited_mod = await import('./test_module.mjs');
const result3 = awaited_mod.hello('Awaited');
// Verify results exist
if (!result3 || !awaited_mod.value)
  console.error('Top-level await import failed');
