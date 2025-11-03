// Test cwd (working directory) option
const child_process = require('node:child_process');
const fs = require('node:fs');
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

function test_cwd_valid() {
  console.log('\nTest: spawn() with valid cwd');

  const child = child_process.spawn('pwd', [], {
    cwd: '/tmp',
  });

  let output = '';
  child.stdout.on('data', (data) => {
    output += data.toString();
  });

  child.on('exit', (code) => {
    const actualPath = output.trim();
    const expectedPath = fs.realpathSync('/tmp');
    assert(code === 0, 'Process exited successfully');
    assert(
      actualPath === expectedPath,
      `Process ran in correct directory (expected: ${expectedPath}, got: ${actualPath})`
    );
  });
}

function test_cwd_nonexistent() {
  console.log('\nTest: spawn() with nonexistent cwd');

  const child = child_process.spawn('echo', ['test'], {
    cwd: '/nonexistent/directory/that/does/not/exist',
  });

  let errorReceived = false;

  child.on('error', (err) => {
    errorReceived = true;
    assert(err.code === 'ENOENT', `Error code is ENOENT (got: ${err.code})`);
    assert(
      err.path === '/nonexistent/directory/that/does/not/exist',
      'Error includes path'
    );
    assert(err.syscall === 'spawn', 'Error includes syscall');
  });

  child.on('exit', (code) => {
    assert(errorReceived, 'Error event was emitted');
  });

  // Timeout
  setTimeout(() => {
    if (!errorReceived) {
      assert(false, 'Test timed out waiting for error');
      process.exit(1);
    }
  }, 2000);
}

function test_cwd_not_directory() {
  console.log('\nTest: spawn() with cwd pointing to a file');

  const child = child_process.spawn('echo', ['test'], {
    cwd: '/etc/hosts', // File, not directory
  });

  let errorReceived = false;

  child.on('error', (err) => {
    errorReceived = true;
    assert(err.code === 'ENOTDIR', `Error code is ENOTDIR (got: ${err.code})`);
    assert(err.path === '/etc/hosts', 'Error includes path');
    assert(err.syscall === 'spawn', 'Error includes syscall');
  });

  child.on('exit', (code) => {
    assert(errorReceived, 'Error event was emitted');
  });

  // Timeout
  setTimeout(() => {
    if (!errorReceived) {
      assert(false, 'Test timed out waiting for error');
      process.exit(1);
    }
  }, 2000);
}

function test_cwd_relative() {
  console.log('\nTest: spawn() with relative cwd');

  const child = child_process.spawn('pwd', [], {
    cwd: '..',
  });

  let output = '';
  child.stdout.on('data', (data) => {
    output += data.toString();
  });

  child.on('exit', (code) => {
    assert(code === 0, 'Process exited successfully');
    assert(output.trim().length > 0, 'Process ran in relative directory');
  });
}

// Run tests sequentially
test_cwd_valid();

setTimeout(() => {
  test_cwd_nonexistent();
}, 1000);

setTimeout(() => {
  test_cwd_not_directory();
}, 3000);

setTimeout(() => {
  test_cwd_relative();
}, 5000);

setTimeout(() => {
  console.log(`\n=== Test Results ===`);
  console.log(`Passed: ${tests_passed}`);
  console.log(`Failed: ${tests_failed}`);

  if (tests_failed > 0) {
    process.exit(1);
  }
}, 7000);
