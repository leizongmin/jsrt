#!/usr/bin/env jsrt
/**
 * WebAssembly Exports Example
 *
 * This example demonstrates:
 * - Creating modules with exported functions
 * - Accessing and calling exported functions
 * - Module.exports() static method
 * - Type conversion between WASM and JS
 */

console.log('=== WebAssembly Exports Example ===\n');

// WASM module that exports a simple function: (i32, i32) -> i32
// The function adds two i32 values
const wasmBytesWithExport = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // Magic number
  0x01,
  0x00,
  0x00,
  0x00, // Version 1

  // Type section: (i32, i32) -> i32
  0x01, // Section ID: Type
  0x07, // Section size
  0x01, // 1 type
  0x60, // Function type
  0x02,
  0x7f,
  0x7f, // 2 params: i32, i32
  0x01,
  0x7f, // 1 result: i32

  // Function section
  0x03, // Section ID: Function
  0x02, // Section size
  0x01, // 1 function
  0x00, // Type index 0

  // Export section: export "add"
  0x07, // Section ID: Export
  0x07, // Section size
  0x01, // 1 export
  0x03,
  0x61,
  0x64,
  0x64, // "add" (length 3 + chars)
  0x00, // Export kind: function
  0x00, // Function index 0

  // Code section: implementation of add(a, b) { return a + b; }
  0x0a, // Section ID: Code
  0x09, // Section size
  0x01, // 1 function body
  0x07, // Body size
  0x00, // Local declaration count
  0x20,
  0x00, // local.get 0 (first param)
  0x20,
  0x01, // local.get 1 (second param)
  0x6a, // i32.add
  0x0b, // end
]);

console.log('Step 1: Inspect module exports before instantiation');

try {
  const module = new WebAssembly.Module(wasmBytesWithExport);
  console.log('  ✓ Module created successfully');

  // Use Module.exports() to inspect exports
  const exports = WebAssembly.Module.exports(module);
  console.log(`  ✓ Module has ${exports.length} export(s):`);

  exports.forEach((exp) => {
    console.log(`    - Name: "${exp.name}"`);
    console.log(`      Kind: ${exp.kind}`);
  });

  console.log('\nStep 2: Instantiate and access exports');
  const instance = new WebAssembly.Instance(module);
  console.log('  ✓ Instance created successfully');

  // Access the exports object
  console.log(`  ✓ instance.exports type: ${typeof instance.exports}`);
  console.log(`  ✓ instance.exports.add type: ${typeof instance.exports.add}`);

  console.log('\nStep 3: Call exported function');

  // Call the exported add function
  const result1 = instance.exports.add(5, 7);
  console.log(`  ✓ add(5, 7) = ${result1}`);

  const result2 = instance.exports.add(100, 42);
  console.log(`  ✓ add(100, 42) = ${result2}`);

  const result3 = instance.exports.add(-10, 10);
  console.log(`  ✓ add(-10, 10) = ${result3}`);

  console.log('\nStep 4: Test type coercion');

  // JavaScript numbers are automatically converted to i32
  const result4 = instance.exports.add(3.7, 2.9);
  console.log(`  ✓ add(3.7, 2.9) = ${result4} (floats truncated to i32)`);

  // String to number conversion
  const result5 = instance.exports.add('15', '25');
  console.log(`  ✓ add("15", "25") = ${result5} (strings converted)`);

  console.log('\nStep 5: Check exports object properties');

  // Exports object is extensible and non-sealed
  const exportKeys = Object.keys(instance.exports);
  console.log(`  ✓ Export names: [${exportKeys.join(', ')}]`);

  // Can add properties (but shouldn't affect WASM)
  instance.exports.customProp = 'test';
  console.log(`  ✓ Can add custom properties: ${instance.exports.customProp}`);

  // Original exports still work
  console.log(
    `  ✓ Original exports still work: add(1, 1) = ${instance.exports.add(1, 1)}`
  );
} catch (error) {
  console.error(`  ✗ Error: ${error.message}`);
  console.error(`     Type: ${error.name}`);
}

console.log('\n=== Example Complete ===');
