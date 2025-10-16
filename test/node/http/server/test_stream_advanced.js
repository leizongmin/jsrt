const http = require('node:http');
const assert = require('jsrt:assert');

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

// ============================================================================
// Test 1: ServerResponse writable state and destroy()
// ============================================================================

asyncTest('ServerResponse exposes writable state and destroy()', async () => {
  let closeCalled = false;

  const server = http.createServer((req, res) => {
    assert.strictEqual(
      res.writable,
      true,
      'writable should be true before end'
    );
    assert.strictEqual(
      res.destroyed,
      false,
      'destroyed should be false initially'
    );
    assert.strictEqual(
      res.writableHighWaterMark,
      16384,
      'writableHighWaterMark should default to 16384'
    );

    res.on('close', () => {
      closeCalled = true;
      assert.strictEqual(
        res.destroyed,
        true,
        'destroy() should mark response as destroyed'
      );
    });

    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.write('hello world');

    res.on('finish', () => {
      assert.strictEqual(
        res.writableEnded,
        true,
        'writableEnded should be true after finish'
      );
      assert.strictEqual(
        res.writableFinished,
        true,
        'writableFinished should be true after finish'
      );
      res.destroy();
    });

    res.end('done');
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      http
        .get(`http://127.0.0.1:${port}/`, (res) => {
          res.resume();
          res.on('end', () => {
            server.close();
            resolve();
          });
        })
        .on('error', (err) => {
          server.close();
          reject(err);
        });
    });
  });

  assert.strictEqual(
    closeCalled,
    true,
    'close event should fire after destroy()'
  );
});

// ============================================================================
// Test 2: IncomingMessage readable state and destroy()
// ============================================================================

asyncTest('IncomingMessage exposes readable state and destroy()', async () => {
  let closeCalled = false;

  const server = http.createServer((req, res) => {
    assert.strictEqual(
      req.readable,
      true,
      'IncomingMessage should start readable'
    );
    assert.strictEqual(
      req.readableHighWaterMark,
      16384,
      'IncomingMessage readableHighWaterMark should default to 16384'
    );
    assert.strictEqual(
      req.destroyed,
      false,
      'destroyed should be false initially'
    );

    req.on('close', () => {
      closeCalled = true;
      assert.strictEqual(
        req.destroyed,
        true,
        'destroy() should set destroyed to true'
      );
    });

    req.on('end', () => {
      assert.strictEqual(
        req.readableEnded,
        true,
        'readableEnded should be true after end event'
      );
      req.destroy();
      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end('ok');
    });
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      const req = http.request(
        {
          method: 'POST',
          hostname: '127.0.0.1',
          port,
          path: '/',
        },
        (res) => {
          res.resume();
          res.on('end', () => {
            server.close();
            resolve();
          });
        }
      );

      req.on('error', (err) => {
        server.close();
        reject(err);
      });

      req.write('payload');
      req.end();
    });
  });

  assert.strictEqual(
    closeCalled,
    true,
    'destroy() should emit close on IncomingMessage'
  );
});

// ============================================================================
// Test 3: ClientRequest writable state and destroy()
// ============================================================================

asyncTest('ClientRequest exposes writable state and destroy()', async () => {
  let closeCalled = false;

  const server = http.createServer((req, res) => {
    req.resume();
    req.on('end', () => {
      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end('ok');
    });
  });

  await new Promise((resolve, reject) => {
    server.listen(0, () => {
      const port = server.address().port;
      const clientReq = http.request(
        {
          method: 'POST',
          hostname: '127.0.0.1',
          port,
          path: '/',
        },
        async (res) => {
          res.resume();
          await new Promise((resDone) => res.on('end', resDone));

          assert.strictEqual(
            clientReq.writableEnded,
            true,
            'writableEnded should be true after end()'
          );
          assert.strictEqual(
            clientReq.writableFinished,
            true,
            'writableFinished should be true after end()'
          );

          const closePromise = new Promise((resolveClose) => {
            clientReq.once('close', () => {
              closeCalled = true;
              resolveClose();
            });
          });

          clientReq.destroy();
          await closePromise;
          assert.strictEqual(
            clientReq.destroyed,
            true,
            'destroy() should set destroyed property'
          );
          assert.strictEqual(
            closeCalled,
            true,
            'destroy() should emit close on ClientRequest'
          );

          server.close();
          resolve();
        }
      );

      clientReq.on('error', (err) => {
        server.close();
        reject(err);
      });

      assert.strictEqual(
        clientReq.writableHighWaterMark,
        16384,
        'ClientRequest writableHighWaterMark should default to 16384'
      );
      assert.strictEqual(
        clientReq.destroyed,
        false,
        'destroyed should be false before destroy()'
      );

      clientReq.write('hello');
      clientReq.end('world');
    });
  });
});

// ============================================================================
// Run all tests
// ============================================================================

setTimeout(() => {
  if (testsPassed !== testsRun) {
    console.log(`✗ ${testsRun - testsPassed} test(s) failed`);
    process.exit(1);
  }
  process.exit(0);
}, 3000);
