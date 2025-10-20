const { spawn } = require('node:child_process');

console.log('Test 1: Simple echo command');

const echo = spawn('echo', ['hello', 'world']);

echo.on('spawn', () => {
  console.log('✓ spawn event fired');
  console.log('✓ PID:', echo.pid);
});

echo.stdout.on('data', (data) => {
  console.log('✓ stdout data:', data.toString().trim());
});

echo.stderr.on('data', (data) => {
  console.log('stderr data:', data.toString());
});

echo.on('error', (err) => {
  console.log('✗ error event:', err.code, err.message);
});

echo.on('exit', (code, signal) => {
  console.log('✓ exit event - code:', code, 'signal:', signal);
});

echo.on('close', (code, signal) => {
  console.log('✓ close event - code:', code, 'signal:', signal);

  // Test 2: Error handling for non-existent command
  console.log('\nTest 2: Non-existent command');
  const bad = spawn('nonexistent-command-12345');

  bad.on('error', (err) => {
    console.log('✓ error event caught:', err.code);
  });

  bad.on('spawn', () => {
    console.log('✗ spawn event should not fire for bad command');
  });

  // Give it a moment for error to fire
  setTimeout(() => {
    console.log('\n✓ All tests completed');
  }, 100);
});
