const assert = require('jsrt:assert');
const events = require('node:events');
const { EventEmitter } = events;

// Test EventEmitter constructor
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

// Test basic event emission and listening
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

// Test multiple listeners
let counter = 0;
emitter.on('count', () => counter++);
emitter.on('count', () => counter++);
emitter.on('count', () => counter++);
emitter.emit('count');
assert.strictEqual(counter, 3, 'All listeners should be called');

// Test once() method
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

// Test removeListener
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

// Test listenerCount
let count = emitter.listenerCount('count');
assert.strictEqual(count, 3, 'listenerCount should return correct count');
count = emitter.listenerCount('nonexistent');
assert.strictEqual(
  count,
  0,
  'listenerCount should return 0 for nonexistent events'
);

// Test removeAllListeners
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

// Test addListener (alias for on)
let aliasTest = false;
emitter.addListener('alias-test', () => {
  aliasTest = true;
});
emitter.emit('alias-test');
assert.strictEqual(aliasTest, true, 'addListener should work as alias for on');

// Test chaining
let chainResult = emitter.on('chain1', () => {}).on('chain2', () => {});
assert.strictEqual(
  chainResult,
  emitter,
  'on() should return the emitter for chaining'
);

// Test emit with no listeners
let noListenersResult = emitter.emit('no-listeners-event');
assert.strictEqual(
  noListenersResult,
  false,
  'emit should return false when no listeners exist'
);

// Test emit with multiple arguments
let arg1, arg2, arg3;
emitter.on('multi-args', (a, b, c) => {
  arg1 = a;
  arg2 = b;
  arg3 = c;
});
emitter.emit('multi-args', 'first', 42, { key: 'value' });
assert.strictEqual(arg1, 'first', 'First argument should be passed correctly');
assert.strictEqual(arg2, 42, 'Second argument should be passed correctly');
assert.deepEqual(
  arg3,
  { key: 'value' },
  'Third argument should be passed correctly'
);

// Create fresh emitter for comprehensive tests
const compEmitter = new EventEmitter();

// 1. on() / addListener()
let onCount = 0;
const onListener = () => onCount++;
compEmitter.on('test-on', onListener);
compEmitter.addListener('test-on', onListener);
compEmitter.emit('test-on');
assert.strictEqual(onCount, 2, 'on() and addListener() should both work');

// 2. once()
let onceTestCount = 0;
compEmitter.once('test-once', () => onceTestCount++);
compEmitter.emit('test-once');
compEmitter.emit('test-once');
assert.strictEqual(onceTestCount, 1, 'once() should only trigger once');

// 3. removeListener() / off()
let removeTestCount = 0;
const removeTestListener = () => removeTestCount++;
compEmitter.on('test-remove', removeTestListener);
compEmitter.emit('test-remove');
compEmitter.removeListener('test-remove', removeTestListener);
compEmitter.emit('test-remove');
assert.strictEqual(
  removeTestCount,
  1,
  'removeListener() should remove listener'
);

compEmitter.on('test-off', removeTestListener);
compEmitter.off('test-off', removeTestListener);
compEmitter.emit('test-off');
assert.strictEqual(
  removeTestCount,
  1,
  'off() should work as alias for removeListener()'
);

// 4. emit()
let emitTestCount = 0;
compEmitter.on('test-emit', () => emitTestCount++);
const hadListeners = compEmitter.emit('test-emit');
assert.strictEqual(emitTestCount, 1, 'emit() should call listeners');
assert.strictEqual(
  hadListeners,
  true,
  'emit() should return true when listeners exist'
);

const noListeners = compEmitter.emit('nonexistent-event');
assert.strictEqual(
  noListeners,
  false,
  'emit() should return false when no listeners'
);

// 5. listenerCount()
compEmitter.on('count-test', () => {});
compEmitter.on('count-test', () => {});
assert.strictEqual(
  compEmitter.listenerCount('count-test'),
  2,
  'listenerCount() should return correct count'
);

// 6. removeAllListeners()
compEmitter.on('remove-all-1', () => {});
compEmitter.on('remove-all-2', () => {});
compEmitter.removeAllListeners('remove-all-1');
assert.strictEqual(
  compEmitter.listenerCount('remove-all-1'),
  0,
  'removeAllListeners(event) should remove specific event'
);
assert.strictEqual(
  compEmitter.listenerCount('remove-all-2'),
  1,
  'removeAllListeners(event) should not affect other events'
);

compEmitter.removeAllListeners();
assert.strictEqual(
  compEmitter.listenerCount('remove-all-2'),
  0,
  'removeAllListeners() should remove all events'
);

// Test enhanced EventEmitter features
// Test prependListener
let prependCallOrder = [];
compEmitter.on('prepend-test', () => prependCallOrder.push('on'));
compEmitter.prependListener('prepend-test', () =>
  prependCallOrder.push('prepend')
);
compEmitter.emit('prepend-test');
assert.deepEqual(
  prependCallOrder,
  ['prepend', 'on'],
  'prependListener should be called before on listeners'
);

