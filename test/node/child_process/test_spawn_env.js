// Test environment variable handling
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

function test_env_inheritance() {
  console.log('\nTest: Environment variable inheritance (default)');

  // Test with PATH which already exists in the environment
  const child = child_process.spawn('sh', ['-c', 'echo "$PATH"'], {});

  let output = '';
  child.stdout.on('data', (data) => {
    output += data.toString();
  });

  child.on('exit', (code) => {
    assert(code === 0, 'Process exited successfully');
    assert(
      output.trim().length > 0 && output.includes('/'),
      'PATH environment variable inherited from parent'
    );
  });
}

function test_env_replacement() {
  console.log('\nTest: Environment variable replacement (no inheritance)');

  // Set a test variable in parent
  process.env.PARENT_VAR = 'should_not_appear';

  const child = child_process.spawn(
    'sh',
    ['-c', 'echo "CUSTOM=$CUSTOM_VAR PARENT=$PARENT_VAR"'],
    {
      env: {
        CUSTOM_VAR: 'custom_value',
        // PARENT_VAR intentionally not included - should be empty
      },
    }
  );

  let output = '';
  child.stdout.on('data', (data) => {
    output += data.toString();
  });

  child.on('exit', (code) => {
    assert(code === 0, 'Process exited successfully');
    const trimmed = output.trim();
    assert(
      trimmed.includes('CUSTOM=custom_value'),
      'Custom env variable set correctly'
    );
    assert(
      trimmed.includes('PARENT=') && !trimmed.includes('should_not_appear'),
      'Parent env not inherited when env is specified'
    );
  });
}

function test_env_merge() {
  console.log('\nTest: Explicit environment merge');

  // Manually merge parent env with custom vars
  // PATH should be inherited from parent env
  const mergedEnv = { ...process.env, CUSTOM_VAR: 'custom_value' };

  const child = child_process.spawn(
    'sh',
    ['-c', 'echo "PATH_LEN=${#PATH} CUSTOM=$CUSTOM_VAR"'],
    {
      env: mergedEnv,
    }
  );

  let output = '';
  child.stdout.on('data', (data) => {
    output += data.toString();
  });

  child.on('exit', (code) => {
    assert(code === 0, 'Process exited successfully');
    const trimmed = output.trim();
    assert(
      trimmed.includes('PATH_LEN=') && !trimmed.includes('PATH_LEN=0'),
      'PATH inherited via merge'
    );
    assert(
      trimmed.includes('CUSTOM=custom_value'),
      'Custom var present via merge'
    );
  });
}

function test_env_empty() {
  console.log('\nTest: Empty environment');

  // Use 'env' command to list all environment variables
  // With empty env object, should have no output
  const child = child_process.spawn('env', [], {
    env: {},
  });

  let output = '';
  child.stdout.on('data', (data) => {
    output += data.toString();
  });

  child.on('exit', (code) => {
    assert(code === 0, 'Process exited successfully');
    assert(
      output.trim() === '',
      'Empty env: no environment variables inherited'
    );
  });
}

// Run tests sequentially
test_env_inheritance();

setTimeout(() => {
  test_env_replacement();
}, 1000);

setTimeout(() => {
  test_env_merge();
}, 2000);

setTimeout(() => {
  test_env_empty();
}, 3000);

setTimeout(() => {
  console.log(`\n=== Test Results ===`);
  console.log(`Passed: ${tests_passed}`);
  console.log(`Failed: ${tests_failed}`);

  if (tests_failed > 0) {
    process.exit(1);
  }
}, 4000);
