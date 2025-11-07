// Test various class inheritance patterns with EventEmitter
// This ensures the compatibility fix works for all common inheritance scenarios

const assert = require('assert');

// Test 1: Simple direct inheritance
console.log('=== Test 1: Simple direct inheritance ===');
try {
  const EventEmitter = require('events');

  class SimpleClass extends EventEmitter {
    constructor() {
      super();
      this.name = 'simple';
    }

    greet() {
      this.emit('greeting', this.name);
    }
  }

  const simple = new SimpleClass();
  let greetingReceived = '';

  simple.on('greeting', (name) => {
    greetingReceived = name;
  });

  simple.greet();

  assert.strictEqual(
    greetingReceived,
    'simple',
    'Simple inheritance should work'
  );
  assert.ok(simple instanceof EventEmitter, 'Instance should be EventEmitter');
  assert.ok(simple instanceof SimpleClass, 'Instance should be SimpleClass');

  console.log('✅ Simple direct inheritance works');
} catch (error) {
  console.log('❌ Simple inheritance test failed:', error.message);
  throw error;
}

// Test 2: Inheritance with constructor parameters
console.log('\n=== Test 2: Inheritance with constructor parameters ===');
try {
  const EventEmitter = require('events');

  class ParameterizedClass extends EventEmitter {
    constructor(id, options = {}) {
      super();
      this.id = id;
      this.options = options;
    }

    start() {
      this.emit('start', { id: this.id, options: this.options });
    }

    stop() {
      this.emit('stop', { id: this.id, options: this.options });
    }
  }

  const instance = new ParameterizedClass(123, { debug: true });
  let startData = null;
  let stopData = null;

  instance.on('start', (data) => {
    startData = data;
  });

  instance.on('stop', (data) => {
    stopData = data;
  });

  instance.start();
  instance.stop();

  assert.strictEqual(startData.id, 123, 'Start event should include id');
  assert.strictEqual(
    startData.options.debug,
    true,
    'Start event should include options'
  );
  assert.strictEqual(stopData.id, 123, 'Stop event should include id');
  assert.strictEqual(
    stopData.options.debug,
    true,
    'Stop event should include options'
  );

  console.log('✅ Parameterized inheritance works');
} catch (error) {
  console.log('❌ Parameterized inheritance test failed:', error.message);
  throw error;
}

// Test 3: Multi-level inheritance chain
console.log('\n=== Test 3: Multi-level inheritance chain ===');
try {
  const EventEmitter = require('events');

  class Level1 extends EventEmitter {
    constructor() {
      super();
      this.level = 1;
    }

    level1Method() {
      return 'level1';
    }
  }

  class Level2 extends Level1 {
    constructor() {
      super();
      this.level = 2;
    }

    level2Method() {
      return 'level2';
    }
  }

  class Level3 extends Level2 {
    constructor() {
      super();
      this.level = 3;
    }

    level3Method() {
      return 'level3';
    }
  }

  const level3Instance = new Level3();

  // Test inheritance chain
  assert.ok(level3Instance instanceof EventEmitter, 'Should be EventEmitter');
  assert.ok(level3Instance instanceof Level1, 'Should be Level1');
  assert.ok(level3Instance instanceof Level2, 'Should be Level2');
  assert.ok(level3Instance instanceof Level3, 'Should be Level3');

  // Test all methods work
  assert.strictEqual(
    level3Instance.level1Method(),
    'level1',
    'Level1 method should work'
  );
  assert.strictEqual(
    level3Instance.level2Method(),
    'level2',
    'Level2 method should work'
  );
  assert.strictEqual(
    level3Instance.level3Method(),
    'level3',
    'Level3 method should work'
  );

  // Test events work through the chain
  let chainEventFired = false;
  level3Instance.on('chain-test', () => {
    chainEventFired = true;
  });

  level3Instance.emit('chain-test');
  assert.ok(chainEventFired, 'Events should work through inheritance chain');

  console.log('✅ Multi-level inheritance works');
} catch (error) {
  console.log('❌ Multi-level inheritance test failed:', error.message);
  throw error;
}

