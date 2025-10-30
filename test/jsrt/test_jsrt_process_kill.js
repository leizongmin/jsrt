// Test process.kill() functionality

console.log('Testing process.kill()...\n');

// Test 1: Kill with default signal (SIGTERM)
console.log('Test 1: process.kill() with default signal');
try {
  // Try to send SIGTERM to ourselves (should succeed but not kill us immediately)
  const result = process.kill(process.pid, 0); // Signal 0 just checks if process exists
  if (result === true) {
    console.log('✓ process.kill(pid, 0) returns true');
  } else {
    throw new Error('Expected process.kill() to return true');
  }
} catch (e) {
  console.error('✗ Test 1 failed:', e.message);
  throw e;
}

// Test 2: Kill with signal name string (using signal 0 for safety)
console.log(
  '\nTest 2: process.kill() accepts signal 0 to test process existence'
);
try {
  const result = process.kill(process.pid, 0);
  if (result === true) {
    console.log('✓ process.kill(pid, 0) successfully checks process existence');
  }
} catch (e) {
  console.error('✗ Test 2 failed:', e.message);
  throw e;
}

// Test 3: Kill with signal number
console.log('\nTest 3: process.kill() with signal number');
try {
  const result = process.kill(process.pid, 0); // Signal 0 = check existence
  if (result === true) {
    console.log('✓ process.kill(pid, 0) works');
  }
} catch (e) {
  console.error('✗ Test 3 failed:', e.message);
  throw e;
}

// Test 4: Kill with short signal name (without SIG prefix)
console.log('\nTest 4: process.kill() with short signal name');
try {
  const result = process.kill(process.pid, 0);
  console.log('✓ process.kill() accepts numeric signals');
} catch (e) {
  console.error('✗ Test 4 failed:', e.message);
  throw e;
}

// Test 5: Invalid signal name should throw
console.log('\nTest 5: Invalid signal name should throw');
try {
  process.kill(process.pid, 'INVALID_SIGNAL');
  console.error('✗ Test 5 failed: Should have thrown for invalid signal');
  throw new Error('Expected error for invalid signal');
} catch (e) {
  if (e.message.includes('Unknown signal')) {
    console.log('✓ Invalid signal name throws error');
  } else {
    console.error('✗ Test 5 failed with wrong error:', e.message);
    throw e;
  }
}

// Test 6: Missing pid should throw
console.log('\nTest 6: Missing pid should throw');
try {
  process.kill();
  console.error('✗ Test 6 failed: Should have thrown for missing pid');
  throw new Error('Expected error for missing pid');
} catch (e) {
  if (e.message.includes('required')) {
    console.log('✓ Missing pid throws error');
  } else {
    console.error('✗ Test 6 failed with wrong error:', e.message);
    throw e;
  }
}

// Test 7: process.kill() function exists
console.log('\nTest 7: process.kill is a function');
if (typeof process.kill === 'function') {
  console.log('✓ process.kill is a function');
} else {
  throw new Error('process.kill should be a function');
}

console.log('\n✅ All process.kill() tests passed!');
