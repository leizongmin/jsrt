// Test require('node:fs/promises') module loading
const assert = require('node:assert');
const fsPromises = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');

console.log('Testing require("node:fs/promises") module loading...\n');

async function testFsPromisesModule() {
  try {
    // Test that fsPromises object exists and has expected methods
    assert.strictEqual(
      typeof fsPromises,
      'object',
      'fsPromises should be an object'
    );
    assert.strictEqual(
      typeof fsPromises.readFile,
      'function',
      'readFile should be a function'
    );
    assert.strictEqual(
      typeof fsPromises.writeFile,
      'function',
      'writeFile should be a function'
    );
    assert.strictEqual(
      typeof fsPromises.stat,
      'function',
      'stat should be a function'
    );
    assert.strictEqual(
      typeof fsPromises.mkdir,
      'function',
      'mkdir should be a function'
    );
    assert.strictEqual(
      typeof fsPromises.unlink,
      'function',
      'unlink should be a function'
    );
    console.log('✓ All required methods exist');

    // Test basic functionality
    const testFile = path.join(os.tmpdir(), 'test-fs-promises-module.txt');
    const testContent = 'Hello from node:fs/promises!';

    // Write file
    await fsPromises.writeFile(testFile, testContent);
    console.log('✓ writeFile works');

    // Read file (returns ArrayBuffer by default)
    const dataBuffer = await fsPromises.readFile(testFile);
    assert.ok(
      dataBuffer instanceof ArrayBuffer,
      'readFile should return ArrayBuffer'
    );

    // Convert to string for verification
    const decodedString = Buffer.from(dataBuffer).toString('utf8');
    assert.strictEqual(decodedString, testContent, 'File content should match');
    console.log('✓ readFile works');

    // Get file stats
    const stats = await fsPromises.stat(testFile);
    assert.ok(stats.isFile(), 'Should be a file');
    console.log('✓ stat works');

    // Clean up
    await fsPromises.unlink(testFile);
    console.log('✓ unlink works');

    console.log('\nAll require("node:fs/promises") tests passed! ✓');
  } catch (error) {
    console.error('✗ Test failed:', error);
    process.exit(1);
  }
}

testFsPromisesModule();
