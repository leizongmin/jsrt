/**
 * Test process.stdin, process.stdout, process.stderr as real streams
 */

// Test 1: Check that stdout is a writable stream
console.log('Test 1: process.stdout is a Writable stream');
try {
  if (typeof process.stdout.write !== 'function') {
    throw new Error('process.stdout.write is not a function');
  }

  if (typeof process.stdout.end !== 'function') {
    throw new Error('process.stdout.end is not a function');
  }

  if (typeof process.stdout.on !== 'function') {
    throw new Error('process.stdout.on is not a function');
  }

  console.log('✓ process.stdout has write(), end(), and on() methods');
} catch (e) {
  console.error('✗ Test 1 failed:', e.message);
  process.exit(1);
}

// Test 2: Check that stderr is a writable stream
console.log('\nTest 2: process.stderr is a Writable stream');
try {
  if (typeof process.stderr.write !== 'function') {
    throw new Error('process.stderr.write is not a function');
  }

  if (typeof process.stderr.end !== 'function') {
    throw new Error('process.stderr.end is not a function');
  }

  if (typeof process.stderr.on !== 'function') {
    throw new Error('process.stderr.on is not a function');
  }

  console.log('✓ process.stderr has write(), end(), and on() methods');
} catch (e) {
  console.error('✗ Test 2 failed:', e.message);
  process.exit(1);
}

// Test 3: Check that stdin is a readable stream
console.log('\nTest 3: process.stdin is a Readable stream');
try {
  if (typeof process.stdin.read !== 'function') {
    throw new Error('process.stdin.read is not a function');
  }

  if (typeof process.stdin.pause !== 'function') {
    throw new Error('process.stdin.pause is not a function');
  }

  if (typeof process.stdin.resume !== 'function') {
    throw new Error('process.stdin.resume is not a function');
  }

  if (typeof process.stdin.pipe !== 'function') {
    throw new Error('process.stdin.pipe is not a function');
  }

  console.log(
    '✓ process.stdin has read(), pause(), resume(), and pipe() methods'
  );
} catch (e) {
  console.error('✗ Test 3 failed:', e.message);
  process.exit(1);
}

// Test 4: Check stream properties
console.log('\nTest 4: Stream properties');
try {
  if (typeof process.stdout.writable !== 'boolean') {
    throw new Error('process.stdout.writable is not a boolean');
  }

  if (typeof process.stdin.readable !== 'boolean') {
    throw new Error('process.stdin.readable is not a boolean');
  }

  console.log('✓ Streams have correct property types');
  console.log('  stdout.writable:', process.stdout.writable);
  console.log('  stdin.readable:', process.stdin.readable);
} catch (e) {
  console.error('✗ Test 4 failed:', e.message);
  process.exit(1);
}

// Test 5: Test direct write
console.log('\nTest 5: Direct write to stdout');
try {
  const result = process.stdout.write('Test message\n');
  if (typeof result !== 'boolean') {
    throw new Error('stdout.write() should return a boolean');
  }
  console.log('✓ stdout.write() returns a boolean:', result);
} catch (e) {
  console.error('✗ Test 5 failed:', e.message);
  process.exit(1);
}

console.log('\nAll process stream tests completed!');
