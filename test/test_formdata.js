// Test FormData API
const assert = require('std:assert');
console.log('Testing FormData API...');

// Test basic FormData constructor
const formData1 = new FormData();
console.log('✓ FormData constructor works');
assert.strictEqual(typeof formData1, 'object', 'FormData should be an object');

// Test FormData methods
const formData = new FormData();

// Test append()
formData.append('name', 'John');
formData.append('age', '30');
formData.append('city', 'Tokyo');
console.log('✓ FormData.append() works');

// Test has()
console.log("Has 'name':", formData.has('name'));
assert.strictEqual(
  formData.has('name'),
  true,
  'FormData should have "name" field'
);
console.log("Has 'email':", formData.has('email'));
assert.strictEqual(
  formData.has('email'),
  false,
  'FormData should not have "email" field'
);
console.log('✓ FormData.has() works');

// Test get()
console.log("Get 'name':", formData.get('name'));
assert.strictEqual(
  formData.get('name'),
  'John',
  'FormData should return correct value'
);
console.log("Get 'nonexistent':", formData.get('nonexistent'));
assert.strictEqual(
  formData.get('nonexistent'),
  null,
  'FormData should return null for nonexistent field'
);
console.log('✓ FormData.get() works');

// Test multiple values with same name
formData.append('hobby', 'reading');
formData.append('hobby', 'coding');

// Test getAll()
const hobbies = formData.getAll('hobby');
console.log('All hobbies:', hobbies);
assert.strictEqual(
  Array.isArray(hobbies),
  true,
  'getAll should return an array'
);
assert.strictEqual(hobbies.length, 2, 'getAll should return all values');
assert.strictEqual(hobbies[0], 'coding', 'First hobby should be "coding"');
assert.strictEqual(hobbies[1], 'reading', 'Second hobby should be "reading"');
console.log('✓ FormData.getAll() works');

// Test set() - should replace existing values
formData.set('name', 'Jane');
console.log('Name after set:', formData.get('name'));
console.log('✓ FormData.set() works');

// Test delete()
formData.delete('age');
console.log("Has 'age' after delete:", formData.has('age'));
console.log('✓ FormData.delete() works');

// Test forEach()
console.log('FormData contents via forEach:');
formData.forEach((value, key) => {
  console.log(`  ${key}: ${value}`);
});
console.log('✓ FormData.forEach() works');

// Test FormData with Blob values
const formData5 = new FormData();
const blob = new Blob(['file content'], { type: 'text/plain' });

formData5.append('file', blob, 'test.txt');
console.log('✓ FormData with Blob works');

const fileValue = formData5.get('file');
console.log('File value type:', typeof fileValue);
assert.strictEqual(
  typeof fileValue,
  'object',
  'File value should be an object'
);
console.log('File value size:', fileValue.size);
assert.strictEqual(fileValue.size, 12n, 'File value should have correct size');
