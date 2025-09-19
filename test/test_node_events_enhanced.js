const assert = require('jsrt:assert');

console.log('=== Node.js Events Enhanced API Tests ===');

const events = require('node:events');
const { EventEmitter } = events;

// Test EventEmitter constructor
console.log('\n🏗️ Testing enhanced EventEmitter features:');
const emitter = new EventEmitter();

// Test prependListener
console.log('\n⬆️ Testing prependListener:');
let callOrder = [];
emitter.on('prepend-test', () => callOrder.push('on'));
emitter.prependListener('prepend-test', () => callOrder.push('prepend'));
emitter.emit('prepend-test');
assert.deepEqual(callOrder, ['prepend', 'on'], 'prependListener should be called before on listeners');

// Test prependOnceListener
console.log('\n⬆️📊 Testing prependOnceListener:');
let onceCallOrder = [];
emitter.on('prepend-once-test', () => onceCallOrder.push('on'));
emitter.prependOnceListener('prepend-once-test', () => onceCallOrder.push('prepend-once'));
emitter.emit('prepend-once-test');
emitter.emit('prepend-once-test'); // Should not call prepend-once again
assert.deepEqual(onceCallOrder, ['prepend-once', 'on', 'on'], 'prependOnceListener should only be called once');

// Test eventNames
console.log('\n📝 Testing eventNames:');
const newEmitter = new EventEmitter();
newEmitter.on('event1', () => {});
newEmitter.on('event2', () => {});
const eventNames = newEmitter.eventNames();
assert.ok(Array.isArray(eventNames), 'eventNames should return an array');
assert.ok(eventNames.includes('event1'), 'eventNames should include event1');
assert.ok(eventNames.includes('event2'), 'eventNames should include event2');

// Test listeners
console.log('\n👥 Testing listeners:');
const listenersEmitter = new EventEmitter();
const listener1 = () => {};
const listener2 = () => {};
listenersEmitter.on('test-listeners', listener1);
listenersEmitter.on('test-listeners', listener2);
const listeners = listenersEmitter.listeners('test-listeners');
assert.strictEqual(listeners.length, 2, 'listeners should return array with 2 listeners');
assert.strictEqual(listeners[0], listener1, 'first listener should match');
assert.strictEqual(listeners[1], listener2, 'second listener should match');

// Test rawListeners (same as listeners for now)
console.log('\n👥🔍 Testing rawListeners:');
const rawListeners = listenersEmitter.rawListeners('test-listeners');
assert.strictEqual(rawListeners.length, 2, 'rawListeners should return array with 2 listeners');
assert.strictEqual(rawListeners[0], listener1, 'first raw listener should match');
assert.strictEqual(rawListeners[1], listener2, 'second raw listener should match');

// Test off (alias for removeListener)
console.log('\n🔇 Testing off method:');
let offCounter = 0;
function offListener() {
  offCounter++;
}
emitter.on('off-test', offListener);
emitter.emit('off-test');
assert.strictEqual(offCounter, 1, 'Listener should be called before removal');

emitter.off('off-test', offListener);
emitter.emit('off-test');
assert.strictEqual(offCounter, 1, 'Listener should not be called after off()');

// Test setMaxListeners and getMaxListeners
console.log('\n🔢 Testing setMaxListeners and getMaxListeners:');
const maxListenersEmitter = new EventEmitter();
assert.strictEqual(maxListenersEmitter.getMaxListeners(), 10, 'Default max listeners should be 10');

maxListenersEmitter.setMaxListeners(5);
assert.strictEqual(maxListenersEmitter.getMaxListeners(), 5, 'Max listeners should be set to 5');

maxListenersEmitter.setMaxListeners(0);
assert.strictEqual(maxListenersEmitter.getMaxListeners(), 0, 'Max listeners should be set to 0 (unlimited)');

// Test chaining with new methods
console.log('\n⛓️ Testing method chaining with new methods:');
const chainEmitter = new EventEmitter();
const chainResult = chainEmitter
  .on('chain1', () => {})
  .prependListener('chain2', () => {})
  .setMaxListeners(15)
  .removeAllListeners();
assert.strictEqual(chainResult, chainEmitter, 'All methods should return the emitter for chaining');

// Test error handling
console.log('\n❌ Testing error handling:');
try {
  emitter.prependListener();
  assert.fail('prependListener should throw with no arguments');
} catch (e) {
  assert.ok(e.message.includes('requires event name'), 'Should throw meaningful error for missing arguments');
}

try {
  emitter.setMaxListeners('invalid');
  assert.fail('setMaxListeners should throw with invalid argument');
} catch (e) {
  assert.ok(e.message.includes('must be a number'), 'Should throw meaningful error for invalid type');
}

try {
  emitter.setMaxListeners(-1);
  assert.fail('setMaxListeners should throw with negative number');
} catch (e) {
  assert.ok(e.message.includes('non-negative'), 'Should throw meaningful error for negative number');
}

// Test integration with existing methods
console.log('\n🔗 Testing integration with existing methods:');
const integrationEmitter = new EventEmitter();
let integrationCalls = [];

integrationEmitter.prependListener('integration', () => integrationCalls.push('prepend'));
integrationEmitter.on('integration', () => integrationCalls.push('on'));
integrationEmitter.once('integration', () => integrationCalls.push('once'));

integrationEmitter.emit('integration');
assert.deepEqual(integrationCalls, ['prepend', 'on', 'once'], 'All listener types should work together');

integrationEmitter.emit('integration'); // once should not fire again
assert.deepEqual(integrationCalls, ['prepend', 'on', 'once', 'prepend', 'on'], 'once should not fire on second emit');

console.log('\n✅ All enhanced EventEmitter tests passed!');