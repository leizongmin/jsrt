// Test Event and EventTarget implementation
const assert = require('jsrt:assert');

// Test 1: Basic Event construction
const event1 = new Event('test');
assert.strictEqual(event1.type, 'test', 'Event type should be "test"');
assert.strictEqual(
  event1.bubbles,
  false,
  'Event bubbles should default to false'
);
assert.strictEqual(
  event1.cancelable,
  false,
  'Event cancelable should default to false'
);
assert.strictEqual(
  event1.defaultPrevented,
  false,
  'Event defaultPrevented should default to false'
);

// Test 2: Event with options
const event2 = new Event('custom', { bubbles: true, cancelable: true });
assert.strictEqual(event2.type, 'custom', 'Event type should be "custom"');
assert.strictEqual(event2.bubbles, true, 'Event bubbles should be true');
assert.strictEqual(event2.cancelable, true, 'Event cancelable should be true');

// Test 3: Event preventDefault
const event3 = new Event('cancel', { cancelable: true });
assert.strictEqual(
  event3.defaultPrevented,
  false,
  'defaultPrevented should be false before preventDefault'
);
event3.preventDefault();
assert.strictEqual(
  event3.defaultPrevented,
  true,
  'defaultPrevented should be true after preventDefault'
);

// Test 4: Basic EventTarget
const target = new EventTarget();
let called = false;
let eventData = null;

target.addEventListener('test', function (e) {
  called = true;
  eventData = e;
});

// Test 5: Dispatch event
const testEvent = new Event('test');
const result = target.dispatchEvent(testEvent);
assert.strictEqual(called, true, 'Event listener should be called');
assert.strictEqual(
  typeof result,
  'boolean',
  'dispatchEvent should return boolean'
);
assert.strictEqual(
  eventData.target,
  target,
  'Event target should be set correctly'
);

// Test 6: Multiple listeners
let count = 0;
target.addEventListener('multi', () => {
  count++;
});
target.addEventListener('multi', () => {
  count++;
});

const multiEvent = new Event('multi');
target.dispatchEvent(multiEvent);
assert.strictEqual(count, 2, 'Multiple listeners should both be called');

// Test 7: Remove event listener
function removableListener() {
  // This should not be called
}

target.addEventListener('removable', removableListener);
target.removeEventListener('removable', removableListener);
target.dispatchEvent(new Event('removable'));

// Test 8: Once option
let onceCount = 0;
target.addEventListener(
  'once',
  () => {
    onceCount++;
  },
  { once: true }
);

target.dispatchEvent(new Event('once'));
target.dispatchEvent(new Event('once'));
assert.strictEqual(onceCount, 1, 'Once listener should only be called once');

// Test 9: stopPropagation and stopImmediatePropagation
const target9 = new EventTarget();
const event9 = new Event('stop');
let beforeStop = false;
let afterStop = false;

target9.addEventListener('stop', (e) => {
  beforeStop = true;
  e.stopImmediatePropagation();
});
target9.addEventListener('stop', () => {
  afterStop = true;
});

target9.dispatchEvent(event9);
assert.strictEqual(beforeStop, true, 'First listener should be called');
assert.strictEqual(
  afterStop,
  false,
  'Second listener should not be called after stopImmediatePropagation'
);
