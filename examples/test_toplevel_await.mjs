// Test top-level await with dynamic import
console.log('Top-level await test:');

const mod = await import('./test_module.mjs');
console.log('Awaited import:', mod.hello('Top-level'));
console.log('Awaited value:', mod.value);

console.log('Top-level await completed');
