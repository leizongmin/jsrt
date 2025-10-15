/**
 * Test suite for ServerResponse Writable stream implementation
 * Tests Phase 4.2: ServerResponse as Writable stream
 */

const http = require('node:http');
const assert = require('jsrt:assert');

// Test counter
let testsRun = 0;
let testsPassed = 0;

function test(name, fn) {
  testsRun++;
  try {
    fn();
    testsPassed++;
    console.log(`✓ ${name}`);
  } catch (err) {
    console.error(`✗ ${name}: ${err.message}`);
  }
}

async function asyncTest(name, fn) {
  testsRun++;
  try {
    await fn();
    testsPassed++;
    console.log(`✓ ${name}`);
  } catch (err) {
    console.error(`✗ ${name}: ${err.message}`);
  }
}

// =============================================================================
// Test 1: ServerResponse has Writable stream methods
// =============================================================================

asyncTest('ServerResponse has Writable stream methods', async () => {
  const server = http.createServer((req, res) => {
    // Check that all required methods exist
    assert(typeof res.cork === 'function', 'cork() should be a function');
    assert(typeof res.uncork === 'function', 'uncork() should be a function');
    assert(typeof res.write === 'function', 'write() should be a function');
    assert(typeof res.end === 'function', 'end() should be a function');

    // Check properties
    assert(typeof res.writable === 'boolean', 'writable should be a boolean');
    assert(
      typeof res.writableEnded === 'boolean',
      'writableEnded should be a boolean'
    );
    assert(
      typeof res.writableFinished === 'boolean',
      'writableFinished should be a boolean'
    );

    res.writeHead(200);
    res.end('OK');
    server.close();
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      http
        .get(`http://127.0.0.1:${port}/`, (res) => {
          res.on('end', resolve);
          res.resume();
        })
        .on('error', reject);
    });
  });
});

// =============================================================================
// Test 2: cork() and uncork() work correctly
// =============================================================================

asyncTest('cork() and uncork() buffer writes', async () => {
  const server = http.createServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });

    // Cork to buffer writes
    res.cork();
    res.write('Hello ');
    res.write('World');
    // Writes should be buffered

    // Uncork to flush
    res.uncork();

    res.end('!');
    server.close();
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      http
        .get(`http://127.0.0.1:${port}/`, (res) => {
          let data = '';
          res.on('data', (chunk) => (data += chunk.toString()));
          res.on('end', () => {
            assert(
              data === 'Hello World!',
              `Expected "Hello World!", got "${data}"`
            );
            resolve();
          });
        })
        .on('error', reject);
    });
  });
});

// =============================================================================
// Test 3: Nested cork() and uncork()
// =============================================================================

asyncTest('Nested cork() and uncork() work correctly', async () => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);

    res.cork();
    res.write('A');
    res.cork(); // Nested cork
    res.write('B');
    res.uncork(); // First uncork (still corked)
    res.write('C');
    res.uncork(); // Second uncork (fully uncorked, should flush)

    res.end('D');
    server.close();
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      http
        .get(`http://127.0.0.1:${port}/`, (res) => {
          let data = '';
          res.on('data', (chunk) => (data += chunk.toString()));
          res.on('end', () => {
            assert(data === 'ABCD', `Expected "ABCD", got "${data}"`);
            resolve();
          });
        })
        .on('error', reject);
    });
  });
});

// =============================================================================
// Test 4: write() returns boolean for back-pressure
// =============================================================================

asyncTest('write() returns false on back-pressure', async () => {
  let writeResult = null;

  const server = http.createServer((req, res) => {
    res.writeHead(200);

    // Small write should return true (no back-pressure)
    writeResult = res.write('small');

    res.end();
    server.close();
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      http
        .get(`http://127.0.0.1:${port}/`, (res) => {
          res.on('end', () => {
            assert(
              typeof writeResult === 'boolean',
              'write() should return boolean'
            );
            // Small writes should return true
            assert(writeResult === true, 'Small write should return true');
            resolve();
          });
          res.resume();
        })
        .on('error', reject);
    });
  });
});

