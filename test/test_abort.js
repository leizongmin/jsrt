// Test AbortController and AbortSignal implementation
console.log('=== AbortController/AbortSignal API Tests ===');

// Test 1: Basic AbortController construction
console.log('Test 1: Basic AbortController construction');
const controller = new AbortController();
console.log('Controller created:', typeof controller);
assert.strictEqual(typeof controller, 'object', 'AbortController should be an object');
console.log('Has signal property:', controller.signal !== undefined);
assert(controller.signal !== undefined, 'AbortController should have a signal property');
console.log('Signal is AbortSignal:', controller.signal.constructor.name === 'AbortSignal');

// Test 2: Initial signal state
console.log('\nTest 2: Initial signal state');
const signal = controller.signal;
console.log('Initial aborted state:', signal.aborted);
assert.strictEqual(signal.aborted, false, 'Signal should not be initially aborted');
console.log('Initial reason:', signal.reason);
assert.strictEqual(signal.reason, undefined, 'Initial reason should be undefined');

// Test 3: AbortSignal static abort method
console.log('\nTest 3: AbortSignal static abort method');
const abortedSignal = AbortSignal.abort('Custom reason');
console.log('Static abort signal created:', typeof abortedSignal);
assert.strictEqual(typeof abortedSignal, 'object', 'AbortSignal.abort should return an object');
console.log('Static signal aborted:', abortedSignal.aborted);
assert.strictEqual(abortedSignal.aborted, true, 'Static abort signal should be aborted');
console.log('Static signal reason:', abortedSignal.reason);
assert.strictEqual(abortedSignal.reason, 'Custom reason', 'Static abort signal should have custom reason');

// Test 4: AbortSignal static timeout method
console.log('\nTest 4: AbortSignal static timeout method');
const timeoutSignal = AbortSignal.timeout(1000);
console.log('Timeout signal created:', typeof timeoutSignal);
assert.strictEqual(typeof timeoutSignal, 'object', 'AbortSignal.timeout should return an object');
console.log('Timeout signal aborted (should be false):', timeoutSignal.aborted);
assert.strictEqual(timeoutSignal.aborted, false, 'Timeout signal should not be initially aborted');

// Test 5: Event listener before abort
console.log('\nTest 5: Event listener before abort');
let abortCalled = false;
let abortEvent = null;

controller.signal.addEventListener('abort', (e) => {
  abortCalled = true;
  abortEvent = e;
  console.log('Abort event received:', e.type);
});

// Test 6: Abort the controller
console.log('\nTest 6: Abort the controller');
console.log('Before abort - aborted:', controller.signal.aborted);
console.log('Before abort - listener called:', abortCalled);

controller.abort('Test abort reason');

console.log('After abort - aborted:', controller.signal.aborted);
assert.strictEqual(controller.signal.aborted, true, 'Signal should be aborted after calling abort()');
console.log('After abort - reason:', controller.signal.reason);
assert.strictEqual(controller.signal.reason, 'Test abort reason', 'Signal reason should match abort reason');
console.log('After abort - listener called:', abortCalled);
assert.strictEqual(abortCalled, true, 'Abort event listener should be called');

// Test 7: Multiple aborts (should not trigger multiple events)
console.log('\nTest 7: Multiple aborts');
let abortCount = 0;
const controller2 = new AbortController();
controller2.signal.addEventListener('abort', () => {
  abortCount++;
  console.log('Abort event count:', abortCount);
});

controller2.abort('First abort');
controller2.abort('Second abort');
console.log('Final abort count (should be 1):', abortCount);
assert.strictEqual(abortCount, 1, 'Abort event should only fire once even with multiple aborts');

// Test 8: Add listener after abort (should be called immediately)
console.log('\nTest 8: Add listener after abort');
let lateListenerCalled = false;
controller.signal.addEventListener('abort', () => {
  lateListenerCalled = true;
  console.log('Late listener called');
});
console.log('Late listener called (implementation dependent):', lateListenerCalled);

// Test 9: Remove event listener
console.log('\nTest 9: Remove event listener');
let removableListenerCalled = false;
function removableListener() {
  removableListenerCalled = true;
  console.log('Removable listener called');
}

const controller3 = new AbortController();
controller3.signal.addEventListener('abort', removableListener);
controller3.signal.removeEventListener('abort', removableListener);
controller3.abort();
console.log('Removable listener called (should be false):', removableListenerCalled);
assert.strictEqual(removableListenerCalled, false, 'Removed event listener should not be called');

console.log('\n=== All AbortController/AbortSignal tests completed ===');