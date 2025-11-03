// Test module.reloadModule() function
// This test verifies that hot module reloading works correctly

const { reloadModule } = require('node:module');
const { getStatistics } = require('node:module');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

console.log('Testing module.reloadModule()...');

// Create a temporary test module file for hot reloading
// Use absolute path based on repository root to avoid path resolution issues
const testModuleDir = path.resolve(__dirname, '../../../target/tmp');
const testModuleFile = path.join(testModuleDir, 'test_reload_module.js');

// Ensure the test directory exists
if (!fs.existsSync(testModuleDir)) {
  fs.mkdirSync(testModuleDir, { recursive: true });
}

// Initial test module content
const initialContent = `
// Initial content
let counter = 0;
function getValue() {
  return counter;
}
function increment() {
  counter++;
  return counter;
}
module.exports = {
  getValue,
  increment,
  counter
};
`;

// Updated test module content
const updatedContent = `
// Updated content
let counter = 0;
function getValue() {
  return counter;
}
function increment() {
  counter += 2;  // Changed increment logic
  return counter;
}
function reset() {
  counter = 0;
  return counter;
}
module.exports = {
  getValue,
  increment,
  reset,
  counter,
  version: 'updated'
};
`;

try {
  // Write initial module
  fs.writeFileSync(testModuleFile, initialContent);
  console.log('✓ Created initial test module');

  // Test 1: Function exists and is callable
  assert.strictEqual(
    typeof reloadModule,
    'function',
    'reloadModule should be a function'
  );

  // Test 2: Initial module load
  const testModule1 = require(testModuleFile);
  assert.strictEqual(
    testModule1.getValue(),
    0,
    'Initial getValue should return 0'
  );
  assert.strictEqual(
    testModule1.increment(),
    1,
    'Initial increment should return 1'
  );
  assert.strictEqual(
    testModule1.getValue(),
    1,
    'getValue after increment should return 1'
  );
  console.log('✓ Initial module loaded and working');

  // Get initial statistics
  const initialStats = getStatistics();
  console.log('Initial stats:', initialStats.loadsTotal, 'loads');

  // Test 3: Reload module with updated content
  fs.writeFileSync(testModuleFile, updatedContent);
  console.log('✓ Updated test module content');

  const reloadResult = reloadModule(testModuleFile);
  console.log('Reload result:', JSON.stringify(reloadResult, null, 2));

  // Verify reload result structure
  assert(
    typeof reloadResult === 'object',
    'reloadModule should return an object'
  );
  assert(reloadResult.reloadSuccess === true, 'reload should succeed');
  assert(typeof reloadResult.path === 'string', 'should have path');
  assert(
    typeof reloadResult.resolvedPath === 'string',
    'should have resolvedPath'
  );
  assert(typeof reloadResult.statistics === 'object', 'should have statistics');

  // Test 4: Load the updated module (require should now use new version)
  // reloadModule() already updated the module in cache, so require() will get the new version

  const testModule2 = require(testModuleFile);
  assert.strictEqual(
    testModule2.getValue(),
    0,
    'Updated getValue should return 0'
  );
  assert.strictEqual(
    testModule2.increment(),
    2,
    'Updated increment should return 2 (counter += 2)'
  );
  assert.strictEqual(
    testModule2.getValue(),
    2,
    'getValue after increment should return 2'
  );
  assert.strictEqual(
    typeof testModule2.reset,
    'function',
    'New reset function should exist'
  );
  assert.strictEqual(
    testModule2.version,
    'updated',
    'New version property should exist'
  );
  console.log('✓ Module successfully reloaded with new content');

  // Test 5: Test reloading a non-existent module
  const nonExistentResult = reloadModule('./non-existent-module.js');
  assert(
    nonExistentResult.reloadSuccess === false,
    'Non-existent module reload should fail'
  );
  assert(
    typeof nonExistentResult.error === 'string',
    'Should have error message'
  );
  console.log('✓ Non-existent module handling works correctly');

  // Test 6: Test reloading built-in module (should work but may not be cacheable)
  const builtinResult = reloadModule('path');
  console.log(
    'Builtin module reload result:',
    JSON.stringify(builtinResult, null, 2)
  );
  assert(
    typeof builtinResult === 'object',
    'Builtin reload should return result object'
  );
  console.log('✓ Built-in module reload completed');

  // Test 7: Verify statistics updated
  const finalStats = getStatistics();
  console.log('Final stats:', finalStats.loadsTotal, 'loads');
  assert(
    finalStats.loadsTotal > initialStats.loadsTotal,
    'Statistics should show more loads'
  );

  // Test 8: Test Module.reloadModule as static method
  const Module = require('module');
  assert.strictEqual(
    typeof Module.reloadModule,
    'function',
    'Module.reloadModule should be a function'
  );

  const staticResult = Module.reloadModule(testModuleFile);
  assert(
    typeof staticResult === 'object',
    'Module.reloadModule() should return an object'
  );
  console.log('✓ Static method Module.reloadModule() works');

  console.log('✓ All tests passed!');
  console.log('Module.reloadModule() test completed successfully');
} catch (error) {
  console.error('Test failed:', error);
  process.exit(1);
} finally {
  // Cleanup: remove test files
  try {
    if (fs.existsSync(testModuleFile)) {
      fs.unlinkSync(testModuleFile);
      console.log('✓ Cleaned up test module file');
    }
    // Try to remove the directory if it's empty
    try {
      fs.rmdirSync(testModuleDir);
    } catch (e) {
      // Directory might not be empty, that's okay
    }
  } catch (cleanupError) {
    console.warn(
      'Warning: Failed to cleanup test files:',
      cleanupError.message
    );
  }
}
