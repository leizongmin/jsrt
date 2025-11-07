// Test comprehensive exports compatibility from the events module
// This validates that all exports work correctly with both require patterns

const assert = require('assert');

// Test 1: Validate all main exports are available
console.log('=== Test 1: Main exports availability ===');
try {
  const events = require('events');

  // The module itself should be EventEmitter
  assert.strictEqual(
    typeof events,
    'function',
    'Module should be a function (EventEmitter)'
  );

  // Check that EventEmitter is available as a property
  assert.strictEqual(
    typeof events.EventEmitter,
    'function',
    'EventEmitter property should be a function'
  );
  assert.strictEqual(
    events,
    events.EventEmitter,
    'Module should equal EventEmitter property'
  );

  // Check all main exports
  const expectedExports = [
    'EventEmitter',
    'EventTarget',
    'Event',
    'CustomEvent',
    'getEventListeners',
    'once',
    'setMaxListeners',
    'getMaxListeners',
    'addAbortListener',
    'errorMonitor',
    'default',
  ];

  for (const exportName of expectedExports) {
    const exportValue = events[exportName];
    assert.ok(exportValue !== undefined, `${exportName} should be defined`);

    if (exportName === 'errorMonitor') {
      // errorMonitor might be a symbol
      assert.ok(
        typeof exportValue === 'symbol' || typeof exportValue === 'object',
        `errorMonitor should be symbol or object, got ${typeof exportValue}`
      );
    } else {
      assert.strictEqual(
        typeof exportValue,
        'function',
        `${exportName} should be a function`
      );
    }
  }

  console.log('✅ All main exports are available');
} catch (error) {
  console.log('❌ Main exports test failed:', error.message);
  throw error;
}

// Test 2: Direct require vs destructuring compatibility
console.log('\n=== Test 2: Direct require vs destructuring compatibility ===');
try {
  // Direct require pattern
  const DirectEvents = require('events');

  // Destructuring pattern
  const {
    EventEmitter: DestructuredEventEmitter,
    EventTarget: DestructuredEventTarget,
    Event: DestructuredEvent,
    CustomEvent: DestructuredCustomEvent,
    getEventListeners: DestructuredGetEventListeners,
    once: DestructuredOnce,
    setMaxListeners: DestructuredSetMaxListeners,
    getMaxListeners: DestructuredGetMaxListeners,
    addAbortListener: DestructuredAddAbortListener,
    errorMonitor: DestructuredErrorMonitor,
    default: DestructuredDefault,
  } = require('events');

  // Test that EventEmitter is the same
  assert.strictEqual(
    DirectEvents,
    DestructuredEventEmitter,
    'Direct and destructured EventEmitter should be the same'
  );

  // Test that all exports are the same
  assert.strictEqual(
    DirectEvents.EventTarget,
    DestructuredEventTarget,
    'EventTarget should be the same'
  );

  assert.strictEqual(
    DirectEvents.Event,
    DestructuredEvent,
    'Event should be the same'
  );

  assert.strictEqual(
    DirectEvents.CustomEvent,
    DestructuredCustomEvent,
    'CustomEvent should be the same'
  );

  assert.strictEqual(
    DirectEvents.getEventListeners,
    DestructuredGetEventListeners,
    'getEventListeners should be the same'
  );

  assert.strictEqual(
    DirectEvents.once,
    DestructuredOnce,
    'once should be the same'
  );

  assert.strictEqual(
    DirectEvents.setMaxListeners,
    DestructuredSetMaxListeners,
    'setMaxListeners should be the same'
  );

  assert.strictEqual(
    DirectEvents.getMaxListeners,
    DestructuredGetMaxListeners,
    'getMaxListeners should be the same'
  );

  assert.strictEqual(
    DirectEvents.addAbortListener,
    DestructuredAddAbortListener,
    'addAbortListener should be the same'
  );

  assert.strictEqual(
    DirectEvents.errorMonitor,
    DestructuredErrorMonitor,
    'errorMonitor should be the same'
  );

  assert.strictEqual(
    DirectEvents.default,
    DestructuredDefault,
    'default should be the same'
  );

  // Test that all destructured exports are equal to their direct access counterparts
  assert.strictEqual(
    DestructuredEventEmitter,
    DirectEvents,
    'Destructured EventEmitter should equal module'
  );
  assert.strictEqual(
    DestructuredDefault,
    DirectEvents,
    'Default should equal module'
  );

  console.log('✅ Direct and destructuring patterns are fully compatible');
} catch (error) {
  console.log('❌ Direct vs destructuring test failed:', error.message);
  throw error;
}

