// Test structuredClone implementation
const assert = require('std:assert');
console.log('=== Structured Clone API Tests ===');

// Test 1: Primitive values
console.log('Test 1: Primitive values');
const clonedNumber = structuredClone(42);
console.log('Clone number:', clonedNumber);
assert.strictEqual(clonedNumber, 42, 'Cloned number should equal original');
const clonedString = structuredClone('hello');
console.log('Clone string:', clonedString);
assert.strictEqual(
  clonedString,
  'hello',
  'Cloned string should equal original'
);
const clonedBoolean = structuredClone(true);
console.log('Clone boolean:', clonedBoolean);
assert.strictEqual(clonedBoolean, true, 'Cloned boolean should equal original');
const clonedNull = structuredClone(null);
console.log('Clone null:', clonedNull);
assert.strictEqual(clonedNull, null, 'Cloned null should equal original');
const clonedUndefined = structuredClone(undefined);
console.log('Clone undefined:', clonedUndefined);
assert.strictEqual(
  clonedUndefined,
  undefined,
  'Cloned undefined should equal original'
);

// Test 2: Simple objects
console.log('\nTest 2: Simple objects');
const obj1 = { a: 1, b: 'hello', c: true };
const cloned_obj1 = structuredClone(obj1);
console.log('Original object:', JSON.stringify(obj1));
console.log('Cloned object:', JSON.stringify(cloned_obj1));
console.log('Are different objects:', obj1 !== cloned_obj1);
assert(obj1 !== cloned_obj1, 'Cloned object should be a different reference');
console.log(
  'Have same content:',
  JSON.stringify(obj1) === JSON.stringify(cloned_obj1)
);
assert.deepEqual(
  cloned_obj1,
  obj1,
  'Cloned object should have same content as original'
);

// Test 3: Nested objects
console.log('\nTest 3: Nested objects');
const nested = {
  user: { name: 'John', age: 30 },
  settings: { theme: 'dark', lang: 'en' },
};
const cloned_nested = structuredClone(nested);
console.log('Original nested:', JSON.stringify(nested));
console.log('Cloned nested:', JSON.stringify(cloned_nested));
console.log(
  'Nested objects are different:',
  nested.user !== cloned_nested.user
);
assert(
  nested.user !== cloned_nested.user,
  'Nested objects should be different references'
);
assert.deepEqual(
  cloned_nested,
  nested,
  'Cloned nested object should have same content'
);

// Modify original to verify deep clone
nested.user.name = 'Jane';
console.log('After modifying original:', JSON.stringify(nested));
console.log('Cloned remains unchanged:', JSON.stringify(cloned_nested));

// Test 4: Arrays
console.log('\nTest 4: Arrays');
const arr1 = [1, 2, 3, 'hello', true];
const cloned_arr1 = structuredClone(arr1);
console.log('Original array:', JSON.stringify(arr1));
console.log('Cloned array:', JSON.stringify(cloned_arr1));
console.log('Are different arrays:', arr1 !== cloned_arr1);

// Test 5: Nested arrays
console.log('\nTest 5: Nested arrays');
const nested_arr = [
  [1, 2],
  [3, 4],
  ['a', 'b'],
];
const cloned_nested_arr = structuredClone(nested_arr);
console.log('Original nested array:', JSON.stringify(nested_arr));
console.log('Cloned nested array:', JSON.stringify(cloned_nested_arr));
nested_arr[0][0] = 999;
console.log('After modifying original:', JSON.stringify(nested_arr));
console.log('Cloned remains unchanged:', JSON.stringify(cloned_nested_arr));

// Test 6: Mixed objects and arrays
console.log('\nTest 6: Mixed objects and arrays');
const mixed = {
  numbers: [1, 2, 3],
  person: { name: 'Alice', hobbies: ['reading', 'swimming'] },
};
const cloned_mixed = structuredClone(mixed);
console.log('Original mixed:', JSON.stringify(mixed));
console.log('Cloned mixed:', JSON.stringify(cloned_mixed));

// Test 7: Date objects
console.log('\nTest 7: Date objects');
const date = new Date('2023-01-01');
const cloned_date = structuredClone(date);
console.log('Original date:', date.toISOString());
console.log('Cloned date:', cloned_date.toISOString());
console.log('Are different Date objects:', date !== cloned_date);
console.log('Have same time value:', date.getTime() === cloned_date.getTime());

// Test 8: RegExp objects
console.log('\nTest 8: RegExp objects');
const regex = /hello/gi;
const cloned_regex = structuredClone(regex);
console.log('Original regex:', regex.toString());
console.log('Cloned regex:', cloned_regex.toString());
console.log('Are different RegExp objects:', regex !== cloned_regex);
console.log('Have same source:', regex.source === cloned_regex.source);
console.log('Have same flags:', regex.flags === cloned_regex.flags);

// Test 9: Circular references
console.log('\nTest 9: Circular references');
const circular = { name: 'circular' };
circular.self = circular;

try {
  const cloned_circular = structuredClone(circular);
  console.log('Cloned circular object successfully');
  console.log('Cloned has name:', cloned_circular.name);
  console.log('Cloned has self reference:', cloned_circular.self !== undefined);
  console.log(
    'Self reference points to same object:',
    cloned_circular.self === cloned_circular
  );
  console.log('But different from original:', cloned_circular !== circular);
} catch (e) {
  console.log('Error cloning circular reference:', e.message);
}

// Test 10: Complex circular reference
console.log('\nTest 10: Complex circular reference');
const parent = { name: 'parent', children: [] };
const child1 = { name: 'child1', parent: parent };
const child2 = { name: 'child2', parent: parent };
parent.children.push(child1, child2);

try {
  const cloned_parent = structuredClone(parent);
  console.log('Cloned complex circular structure successfully');
  console.log('Parent name:', cloned_parent.name);
  console.log('Children count:', cloned_parent.children.length);
  console.log(
    'Child1 parent reference points to parent:',
    cloned_parent.children[0].parent === cloned_parent
  );
  console.log(
    'Child2 parent reference points to parent:',
    cloned_parent.children[1].parent === cloned_parent
  );
} catch (e) {
  console.log('Error cloning complex circular reference:', e.message);
}

// Test 11: Object with array containing circular reference
console.log('\nTest 11: Object with array containing circular reference');
const container = { items: [] };
container.items.push(container);

try {
  const cloned_container = structuredClone(container);
  console.log('Cloned container with circular array successfully');
  console.log('Items length:', cloned_container.items.length);
  console.log(
    'First item is container itself:',
    cloned_container.items[0] === cloned_container
  );
} catch (e) {
  console.log('Error cloning container with circular array:', e.message);
}

console.log('\n=== All Structured Clone tests completed ===');
