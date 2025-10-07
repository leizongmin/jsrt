// Test fs.promises.readFile/writeFile/appendFile operations
const { promises: fsPromises } = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { Buffer } = require('node:buffer');

// Get tmpdir from os module
const tmpdir = os.tmpdir();

function assert(condition, message) {
  if (!condition) {
    throw new Error(`Assertion failed: ${message}`);
  }
}

async function runTests() {
  console.log('Test 1: fsPromises.writeFile() - write string');
  await fsPromises.writeFile(path.join(tmpdir, 'test_promises_write.txt'), 'Hello World');
  console.log('  ✓ File written successfully');

  console.log('Test 2: fsPromises.readFile() - read back string');
  const data = await fsPromises.readFile(path.join(tmpdir, 'test_promises_write.txt'));
  const text = Buffer.from(data).toString('utf8');
  console.log(`  Read: "${text}"`);
  assert(text === 'Hello World', 'content should match');

  console.log('Test 3: fsPromises.appendFile() - append text');
  await fsPromises.appendFile(path.join(tmpdir, 'test_promises_write.txt'), ' - Appended');
  const data2 = await fsPromises.readFile(path.join(tmpdir, 'test_promises_write.txt'));
  const text2 = Buffer.from(data2).toString('utf8');
  console.log(`  After append: "${text2}"`);
  assert(text2 === 'Hello World - Appended', 'appended content should match');

  console.log('Test 4: fsPromises.writeFile() with ArrayBuffer');
  const buf = Buffer.from('Binary data test');
  await fsPromises.writeFile(path.join(tmpdir, 'test_promises_binary.txt'), buf);
  const data3 = await fsPromises.readFile(path.join(tmpdir, 'test_promises_binary.txt'));
  const text3 = Buffer.from(data3).toString('utf8');
  assert(text3 === 'Binary data test', 'binary content should match');

  console.log('Test 5: fsPromises.readFile() - error handling');
  try {
    await fsPromises.readFile(path.join(tmpdir, 'nonexistent_file.txt'));
    assert(false, 'should have thrown error');
  } catch (err) {
    console.log(`  ✓ Caught expected error: ${err.code}`);
    assert(err.code === 'ENOENT', 'error code should be ENOENT');
  }

  console.log('Test 6: fsPromises.writeFile() - overwrite file');
  await fsPromises.writeFile(path.join(tmpdir, 'test_promises_overwrite.txt'), 'First write');
  await fsPromises.writeFile(
    path.join(tmpdir, 'test_promises_overwrite.txt'),
    'Second write'
  );
  const data4 = await fsPromises.readFile(path.join(tmpdir, 'test_promises_overwrite.txt'));
  const text4 = Buffer.from(data4).toString('utf8');
  assert(text4 === 'Second write', 'overwrite should replace content');

  console.log('Test 7: fsPromises.appendFile() - create new file');
  await fsPromises.unlink(path.join(tmpdir, 'test_promises_append_new.txt')).catch(() => {});
  await fsPromises.appendFile(path.join(tmpdir, 'test_promises_append_new.txt'), 'New file');
  const data5 = await fsPromises.readFile(path.join(tmpdir, 'test_promises_append_new.txt'));
  const text5 = Buffer.from(data5).toString('utf8');
  assert(text5 === 'New file', 'append should create new file');

  // Cleanup
  await fsPromises.unlink(path.join(tmpdir, 'test_promises_write.txt'));
  await fsPromises.unlink(path.join(tmpdir, 'test_promises_binary.txt'));
  await fsPromises.unlink(path.join(tmpdir, 'test_promises_overwrite.txt'));
  await fsPromises.unlink(path.join(tmpdir, 'test_promises_append_new.txt'));

  console.log('✓ All fsPromises file I/O tests passed!');
}

runTests().catch((err) => {
  console.error('Test failed:', err);
  process.exit(1);
});