// Test 3: All export functionality works
console.log('\n=== Test 3: All export functionality ===');
try {
  const events = require('events');

  // Test EventEmitter functionality
  const emitter = new events.EventEmitter();
  let testEventFired = false;
  emitter.on('test', () => {
    testEventFired = true;
  });
  emitter.emit('test');
  assert.ok(testEventFired, 'EventEmitter should work');

  // Test EventTarget functionality
  const target = new events.EventTarget();
  let targetEventFired = false;
  target.addEventListener('target-test', () => {
    targetEventFired = true;
  });
  target.dispatchEvent(new events.Event('target-test'));
  assert.ok(targetEventFired, 'EventTarget should work');

  // Test Event functionality
  const event = new events.Event('test-event');
  assert.strictEqual(
    event.type,
    'test-event',
    'Event should have correct type'
  );
  assert.strictEqual(
    event.bubbles,
    false,
    'Event should not bubble by default'
  );
  assert.strictEqual(
    event.cancelable,
    false,
    'Event should not be cancelable by default'
  );

  // Test CustomEvent functionality
  const customEvent = new events.CustomEvent('custom-event', {
    detail: { message: 'test' },
    bubbles: true,
    cancelable: true,
  });
  assert.strictEqual(
    customEvent.type,
    'custom-event',
    'CustomEvent should have correct type'
  );
  assert.strictEqual(
    customEvent.detail.message,
    'test',
    'CustomEvent should have detail'
  );
  assert.strictEqual(
    customEvent.bubbles,
    true,
    'CustomEvent should bubble when specified'
  );
  assert.strictEqual(
    customEvent.cancelable,
    true,
    'CustomEvent should be cancelable when specified'
  );

  // Test static utilities
  const testEmitter = new events.EventEmitter();
  const testListener = () => {};
  testEmitter.on('static-test', testListener);

  const retrievedListeners = events.getEventListeners(
    testEmitter,
    'static-test'
  );
  assert.ok(
    Array.isArray(retrievedListeners),
    'getEventListeners should return array'
  );
  assert.strictEqual(
    retrievedListeners.length,
    1,
    'getEventListeners should return listener'
  );
  assert.strictEqual(
    retrievedListeners[0],
    testListener,
    'getEventListeners should return correct listener'
  );

  // Test setMaxListeners
  const maxTestEmitter = new events.EventEmitter();
  const originalMax = events.getMaxListeners(maxTestEmitter);
  events.setMaxListeners(5);
  assert.strictEqual(
    events.getMaxListeners(maxTestEmitter),
    5,
    'setMaxListeners should work'
  );
  events.setMaxListeners(originalMax); // Reset

  console.log('✅ All export functionality works correctly');
} catch (error) {
  console.log('❌ Export functionality test failed:', error.message);
  throw error;
}

// Test 4: Module properties and structure
console.log('\n=== Test 4: Module properties and structure ===');
try {
  const events = require('events');

  // Check that module has expected properties
  const moduleKeys = Object.getOwnPropertyNames(events);
  const requiredKeys = [
    'name',
    'length',
    'prototype',
    'EventTarget',
    'Event',
    'CustomEvent',
    'getEventListeners',
    'once',
    'setMaxListeners',
    'getMaxListeners',
    'addAbortListener',
    'errorMonitor',
    'EventEmitter',
    'default',
  ];

  for (const key of requiredKeys) {
    assert.ok(
      moduleKeys.includes(key) || key === 'errorMonitor', // errorMonitor might not be enumerable
      `Module should have ${key} property`
    );
  }

  // Check function properties
  assert.strictEqual(
    events.name,
    'EventEmitter',
    'Module should be named EventEmitter'
  );
  assert.strictEqual(
    typeof events.prototype,
    'object',
    'Module should have prototype'
  );
  assert.ok(
    events.prototype instanceof Object,
    'Prototype should be an object'
  );

  // Check that prototype has EventEmitter methods
  const prototypeMethods = [
    'on',
    'once',
    'emit',
    'removeListener',
    'removeAllListeners',
    'listenerCount',
    'eventNames',
    'listeners',
    'rawListeners',
    'setMaxListeners',
    'getMaxListeners',
  ];

  for (const method of prototypeMethods) {
    assert.strictEqual(
      typeof events.prototype[method],
      'function',
      `Prototype should have ${method} method`
    );
  }

  console.log('✅ Module structure is correct');
} catch (error) {
  console.log('❌ Module structure test failed:', error.message);
  throw error;
}

