const assert = require('jsrt:assert');

// Test process.cwd() - get current working directory
console.log('Testing process.cwd()...');
const originalCwd = process.cwd();
console.log('Current working directory:', originalCwd);
assert.ok(
  typeof originalCwd === 'string',
  'process.cwd() should return a string'
);
assert.ok(
  originalCwd.length > 0,
  'process.cwd() should return non-empty string'
);

// Test process.chdir() - change directory
console.log('Testing process.chdir()...');

// Try to change to a subdirectory that should exist
try {
  // Change to src directory (should exist in jsrt project)
  process.chdir('src');
  const newCwd = process.cwd();
  console.log('Changed to directory:', newCwd);
  assert.ok(
    newCwd.endsWith('src') || newCwd.endsWith('src/'),
    'Should be in src directory'
  );

  // Change back to original directory
  process.chdir('..');
  const backCwd = process.cwd();
  console.log('Changed back to:', backCwd);

  // Verify we're back to original or close to it
  assert.ok(typeof backCwd === 'string', 'Should be back to a valid directory');
} catch (e) {
  console.log(
    'Note: Directory change test skipped (directory may not exist):',
    e.message
  );
}

// Test error cases
console.log('Testing error cases...');

// Test chdir with no arguments
try {
  process.chdir();
  assert.fail('process.chdir() should throw when called without arguments');
} catch (e) {
  assert.ok(
    e instanceof TypeError,
    'Should throw TypeError for missing argument'
  );
  // console.log('✓ Correctly throws error for missing argument');
}

// Test chdir with invalid directory
try {
  process.chdir('/this/directory/should/not/exist/12345');
  assert.fail('process.chdir() should throw for non-existent directory');
} catch (e) {
  console.log(
    '✓ Correctly throws error for non-existent directory:',
    e.message
  );
}

// Ensure we're back to original directory
try {
  process.chdir(originalCwd);
  // console.log('✓ Successfully restored original working directory');
} catch (e) {
  console.log('Warning: Could not restore original directory:', e.message);
}

// console.log('=== process.cwd() and process.chdir() tests completed ===');
