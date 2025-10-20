const { spawnSync, execSync, execFileSync } = require('node:child_process');

console.log('Testing child_process sync APIs...\n');

// Test 1: spawnSync basic
console.log('Test 1: spawnSync basic');
try {
  const result = spawnSync('echo', ['hello', 'world']);
  console.log('  status:', result.status);
  console.log('  stdout:', result.stdout.toString().trim());
  console.log('  stderr:', result.stderr.toString().trim());
  console.log('  pid:', result.pid);
  console.log('  Pass\n');
} catch (error) {
  console.error('  FAIL:', error.message, '\n');
}

// Test 2: execSync basic
console.log('Test 2: execSync basic');
try {
  const stdout = execSync('echo "test execSync"');
  console.log('  output:', stdout.toString().trim());
  console.log('  Pass\n');
} catch (error) {
  console.error('  FAIL:', error.message, '\n');
}

// Test 3: execSync with error (should throw)
console.log('Test 3: execSync with error');
try {
  execSync('exit 1');
  console.error('  FAIL: Should have thrown\n');
} catch (error) {
  console.log('  Caught error as expected');
  console.log('  status:', error.status);
  console.log('  Pass\n');
}

// Test 4: execFileSync with echo
console.log('Test 4: execFileSync with echo');
try {
  const result = execFileSync('echo', ['hello', 'from', 'execFileSync']);
  console.log('  output:', result.toString().trim());
  console.log('  Pass\n');
} catch (error) {
  console.error('  FAIL:', error.message, '\n');
}

// Test 5: spawnSync with non-zero exit
console.log('Test 5: spawnSync with non-zero exit');
try {
  const result = spawnSync('sh', ['-c', 'exit 42']);
  console.log('  status:', result.status);
  console.log('  signal:', result.signal);
  console.log('  Pass\n');
} catch (error) {
  console.error('  FAIL:', error.message, '\n');
}

// Test 6: Verify blocking behavior
console.log('Test 6: Verify blocking behavior (sleep test)');
try {
  console.log('  Before sleep...');
  const start = Date.now();
  const result = spawnSync('sleep', ['1']);
  const elapsed = Date.now() - start;
  console.log('  After sleep. Elapsed:', elapsed + 'ms');
  console.log('  status:', result.status);

  if (elapsed >= 900) {
    // Allow some margin
    console.log('  Pass - truly blocked\n');
  } else {
    console.error('  FAIL: Did not block for full duration\n');
  }
} catch (error) {
  console.error('  FAIL:', error.message, '\n');
}

// Test 7: execSync blocking
console.log('Test 7: execSync blocking');
try {
  console.log('  Before sleep...');
  const start = Date.now();
  execSync('sleep 1');
  const elapsed = Date.now() - start;
  console.log('  After sleep. Elapsed:', elapsed + 'ms');

  if (elapsed >= 900) {
    console.log('  Pass - truly blocked\n');
  } else {
    console.error('  FAIL: Did not block for full duration\n');
  }
} catch (error) {
  console.error('  FAIL:', error.message, '\n');
}

// Test 8: spawnSync output array
console.log('Test 8: spawnSync output array');
try {
  const result = spawnSync('echo', ['test']);
  console.log('  output[0] (stdin):', result.output[0]);
  console.log('  output[1] (stdout):', result.output[1].toString().trim());
  console.log('  output[2] (stderr):', result.output[2].toString().trim());
  console.log('  Pass\n');
} catch (error) {
  console.error('  FAIL:', error.message, '\n');
}

// Test 9: execFileSync with error
console.log('Test 9: execFileSync with error');
try {
  execFileSync('sh', ['-c', 'exit 3']);
  console.error('  FAIL: Should have thrown\n');
} catch (error) {
  console.log('  Caught error as expected');
  console.log('  status:', error.status);
  console.log('  Pass\n');
}

// Test 10: spawnSync stderr capture
console.log('Test 10: spawnSync stderr capture');
try {
  const result = spawnSync('sh', ['-c', 'echo "error" >&2']);
  console.log('  stdout:', result.stdout.toString().trim());
  console.log('  stderr:', result.stderr.toString().trim());

  if (result.stderr.toString().includes('error')) {
    console.log('  Pass\n');
  } else {
    console.error('  FAIL: stderr not captured\n');
  }
} catch (error) {
  console.error('  FAIL:', error.message, '\n');
}

// Test 11: execSync stderr in error
console.log('Test 11: execSync stderr in error');
try {
  execSync('sh -c "echo error >&2; exit 1"');
  console.error('  FAIL: Should have thrown\n');
} catch (error) {
  console.log('  Caught error as expected');
  console.log('  stderr:', error.stderr.toString().trim());

  if (error.stderr.toString().includes('error')) {
    console.log('  Pass\n');
  } else {
    console.error('  FAIL: stderr not attached to error\n');
  }
}

// Test 12: Timeout (short timeout for fast test)
console.log('Test 12: Timeout');
try {
  const result = spawnSync('sleep', ['10'], { timeout: 500 });
  console.log('  status:', result.status);
  console.log('  signal:', result.signal);
  console.log('  error:', result.error ? result.error.code : 'none');

  if (result.error && result.error.code === 'ETIMEDOUT') {
    console.log('  Pass - timeout worked\n');
  } else {
    console.error('  FAIL: timeout did not work as expected\n');
  }
} catch (error) {
  console.error('  FAIL:', error.message, '\n');
}

console.log('All sync tests complete!');
