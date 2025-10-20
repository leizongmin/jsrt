const { exec, execFile } = require('node:child_process');

let tests_passed = 0;
let tests_failed = 0;

function test(name, fn) {
  console.log(`\n[TEST] ${name}`);
  try {
    fn();
  } catch (e) {
    console.log(`[FAIL] ${name}: ${e.message}`);
    tests_failed++;
  }
}

function expect(condition, message) {
  if (!condition) {
    throw new Error(message || 'Assertion failed');
  }
  tests_passed++;
}

// Test 1: Simple exec with success
test('exec() with successful command', (done) => {
  exec('echo "hello world"', (error, stdout, stderr) => {
    console.log('  exec stdout:', stdout.toString().trim());
    console.log('  exec stderr:', stderr.toString().trim());
    console.log('  exec error:', error);

    expect(!error, 'Should not have error');
    expect(
      stdout.toString().trim() === 'hello world',
      'stdout should be "hello world"'
    );
  });

  // Wait a bit for callback
  setTimeout(() => {
    console.log('[PASS] exec() with successful command');
  }, 1000);
});

// Test 2: exec with non-zero exit code
setTimeout(() => {
  test('exec() with non-zero exit code', () => {
    exec('exit 42', (error, stdout, stderr) => {
      console.log('  error code:', error ? error.code : null);
      console.log('  error message:', error ? error.message : null);

      expect(error !== null, 'Should have error');
      expect(error.code === 42, `Exit code should be 42, got ${error.code}`);

      setTimeout(() => {
        console.log('[PASS] exec() with non-zero exit code');
      }, 100);
    });
  });
}, 1500);

// Test 3: exec with timeout
setTimeout(() => {
  test('exec() with timeout', () => {
    exec('sleep 10', { timeout: 500 }, (error, stdout, stderr) => {
      console.log('  timeout error:', error ? error.killed : false);
      console.log('  timeout message:', error ? error.message : null);

      expect(error !== null, 'Should have error due to timeout');
      expect(error.killed === true, 'Process should be killed');

      setTimeout(() => {
        console.log('[PASS] exec() with timeout');
      }, 100);
    });
  });
}, 3000);

// Test 4: execFile with successful command
setTimeout(() => {
  test('execFile() without shell', () => {
    execFile('echo', ['hello', 'from', 'execFile'], (error, stdout, stderr) => {
      console.log('  execFile stdout:', stdout.toString().trim());
      console.log('  execFile error:', error);

      expect(!error, 'Should not have error');
      expect(
        stdout.toString().includes('hello'),
        'stdout should contain "hello"'
      );

      setTimeout(() => {
        console.log('[PASS] execFile() without shell');
      }, 100);
    });
  });
}, 4500);

// Test 5: execFile with args
setTimeout(() => {
  test('execFile() with multiple args', () => {
    execFile(
      '/bin/sh',
      ['-c', 'echo test1; echo test2'],
      (error, stdout, stderr) => {
        console.log('  execFile output:', stdout.toString().trim());

        expect(!error, 'Should not have error');
        expect(stdout.toString().includes('test1'), 'Should contain test1');
        expect(stdout.toString().includes('test2'), 'Should contain test2');

        setTimeout(() => {
          console.log('[PASS] execFile() with multiple args');
        }, 100);
      }
    );
  });
}, 6000);

// Summary after all tests
setTimeout(() => {
  console.log('\n=== Test Summary ===');
  console.log(`Passed: ${tests_passed}`);
  console.log(`Failed: ${tests_failed}`);
}, 8000);