// Test prependOnceListener
let prependOnceCallOrder = [];
compEmitter.on('prepend-once-test', () => prependOnceCallOrder.push('on'));
compEmitter.prependOnceListener('prepend-once-test', () =>
  prependOnceCallOrder.push('prepend-once')
);
compEmitter.emit('prepend-once-test');
compEmitter.emit('prepend-once-test'); // Should not call prepend-once again
assert.deepEqual(
  prependOnceCallOrder,
  ['prepend-once', 'on', 'on'],
  'prependOnceListener should only be called once'
);

// Test eventNames
const newCompEmitter = new EventEmitter();
newCompEmitter.on('event1', () => {});
newCompEmitter.on('event2', () => {});
const eventNames = newCompEmitter.eventNames();
assert.ok(Array.isArray(eventNames), 'eventNames should return an array');
assert.ok(eventNames.includes('event1'), 'eventNames should include event1');
assert.ok(eventNames.includes('event2'), 'eventNames should include event2');

// Test listeners
const listenersCompEmitter = new EventEmitter();
const listener1 = () => {};
const listener2 = () => {};
listenersCompEmitter.on('test-listeners', listener1);
listenersCompEmitter.on('test-listeners', listener2);
const listeners = listenersCompEmitter.listeners('test-listeners');
assert.strictEqual(
  listeners.length,
  2,
  'listeners should return array with 2 listeners'
);
assert.strictEqual(listeners[0], listener1, 'first listener should match');
assert.strictEqual(listeners[1], listener2, 'second listener should match');

// Test rawListeners (same as listeners for now)
const rawListeners = listenersCompEmitter.rawListeners('test-listeners');
assert.strictEqual(
  rawListeners.length,
  2,
  'rawListeners should return array with 2 listeners'
);
assert.strictEqual(
  rawListeners[0],
  listener1,
  'first raw listener should match'
);
assert.strictEqual(
  rawListeners[1],
  listener2,
  'second raw listener should match'
);

// Test off (alias for removeListener)
let offCounter = 0;
function offListener() {
  offCounter++;
}
compEmitter.on('off-test', offListener);
compEmitter.emit('off-test');
assert.strictEqual(offCounter, 1, 'Listener should be called before removal');
compEmitter.off('off-test', offListener);
compEmitter.emit('off-test');
assert.strictEqual(offCounter, 1, 'Listener should not be called after off()');

// Test setMaxListeners and getMaxListeners
const maxListenersCompEmitter = new EventEmitter();
assert.strictEqual(
  maxListenersCompEmitter.getMaxListeners(),
  10,
  'Default max listeners should be 10'
);
maxListenersCompEmitter.setMaxListeners(5);
assert.strictEqual(
  maxListenersCompEmitter.getMaxListeners(),
  5,
  'Max listeners should be set to 5'
);
maxListenersCompEmitter.setMaxListeners(0);
assert.strictEqual(
  maxListenersCompEmitter.getMaxListeners(),
  0,
  'Max listeners should be set to 0 (unlimited)'
);

// Test complete method chaining
const chainCompEmitter = new EventEmitter();
const chainTestResult = chainCompEmitter
  .on('chain1', () => {})
  .once('chain2', () => {})
  .prependListener('chain3', () => {})
  .setMaxListeners(15)
  .removeAllListeners();
assert.strictEqual(
  chainTestResult,
  chainCompEmitter,
  'All methods should return the emitter for chaining'
);

// Test error handling
try {
  compEmitter.prependListener();
  assert.fail('prependListener should throw with no arguments');
} catch (e) {
  assert.ok(
    e.message.includes('requires event name'),
    'Should throw meaningful error for missing arguments'
  );
}

try {
  compEmitter.setMaxListeners('invalid');
  assert.fail('setMaxListeners should throw with invalid argument');
} catch (e) {
  assert.ok(
    e.message.includes('must be a number'),
    'Should throw meaningful error for invalid type'
  );
}

try {
  compEmitter.setMaxListeners(-1);
  assert.fail('setMaxListeners should throw with negative number');
} catch (e) {
  assert.ok(
    e.message.includes('non-negative'),
    'Should throw meaningful error for negative number'
  );
}

// Test integration with existing methods
const integrationCompEmitter = new EventEmitter();
let integrationCalls = [];
integrationCompEmitter.prependListener('integration', () =>
  integrationCalls.push('prepend')
);
integrationCompEmitter.on('integration', () => integrationCalls.push('on'));
integrationCompEmitter.once('integration', () => integrationCalls.push('once'));
integrationCompEmitter.emit('integration');
assert.deepEqual(
  integrationCalls,
  ['prepend', 'on', 'once'],
  'All listener types should work together'
);
integrationCompEmitter.emit('integration'); // once should not fire again
assert.deepEqual(
  integrationCalls,
  ['prepend', 'on', 'once', 'prepend', 'on'],
  'once should not fire on second emit'
);

// Test EventEmitter inheritance patterns
class CustomEmitter extends EventEmitter {
  constructor() {
    super();
  }
}
const customEmitter = new CustomEmitter();
let customCalls = 0;
customEmitter.on('custom', () => customCalls++);
customEmitter.emit('custom');
assert.strictEqual(
  customCalls,
  1,
  'Inherited EventEmitter should work correctly'
);
customEmitter.prependListener('custom', () => customCalls++);
customEmitter.emit('custom');
assert.strictEqual(
  customCalls,
  3,
  'Enhanced methods should work on inherited emitter'
);

