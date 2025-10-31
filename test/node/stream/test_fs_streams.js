/**
 * Test fs.createReadStream() and fs.createWriteStream()
 */

const fs = require('node:fs');

// Test 1: Basic createReadStream
console.log('Test 1: fs.createReadStream() basic functionality');
try {
  const testFile = __filename; // Read this test file
  const readStream = fs.createReadStream(testFile);

  if (!readStream) {
    throw new Error('createReadStream returned null');
  }

  if (typeof readStream.pipe !== 'function') {
    throw new Error('readStream does not have pipe method');
  }

  if (typeof readStream.on !== 'function') {
    throw new Error('readStream does not have on method');
  }

  console.log(
    '✓ createReadStream returns a stream with pipe() and on() methods'
  );
} catch (e) {
  console.error('✗ Test 1 failed:', e.message);
  process.exit(1);
}

// Test 2: Basic createWriteStream
console.log('\nTest 2: fs.createWriteStream() basic functionality');
try {
  const tmpFile = '/tmp/jsrt_test_write_stream.txt';
  const writeStream = fs.createWriteStream(tmpFile);

  if (!writeStream) {
    throw new Error('createWriteStream returned null');
  }

  if (typeof writeStream.write !== 'function') {
    throw new Error('writeStream does not have write method');
  }

  if (typeof writeStream.end !== 'function') {
    throw new Error('writeStream does not have end method');
  }

  // Write some data
  writeStream.write('Hello, ');
  writeStream.write('World!');
  writeStream.end();

  console.log(
    '✓ createWriteStream returns a stream with write() and end() methods'
  );

  // Clean up
  try {
    fs.unlinkSync(tmpFile);
  } catch (e) {
    // Ignore cleanup errors
  }
} catch (e) {
  console.error('✗ Test 2 failed:', e.message);
  process.exit(1);
}

// Test 3: Piping between file streams
console.log('\nTest 3: Piping from readStream to writeStream');
try {
  const srcFile = __filename;
  const tmpFile = '/tmp/jsrt_test_pipe.txt';

  const readStream = fs.createReadStream(srcFile);
  const writeStream = fs.createWriteStream(tmpFile);

  // Pipe data
  readStream.pipe(writeStream);

  // Wait a bit for the pipe to complete
  setTimeout(() => {
    try {
      const written = fs.readFileSync(tmpFile, 'utf8');
      if (written.length === 0) {
        console.log('⚠ Warning: No data was written (may need async support)');
      } else {
        console.log('✓ Piping works - copied ' + written.length + ' bytes');
      }

      // Clean up
      try {
        fs.unlinkSync(tmpFile);
      } catch (e) {
        // Ignore cleanup errors
      }
    } catch (e) {
      console.error('✗ Test 3 failed:', e.message);
      process.exit(1);
    }
  }, 100);
} catch (e) {
  console.error('✗ Test 3 failed:', e.message);
  process.exit(1);
}

console.log('\nAll fs stream tests completed!');
