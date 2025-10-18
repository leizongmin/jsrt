#!/usr/bin/env jsrt
/**
 * WebAssembly Error Handling Example
 *
 * This example demonstrates:
 * - WebAssembly.CompileError - thrown on invalid bytecode
 * - WebAssembly.LinkError - thrown on instantiation failures
 * - WebAssembly.RuntimeError - thrown during execution
 * - Proper error types and inheritance
 * - Common error scenarios
 */

console.log('=== WebAssembly Error Handling Example ===\n');

// Valid minimal WASM module for reference
const validWasm = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // Magic
  0x01,
  0x00,
  0x00,
  0x00, // Version
]);

// === CompileError Examples ===
console.log('Example 1: WebAssembly.CompileError');

// Invalid magic number
try {
  const badMagic = new Uint8Array([
    0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00,
  ]);
  new WebAssembly.Module(badMagic);
  console.log('  ✗ Should have thrown');
} catch (error) {
  console.log(`  ✓ Caught ${error.name}: ${error.message}`);
  console.log(
    `  ✓ Is CompileError: ${error instanceof WebAssembly.CompileError}`
  );
}

// Invalid version
try {
  const badVersion = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0xff, 0x00, 0x00, 0x00,
  ]);
  new WebAssembly.Module(badVersion);
  console.log('  ✗ Should have thrown');
} catch (error) {
  console.log(`  ✓ Caught ${error.name}`);
}

// Truncated module
try {
  const truncated = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01]);
  new WebAssembly.Module(truncated);
  console.log('  ✗ Should have thrown');
} catch (error) {
  console.log(`  ✓ Caught ${error.name} (truncated module)`);
}

// === LinkError Examples ===
console.log('\nExample 2: WebAssembly.LinkError');

// Module with missing import
const wasmWithImport = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // Magic
  0x01,
  0x00,
  0x00,
  0x00, // Version
  // Type section
  0x01,
  0x05,
  0x01,
  0x60,
  0x01,
  0x7f,
  0x00,
  // Import section: imports "env.missing"
  0x02,
  0x0f,
  0x01,
  0x03,
  0x65,
  0x6e,
  0x76, // "env"
  0x07,
  0x6d,
  0x69,
  0x73,
  0x73,
  0x69,
  0x6e,
  0x67, // "missing"
  0x00,
  0x00,
]);

try {
  const module = new WebAssembly.Module(wasmWithImport);
  // Try to instantiate without providing the required import
  new WebAssembly.Instance(module, {});
  console.log('  ✗ Should have thrown LinkError');
} catch (error) {
  console.log(`  ✓ Caught ${error.name}: missing import`);
  console.log(`  ✓ Is LinkError: ${error instanceof WebAssembly.LinkError}`);
}

// Wrong import type
try {
  const module = new WebAssembly.Module(wasmWithImport);
  // Provide import with wrong type (should be function, giving number)
  new WebAssembly.Instance(module, { env: { missing: 42 } });
  console.log('  ✗ Should have thrown LinkError');
} catch (error) {
  console.log(`  ✓ Caught ${error.name}: wrong import type`);
}

// === TypeError Examples ===
console.log('\nExample 3: TypeError (not WebAssembly-specific)');

// Wrong argument type to Module constructor
try {
  new WebAssembly.Module('not a buffer');
  console.log('  ✗ Should have thrown TypeError');
} catch (error) {
  console.log(`  ✓ Caught ${error.name}: invalid argument type`);
  console.log(`  ✓ Is TypeError: ${error instanceof TypeError}`);
}

// Null argument
try {
  new WebAssembly.Module(null);
  console.log('  ✗ Should have thrown TypeError');
} catch (error) {
  console.log(`  ✓ Caught ${error.name}: null argument`);
}

// Missing required argument
try {
  new WebAssembly.Module();
  console.log('  ✗ Should have thrown TypeError');
} catch (error) {
  console.log(`  ✓ Caught ${error.name}: missing argument`);
}

// === Error Constructor Tests ===
console.log('\nExample 4: Error constructors');

// Create errors manually
const compileErr = new WebAssembly.CompileError('Custom compile error');
console.log(`  ✓ Created CompileError: ${compileErr.message}`);
console.log(`  ✓ Name: ${compileErr.name}`);

const linkErr = new WebAssembly.LinkError('Custom link error');
console.log(`  ✓ Created LinkError: ${linkErr.message}`);

const runtimeErr = new WebAssembly.RuntimeError('Custom runtime error');
console.log(`  ✓ Created RuntimeError: ${runtimeErr.message}`);

// Test inheritance
console.log('\nExample 5: Error inheritance');
console.log(
  `  ✓ CompileError instanceof Error: ${compileErr instanceof Error}`
);
console.log(`  ✓ LinkError instanceof Error: ${linkErr instanceof Error}`);
console.log(
  `  ✓ RuntimeError instanceof Error: ${runtimeErr instanceof Error}`
);

// === Validation vs Compilation ===
console.log('\nExample 6: validate() vs Module() errors');

const invalidBytes = new Uint8Array([0xff, 0xff, 0xff, 0xff]);

// validate() returns false, doesn't throw
const isValid = WebAssembly.validate(invalidBytes);
console.log(`  ✓ validate() returned: ${isValid} (no error thrown)`);

// Module() throws CompileError
try {
  new WebAssembly.Module(invalidBytes);
  console.log('  ✗ Should have thrown');
} catch (error) {
  console.log(`  ✓ Module() threw: ${error.name}`);
}

console.log('\n=== Example Complete ===');
