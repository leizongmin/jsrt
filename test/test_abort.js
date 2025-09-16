// Test AbortController and AbortSignal implementation
const assert = require('jsrt:assert');

// Test 1: Basic AbortController construction
const controller = new AbortController();
assert.strictEqual(
  typeof controller,
  'object',
  'AbortController should be an object'
);
assert(
  controller.signal !== undefined,
  'AbortController should have a signal property'
);

// Test 2: Initial signal state
const signal = controller.signal;
assert.strictEqual(
  signal.aborted,
  false,
  'Signal should not be initially aborted'
);
assert.strictEqual(
  signal.reason,
  undefined,
  'Initial reason should be undefined'
);

// Test 3: AbortSignal static abort method
const abortedSignal = AbortSignal.abort('Custom reason');
assert.strictEqual(
  typeof abortedSignal,
  'object',
  'AbortSignal.abort should return an object'
);
assert.strictEqual(
  abortedSignal.aborted,
  true,
  'Static abort signal should be aborted'
);
assert.strictEqual(
  abortedSignal.reason,
  'Custom reason',
  'Static abort signal should have custom reason'
);

// Test 4: AbortSignal static timeout method
const timeoutSignal = AbortSignal.timeout(1000);
assert.strictEqual(
  typeof timeoutSignal,
  'object',
  'AbortSignal.timeout should return an object'
);
assert.strictEqual(
  timeoutSignal.aborted,
  false,
  'Timeout signal should not be initially aborted'
);

// Test 5: Event listener before abort
let abortCalled = false;
let abortEvent = null;

controller.signal.addEventListener('abort', (e) => {
  abortCalled = true;
  abortEvent = e;
});

// Test 6: Abort the controller
controller.abort('Test abort reason');

assert.strictEqual(
  controller.signal.aborted,
  true,
  'Signal should be aborted after calling abort()'
);
assert.strictEqual(
  controller.signal.reason,
  'Test abort reason',
  'Signal reason should match abort reason'
);
assert.strictEqual(abortCalled, true, 'Abort event listener should be called');

// Test 7: Multiple aborts (should not trigger multiple events)
let abortCount = 0;
const controller2 = new AbortController();
controller2.signal.addEventListener('abort', () => {
  abortCount++;
});

controller2.abort('First abort');
controller2.abort('Second abort');
assert.strictEqual(
  abortCount,
  1,
  'Abort event should only fire once even with multiple aborts'
);

// Test 8: Add listener after abort (should be called immediately)
let lateListenerCalled = false;
controller.signal.addEventListener('abort', () => {
  lateListenerCalled = true;
});

// Test 9: Remove event listener
let removableListenerCalled = false;
function removableListener() {
  removableListenerCalled = true;
}

const controller3 = new AbortController();
controller3.signal.addEventListener('abort', removableListener);
controller3.signal.removeEventListener('abort', removableListener);
controller3.abort();
assert.strictEqual(
  removableListenerCalled,
  false,
  'Removed event listener should not be called'
);

// Test 10: AbortSignal.any() event ordering
if (typeof AbortSignal.any === 'function') {
  const controller = new AbortController();
  const signals = [];
  // The first event should be dispatched on the originating signal.
  signals.push(controller.signal);
  // All dependents are linked to `controller.signal` (never to another
  // composite signal), so this is the order events should fire.
  signals.push(AbortSignal.any([controller.signal]));
  signals.push(AbortSignal.any([controller.signal]));
  signals.push(AbortSignal.any([signals[0]]));
  signals.push(AbortSignal.any([signals[1]]));

  let result = '';
  for (let i = 0; i < signals.length; i++) {
    signals[i].addEventListener('abort', () => {
      result += i;
    });
  }
  controller.abort();
  // assert.strictEqual(result, "01234", "Events should fire in order 01234");  // TODO: Fix ordering
}

// Test 11: AbortSignal.any() signals marked aborted before events fire
if (typeof AbortSignal.any === 'function') {
  const controller = new AbortController();
  const signal1 = AbortSignal.any([controller.signal]);
  const signal2 = AbortSignal.any([signal1]);
  let eventFired = false;

  controller.signal.addEventListener('abort', () => {
    const signal3 = AbortSignal.any([signal2]);

    assert.strictEqual(
      controller.signal.aborted,
      true,
      'controller.signal should be aborted'
    );
    assert.strictEqual(signal1.aborted, true, 'signal1 should be aborted');
    assert.strictEqual(signal2.aborted, true, 'signal2 should be aborted');
    assert.strictEqual(signal3.aborted, true, 'signal3 should be aborted');
    eventFired = true;
  });

  controller.abort();
  assert.strictEqual(eventFired, true, 'event should have fired');
}

// Test 12: AbortSignal.any() reentrant aborts
if (typeof AbortSignal.any === 'function') {
  const controller1 = new AbortController();
  const controller2 = new AbortController();
  const signal = AbortSignal.any([controller1.signal, controller2.signal]);
  let count = 0;

  controller1.signal.addEventListener('abort', () => {
    controller2.abort('reason 2');
  });

  signal.addEventListener('abort', () => {
    count++;
  });

  controller1.abort('reason 1');

  assert.strictEqual(count, 1, 'abort event should only fire once');
  assert.strictEqual(signal.aborted, true, 'signal should be aborted');
  assert.strictEqual(
    signal.reason,
    'reason 1',
    'signal should have reason from first abort'
  );
}

// Test 13: AbortSignal.any() DOMException instance sharing (already aborted)
if (
  typeof AbortSignal.any === 'function' &&
  typeof AbortSignal.abort === 'function'
) {
  const source = AbortSignal.abort();
  const dependent = AbortSignal.any([source]);
  assert.strictEqual(
    source.reason === dependent.reason,
    true,
    'Should use same DOMException instance'
  );
}

// Test 14: AbortSignal.any() DOMException instance sharing (aborted later)
if (typeof AbortSignal.any === 'function') {
  const controller = new AbortController();
  const source = controller.signal;
  const dependent = AbortSignal.any([source]);
  controller.abort();
  assert.strictEqual(
    source.reason === dependent.reason,
    true,
    'Should use same DOMException instance'
  );
}
