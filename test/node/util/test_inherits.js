// Test file for util.inherits()
const util = require('node:util');
const assert = require('jsrt:assert');

console.log('Testing util.inherits()\n');

// Test 1: Basic inheritance
console.log('=== Test 1: Basic inheritance ===');
function Animal(name) {
  this.name = name;
}
Animal.prototype.speak = function () {
  return this.name + ' makes a sound';
};

function Dog(name, breed) {
  Animal.call(this, name);
  this.breed = breed;
}
util.inherits(Dog, Animal);

const dog = new Dog('Rex', 'Labrador');
assert.strictEqual(dog.name, 'Rex', 'Constructor property set');
assert.strictEqual(dog.breed, 'Labrador', 'Child property set');
assert.strictEqual(dog.speak(), 'Rex makes a sound', 'Inherited method works');
console.log('✓ Basic inheritance works');

// Test 2: instanceof check
console.log('\n=== Test 2: instanceof check ===');
assert.strictEqual(dog instanceof Dog, true, 'instanceof Dog');
assert.strictEqual(dog instanceof Animal, true, 'instanceof Animal');
console.log('✓ instanceof checks work');

// Test 3: super_ property
console.log('\n=== Test 3: super_ property ===');
assert.strictEqual(Dog.super_, Animal, 'super_ property set correctly');
console.log('✓ super_ property works');

// Test 4: Method overriding
console.log('\n=== Test 4: Method overriding ===');
Dog.prototype.speak = function () {
  return this.name + ' barks';
};
assert.strictEqual(dog.speak(), 'Rex barks', 'Overridden method works');
console.log('✓ Method overriding works');

// Test 5: Multiple inheritance chain
console.log('\n=== Test 5: Multiple inheritance chain ===');
function Puppy(name, breed, age) {
  Dog.call(this, name, breed);
  this.age = age;
}
util.inherits(Puppy, Dog);

const puppy = new Puppy('Max', 'Beagle', 1);
assert.strictEqual(puppy instanceof Puppy, true, 'instanceof Puppy');
assert.strictEqual(puppy instanceof Dog, true, 'instanceof Dog');
assert.strictEqual(puppy instanceof Animal, true, 'instanceof Animal');
assert.strictEqual(Puppy.super_, Dog, 'Puppy.super_ is Dog');
console.log('✓ Multiple inheritance chain works');

console.log('\n' + '='.repeat(60));
console.log('✅ All inherits tests passed!');
console.log('='.repeat(60));
