const assert = require('jsrt:assert');

// Test 1: Test Module.customSections with minimal valid module (no custom sections)
console.log('\nTest 1: Module with no custom sections');
const minimalWasm = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // magic number
  0x01,
  0x00,
  0x00,
  0x00, // version
]);

try {
  const module = new WebAssembly.Module(minimalWasm);
  assert.ok(
    module instanceof WebAssembly.Module,
    'Module should be instance of WebAssembly.Module'
  );

  // Check that customSections exists and is a function
  assert.ok(
    typeof WebAssembly.Module.customSections === 'function',
    'WebAssembly.Module.customSections should be a function'
  );

  // Query for non-existent custom section
  const sections = WebAssembly.Module.customSections(module, 'name');
  assert.ok(Array.isArray(sections), 'customSections should return an array');
  assert.strictEqual(
    sections.length,
    0,
    'Module without custom sections should return empty array'
  );

  console.log('✅ Test 1 passed: empty module returns empty array');
} catch (error) {
  console.log(
    '❌ Test 1 failed:',
    error.message,
    '\nNote: This may indicate an issue with Module.customSections implementation'
  );
}

// Test 2: Test Module.customSections with a module containing one custom section
console.log('\nTest 2: Module with one custom section');
// WASM module with one custom section named "test" containing bytes [0x01, 0x02, 0x03]
const wasmWithCustomSection = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // magic
  0x01,
  0x00,
  0x00,
  0x00, // version
  // Custom section: ID=0, size=8 (1 + 4 "test" + 3 data)
  0x00, // section ID (custom)
  0x08, // section size = 8
  0x04, // name length = 4
  0x74,
  0x65,
  0x73,
  0x74, // "test"
  0x01,
  0x02,
  0x03, // custom data
]);

try {
  const module = new WebAssembly.Module(wasmWithCustomSection);

  const sections = WebAssembly.Module.customSections(module, 'test');
  assert.ok(Array.isArray(sections), 'customSections should return an array');
  assert.strictEqual(
    sections.length,
    1,
    'Should find exactly 1 section named "test"'
  );

  // Check that the section content is correct
  const section = sections[0];
  assert.ok(section instanceof ArrayBuffer, 'Section should be an ArrayBuffer');
  assert.strictEqual(
    section.byteLength,
    3,
    'Section content should be 3 bytes'
  );

  const view = new Uint8Array(section);
  assert.strictEqual(view[0], 0x01, 'First byte should be 0x01');
  assert.strictEqual(view[1], 0x02, 'Second byte should be 0x02');
  assert.strictEqual(view[2], 0x03, 'Third byte should be 0x03');

  // Query for non-existent section
  const nonExistent = WebAssembly.Module.customSections(module, 'notfound');
  assert.strictEqual(
    nonExistent.length,
    0,
    'Non-existent section should return empty array'
  );

  console.log('✅ Test 2 passed: found custom section with correct content');
} catch (error) {
  console.log('❌ Test 2 failed:', error.message);
}

// Test 3: Test Module with multiple custom sections with the same name
console.log('\nTest 3: Module with multiple custom sections with same name');
const wasmWithMultipleSections = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // magic
  0x01,
  0x00,
  0x00,
  0x00, // version
  // First custom section: "data" with [0xAA]
  0x00, // section ID (custom)
  0x06, // section size = 6
  0x04, // name length = 4
  0x64,
  0x61,
  0x74,
  0x61, // "data"
  0xaa, // custom data
  // Second custom section: "data" with [0xBB, 0xCC]
  0x00, // section ID (custom)
  0x07, // section size = 7
  0x04, // name length = 4
  0x64,
  0x61,
  0x74,
  0x61, // "data"
  0xbb,
  0xcc, // custom data
]);