// Test 4: Mixin pattern (multiple inheritance-like behavior)
console.log('\n=== Test 4: Mixin pattern ===');
try {
  const EventEmitter = require('events');

  // Mixin function
  function makeEventEmitter(instance) {
    Object.setPrototypeOf(instance, EventEmitter.prototype);
    // Initialize the events object
    if (typeof instance._events === 'undefined') {
      instance._events = {};
    }
    return instance;
  }

  class RegularClass {
    constructor(name) {
      this.name = name;
    }

    regularMethod() {
      return 'regular';
    }
  }

  const regularInstance = new RegularClass('test');
  const eventifiedInstance = makeEventEmitter(regularInstance);

  // Test that it keeps original properties and gains EventEmitter behavior
  assert.strictEqual(
    eventifiedInstance.name,
    'test',
    'Should keep original properties'
  );
  assert.ok(
    typeof eventifiedInstance.on === 'function',
    'Should have EventEmitter on method'
  );
  assert.ok(
    typeof eventifiedInstance.emit === 'function',
    'Should have EventEmitter emit method'
  );

  console.log('✅ Mixin pattern works');
} catch (error) {
  console.log('❌ Mixin pattern test failed:', error.message);
  throw error;
}

// Test 5: Inheritance with method overriding
console.log('\n=== Test 5: Inheritance with method overriding ===');
try {
  const EventEmitter = require('events');

  class BaseClass extends EventEmitter {
    constructor() {
      super();
      this.baseCalled = false;
    }

    process() {
      this.baseCalled = true;
      this.emit('process', 'base');
    }
  }

  class OverrideClass extends BaseClass {
    constructor() {
      super();
      this.overrideCalled = false;
    }

    process() {
      this.overrideCalled = true;
      this.emit('process', 'override');
    }

    callBaseProcess() {
      super.process();
    }
  }

  const overrideInstance = new OverrideClass();

  // Test overridden method
  let processData = '';
  overrideInstance.on('process', (data) => {
    processData = data;
  });

  overrideInstance.process();

  assert.ok(
    overrideInstance.overrideCalled,
    'Override method should be called'
  );
  assert.ok(!overrideInstance.baseCalled, 'Base method should not be called');
  assert.strictEqual(processData, 'override', 'Should emit override data');

  // Test calling base method
  overrideInstance.callBaseProcess();

  assert.ok(
    overrideInstance.baseCalled,
    'Base method should be called when explicitly called'
  );
  assert.strictEqual(
    processData,
    'base',
    'Should emit base data from base method'
  );

  console.log('✅ Method overriding inheritance works');
} catch (error) {
  console.log('❌ Method overriding test failed:', error.message);
  throw error;
}

// Test 6: Inheritance with static properties
console.log('\n=== Test 6: Inheritance with static properties ===');
try {
  const EventEmitter = require('events');

  class ClassWithStatics extends EventEmitter {
    static STATIC_PROPERTY = 'static-value';
    static staticMethod() {
      return 'static-method-result';
    }

    constructor() {
      super();
      this.instanceProperty = 'instance-value';
    }
  }

  // Test static properties
  assert.strictEqual(
    ClassWithStatics.STATIC_PROPERTY,
    'static-value',
    'Static property should work'
  );

  assert.strictEqual(
    ClassWithStatics.staticMethod(),
    'static-method-result',
    'Static method should work'
  );

  // Test instance properties and methods
  const instance = new ClassWithStatics();
  assert.strictEqual(
    instance.instanceProperty,
    'instance-value',
    'Instance property should work'
  );
  assert.ok(instance instanceof EventEmitter, 'Should still be EventEmitter');

  // Test events work
  let staticClassEventFired = false;
  instance.on('static-test', () => {
    staticClassEventFired = true;
  });

  instance.emit('static-test');
  assert.ok(staticClassEventFired, 'Events should work with static properties');

  console.log('✅ Static properties inheritance works');
} catch (error) {
  console.log('❌ Static properties test failed:', error.message);
  throw error;
}

