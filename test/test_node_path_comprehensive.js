const assert = require('jsrt:assert');

console.log('=== Node.js Path Compatibility Tests ===');
console.log('Platform:', process.platform);

// Test if we have any Node.js-style path functionality
let pathModule = null;
try {
  pathModule = require('node:path');
  console.log('✓ node:path module available');
} catch (e) {
  console.log('✗ node:path module not available:', e.message);
}

// Test basic path operations that Node.js path module should provide
function testNodePathAPI() {
  if (!pathModule) {
    console.log('Skipping Node.js path API tests - module not available');
    return;
  }
  
  console.log('Testing Node.js path API...');
  
  // Test path.join
  const joinResult = pathModule.join('test', 'subdir', 'file.js');
  const expectedJoin = process.platform === 'win32' ? 'test\\subdir\\file.js' : 'test/subdir/file.js';
  console.log('path.join result:', joinResult);
  console.log('Expected result:', expectedJoin);
  assert.strictEqual(joinResult, expectedJoin, 'path.join should use platform-specific separators');
  
  // Test path.normalize
  const normalizeInput = 'test/subdir/../file.js';
  const normalizeResult = pathModule.normalize(normalizeInput);
  const expectedNormalize = process.platform === 'win32' ? 'test\\file.js' : 'test/file.js';
  console.log('path.normalize result:', normalizeResult);
  console.log('Expected result:', expectedNormalize);
  assert.strictEqual(normalizeResult, expectedNormalize, 'path.normalize should resolve .. and use platform separators');
  
  // Test path.resolve  
  const resolveResult = pathModule.resolve('test', 'file.js');
  console.log('path.resolve result:', resolveResult);
  // resolve should return absolute path, hard to test exact value
  assert.ok(resolveResult.includes('test'), 'path.resolve should include test directory');
  
  // Test path separators
  console.log('path.sep:', pathModule.sep);
  if (process.platform === 'win32') {
    assert.strictEqual(pathModule.sep, '\\', 'Windows path separator should be backslash');
  } else {
    assert.strictEqual(pathModule.sep, '/', 'Unix path separator should be forward slash');
  }
  
  console.log('✓ Node.js path API tests passed');
}

// Test cross-platform path handling in module resolution
function testModulePathResolution() {
  console.log('Testing module path resolution...');
  
  // Test that paths are consistently handled
  const testPaths = [
    './test_empty.js',
    '.\\test_empty.js',
    'test_empty.js'
  ];
  
  testPaths.forEach(testPath => {
    console.log('Testing path:', testPath);
    try {
      const moduleExports = require(testPath);
      console.log('  ✓ Successfully resolved');
    } catch (e) {
      console.log('  ✗ Failed to resolve:', e.message);
    }
  });
}

// Test platform-specific path assertions that might be failing
function testPlatformSpecificAssertions() {
  console.log('Testing platform-specific path assertions...');
  
  // Simulate the type of assertion that might be failing on Windows
  const inputPath = 'test/path/to/file.js';
  const actualPath = inputPath.replace(/\//g, process.platform === 'win32' ? '\\' : '/');
  
  console.log('Input path:', inputPath);
  console.log('Platform-normalized path:', actualPath);
  
  // This is the type of assertion that might be failing on Windows
  if (process.platform === 'win32') {
    // On Windows, we expect backslashes
    const expectedWindows = 'test\\path\\to\\file.js';
    assert.strictEqual(actualPath, expectedWindows, 'Windows paths should use backslashes');
    console.log('✓ Windows path assertion passed');
  } else {
    // On Unix, we expect forward slashes
    const expectedUnix = 'test/path/to/file.js';
    assert.strictEqual(actualPath, expectedUnix, 'Unix paths should use forward slashes');
    console.log('✓ Unix path assertion passed');
  }
}

// Main test execution
try {
  testNodePathAPI();
  testModulePathResolution();
  testPlatformSpecificAssertions();
  
  console.log('=== Node.js Path Compatibility Tests Complete ===');
} catch (e) {
  console.error('❌ Test failed with error:', e.message);
  console.error('This might be the assertion error occurring on Windows!');
  throw e;
}