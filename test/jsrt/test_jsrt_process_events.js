// Minimal test for process event handling

console.log('Testing process events...\n');

// Test 1: process.on() and process.emit() exist
console.log('Test 1: API existence');
if (typeof process.on === 'function') {
  console.log('✓ process.on() exists');
} else {
  throw new Error('process.on() not found');
}

if (typeof process.emit === 'function') {
  console.log('✓ process.emit() exists');
} else {
  throw new Error('process.emit() not found');
}

// Test 2: process.emitWarning() exists
console.log('\nTest 2: process.emitWarning()');
if (typeof process.emitWarning === 'function') {
  console.log('✓ process.emitWarning() exists');
} else {
  throw new Error('process.emitWarning() not found');
}

// Test 3: Uncaught exception API
console.log('\nTest 3: Uncaught exception API');
if (typeof process.setUncaughtExceptionCaptureCallback === 'function') {
  console.log('✓ setUncaughtExceptionCaptureCallback exists');
} else {
  throw new Error('setUncaughtExceptionCaptureCallback not found');
}

if (typeof process.hasUncaughtExceptionCaptureCallback === 'function') {
  console.log('✓ hasUncaughtExceptionCaptureCallback exists');
} else {
  throw new Error('hasUncaughtExceptionCaptureCallback not found');
}

console.log('\n✅ All process event API tests passed!');
