// Test shell mode via exec()
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

function test_exec_shell_pipe() {
  console.log('\nTest: exec() with shell pipe');

  child_process.exec('echo "hello" | cat', (error, stdout, stderr) => {
    assert(!error, 'Shell pipe command executed without error');
    assert(
      stdout.trim() === 'hello',
      'Shell pipe executed correctly: ' + stdout.trim()
    );
  });
}

function test_exec_shell_env_var() {
  console.log('\nTest: exec() with environment variable expansion');

  child_process.exec(
    'echo "$TEST_VAR"',
    { env: { TEST_VAR: 'test_value' } },
    (error, stdout, stderr) => {
      assert(!error, 'Shell env expansion executed without error');
      assert(
        stdout.trim() === 'test_value',
        'Shell expanded environment variable: ' + stdout.trim()
      );
    }
  );
}

function test_exec_shell_complex() {
  console.log('\nTest: exec() with complex shell command');

  child_process.exec('A=123 && echo "Value: $A"', (error, stdout, stderr) => {
    assert(!error, 'Complex shell command executed without error');
    assert(
      stdout.trim() === 'Value: 123',
      'Complex shell command executed: ' + stdout.trim()
    );
  });
}

function test_exec_shell_redirect() {
  console.log('\nTest: exec() with shell redirection');

  child_process.exec('echo "test" 2>&1', (error, stdout, stderr) => {
    assert(!error, 'Shell redirection executed without error');
    assert(
      stdout.trim() === 'test',
      'Shell redirection worked: ' + stdout.trim()
    );
  });
}

function test_direct_spawn_no_shell() {
  console.log('\nTest: spawn() without shell (direct command)');

  const child = child_process.spawn('echo', ['direct spawn']);

  let output = '';
  child.stdout.on('data', (data) => {
    output += data.toString();
  });

  child.on('exit', (code) => {
    assert(code === 0, 'Direct spawn exited successfully');
    assert(
      output.trim() === 'direct spawn',
      'Direct spawn produced output: ' + output.trim()
    );
  });
}

// Run tests sequentially
test_exec_shell_pipe();

setTimeout(() => {
  test_exec_shell_env_var();
}, 1000);

setTimeout(() => {
  test_exec_shell_complex();
}, 2000);

setTimeout(() => {
  test_exec_shell_redirect();
}, 3000);

setTimeout(() => {
  test_direct_spawn_no_shell();
}, 4000);

setTimeout(() => {
  console.log(`\n=== Test Results ===`);
  console.log(`Passed: ${tests_passed}`);
  console.log(`Failed: ${tests_failed}`);

  if (tests_failed > 0) {
    process.exit(1);
  }
}, 5500);