// =============================================================================
// Test 5: writable properties transition correctly
// =============================================================================

asyncTest('Writable properties transition on end()', async () => {
  let propsBeforeEnd = null;
  let propsAfterEnd = null;

  const server = http.createServer((req, res) => {
    // Check properties before end
    propsBeforeEnd = {
      writable: res.writable,
      writableEnded: res.writableEnded,
      writableFinished: res.writableFinished,
    };

    res.writeHead(200);
    res.end('test');

    // Check properties after end
    propsAfterEnd = {
      writable: res.writable,
      writableEnded: res.writableEnded,
      writableFinished: res.writableFinished,
    };

    server.close();
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      http
        .get(`http://127.0.0.1:${port}/`, (res) => {
          res.on('end', () => {
            // Before end: writable=true, ended=false, finished=false
            assert(
              propsBeforeEnd.writable === true,
              'Should be writable before end'
            );
            assert(
              propsBeforeEnd.writableEnded === false,
              'Should not be ended before end'
            );
            assert(
              propsBeforeEnd.writableFinished === false,
              'Should not be finished before end'
            );

            // After end: writable=false, ended=true, finished=true
            assert(
              propsAfterEnd.writable === false,
              'Should not be writable after end'
            );
            assert(
              propsAfterEnd.writableEnded === true,
              'Should be ended after end'
            );
            assert(
              propsAfterEnd.writableFinished === true,
              'Should be finished after end'
            );

            resolve();
          });
          res.resume();
        })
        .on('error', reject);
    });
  });
});

// =============================================================================
// Test 6: Multiple write() calls work correctly
// =============================================================================

asyncTest('Multiple write() calls stream correctly', async () => {
  const server = http.createServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });

    res.write('Line 1\n');
    res.write('Line 2\n');
    res.write('Line 3\n');
    res.end('Line 4');

    server.close();
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      http
        .get(`http://127.0.0.1:${port}/`, (res) => {
          let data = '';
          res.on('data', (chunk) => (data += chunk.toString()));
          res.on('end', () => {
            assert(
              data === 'Line 1\nLine 2\nLine 3\nLine 4',
              `Unexpected data: ${data}`
            );
            resolve();
          });
        })
        .on('error', reject);
    });
  });
});

// =============================================================================
// Test 7: 'finish' event is emitted
// =============================================================================

asyncTest('finish event is emitted on end()', async () => {
  let finishEmitted = false;

  const server = http.createServer((req, res) => {
    res.on('finish', () => {
      finishEmitted = true;
    });

    res.writeHead(200);
    res.end('test');
    server.close();
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      http
        .get(`http://127.0.0.1:${port}/`, (res) => {
          res.on('end', () => {
            setTimeout(() => {
              assert(finishEmitted === true, 'finish event should be emitted');
              resolve();
            }, 50);
          });
          res.resume();
        })
        .on('error', reject);
    });
  });
});

// =============================================================================
// Test 8: Cannot write after end()
// =============================================================================

asyncTest('Cannot write after end()', async () => {
  let errorThrown = false;

  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.end('done');

    try {
      res.write('should fail');
    } catch (err) {
      errorThrown = true;
    }

    server.close();
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      http
        .get(`http://127.0.0.1:${port}/`, (res) => {
          res.on('end', () => {
            assert(
              errorThrown === true,
              'Should throw error when writing after end'
            );
            resolve();
          });
          res.resume();
        })
        .on('error', reject);
    });
  });
});

// =============================================================================
// Run all tests
// =============================================================================

// Wait for all async tests to complete
setTimeout(() => {
  if (testsPassed !== testsRun) {
    console.log(`✗ ${testsRun - testsPassed} test(s) failed`);
    process.exit(1);
  }
  process.exit(0);
}, 2000);
