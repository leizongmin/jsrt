const assert = require('jsrt:assert');

console.log('=== Node.js events Module Tests ===');

// Test CommonJS import
const events = require('node:events');
const { EventEmitter } = events;
console.log('✓ CommonJS require("node:events") successful');

// Test EventEmitter constructor
console.log('\n🏗️ Testing EventEmitter constructor:');
const emitter = new EventEmitter();
assert.ok(emitter instanceof Object, 'EventEmitter should create an object');
assert.strictEqual(
  typeof emitter.on,
  'function',
  'EventEmitter should have on method'
);
assert.strictEqual(
  typeof emitter.emit,
  'function',
  'EventEmitter should have emit method'
);
console.log('✓ EventEmitter constructor works');

// Test basic event emission and listening
console.log('\n📡 Testing basic event emission:');
let testValue = null;
emitter.on('test', (value) => {
  testValue = value;
});

let emitResult = emitter.emit('test', 'hello world');
assert.strictEqual(
  testValue,
  'hello world',
  'Event listener should receive arguments'
);
assert.strictEqual(
  emitResult,
  true,
  'emit should return true when listeners exist'
);
console.log('✓ Basic event emission and listening works');

// Test multiple listeners
console.log('\n🔄 Testing multiple listeners:');
let counter = 0;
emitter.on('count', () => counter++);
emitter.on('count', () => counter++);
emitter.on('count', () => counter++);

emitter.emit('count');
assert.strictEqual(counter, 3, 'All listeners should be called');
console.log('✓ Multiple listeners work correctly');

// Test once() method
console.log('\n🎯 Testing once() method:');
let onceCounter = 0;
emitter.once('once-test', () => onceCounter++);

emitter.emit('once-test');
emitter.emit('once-test');
emitter.emit('once-test');

assert.strictEqual(
  onceCounter,
  1,
  'once() listener should only be called once'
);
console.log('✓ once() method works correctly');

// Test removeListener
console.log('\n🗑️ Testing removeListener:');
let removableCounter = 0;
function removableListener() {
  removableCounter++;
}

emitter.on('removable', removableListener);
emitter.emit('removable');
assert.strictEqual(
  removableCounter,
  1,
  'Listener should be called before removal'
);

emitter.removeListener('removable', removableListener);
emitter.emit('removable');
assert.strictEqual(
  removableCounter,
  1,
  'Listener should not be called after removal'
);
console.log('✓ removeListener works correctly');

// Test listenerCount
console.log('\n📊 Testing listenerCount:');
let count = emitter.listenerCount('count');
assert.strictEqual(count, 3, 'listenerCount should return correct count');

count = emitter.listenerCount('nonexistent');
assert.strictEqual(
  count,
  0,
  'listenerCount should return 0 for nonexistent events'
);
console.log('✓ listenerCount works correctly');

// Test removeAllListeners
console.log('\n🧹 Testing removeAllListeners:');
emitter.on('cleanup', () => {});
emitter.on('cleanup', () => {});

assert.strictEqual(
  emitter.listenerCount('cleanup'),
  2,
  'Should have 2 listeners before cleanup'
);

emitter.removeAllListeners('cleanup');
assert.strictEqual(
  emitter.listenerCount('cleanup'),
  0,
  'Should have 0 listeners after cleanup'
);
console.log('✓ removeAllListeners works correctly');

// Test addListener (alias for on)
console.log('\n🔗 Testing addListener alias:');
let aliasTest = false;
emitter.addListener('alias-test', () => {
  aliasTest = true;
});
emitter.emit('alias-test');
assert.strictEqual(aliasTest, true, 'addListener should work as alias for on');
console.log('✓ addListener alias works correctly');

// Test chaining
console.log('\n⛓️ Testing method chaining:');
let chainResult = emitter.on('chain1', () => {}).on('chain2', () => {});
assert.strictEqual(
  chainResult,
  emitter,
  'on() should return the emitter for chaining'
);
console.log('✓ Method chaining works correctly');

// Test emit with no listeners
console.log('\n🔇 Testing emit with no listeners:');
let noListenersResult = emitter.emit('no-listeners-event');
assert.strictEqual(
  noListenersResult,
  false,
  'emit should return false when no listeners exist'
);
console.log('✓ emit with no listeners works correctly');

// Test emit with multiple arguments
console.log('\n📦 Testing emit with multiple arguments:');
let arg1, arg2, arg3;
emitter.on('multi-args', (a, b, c) => {
  arg1 = a;
  arg2 = b;
  arg3 = c;
});

emitter.emit('multi-args', 'first', 42, { key: 'value' });
assert.strictEqual(arg1, 'first', 'First argument should be passed correctly');
assert.strictEqual(arg2, 42, 'Second argument should be passed correctly');
assert.deepEqual(arg3, { key: 'value' }, 'Third argument should be passed correctly');
console.log('✓ Multiple arguments work correctly');

console.log('\n✅ All events module tests passed!');
console.log('🎉 node:events module with EventEmitter is working correctly');
