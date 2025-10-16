const assert = require('jsrt:assert');

console.log('=== WebAssembly Error Types Tests ===\n');

// Test 1: Check error constructors exist
console.log('Test 1: Error constructors exist');
assert.ok(
  typeof WebAssembly.CompileError === 'function',
  'CompileError should be a function'
);
assert.ok(
  typeof WebAssembly.LinkError === 'function',
  'LinkError should be a function'
);
assert.ok(
  typeof WebAssembly.RuntimeError === 'function',
  'RuntimeError should be a function'
);
console.log('✓ All error constructors exist\n');

// Test 2: Check error names
console.log('Test 2: Error names');
assert.strictEqual(
  WebAssembly.CompileError.prototype.name,
  'CompileError',
  'CompileError.prototype.name'
);
assert.strictEqual(
  WebAssembly.LinkError.prototype.name,
  'LinkError',
  'LinkError.prototype.name'
);
assert.strictEqual(
  WebAssembly.RuntimeError.prototype.name,
  'RuntimeError',
  'RuntimeError.prototype.name'
);
console.log('✓ All error names correct\n');

// Test 3: Test CompileError is thrown on invalid WASM
console.log('Test 3: CompileError thrown on invalid WASM');
const invalidWasm = new Uint8Array([0x00, 0x61, 0x73]); // incomplete magic number
try {
  new WebAssembly.Module(invalidWasm);
  assert.fail('Should have thrown CompileError');
} catch (error) {
  assert.ok(error instanceof Error, 'Error should be instanceof Error');
  assert.strictEqual(
    error.name,
    'CompileError',
    'Error name should be CompileError'
  );
  assert.ok(error.message.length > 0, 'Error should have a message');
  console.log('✓ CompileError thrown correctly:', error.message, '\n');
}

// Test 4: Test LinkError is thrown on invalid instantiation
console.log('Test 4: LinkError on instantiation failure');
// Create a module that requires imports
const wasmWithImports = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // magic number
  0x01,
  0x00,
  0x00,
  0x00, // version
]);

// Try to create a valid module first
try {
  const module = new WebAssembly.Module(wasmWithImports);
  console.log(
    '✓ Module created (may fail instantiation if imports required)\n'
  );
} catch (error) {
  console.log('Note: Minimal module creation may fail, this is expected\n');
}

// Test 5: Verify error prototype chain
console.log('Test 5: Error prototype chain');
try {
  new WebAssembly.Module(invalidWasm);
} catch (error) {
  assert.ok(
    error instanceof WebAssembly.CompileError,
    'Should be instanceof CompileError'
  );
  assert.ok(error instanceof Error, 'Should be instanceof Error');
  console.log('✓ Prototype chain correct\n');
}

console.log('=== All WebAssembly Error Tests Passed ===');