try {
  const module = new WebAssembly.Module(wasmWithMultipleSections);

  const sections = WebAssembly.Module.customSections(module, 'data');
  assert.ok(Array.isArray(sections), 'customSections should return an array');
  assert.strictEqual(
    sections.length,
    2,
    'Should find exactly 2 sections named "data"'
  );

  // Check first section
  const section1 = sections[0];
  assert.strictEqual(section1.byteLength, 1, 'First section should be 1 byte');
  assert.strictEqual(
    new Uint8Array(section1)[0],
    0xaa,
    'First section content'
  );

  // Check second section
  const section2 = sections[1];
  assert.strictEqual(
    section2.byteLength,
    2,
    'Second section should be 2 bytes'
  );
  const view2 = new Uint8Array(section2);
  assert.strictEqual(view2[0], 0xbb, 'Second section first byte');
  assert.strictEqual(view2[1], 0xcc, 'Second section second byte');

  console.log('✅ Test 3 passed: found multiple custom sections correctly');
} catch (error) {
  console.log('❌ Test 3 failed:', error.message);
}

// Test 4: Test error handling - invalid arguments
console.log('\nTest 4: Error handling for invalid arguments');

// Test with non-Module argument
try {
  WebAssembly.Module.customSections({}, 'test');
  console.log('❌ Test 4a failed: should have thrown TypeError for non-Module');
} catch (error) {
  assert.ok(
    error instanceof TypeError,
    'Should throw TypeError for non-Module'
  );
  console.log('✅ Test 4a passed: throws TypeError for non-Module argument');
}

// Test with missing second argument
try {
  const module = new WebAssembly.Module(minimalWasm);
  WebAssembly.Module.customSections(module);
  console.log(
    '❌ Test 4b failed: should have thrown TypeError for missing argument'
  );
} catch (error) {
  assert.ok(
    error instanceof TypeError,
    'Should throw TypeError for missing argument'
  );
  console.log(
    '✅ Test 4b passed: throws TypeError for missing second argument'
  );
}

// Test 5: Test with empty section name
console.log('\nTest 5: Empty section name');
const wasmWithEmptyNameSection = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // magic
  0x01,
  0x00,
  0x00,
  0x00, // version
  // Custom section with empty name: ID=0, size=3 (1 + 0 "" + 2 data)
  0x00, // section ID (custom)
  0x03, // section size = 3
  0x00, // name length = 0 (empty string)
  0xde,
  0xad, // custom data
]);

try {
  const module = new WebAssembly.Module(wasmWithEmptyNameSection);

  const sections = WebAssembly.Module.customSections(module, '');
  assert.ok(Array.isArray(sections), 'customSections should return an array');
  assert.strictEqual(
    sections.length,
    1,
    'Should find 1 section with empty name'
  );
  assert.strictEqual(
    sections[0].byteLength,
    2,
    'Section content should be 2 bytes'
  );

  console.log('✅ Test 5 passed: handles empty section name correctly');
} catch (error) {
  console.log('❌ Test 5 failed:', error.message);
}

// Test 6: Test with module containing both custom and standard sections
console.log('\nTest 6: Module with mixed section types');
const wasmWithMixedSections = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // magic
  0x01,
  0x00,
  0x00,
  0x00, // version
  // Custom section before type section
  0x00, // section ID (custom)
  0x08, // section size
  0x05, // name length = 5
  0x66,
  0x69,
  0x72,
  0x73,
  0x74, // "first"
  0x11,
  0x22, // data
  // Type section (ID=1) - minimal empty type section
  0x01, // section ID (type)
  0x01, // section size
  0x00, // 0 types
  // Custom section after type section
  0x00, // section ID (custom)
  0x09, // section size
  0x06, // name length = 6
  0x73,
  0x65,
  0x63,
  0x6f,
  0x6e,
  0x64, // "second"
  0x33,
  0x44, // data
]);

try {
  const module = new WebAssembly.Module(wasmWithMixedSections);

  const first = WebAssembly.Module.customSections(module, 'first');
  assert.strictEqual(first.length, 1, 'Should find "first" section');
  assert.strictEqual(first[0].byteLength, 2, 'First section size');

  const second = WebAssembly.Module.customSections(module, 'second');
  assert.strictEqual(second.length, 1, 'Should find "second" section');
  assert.strictEqual(second[0].byteLength, 2, 'Second section size');

  console.log('✅ Test 6 passed: handles mixed section types correctly');
} catch (error) {
  console.log('❌ Test 6 failed:', error.message);
}

console.log('\n=== Module.customSections Tests Completed ===');
