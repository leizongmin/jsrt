const assert = require('jsrt:assert');

console.log('=== FFI Struct Support Tests ===');

// Test struct support
try {
  const ffi = require('jsrt:ffi');

  console.log('Test 1: Check struct support availability');
  assert.strictEqual(
    typeof ffi.Struct,
    'function',
    'Struct should be available'
  );
  assert.strictEqual(
    typeof ffi.allocStruct,
    'function',
    'allocStruct should be available'
  );
  assert.strictEqual(
    ffi.types.struct,
    'struct',
    'Struct type should be available'
  );

  console.log('Test 2: Creating a simple struct');

  const pointStruct = ffi.Struct('Point', {
    x: 'int32',
    y: 'int32',
  });

  assert.strictEqual(
    typeof pointStruct,
    'object',
    'Struct should return an object'
  );
  assert.strictEqual(
    pointStruct.name,
    'Point',
    'Struct name should be correct'
  );
  assert.strictEqual(
    typeof pointStruct.size,
    'number',
    'Struct should have size'
  );
  assert.strictEqual(pointStruct.fieldCount, 2, 'Struct should have 2 fields');
  assert.strictEqual(
    typeof pointStruct.id,
    'number',
    'Struct should have an ID'
  );

  console.log('✓ Simple struct created successfully');
  console.log('Struct info:', pointStruct);

  console.log('Test 3: Creating a more complex struct');

  const personStruct = ffi.Struct('Person', {
    age: 'int32',
    height: 'float',
    weight: 'double',
    name_ptr: 'pointer',
  });

  assert.strictEqual(
    personStruct.name,
    'Person',
    'Complex struct name should be correct'
  );
  assert.strictEqual(
    personStruct.fieldCount,
    4,
    'Complex struct should have 4 fields'
  );
  assert.strictEqual(
    typeof personStruct.size,
    'number',
    'Complex struct should have size'
  );

  console.log('✓ Complex struct created successfully');
  console.log('Complex struct info:', personStruct);

  console.log('Test 4: Allocating struct memory');

  const pointPtr = ffi.allocStruct('Point');
  assert.strictEqual(
    typeof pointPtr,
    'number',
    'allocStruct should return a pointer'
  );
  assert.notStrictEqual(pointPtr, 0, 'Allocated pointer should not be null');

  console.log('✓ Struct memory allocated successfully');
  console.log('Point pointer:', pointPtr);

  const personPtr = ffi.allocStruct('Person');
  assert.strictEqual(
    typeof personPtr,
    'number',
    'Complex struct allocation should return a pointer'
  );
  assert.notStrictEqual(
    personPtr,
    0,
    'Complex allocated pointer should not be null'
  );

  console.log('✓ Complex struct memory allocated successfully');
  console.log('Person pointer:', personPtr);

  console.log('Test 5: Testing error handling - duplicate struct');

  try {
    ffi.Struct('Point', { z: 'int32' }); // Try to create Point again
    assert.fail('Should have thrown an error for duplicate struct name');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('Struct with this name already exists'),
      true
    );
    console.log('✓ Duplicate struct name error handling works');
  }

  console.log('Test 6: Testing error handling - invalid struct name');

  try {
    ffi.Struct(123, { x: 'int32' }); // Invalid name type
    assert.fail('Should have thrown an error for invalid struct name');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('Struct name must be a string'),
      true
    );
    console.log('✓ Invalid struct name error handling works');
  }

  console.log('Test 7: Testing error handling - invalid field definitions');

  try {
    ffi.Struct('TestStruct', 'not an object'); // Invalid field definitions
    assert.fail('Should have thrown an error for invalid field definitions');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('Field definitions must be an object'),
      true
    );
    console.log('✓ Invalid field definitions error handling works');
  }

  console.log(
    'Test 8: Testing error handling - non-existent struct allocation'
  );

  try {
    ffi.allocStruct('NonExistentStruct');
    assert.fail('Should have thrown an error for non-existent struct');
  } catch (error) {
    assert.strictEqual(error.message.includes('Struct not found'), true);
    console.log('✓ Non-existent struct error handling works');
  }

  console.log(
    'Test 9: Testing error handling - invalid struct allocation arguments'
  );

  try {
    ffi.allocStruct(); // No arguments
    assert.fail('Should have thrown an error for missing arguments');
  } catch (error) {
    assert.strictEqual(error.message.includes('Expected 1 argument'), true);
    console.log('✓ Missing arguments error handling works');
  }

  try {
    ffi.allocStruct(123); // Invalid name type
    assert.fail('Should have thrown an error for invalid struct name type');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('Struct name must be a string'),
      true
    );
    console.log('✓ Invalid struct name type error handling works');
  }

  console.log('Test 10: Testing various data types in structs');

  const allTypesStruct = ffi.Struct('AllTypes', {
    int_field: 'int',
    int32_field: 'int32',
    uint32_field: 'uint32',
    int64_field: 'int64',
    uint64_field: 'uint64',
    float_field: 'float',
    double_field: 'double',
    pointer_field: 'pointer',
    string_field: 'string',
  });

  assert.strictEqual(
    allTypesStruct.fieldCount,
    9,
    'All types struct should have 9 fields'
  );
  assert.strictEqual(
    typeof allTypesStruct.size,
    'number',
    'All types struct should have size'
  );

  console.log('✓ All data types struct created successfully');
  console.log('All types struct info:', allTypesStruct);

  // Test allocation for all types struct
  const allTypesPtr = ffi.allocStruct('AllTypes');
  assert.strictEqual(
    typeof allTypesPtr,
    'number',
    'All types struct allocation should work'
  );
  assert.notStrictEqual(
    allTypesPtr,
    0,
    'All types struct pointer should not be null'
  );

  console.log('✓ All types struct allocated successfully');

  // Clean up allocated memory
  if (pointPtr) ffi.free(pointPtr);
  if (personPtr) ffi.free(personPtr);
  if (allTypesPtr) ffi.free(allTypesPtr);

  console.log('✓ Memory cleanup completed');

  console.log('✓ All struct tests passed');
} catch (error) {
  console.error('❌ Struct test failed:', error.message);
  if (error.stack) {
    console.error('Stack trace:', error.stack);
  }
  throw error;
}

console.log('=== Struct Support Tests Completed ===');
