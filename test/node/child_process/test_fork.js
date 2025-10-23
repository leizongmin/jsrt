// Test fork() and IPC functionality
const child_process = require('node:child_process');
const path = require('node:path');

let tests_passed = 0;
let tests_failed = 0;

function assert(condition, message) {
  if (condition) {
    tests_passed++;
    console.log(`✓ ${message}`);
  } else {
    tests_failed++;
    console.error(`✗ ${message}`);
  }
}

function test_fork_basic() {
  console.log('\nTest: fork() creates child process');

  const child = child_process.fork(
    path.join(__dirname, 'fixtures', 'ipc_child.js')
  );

  assert(child !== null, 'fork() returns ChildProcess');
  assert(typeof child.pid === 'number', 'Child has PID');
  assert(child.connected === true, 'Child is connected');
  assert(typeof child.send === 'function', 'Child has send() method');
  assert(
    typeof child.disconnect === 'function',
    'Child has disconnect() method'
  );

  // Cleanup - disconnect first, then kill
  child.disconnect();
  child.kill();
}

function test_fork_message_parent_to_child(callback) {
  console.log('\nTest: Send message from parent to child');

  const child = child_process.fork(
    path.join(__dirname, 'fixtures', 'ipc_child.js')
  );

  let receivedReady = false;
  let receivedEcho = false;
  let callbackCalled = false;

  function complete() {
    if (callbackCalled) return;
    callbackCalled = true;
    clearTimeout(timer);
    callback();
  }

  child.on('message', (msg) => {
    console.log('Parent received:', JSON.stringify(msg));

    if (msg.type === 'ready') {
      receivedReady = true;
      // Send echo request
      child.send({ type: 'echo', data: 'Hello from parent' });
    } else if (msg.type === 'echo-response') {
      receivedEcho = true;
      assert(
        msg.data === 'Hello from parent',
        'Received correct echo response'
      );
      child.send({ type: 'exit' });
    } else if (msg.type === 'goodbye') {
      assert(receivedReady, 'Received ready message');
      assert(receivedEcho, 'Received echo response');
      complete();
    }
  });

  child.on('exit', (code) => {
    if (code !== 0 && !receivedEcho) {
      assert(false, 'Child exited with non-zero code: ' + code);
      complete();
    }
  });

  child.on('error', (err) => {
    assert(false, 'Child process error: ' + err.message);
    complete();
  });

  // Timeout after 5 seconds
  const timer = setTimeout(() => {
    if (child.connected) {
      child.kill();
      assert(false, 'Test timed out');
      complete();
    }
  }, 5000);
}

function test_fork_disconnect(callback) {
  console.log('\nTest: Disconnect IPC channel');

  const child = child_process.fork(
    path.join(__dirname, 'fixtures', 'ipc_child.js')
  );

  let disconnected = false;
  let callbackCalled = false;

  function complete() {
    if (callbackCalled) return;
    callbackCalled = true;
    clearTimeout(timer);
    callback();
  }

  child.on('message', (msg) => {
    if (msg.type === 'ready') {
      // Disconnect from parent side
      child.disconnect();
    }
  });

  child.on('disconnect', () => {
    disconnected = true;
    assert(child.connected === false, 'Child marked as disconnected');
    assert(true, 'Disconnect event emitted');
  });

  child.on('exit', () => {
    assert(disconnected, 'Disconnect happened before exit');
    complete();
  });

  // Timeout
  const timer = setTimeout(() => {
    if (child.connected) {
      child.kill();
      assert(false, 'Test timed out');
      complete();
    }
  }, 5000);
}

// Run tests
try {
  test_fork_basic();

  test_fork_message_parent_to_child(() => {
    test_fork_disconnect(() => {
      console.log(`\n=== Test Results ===`);
      console.log(`Passed: ${tests_passed}`);
      console.log(`Failed: ${tests_failed}`);

      if (tests_failed > 0) {
        process.exit(1);
      }
    });
  });
} catch (error) {
  console.error('Test error:', error.message);
  console.error(error.stack);
  process.exit(1);
}
