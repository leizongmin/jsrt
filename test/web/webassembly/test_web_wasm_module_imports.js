// Test Module.imports() static method

// Helper to create simple WASM module with function import
// Use a real WASM file from WPT that we know works
function createModuleWithFunctionImport() {
  const fs = require('node:fs');
  const wasmPath = 'test/resources/wasm-import-func.wasm';
  const wasmBytes = fs.readFileSync(wasmPath);
  return new WebAssembly.Module(wasmBytes);
}

// Helper to create module with no imports
function createModuleNoImports() {
  // Minimal WASM module with no imports
  const wasmBytes = new Uint8Array([
    0x00,
    0x61,
    0x73,
    0x6d, // magic
    0x01,
    0x00,
    0x00,
    0x00, // version
  ]);
  return new WebAssembly.Module(wasmBytes);
}

// Test 1: Module.imports exists
console.log('Test 1: Module.imports static method exists');
if (typeof WebAssembly.Module.imports !== 'function') {
  throw new Error('WebAssembly.Module.imports should be a function');
}
console.log('  PASS: Module.imports is a function');

// Test 2: Basic functionality with function import
console.log('\nTest 2: Module.imports with function import');
try {
  const module1 = createModuleWithFunctionImport();
  const imports1 = WebAssembly.Module.imports(module1);

  if (!Array.isArray(imports1)) {
    throw new Error('imports should return an array');
  }
  console.log('  PASS: Returns array');

  if (imports1.length < 1) {
    throw new Error('should have at least 1 import');
  }
  console.log('  PASS: Has at least 1 import');

  if (imports1[0].module !== './wasm-import-func.js') {
    throw new Error(
      `import module should be './wasm-import-func.js', got '${imports1[0].module}'`
    );
  }
  console.log("  PASS: Import module is './wasm-import-func.js'");

  if (imports1[0].name !== 'f') {
    throw new Error(`import name should be 'f', got '${imports1[0].name}'`);
  }
  console.log("  PASS: Import name is 'f'");

  if (imports1[0].kind !== 'function') {
    throw new Error(
      `import kind should be 'function', got '${imports1[0].kind}'`
    );
  }
  console.log("  PASS: Import kind is 'function'");

  console.log(`  Import descriptors: ${JSON.stringify(imports1)}`);
} catch (e) {
  console.error('  FAIL:', e.message);
  throw e;
}

// Test 3: Module with no imports
console.log('\nTest 3: Module with no imports');
try {
  const module2 = createModuleNoImports();
  const imports2 = WebAssembly.Module.imports(module2);

  if (!Array.isArray(imports2)) {
    throw new Error('imports should return an array');
  }
  console.log('  PASS: Returns array');

  if (imports2.length !== 0) {
    throw new Error(`should have 0 imports, got ${imports2.length}`);
  }
  console.log('  PASS: Empty array for module with no imports');
} catch (e) {
  console.error('  FAIL:', e.message);
  throw e;
}

// Test 4: Invalid argument - not a Module
console.log('\nTest 4: Invalid argument error handling');
try {
  WebAssembly.Module.imports({});
  throw new Error('should throw TypeError for non-Module argument');
} catch (e) {
  if (!(e instanceof TypeError)) {
    throw new Error(`should be TypeError, got ${e.constructor.name}`);
  }
  console.log('  PASS: Correctly threw TypeError:', e.message);
}

// Test 5: Missing argument
console.log('\nTest 5: Missing argument');
try {
  WebAssembly.Module.imports();
  throw new Error('should throw TypeError for missing argument');
} catch (e) {
  if (!(e instanceof TypeError)) {
    throw new Error(`should be TypeError, got ${e.constructor.name}`);
  }
  console.log('  PASS: Correctly threw TypeError:', e.message);
}

// Test 6: null argument
console.log('\nTest 6: null argument');
try {
  WebAssembly.Module.imports(null);
  throw new Error('should throw TypeError for null argument');
} catch (e) {
  if (!(e instanceof TypeError)) {
    throw new Error(`should be TypeError, got ${e.constructor.name}`);
  }
  console.log('  PASS: Correctly threw TypeError:', e.message);
}

console.log('\n========================================');
console.log('All Module.imports tests passed!');
console.log('========================================');
