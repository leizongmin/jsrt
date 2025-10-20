// Test detached process option
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

function test_detached_process() {
  console.log('\nTest: Detached process (detached: true)');

  // Spawn a short process in detached mode
  const child = child_process.spawn('echo', ['detached test'], {
    detached: true,
  });

  assert(child.pid > 0, 'Detached process spawned with valid PID');

  let output = '';
  child.stdout.on('data', (data) => {
    output += data.toString();
  });

  child.on('exit', (code) => {
    assert(code === 0, 'Detached process exited successfully');
    assert(
      output.trim() === 'detached test',
      'Detached process produced output'
    );
  });
}

function test_attached_process() {
  console.log('\nTest: Normal (attached) process (detached: false)');

  const child = child_process.spawn('echo', ['hello'], {
    detached: false,
  });

  assert(child.pid > 0, 'Attached process spawned with valid PID');

  let output = '';
  child.stdout.on('data', (data) => {
    output += data.toString();
  });

  child.on('exit', (code) => {
    assert(code === 0, 'Attached process exited successfully');
    assert(output.trim() === 'hello', 'Attached process produced output');
  });
}

function test_detached_default() {
  console.log('\nTest: Default behavior (detached not specified)');

  // Default should be attached (not detached)
  const child = child_process.spawn('echo', ['test'], {});

  assert(child.pid > 0, 'Process spawned with default options');

  child.on('exit', (code) => {
    assert(code === 0, 'Default process exited successfully');
  });
}

// Run tests sequentially
test_detached_process();

setTimeout(() => {
  test_attached_process();
}, 1000);

setTimeout(() => {
  test_detached_default();
}, 2000);

setTimeout(() => {
  console.log(`\n=== Test Results ===`);
  console.log(`Passed: ${tests_passed}`);
  console.log(`Failed: ${tests_failed}`);

  if (tests_failed > 0) {
    process.exit(1);
  }
}, 3500);
