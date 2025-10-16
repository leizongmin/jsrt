// Test Module.exports() static method

// Helper to create simple WASM module with function export
function createModuleWithFunctionExport() {
  // Minimal WASM module with 1 function export
  // Magic + version + function type + function + export + code
  const wasmBytes = new Uint8Array([
    0x00,
    0x61,
    0x73,
    0x6d, // magic: \0asm
    0x01,
    0x00,
    0x00,
    0x00, // version: 1
    // Type section: 1 function type (no params, returns i32)
    0x01,
    0x05,
    0x01,
    0x60,
    0x00,
    0x01,
    0x7f,
    // Function section: 1 function with type 0
    0x03,
    0x02,
    0x01,
    0x00,
    // Export section: export "add42" as function index 0
    0x07,
    0x09,
    0x01,
    0x05,
    0x61,
    0x64,
    0x64,
    0x34,
    0x32,
    0x00,
    0x00,
    // Code section: function body
    0x0a,
    0x06,
    0x01,
    0x04,
    0x00,
    0x41,
    0x2a,
    0x0b, // i32.const 42, end
  ]);
  return new WebAssembly.Module(wasmBytes);
}

// Helper to create module with no exports
function createModuleNoExports() {
  // Minimal WASM module with no exports
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

// Test 1: Module.exports exists
console.log('Test 1: Module.exports static method exists');
if (typeof WebAssembly.Module.exports !== 'function') {
  throw new Error('WebAssembly.Module.exports should be a function');
}
console.log('  PASS: Module.exports is a function');

// Test 2: Basic functionality with function export
console.log('\nTest 2: Module.exports with function export');
try {
  const module1 = createModuleWithFunctionExport();
  const exports1 = WebAssembly.Module.exports(module1);

  if (!Array.isArray(exports1)) {
    throw new Error('exports should return an array');
  }
  console.log('  PASS: Returns array');

  if (exports1.length < 1) {
    throw new Error('should have at least 1 export');
  }
  console.log('  PASS: Has at least 1 export');

  if (exports1[0].name !== 'add42') {
    throw new Error(`export name should be 'add42', got '${exports1[0].name}'`);
  }
  console.log("  PASS: Export name is 'add42'");

  if (exports1[0].kind !== 'function') {
    throw new Error(
      `export kind should be 'function', got '${exports1[0].kind}'`
    );
  }
  console.log("  PASS: Export kind is 'function'");

  console.log(`  Export descriptors: ${JSON.stringify(exports1)}`);
} catch (e) {
  console.error('  FAIL:', e.message);
  throw e;
}

// Test 3: Module with no exports
console.log('\nTest 3: Module with no exports');
try {
  const module2 = createModuleNoExports();
  const exports2 = WebAssembly.Module.exports(module2);

  if (!Array.isArray(exports2)) {
    throw new Error('exports should return an array');
  }
  console.log('  PASS: Returns array');

  if (exports2.length !== 0) {
    throw new Error(`should have 0 exports, got ${exports2.length}`);
  }
  console.log('  PASS: Empty array for module with no exports');
} catch (e) {
  console.error('  FAIL:', e.message);
  throw e;
}

// Test 4: Invalid argument - not a Module
console.log('\nTest 4: Invalid argument error handling');
try {
  WebAssembly.Module.exports({});
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
  WebAssembly.Module.exports();
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
  WebAssembly.Module.exports(null);
  throw new Error('should throw TypeError for null argument');
} catch (e) {
  if (!(e instanceof TypeError)) {
    throw new Error(`should be TypeError, got ${e.constructor.name}`);
  }
  console.log('  PASS: Correctly threw TypeError:', e.message);
}

console.log('\n========================================');
console.log('All Module.exports tests passed!');
console.log('========================================');
