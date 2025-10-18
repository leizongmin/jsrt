#!/usr/bin/env jsrt
/**
 * Basic WebAssembly Module Example
 *
 * This example demonstrates:
 * - Creating a minimal valid WASM module
 * - Validating WASM bytecode
 * - Creating Module and Instance objects
 * - Basic error handling
 */

console.log('=== Basic WebAssembly Example ===\n');

// Step 1: Create a minimal valid WebAssembly module
// Structure: magic number (4 bytes) + version (4 bytes)
const wasmBytes = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // Magic number "\0asm"
  0x01,
  0x00,
  0x00,
  0x00, // Version 1
]);

console.log('Step 1: Validate WASM bytecode');
const isValid = WebAssembly.validate(wasmBytes);
console.log(`  ✓ WebAssembly.validate() returned: ${isValid}`);

// Step 2: Create a Module from bytecode
console.log('\nStep 2: Create WebAssembly.Module');
try {
  const module = new WebAssembly.Module(wasmBytes);
  console.log('  ✓ Module created successfully');
  console.log(`  ✓ Module type: ${Object.prototype.toString.call(module)}`);

  // Step 3: Instantiate the module
  console.log('\nStep 3: Create WebAssembly.Instance');
  const instance = new WebAssembly.Instance(module);
  console.log('  ✓ Instance created successfully');
  console.log(`  ✓ Exports: ${Object.keys(instance.exports).length} items`);
} catch (error) {
  console.error(`  ✗ Error: ${error.message}`);
}

// Step 4: Test error handling
console.log('\nStep 4: Test error handling');

// Invalid magic number
try {
  const invalid = new Uint8Array([0xff, 0xff, 0xff, 0xff]);
  new WebAssembly.Module(invalid);
  console.log('  ✗ Should have thrown error');
} catch (error) {
  console.log(`  ✓ Correctly rejected invalid magic: ${error.name}`);
}

// Invalid type (not ArrayBuffer or TypedArray)
try {
  new WebAssembly.Module('not a buffer');
  console.log('  ✗ Should have thrown error');
} catch (error) {
  console.log(`  ✓ Correctly rejected invalid type: ${error.name}`);
}

console.log('\n=== Example Complete ===');
