// Test Event and EventTarget implementation
console.log('=== Event/EventTarget API Tests ===');

// Test 1: Basic Event construction
console.log('Test 1: Basic Event construction');
const event1 = new Event('test');
console.log('Event type:', event1.type);
console.log('Event bubbles:', event1.bubbles);
console.log('Event cancelable:', event1.cancelable);
console.log('Event defaultPrevented:', event1.defaultPrevented);

// Test 2: Event with options
console.log('\nTest 2: Event with options');
const event2 = new Event('custom', { bubbles: true, cancelable: true });
console.log('Event type:', event2.type);
console.log('Event bubbles:', event2.bubbles);
console.log('Event cancelable:', event2.cancelable);

// Test 3: Event preventDefault
console.log('\nTest 3: Event preventDefault');
const event3 = new Event('cancel', { cancelable: true });
console.log('Before preventDefault:', event3.defaultPrevented);
event3.preventDefault();
console.log('After preventDefault:', event3.defaultPrevented);

// Test 4: Basic EventTarget
console.log('\nTest 4: Basic EventTarget');
const target = new EventTarget();
let called = false;
let eventData = null;

target.addEventListener('test', function(e) {
  called = true;
  eventData = e;
  console.log('Event listener called with event type:', e.type);
});

// Test 5: Dispatch event
console.log('\nTest 5: Dispatch event');
const testEvent = new Event('test');
console.log('Before dispatch - called:', called);
const result = target.dispatchEvent(testEvent);
console.log('After dispatch - called:', called);
console.log('Dispatch result:', result);
console.log('Event target set correctly:', eventData && eventData.target === target);

// Test 6: Multiple listeners
console.log('\nTest 6: Multiple listeners');
let count = 0;
target.addEventListener('multi', () => { count++; console.log('Listener 1'); });
target.addEventListener('multi', () => { count++; console.log('Listener 2'); });

const multiEvent = new Event('multi');
target.dispatchEvent(multiEvent);
console.log('Total listeners called:', count);

// Test 7: Remove event listener
console.log('\nTest 7: Remove event listener');
function removableListener() {
  console.log('This should not be called');
}

target.addEventListener('removable', removableListener);
target.removeEventListener('removable', removableListener);
target.dispatchEvent(new Event('removable'));
console.log('Removed listener test completed (should see no message above)');

// Test 8: Once option
console.log('\nTest 8: Once option');
let onceCount = 0;
target.addEventListener('once', () => { onceCount++; console.log('Once listener called'); }, { once: true });

target.dispatchEvent(new Event('once'));
console.log('First dispatch, count:', onceCount);
target.dispatchEvent(new Event('once'));
console.log('Second dispatch, count:', onceCount);

// Test 9: stopPropagation and stopImmediatePropagation
console.log('\nTest 9: stopImmediatePropagation');
const target9 = new EventTarget();
const event9 = new Event('stop');
let beforeStop = false;
let afterStop = false;

target9.addEventListener('stop', (e) => {
  beforeStop = true;
  console.log('First listener called');
  e.stopImmediatePropagation();
});
target9.addEventListener('stop', () => {
  afterStop = true;
  console.log('Second listener called (should not happen)');
});

target9.dispatchEvent(event9);
console.log('Before stop listener called:', beforeStop);
console.log('After stop listener called (should be false):', afterStop);

console.log('\n=== All Event/EventTarget tests completed ===');