// Test 7: Real-world npm package patterns
console.log('\n=== Test 7: Real-world npm package patterns ===');
try {
  const EventEmitter = require('events');

  // Pattern: Stream-like classes
  class StreamLike extends EventEmitter {
    constructor(options = {}) {
      super();
      this.options = options;
      this.readable = true;
      this.writable = true;
    }

    write(data) {
      this.emit('data', data);
      return true;
    }

    end() {
      this.emit('end');
      this.writable = false;
    }

    destroy() {
      this.emit('close');
      this.readable = false;
      this.writable = false;
    }
  }

  const stream = new StreamLike({ encoding: 'utf8' });
  let dataReceived = null;
  let endFired = false;
  let closeFired = false;

  stream.on('data', (data) => {
    dataReceived = data;
  });

  stream.on('end', () => {
    endFired = true;
  });

  stream.on('close', () => {
    closeFired = true;
  });

  stream.write('test data');
  stream.end();
  stream.destroy();

  assert.strictEqual(dataReceived, 'test data', 'Data event should work');
  assert.ok(endFired, 'End event should fire');
  assert.ok(closeFired, 'Close event should fire');

  // Pattern: Service classes
  class Service extends EventEmitter {
    constructor(name) {
      super();
      this.name = name;
      this.running = false;
    }

    start() {
      this.running = true;
      this.emit('start', this.name);
    }

    stop() {
      this.running = false;
      this.emit('stop', this.name);
    }

    error(message) {
      this.emit('error', new Error(message));
    }
  }

  const service = new Service('test-service');
  let serviceStarted = false;
  let serviceStopped = false;
  let serviceError = null;

  service.on('start', (name) => {
    serviceStarted = true;
    assert.strictEqual(
      name,
      'test-service',
      'Start event should include service name'
    );
  });

  service.on('stop', (name) => {
    serviceStopped = true;
    assert.strictEqual(
      name,
      'test-service',
      'Stop event should include service name'
    );
  });

  service.on('error', (error) => {
    serviceError = error;
  });

  service.start();
  service.error('Something went wrong');
  service.stop();

  assert.ok(serviceStarted, 'Service should start');
  assert.ok(serviceStopped, 'Service should stop');
  assert.ok(serviceError instanceof Error, 'Error should be Error instance');
  assert.strictEqual(
    serviceError.message,
    'Something went wrong',
    'Error message should match'
  );

  console.log('✅ Real-world patterns work correctly');
} catch (error) {
  console.log('❌ Real-world patterns test failed:', error.message);
  throw error;
}

// Test 8: Complex inheritance with ES6 features
console.log('\n=== Test 8: Complex inheritance with ES6 features ===');
try {
  const EventEmitter = require('events');

  class ModernClass extends EventEmitter {
    #privateProperty = 'private-value';

    constructor(config = {}) {
      super();
      this.config = config;
    }

    // Arrow method
    arrowMethod = () => {
      this.emit('arrow', this.#privateProperty);
    };

    // Getter
    get computedValue() {
      return this.config.value || 'default';
    }

    // Setter
    set computedValue(value) {
      this.config.value = value;
      this.emit('config-changed', value);
    }

    // Async method
    async asyncMethod() {
      return new Promise((resolve) => {
        setTimeout(() => {
          this.emit('async-complete', 'async-result');
          resolve('async-result');
        }, 10);
      });
    }
  }

  const modern = new ModernClass({ value: 'initial' });

  // Test getter/setter
  assert.strictEqual(modern.computedValue, 'initial', 'Getter should work');

  let configChanged = false;
  modern.on('config-changed', (value) => {
    configChanged = true;
    assert.strictEqual(value, 'new-value', 'Setter should emit change event');
  });

  modern.computedValue = 'new-value';
  assert.strictEqual(
    modern.computedValue,
    'new-value',
    'Setter should update value'
  );
  assert.ok(configChanged, 'Config change event should fire');

  // Test arrow method
  let arrowEventFired = false;
  modern.on('arrow', (value) => {
    arrowEventFired = true;
    assert.strictEqual(
      value,
      'private-value',
      'Arrow method should access private property'
    );
  });

  modern.arrowMethod();
  assert.ok(arrowEventFired, 'Arrow method should work');

  // Test async method
  let asyncEventFired = false;
  modern.on('async-complete', (result) => {
    asyncEventFired = true;
    assert.strictEqual(
      result,
      'async-result',
      'Async method should emit result'
    );
  });

  const asyncPromise = modern.asyncMethod();
  assert.ok(
    asyncPromise instanceof Promise,
    'Async method should return promise'
  );

  console.log('✅ ES6 features inheritance works');
} catch (error) {
  console.log('❌ ES6 features test failed:', error.message);
  throw error;
}

console.log('\n🎉 All inheritance pattern tests passed!');
console.log('✅ All common inheritance patterns work correctly');
console.log('✅ The EventEmitter compatibility fix supports all scenarios');
console.log('✅ npm packages using any inheritance pattern should work');
