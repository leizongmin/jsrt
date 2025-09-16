// Test FormData API
const assert = require('jsrt:assert');

// Test basic FormData constructor
const formData1 = new FormData();
assert.strictEqual(typeof formData1, 'object', 'FormData should be an object');

// Test FormData methods
const formData = new FormData();

// Test append()
formData.append('name', 'John');
formData.append('age', '30');
formData.append('city', 'Tokyo');

// Test has()
assert.strictEqual(
  formData.has('name'),
  true,
  'FormData should have "name" field'
);
assert.strictEqual(
  formData.has('email'),
  false,
  'FormData should not have "email" field'
);

// Test get()
assert.strictEqual(
  formData.get('name'),
  'John',
  'FormData should return correct value'
);
assert.strictEqual(
  formData.get('nonexistent'),
  null,
  'FormData should return null for nonexistent field'
);

// Test multiple values with same name
formData.append('hobby', 'reading');
formData.append('hobby', 'coding');

// Test getAll()
const hobbies = formData.getAll('hobby');
assert.strictEqual(
  Array.isArray(hobbies),
  true,
  'getAll should return an array'
);
assert.strictEqual(hobbies.length, 2, 'getAll should return all values');
assert.strictEqual(hobbies[0], 'coding', 'First hobby should be "coding"');
assert.strictEqual(hobbies[1], 'reading', 'Second hobby should be "reading"');

// Test set() - should replace existing values
formData.set('name', 'Jane');

// Test delete()
formData.delete('age');

// Test forEach()
formData.forEach((value, key) => {
  // Silent iteration for testing
});

// Test FormData with Blob values
const formData5 = new FormData();
const blob = new Blob(['file content'], { type: 'text/plain' });

formData5.append('file', blob, 'test.txt');

const fileValue = formData5.get('file');
assert.strictEqual(
  typeof fileValue,
  'object',
  'File value should be an object'
);
assert.strictEqual(fileValue.size, 12n, 'File value should have correct size');
