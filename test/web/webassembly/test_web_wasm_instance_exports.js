// Test Instance.exports property and function wrapping

// Helper to create simple WASM module with function export
function createModuleWithFunctionExport() {
  // Minimal WASM module with 1 function export
  // Function returns i32 constant 42
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

// Helper to create module with add function (i32 param, returns i32)
function createModuleWithAddFunction() {
  // WASM module with function: (i32) -> i32, returns param + 10
  const wasmBytes = new Uint8Array([
    0x00,
    0x61,
    0x73,
    0x6d, // magic
    0x01,
    0x00,
    0x00,
    0x00, // version
    // Type section: (i32) -> i32
    0x01,
    0x06,
    0x01,
    0x60,
    0x01,
    0x7f,
    0x01,
    0x7f,
    // Function section
    0x03,
    0x02,
    0x01,
    0x00,
    // Export section: "addTen"
    0x07,
    0x0a,
    0x01,
    0x06,
    0x61,
    0x64,
    0x64,
    0x54,
    0x65,
    0x6e,
    0x00,
    0x00,
    // Code section: local.get 0, i32.const 10, i32.add, end
    0x0a,
    0x09,
    0x01,
    0x07,
    0x00,
    0x20,
    0x00,
    0x41,
    0x0a,
    0x6a,
    0x0b,
  ]);
  return new WebAssembly.Module(wasmBytes);
}

console.log('========================================');
console.log('WebAssembly Instance.exports Tests');
console.log('========================================');

// Test 1: Instance.exports property exists
console.log('\nTest 1: Instance.exports property exists');
try {
  const module1 = createModuleWithFunctionExport();
  const instance1 = new WebAssembly.Instance(module1);

  if (!instance1.hasOwnProperty('exports')) {
    throw new Error('Instance should have exports property');
  }
  console.log('  PASS: exports property exists');

  if (typeof instance1.exports !== 'object') {
    throw new Error('exports should be an object');
  }
  console.log('  PASS: exports is an object');
} catch (e) {
  console.error('  FAIL:', e.message);
  throw e;
}

// Test 2: Exports contains exported function
console.log('\nTest 2: Exports contains exported function');
try {
  const module2 = createModuleWithFunctionExport();
  const instance2 = new WebAssembly.Instance(module2);

  if (!instance2.exports.add42) {
    throw new Error("exports should have 'add42' function");
  }
  console.log("  PASS: exports has 'add42' property");

  if (typeof instance2.exports.add42 !== 'function') {
    throw new Error('exports.add42 should be a function');
  }
  console.log('  PASS: add42 is a function');
} catch (e) {
  console.error('  FAIL:', e.message);
  throw e;
}

// Test 3: Call exported function (no params)
console.log('\nTest 3: Call exported function (no params)');
try {
  const module3 = createModuleWithFunctionExport();
  const instance3 = new WebAssembly.Instance(module3);

  const result = instance3.exports.add42();
  console.log('  Result:', result);

  if (result !== 42) {
    throw new Error(`Expected 42, got ${result}`);
  }
  console.log('  PASS: Function returned correct value (42)');
} catch (e) {
  console.error('  FAIL:', e.message);
  throw e;
}

// Test 4: Call exported function with parameters
console.log('\nTest 4: Call exported function with parameters');
try {
  const module4 = createModuleWithAddFunction();
  const instance4 = new WebAssembly.Instance(module4);

  const result1 = instance4.exports.addTen(5);
  console.log('  addTen(5) =', result1);

  if (result1 !== 15) {
    throw new Error(`Expected 15, got ${result1}`);
  }
  console.log('  PASS: addTen(5) = 15');

  const result2 = instance4.exports.addTen(100);
  console.log('  addTen(100) =', result2);

  if (result2 !== 110) {
    throw new Error(`Expected 110, got ${result2}`);
  }
  console.log('  PASS: addTen(100) = 110');
} catch (e) {
  console.error('  FAIL:', e.message);
  throw e;
}

// Test 5: Exports object is frozen
console.log('\nTest 5: Exports object is frozen');
try {
  const module5 = createModuleWithFunctionExport();
  const instance5 = new WebAssembly.Instance(module5);

  if (!Object.isFrozen(instance5.exports)) {
    throw new Error('exports object should be frozen');
  }
  console.log('  PASS: exports object is frozen');

  // Try to modify it
  try {
    instance5.exports.newProp = 123;
    if (instance5.exports.newProp === 123) {
      throw new Error('Should not be able to add property to frozen object');
    }
  } catch (e) {
    // In strict mode this would throw, in non-strict it silently fails
    console.log('  PASS: Cannot modify frozen exports object');
  }
} catch (e) {
  console.error('  FAIL:', e.message);
  throw e;
}

// Test 6: Same exports object returned on multiple access
console.log('\nTest 6: Same exports object returned on multiple access');
try {
  const module6 = createModuleWithFunctionExport();
  const instance6 = new WebAssembly.Instance(module6);

  const exports1 = instance6.exports;
  const exports2 = instance6.exports;

  if (exports1 !== exports2) {
    throw new Error('Should return same exports object');
  }
  console.log('  PASS: Same exports object returned');
} catch (e) {
  console.error('  FAIL:', e.message);
  throw e;
}

console.log('\n========================================');
console.log('All Instance.exports tests passed!');
console.log('========================================');
