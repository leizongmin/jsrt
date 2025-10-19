#!/usr/bin/env jsrt
/**
 * WebAssembly Imports Example
 *
 * This example demonstrates:
 * - Creating WASM modules that import JavaScript functions
 * - Providing importObject during instantiation
 * - Module.imports() static method
 * - LinkError when imports are missing
 *
 * Note: Uses a real WASM file from the WPT suite that imports a function
 */

console.log('=== WebAssembly Imports Example ===\n');

// Load a real WASM file that has imports
const fs = require('node:fs');
const wasmPath = 'test/resources/wasm-import-func.wasm';

console.log('Step 1: Load WASM file with function import');
const wasmBytes = fs.readFileSync(wasmPath);
console.log(`  ✓ Loaded ${wasmBytes.length} bytes from wasm-import-func.wasm`);

console.log('\nStep 2: Create module and inspect imports');
try {
  const module = new WebAssembly.Module(wasmBytes);
  console.log('  ✓ Module created successfully');

  // Use Module.imports() to inspect what this module needs
  const imports = WebAssembly.Module.imports(module);
  console.log(`  ✓ Module has ${imports.length} import(s):`);

  imports.forEach((imp) => {
    console.log(`    - Module: "${imp.module}"`);
    console.log(`      Name: "${imp.name}"`);
    console.log(`      Kind: ${imp.kind}`);
  });

  // Step 3: Try to instantiate without providing imports (will fail)
  console.log('\nStep 3: Try instantiation without imports');
  try {
    new WebAssembly.Instance(module, {});
    console.log('  ✗ Should have thrown LinkError');
  } catch (error) {
    console.log(`  ✓ Correctly threw ${error.name}: ${error.message}`);
    console.log(`  ✓ Is LinkError: ${error instanceof WebAssembly.LinkError}`);
  }

  // Step 4: Create proper importObject
  console.log('\nStep 4: Create importObject with required imports');

  // The WASM file imports from './wasm-import-func.js', need to provide it
  const importObject = {
    './wasm-import-func.js': {
      f: function () {
        console.log('  📞 WASM called JavaScript function f()');
        return 42;
      },
    },
  };

  console.log('  ✓ Created importObject with matching structure');

  // Step 5: Instantiate with correct imports
  console.log('\nStep 5: Instantiate with correct imports');
  const instance = new WebAssembly.Instance(module, importObject);
  console.log('  ✓ Instance created successfully');

  // Check exports
  const exports = WebAssembly.Module.exports(module);
  console.log(`  ✓ Module has ${exports.length} export(s)`);
  if (exports.length > 0) {
    console.log(`  ✓ Export names: ${exports.map((e) => e.name).join(', ')}`);
  }
} catch (error) {
  console.error(`  ✗ Error: ${error.message}`);
  console.error(`     Type: ${error.name}`);
  if (error.stack) {
    console.error(`     Stack: ${error.stack}`);
  }
}

// Test with minimal module (no imports)
console.log('\nStep 6: Compare with module that has no imports');
const minimalWasm = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // Magic
  0x01,
  0x00,
  0x00,
  0x00, // Version
]);

try {
  const module2 = new WebAssembly.Module(minimalWasm);
  const imports2 = WebAssembly.Module.imports(module2);
  console.log(`  ✓ Minimal module has ${imports2.length} imports`);

  // Can instantiate without importObject
  const instance2 = new WebAssembly.Instance(module2);
  console.log('  ✓ Instantiated without importObject (no imports needed)');
} catch (error) {
  console.error(`  ✗ Error: ${error.message}`);
}

console.log('\n=== Example Complete ===');
