// Test module compatibility for the events module
// This test validates the fix for the "parent class must be a constructor" issue

const assert = require('assert');

// Test 1: Direct require pattern (this was the failing pattern)
console.log('=== Test 1: Direct require pattern ===');
try {
  const EventEmitter = require('events');

  assert.strictEqual(
    typeof EventEmitter,
    'function',
    'require("events") should return a function (EventEmitter constructor)'
  );

  assert.strictEqual(
    EventEmitter.name,
    'EventEmitter',
    'Returned function should be named EventEmitter'
  );

  // This is the critical test - class inheritance
  class DirectTestClass extends EventEmitter {
    constructor() {
      super();
      this.value = 'direct-test';
    }

    getValue() {
      return this.value;
    }
  }

  const directInstance = new DirectTestClass();
  assert.ok(
    directInstance instanceof DirectTestClass,
    'Instance should be instance of DirectTestClass'
  );

  assert.ok(
    directInstance instanceof EventEmitter,
    'Instance should be instance of EventEmitter'
  );

  assert.strictEqual(
    directInstance.getValue(),
    'direct-test',
    'Instance should work correctly'
  );

  // Test that event methods work on inherited class
  let eventFired = false;
  directInstance.on('test-event', () => {
    eventFired = true;
  });

  directInstance.emit('test-event');
  assert.ok(eventFired, 'Event methods should work on inherited class');

  console.log('✅ Direct require pattern works correctly');
} catch (error) {
  console.log('❌ Direct require pattern failed:', error.message);
  throw error;
}

// Test 2: Destructuring pattern (should still work)
console.log('\n=== Test 2: Destructuring pattern ===');
try {
  const { EventEmitter, EventTarget, Event, CustomEvent } = require('events');

  assert.strictEqual(
    typeof EventEmitter,
    'function',
    'Destructured EventEmitter should be a function'
  );

  // Test inheritance with destructured EventEmitter
  class DestructuredTestClass extends EventEmitter {
    constructor() {
      super();
      this.value = 'destructured-test';
    }
  }

  const destructuredInstance = new DestructuredTestClass();
  assert.ok(
    destructuredInstance instanceof EventEmitter,
    'Instance should be instance of EventEmitter'
  );

  assert.strictEqual(
    destructuredInstance.value,
    'destructured-test',
    'Instance should work correctly'
  );

  // Test other exports
  assert.strictEqual(
    typeof EventTarget,
    'function',
    'EventTarget should be a function'
  );
  assert.strictEqual(typeof Event, 'function', 'Event should be a function');
  assert.strictEqual(
    typeof CustomEvent,
    'function',
    'CustomEvent should be a function'
  );

  console.log('✅ Destructuring pattern works correctly');
} catch (error) {
  console.log('❌ Destructuring pattern failed:', error.message);
  throw error;
}

// Test 3: Both patterns return the same EventEmitter
console.log('\n=== Test 3: Consistency between patterns ===');
try {
  const EventEmitterDirect = require('events');
  const { EventEmitter: EventEmitterDestructured } = require('events');

  assert.strictEqual(
    EventEmitterDirect,
    EventEmitterDestructured,
    'Both patterns should return the same EventEmitter function'
  );

  // Test that instances from both patterns are compatible
  class Class1 extends EventEmitterDirect {}
  class Class2 extends EventEmitterDestructured {}

  const instance1 = new Class1();
  const instance2 = new Class2();

  assert.ok(
    instance1 instanceof EventEmitterDirect,
    'Instance from direct require should be EventEmitter'
  );

  assert.ok(
    instance2 instanceof EventEmitterDestructured,
    'Instance from destructured require should be EventEmitter'
  );

  assert.ok(
    instance1 instanceof EventEmitterDestructured,
    'Cross-compatibility: instance1 should be instance of destructured EventEmitter'
  );

  assert.ok(
    instance2 instanceof EventEmitterDirect,
    'Cross-compatibility: instance2 should be instance of direct EventEmitter'
  );

  console.log('✅ Both patterns are consistent');
} catch (error) {
  console.log('❌ Consistency test failed:', error.message);
  throw error;
}