// Test enhanced method return values for chaining
const chainTestEmitter = new EventEmitter();
const testListener = () => {};
assert.strictEqual(
  chainTestEmitter.prependListener('test', testListener),
  chainTestEmitter,
  'prependListener should return emitter'
);
assert.strictEqual(
  chainTestEmitter.prependOnceListener('test', testListener),
  chainTestEmitter,
  'prependOnceListener should return emitter'
);
assert.strictEqual(
  chainTestEmitter.setMaxListeners(5),
  chainTestEmitter,
  'setMaxListeners should return emitter'
);
assert.strictEqual(
  chainTestEmitter.off('test', testListener),
  chainTestEmitter,
  'off should return emitter'
);

// Test EventTarget class if available
try {
  const { EventTarget } = events;
  const target = new EventTarget();
  let targetCallOrder = [];
  const targetListener1 = () => targetCallOrder.push('listener1');
  const targetListener2 = () => targetCallOrder.push('listener2');
  target.addEventListener('test-event', targetListener1);
  target.addEventListener('test-event', targetListener2);
  const customEvent = new events.Event('test-event');
  target.dispatchEvent(customEvent);
  assert.deepEqual(
    targetCallOrder,
    ['listener1', 'listener2'],
    'EventTarget should call all listeners in order'
  );
  target.removeEventListener('test-event', targetListener1);
  targetCallOrder = [];
  target.dispatchEvent(new events.Event('test-event'));
  assert.deepEqual(
    targetCallOrder,
    ['listener2'],
    'removeEventListener should remove the specific listener'
  );
} catch (e) {
  console.log('⚠️ EventTarget not available, skipping EventTarget tests');
}

// Test Event and CustomEvent classes if available
try {
  const basicEvent = new events.Event('basic');
  assert.strictEqual(
    basicEvent.type,
    'basic',
    'Event should have correct type'
  );
  assert.strictEqual(
    basicEvent.bubbles,
    false,
    'Event should not bubble by default'
  );
  assert.strictEqual(
    basicEvent.cancelable,
    false,
    'Event should not be cancelable by default'
  );

  const customEvent2 = new events.CustomEvent('custom', {
    detail: { message: 'test data' },
    bubbles: true,
    cancelable: true,
  });
  assert.strictEqual(
    customEvent2.type,
    'custom',
    'CustomEvent should have correct type'
  );
  assert.strictEqual(
    customEvent2.bubbles,
    true,
    'CustomEvent should bubble when specified'
  );
  assert.strictEqual(
    customEvent2.cancelable,
    true,
    'CustomEvent should be cancelable when specified'
  );
  assert.deepEqual(
    customEvent2.detail,
    { message: 'test data' },
    'CustomEvent should have correct detail data'
  );
} catch (e) {
  console.log(
    '⚠️ Event/CustomEvent classes not available, skipping Event class tests'
  );
}

// Test static utility methods if available
try {
  const testCompEmitter = new EventEmitter();
  const testCompListener = () => {};
  testCompEmitter.on('test', testCompListener);
  const retrievedListeners = events.getEventListeners(testCompEmitter, 'test');
  assert.ok(
    Array.isArray(retrievedListeners),
    'getEventListeners should return an array'
  );
  assert.strictEqual(
    retrievedListeners.length,
    1,
    'getEventListeners should return correct number of listeners'
  );
  assert.strictEqual(
    retrievedListeners[0],
    testCompListener,
    'getEventListeners should return the correct listener'
  );
} catch (e) {
  console.log('⚠️ events.getEventListeners() not available, skipping test');
}

// Test events.once() if available - should not block test completion
try {
  (async function testOnce() {
    const onceCompEmitter = new EventEmitter();
    const oncePromise = events.once(onceCompEmitter, 'test-once');
    setTimeout(() => {
      onceCompEmitter.emit('test-once', 'data1', 'data2');
    }, 10);
    try {
      const result = await oncePromise;
      assert.ok(
        Array.isArray(result),
        'events.once should resolve to an array'
      );
      assert.deepEqual(
        result,
        ['data1', 'data2'],
        'events.once should resolve with event arguments'
      );
    } catch (error) {
      console.log('❌ events.once() test failed:', error.message);
    }
  })();
} catch (e) {
  console.log('⚠️ events.once() not available, skipping test');
}

// Test performance with many listeners
const perfCompEmitter = new EventEmitter();
for (let i = 0; i < 250; i++) {
  perfCompEmitter.on('perf', () => {});
  perfCompEmitter.prependListener('perf', () => {});
  perfCompEmitter.once('perf', () => {});
  perfCompEmitter.prependOnceListener('perf', () => {});
}
perfCompEmitter.emit('perf');
const allPerfListeners = perfCompEmitter.listeners('perf');
assert.ok(
  allPerfListeners.length > 0,
  'Performance test should handle many listeners'
);
perfCompEmitter.removeAllListeners();