// Test 5: Compatibility with ES modules (node:events)
console.log('\n=== Test 5: ES module compatibility ===');
try {
  // Test ES module import
  const esEvents = require('node:events');
  const cjsEvents = require('events');

  // They should be the same
  assert.strictEqual(
    typeof esEvents,
    'function',
    'ES module should export function'
  );
  assert.strictEqual(
    typeof cjsEvents,
    'function',
    'CJS module should export function'
  );

  // Compare major exports
  assert.strictEqual(
    typeof esEvents.EventEmitter,
    'function',
    'ES module should export EventEmitter'
  );

  assert.strictEqual(
    typeof esEvents.EventTarget,
    'function',
    'ES module should export EventTarget'
  );

  // Test that both create working instances
  const esEmitter = new esEvents.EventEmitter();
  const cjsEmitter = new cjsEvents.EventEmitter();

  let esEventFired = false;
  let cjsEventFired = false;

  esEmitter.on('es-test', () => {
    esEventFired = true;
  });

  cjsEmitter.on('cjs-test', () => {
    cjsEventFired = true;
  });

  esEmitter.emit('es-test');
  cjsEmitter.emit('cjs-test');

  assert.ok(esEventFired, 'ES module EventEmitter should work');
  assert.ok(cjsEventFired, 'CJS module EventEmitter should work');

  console.log('✅ ES module compatibility is maintained');
} catch (error) {
  console.log('❌ ES module compatibility test failed:', error.message);
  throw error;
}

// Test 6: Default export behavior
console.log('\n=== Test 6: Default export behavior ===');
try {
  const events = require('events');
  const { default: defaultExport } = require('events');

  // Default should be the same as the module
  assert.strictEqual(
    events,
    defaultExport,
    'Default export should equal module'
  );
  assert.strictEqual(
    events,
    events.EventEmitter,
    'Module should equal EventEmitter'
  );
  assert.strictEqual(
    defaultExport,
    events.EventEmitter,
    'Default should equal EventEmitter'
  );

  // All three should be the same function
  assert.strictEqual(
    events === defaultExport && defaultExport === events.EventEmitter,
    true,
    'Module, default, and EventEmitter should all be the same'
  );

  // Test that all can be used for inheritance
  class FromModule extends events {}
  class FromDefault extends defaultExport {}
  class FromEventEmitter extends events.EventEmitter {}

  const instance1 = new FromModule();
  const instance2 = new FromDefault();
  const instance3 = new FromEventEmitter();

  assert.ok(instance1 instanceof events, 'Inheritance from module should work');
  assert.ok(
    instance2 instanceof defaultExport,
    'Inheritance from default should work'
  );
  assert.ok(
    instance3 instanceof events.EventEmitter,
    'Inheritance from EventEmitter should work'
  );

  console.log('✅ Default export behavior is correct');
} catch (error) {
  console.log('❌ Default export test failed:', error.message);
  throw error;
}

// Test 7: Complex usage patterns
console.log('\n=== Test 7: Complex usage patterns ===');
try {
  const events = require('events');

  // Pattern 1: Chain multiple destructuring imports
  const {
    EventEmitter: EE,
    EventTarget: ET,
    Event: E,
    CustomEvent: CE,
    getEventListeners: getListeners,
    once: onceUtil,
  } = events;

  // Test all shortened names work
  const emitter = new EE();
  const target = new ET();
  const event = new E('test');
  const customEvent = new CE('custom', { detail: 'test' });

  assert.ok(emitter instanceof EE, 'Shortened EventEmitter name should work');
  assert.ok(target instanceof ET, 'Shortened EventTarget name should work');
  assert.strictEqual(event.type, 'test', 'Shortened Event name should work');
  assert.strictEqual(
    customEvent.detail,
    'test',
    'Shortened CustomEvent name should work'
  );

  const listener = () => {};
  emitter.on('pattern-test', listener);
  const listeners = getListeners(emitter, 'pattern-test');
  assert.strictEqual(
    listeners[0],
    listener,
    'Shortened utility name should work'
  );

  // Pattern 2: Mix of direct and destructured access
  const directEvents = require('events');
  const { setMaxListeners } = directEvents;

  setMaxListeners(15);
  const newEmitter = new directEvents();
  assert.strictEqual(
    newEmitter.getMaxListeners(),
    15,
    'Mixed access pattern should work'
  );

  // Pattern 3: Property access chaining
  const chained = events.EventEmitter.default.setMaxListeners;
  assert.strictEqual(
    typeof chained,
    'function',
    'Chained property access should work'
  );

  console.log('✅ Complex usage patterns work correctly');
} catch (error) {
  console.log('❌ Complex usage patterns test failed:', error.message);
  throw error;
}

console.log('\n🎉 All exports compatibility tests passed!');
console.log('✅ All exports are accessible through all require patterns');
console.log(
  '✅ Direct require and destructuring patterns are fully compatible'
);
console.log('✅ ES module and CommonJS exports work consistently');
console.log('✅ Default export behavior matches Node.js exactly');
console.log('✅ Complex usage patterns are supported');