// Test 4: All exports are accessible from the module
console.log('\n=== Test 4: All exports accessibility ===');
try {
  const events = require('events');

  // Check that EventEmitter is both the module and a property
  assert.strictEqual(
    typeof events,
    'function',
    'Module itself should be a function (EventEmitter)'
  );

  assert.strictEqual(
    typeof events.EventEmitter,
    'function',
    'EventEmitter property should be a function'
  );

  assert.strictEqual(
    events,
    events.EventEmitter,
    'Module should be the same as EventEmitter property'
  );

  // Check other exports
  assert.strictEqual(
    typeof events.EventTarget,
    'function',
    'EventTarget should be accessible'
  );
  assert.strictEqual(
    typeof events.Event,
    'function',
    'Event should be accessible'
  );
  assert.strictEqual(
    typeof events.CustomEvent,
    'function',
    'CustomEvent should be accessible'
  );
  assert.strictEqual(
    typeof events.getEventListeners,
    'function',
    'getEventListeners should be accessible'
  );
  assert.strictEqual(
    typeof events.once,
    'function',
    'once should be accessible'
  );
  assert.strictEqual(
    typeof events.setMaxListeners,
    'function',
    'setMaxListeners should be accessible'
  );
  assert.strictEqual(
    typeof events.getMaxListeners,
    'function',
    'getMaxListeners should be accessible'
  );
  assert.strictEqual(
    typeof events.addAbortListener,
    'function',
    'addAbortListener should be accessible'
  );

  // Check default export
  assert.strictEqual(
    typeof events.default,
    'function',
    'default export should be a function'
  );

  assert.strictEqual(
    events.default,
    events,
    'default export should be the same as EventEmitter'
  );

  console.log('✅ All exports are accessible');
} catch (error) {
  console.log('❌ Export accessibility test failed:', error.message);
  throw error;
}

// Test 5: Complex inheritance scenarios
console.log('\n=== Test 5: Complex inheritance scenarios ===');
try {
  const EventEmitter = require('events');

  // Multi-level inheritance
  class BaseClass extends EventEmitter {
    constructor() {
      super();
      this.baseProperty = 'base';
    }

    baseMethod() {
      return 'base-method';
    }
  }

  class MiddleClass extends BaseClass {
    constructor() {
      super();
      this.middleProperty = 'middle';
    }

    middleMethod() {
      return 'middle-method';
    }
  }

  class FinalClass extends MiddleClass {
    constructor() {
      super();
      this.finalProperty = 'final';
    }

    finalMethod() {
      return 'final-method';
    }
  }

  const finalInstance = new FinalClass();

  // Test inheritance chain
  assert.ok(
    finalInstance instanceof FinalClass,
    'Should be instance of FinalClass'
  );
  assert.ok(
    finalInstance instanceof MiddleClass,
    'Should be instance of MiddleClass'
  );
  assert.ok(
    finalInstance instanceof BaseClass,
    'Should be instance of BaseClass'
  );
  assert.ok(
    finalInstance instanceof EventEmitter,
    'Should be instance of EventEmitter'
  );

  // Test all methods and properties work
  assert.strictEqual(
    finalInstance.baseProperty,
    'base',
    'Base property should work'
  );
  assert.strictEqual(
    finalInstance.middleProperty,
    'middle',
    'Middle property should work'
  );
  assert.strictEqual(
    finalInstance.finalProperty,
    'final',
    'Final property should work'
  );

  assert.strictEqual(
    finalInstance.baseMethod(),
    'base-method',
    'Base method should work'
  );
  assert.strictEqual(
    finalInstance.middleMethod(),
    'middle-method',
    'Middle method should work'
  );
  assert.strictEqual(
    finalInstance.finalMethod(),
    'final-method',
    'Final method should work'
  );

  // Test event functionality through inheritance chain
  let eventCount = 0;
  finalInstance.on('chain-test', () => eventCount++);
  finalInstance.emit('chain-test');

  assert.strictEqual(
    eventCount,
    1,
    'Events should work through inheritance chain'
  );

  console.log('✅ Complex inheritance scenarios work correctly');
} catch (error) {
  console.log('❌ Complex inheritance test failed:', error.message);
  throw error;
}

