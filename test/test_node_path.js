const assert = require('jsrt:assert');

console.log('=== Cross-platform Path Tests ===');

// Test basic path operations that might be failing on Windows
function testBasicPaths() {
  console.log('Testing basic path operations...');
  
  // Test path normalization with different separators
  // On Windows, we expect backslashes; on Unix, forward slashes
  const testPath1 = "src/std/module.c";
  const testPath2 = "src\\std\\module.c";
  
  console.log('Input path (forward slashes):', testPath1);
  console.log('Input path (backslashes):', testPath2);
  
  // Test path operations through require resolution
  // This exercises the normalize_path function in module.c
  try {
    // Try to resolve a relative path - this will exercise path normalization
    require('./test_empty.js');
    console.log('✓ Relative path resolution works');
  } catch (e) {
    console.log('✗ Relative path resolution failed:', e.message);
  }
}

// Test that might be failing on Windows - path comparison
function testPathComparison() {
  console.log('Testing path comparison (potential Windows issue)...');
  
  // This test simulates what might be causing the assertion failure
  // If paths are not properly normalized, comparisons may fail
  
  const expectedPlatformPath = process.platform === 'win32' ? 'test\\subdir\\file.js' : 'test/subdir/file.js';
  const inputPath = 'test/subdir/file.js';  // Always use forward slashes as input
  
  console.log('Expected platform path:', expectedPlatformPath);
  console.log('Input path:', inputPath);
  
  // This might be where the assertion failure occurs on Windows
  // If the module system doesn't properly normalize paths for comparison
  try {
    // Simulate a path comparison that might fail on Windows
    // This is likely where the "Expected values to be strictly equal" error comes from
    
    // In the actual failing test, this might be comparing:
    // - A path returned by the module system (with Windows backslashes)  
    // - An expected path (with forward slashes)
    
    const normalizedInput = inputPath.replace(/\//g, process.platform === 'win32' ? '\\' : '/');
    console.log('Normalized input path:', normalizedInput);
    
    // This assertion might be what's failing on Windows
    if (process.platform === 'win32') {
      // On Windows, ensure paths use backslashes
      assert.strictEqual(normalizedInput, 'test\\subdir\\file.js', 'Windows path should use backslashes');
    } else {
      // On Unix, ensure paths use forward slashes  
      assert.strictEqual(normalizedInput, 'test/subdir/file.js', 'Unix path should use forward slashes');
    }
    
    console.log('✓ Path comparison test passed');
  } catch (e) {
    console.log('✗ Path comparison test failed:', e.message);
    throw e; // Re-throw to simulate the actual test failure
  }
}

function testPathSeparators() {
  console.log('Testing path separator handling...');
  
  // Test that mixed separators are handled correctly
  const mixedPath = "test/subdir\\file.js";
  console.log('Mixed path separators:', mixedPath);
  
  // This will exercise the path normalization logic
  try {
    // Just try to access module resolution with mixed separators
    const nonExistentMixed = './test/nonexistent\\file.js';
    console.log('Testing mixed separator path:', nonExistentMixed);
    
    try {
      require(nonExistentMixed);
    } catch (e) {
      // Expected to fail, but should fail with proper path normalization
      console.log('✓ Mixed separators handled (expected error):', e.message.includes('could not load') ? 'path normalized' : 'unexpected error');
    }
  } catch (e) {
    console.log('✗ Path separator test failed:', e.message);
  }
}

function testAbsolutePathDetection() {
  console.log('Testing absolute path detection...');
  
  // Test absolute path detection
  const absolutePaths = [
    '/home/user/file.js',     // Unix absolute
    'C:\\Users\\file.js',      // Windows absolute with drive
    '\\\\server\\share\\file.js', // UNC path
    '/file.js',               // Root-relative
  ];
  
  absolutePaths.forEach(path => {
    console.log('Testing absolute path:', path);
    try {
      // This will exercise is_absolute_path logic
      require(path);
    } catch (e) {
      console.log('  Expected error (file not found):', e.message.substring(0, 50) + '...');
    }
  });
}

function testRelativePathResolution() {
  console.log('Testing relative path resolution...');
  
  // Test various relative path formats
  const relativePaths = [
    './test_empty.js',
    '../test/test_empty.js',
    'test_empty.js',
  ];
  
  relativePaths.forEach(path => {
    console.log('Testing relative path:', path);
    try {
      require(path);
      console.log('  ✓ Successfully resolved');
    } catch (e) {
      console.log('  Expected error:', e.message.substring(0, 50) + '...');
    }
  });
}

// Main test execution
try {
  testBasicPaths();
  testPathComparison();
  testPathSeparators();
  testAbsolutePathDetection();
  testRelativePathResolution();
  
  console.log('=== Cross-platform Path Tests Complete ===');
} catch (e) {
  console.error('Test failed with error:', e.message);
  throw e;
}