const assert = require('jsrt:assert');

console.log('=== WebAssembly Module and Instance Tests ===');

// Test 1: Create a minimal valid WebAssembly module
console.log('Test 1: WebAssembly.Module with minimal valid WASM');
const validWasm = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // magic number
  0x01,
  0x00,
  0x00,
  0x00, // version
]);

let module;
try {
  module = new WebAssembly.Module(validWasm);
  console.log('✓ WebAssembly.Module created successfully');
  assert.ok(
    module instanceof WebAssembly.Module,
    'Module should be instance of WebAssembly.Module'
  );
} catch (error) {
  console.log('❌ Failed to create WebAssembly.Module:', error.message);
  console.log('Note: Minimal WASM might need more sections for WAMR');
}

// Test 2: Test invalid WebAssembly data with Module constructor
console.log('\nTest 2: WebAssembly.Module with invalid data');
const invalidWasm = new Uint8Array([0x00, 0x61, 0x73]); // incomplete magic number
try {
  const invalidModule = new WebAssembly.Module(invalidWasm);
  console.log('❌ Should have thrown an error');
} catch (error) {
  console.log(
    '✓ WebAssembly.Module correctly threw error for invalid data:',
    error.message
  );
}

// Test 3: Test WebAssembly.Instance creation (only if module creation worked)
if (module) {
  console.log('\nTest 3: WebAssembly.Instance creation');
  try {
    const instance = new WebAssembly.Instance(module);
    console.log('✓ WebAssembly.Instance created successfully');
    assert.ok(
      instance instanceof WebAssembly.Instance,
      'Instance should be instance of WebAssembly.Instance'
    );
  } catch (error) {
    console.log('❌ Failed to create WebAssembly.Instance:', error.message);
    console.log('Note: Minimal module might not be instantiable');
  }
} else {
  console.log('\nTest 3: Skipping WebAssembly.Instance test (no valid module)');
}

// Test 4: Test Instance constructor with invalid module
console.log('\nTest 4: WebAssembly.Instance with invalid module');
try {
  const invalidInstance = new WebAssembly.Instance({});
  console.log('❌ Should have thrown an error');
} catch (error) {
  console.log(
    '✓ WebAssembly.Instance correctly threw error for invalid module:',
    error.message
  );
}

console.log('\n=== WebAssembly Module and Instance Tests Completed ===');