// Test 6: Static method compatibility
console.log('\n=== Test 6: Static method compatibility ===');
try {
  const EventEmitter = require('events');

  // Test setMaxListeners as static method
  const emitter1 = new EventEmitter();
  const emitter2 = new EventEmitter();

  assert.strictEqual(
    emitter1.getMaxListeners(),
    10,
    'Default max listeners should be 10'
  );
  assert.strictEqual(
    emitter2.getMaxListeners(),
    10,
    'Default max listeners should be 10'
  );

  EventEmitter.setMaxListeners(5);

  assert.strictEqual(
    emitter1.getMaxListeners(),
    10,
    'Existing instances should not be affected'
  );
  assert.strictEqual(
    emitter2.getMaxListeners(),
    10,
    'Existing instances should not be affected'
  );

  // New instances should use the new default
  const emitter3 = new EventEmitter();
  assert.strictEqual(
    emitter3.getMaxListeners(),
    5,
    'New instances should use new default'
  );

  // Test getMaxListeners as static method (takes emitter argument)
  const globalMax = EventEmitter.getMaxListeners(emitter3);
  assert.strictEqual(
    globalMax,
    5,
    'Static getMaxListeners should return emitter max'
  );

  console.log('✅ Static methods work correctly');
} catch (error) {
  console.log('❌ Static method test failed:', error.message);
  throw error;
}

// Test 7: Real-world usage patterns
console.log('\n=== Test 7: Real-world usage patterns ===');
try {
  // Pattern 1: Simple class extension (common in npm packages)
  const EventEmitter = require('events');

  class SimpleClass extends EventEmitter {
    constructor(options = {}) {
      super();
      this.options = options;
    }

    start() {
      this.emit('start', this.options);
    }

    stop() {
      this.emit('stop', this.options);
    }
  }

  const simpleInstance = new SimpleClass({ id: 123 });
  let startEventFired = false;
  let stopEventFired = false;

  simpleInstance.on('start', (options) => {
    startEventFired = true;
    assert.strictEqual(options.id, 123, 'Start event should receive options');
  });

  simpleInstance.on('stop', (options) => {
    stopEventFired = true;
    assert.strictEqual(options.id, 123, 'Stop event should receive options');
  });

  simpleInstance.start();
  simpleInstance.stop();

  assert.ok(startEventFired, 'Start event should fire');
  assert.ok(stopEventFired, 'Stop event should fire');

  // Pattern 2: Mixin-like usage
  function makeEventEmitter(obj) {
    Object.setPrototypeOf(obj, EventEmitter.prototype);
    // Initialize the events object
    if (typeof obj._events === 'undefined') {
      obj._events = {};
    }
    return obj;
  }

  const plainObject = { name: 'test' };
  const eventEnabledObject = makeEventEmitter(plainObject);

  assert.ok(
    eventEnabledObject instanceof EventEmitter,
    'Object should become EventEmitter instance'
  );
  assert.strictEqual(
    eventEnabledObject.name,
    'test',
    'Original properties should be preserved'
  );

  let mixinEventFired = false;
  eventEnabledObject.on('mixin-test', () => {
    mixinEventFired = true;
  });

  eventEnabledObject.emit('mixin-test');
  assert.ok(mixinEventFired, 'Mixin pattern should work');

  console.log('✅ Real-world usage patterns work correctly');
} catch (error) {
  console.log('❌ Real-world usage test failed:', error.message);
  throw error;
}

console.log('\n🎉 All module compatibility tests passed!');
console.log('✅ The events module compatibility fix is working correctly!');
console.log('✅ Both require patterns work as expected');
console.log('✅ Class inheritance works in all scenarios');
console.log('✅ All exports remain accessible');
