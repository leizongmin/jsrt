// Test signal handling
const child_process = require('node:child_process');

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

function test_kill_default_signal() {
  console.log('\nTest: kill() with default signal (SIGTERM)');

  // Create a long-running process
  const child = child_process.spawn('sleep', ['10']);

  assert(typeof child.pid === 'number', 'Child has PID');
  assert(child.killed === false, 'Initially not killed');

  // Kill with default signal
  const result = child.kill();

  assert(result === true, 'kill() returned true');
  assert(child.killed === true, 'Child marked as killed');

  child.on('exit', (code, signal) => {
    assert(signal === 'SIGTERM', `Exit signal is SIGTERM (got: ${signal})`);
  });
}

function test_kill_string_signal() {
  console.log('\nTest: kill() with string signal name');

  const child = child_process.spawn('sleep', ['10']);

  // Kill with SIGKILL string
  const result = child.kill('SIGKILL');

  assert(result === true, 'kill("SIGKILL") returned true');
  assert(child.killed === true, 'Child marked as killed');

  child.on('exit', (code, signal) => {
    assert(signal === 'SIGKILL', `Exit signal is SIGKILL (got: ${signal})`);
  });
}

function test_kill_numeric_signal() {
  console.log('\nTest: kill() with numeric signal');

  const child = child_process.spawn('sleep', ['10']);

  // Kill with SIGKILL (9)
  const result = child.kill(9);

  assert(result === true, 'kill(9) returned true');
  assert(child.killed === true, 'Child marked as killed');

  child.on('exit', (code, signal) => {
    assert(signal === 'SIGKILL', `Exit signal is SIGKILL (got: ${signal})`);
  });
}

function test_kill_sigint() {
  console.log('\nTest: kill() with SIGINT');

  const child = child_process.spawn('sleep', ['10']);

  const result = child.kill('SIGINT');

  assert(result === true, 'kill("SIGINT") returned true');

  child.on('exit', (code, signal) => {
    assert(signal === 'SIGINT', `Exit signal is SIGINT (got: ${signal})`);
  });
}

function test_kill_already_exited() {
  console.log('\nTest: kill() on already exited process');

  const child = child_process.spawn('echo', ['hello']);

  child.on('exit', () => {
    // Try to kill after exit
    setTimeout(() => {
      const result = child.kill();
      assert(result === false, 'kill() returned false for exited process');
    }, 100);
  });
}

// Run tests sequentially
test_kill_default_signal();

setTimeout(() => {
  test_kill_string_signal();
}, 500);

setTimeout(() => {
  test_kill_numeric_signal();
}, 1000);

setTimeout(() => {
  test_kill_sigint();
}, 1500);

setTimeout(() => {
  test_kill_already_exited();
}, 2000);

setTimeout(() => {
  console.log(`\n=== Test Results ===`);
  console.log(`Passed: ${tests_passed}`);
  console.log(`Failed: ${tests_failed}`);

  if (tests_failed > 0) {
    process.exit(1);
  }
}, 3000);
