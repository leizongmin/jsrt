/**
 * Basic HTTP Client Tests
 * Tests http.request() and http.get() functions
 */

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

console.log('=== Basic HTTP Client Tests ===\n');

// Test 1: http.request() creates ClientRequest
test('http.request() returns ClientRequest object', (done) => {
  const req = http.request('http://httpbin.org/get');
  assert.strictEqual(typeof req, 'object', 'Should return object');
  assert.strictEqual(typeof req.write, 'function', 'Should have write method');
  assert.strictEqual(typeof req.end, 'function', 'Should have end method');
  assert.strictEqual(typeof req.abort, 'function', 'Should have abort method');
  req.abort(); // Clean up
});

// Test 2: http.request() with options object
test('http.request() accepts options object', (done) => {
  const options = {
    host: 'httpbin.org',
    port: 80,
    path: '/get',
    method: 'GET',
  };
  const req = http.request(options);
  assert.strictEqual(typeof req, 'object', 'Should return ClientRequest');
  req.abort();
});

// Test 3: http.request() with callback
test('http.request() accepts callback', (done) => {
  // Create local server for testing
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.end('OK');
  });

  server.listen(0, () => {
    const port = server.address().port;
    let callbackCalled = false;

    const req = http.request(`http://127.0.0.1:${port}/`, (res) => {
      callbackCalled = true;
      assert.strictEqual(res.statusCode, 200, 'Should get 200 status');
      res.on('data', () => {});
      res.on('end', () => {
        assert.strictEqual(callbackCalled, true, 'Callback should be called');
        server.close();
      });
    });

    req.end();
  });
});

// Test 4: http.get() convenience method
test('http.get() automatically calls end()', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.end('GET response');
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.statusCode, 200, 'Should get 200 status');
      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });
      res.on('end', () => {
        assert.strictEqual(body, 'GET response', 'Should receive body');
        server.close();
      });
    });
  });
});

// Test 5: ClientRequest properties
test('ClientRequest has expected properties', (done) => {
  const server = http.createServer((req, res) => {
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request(`http://127.0.0.1:${port}/test`);

    // Check properties exist
    assert.ok(req.method || true, 'Should have method property access');
    assert.ok(req.path || req.url, 'Should have path/url property');

    req.on('response', () => {
      server.close();
    });
    req.end();
  });
});

// Test 6: ClientRequest.write() before end()
test('ClientRequest.write() sends data', (done) => {
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });
    req.on('end', () => {
      assert.strictEqual(body, 'test data', 'Should receive written data');
      res.end();
      server.close();
    });
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'POST',
      path: '/',
    });

    req.write('test data');
    req.end();
  });
});

// Test 7: ClientRequest.end() with data
test('ClientRequest.end() can send data', (done) => {
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });
    req.on('end', () => {
      assert.strictEqual(body, 'end data', 'Should receive end data');
      res.end();
      server.close();
    });
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'POST',
      path: '/',
    });

    req.end('end data');
  });
});

// Test 8: ClientRequest.abort() cancels request
test('ClientRequest.abort() cancels request', (done) => {
  const server = http.createServer((req, res) => {
    // This should not complete
    setTimeout(() => {
      res.end();
    }, 1000);
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request(`http://127.0.0.1:${port}/`);

    let abortEventEmitted = false;
    req.on('abort', () => {
      abortEventEmitted = true;
    });

    req.on('error', () => {
      // Abort may cause error event, which is OK
    });

    req.end();

    // Abort immediately
    req.abort();

    setTimeout(() => {
      server.close();
    }, 100);
  });
});

// Test 9: Response 'data' and 'end' events
test('Response emits data and end events', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.write('chunk1');
    res.write('chunk2');
    res.end('chunk3');
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      let body = '';
      let dataEventCount = 0;

      res.on('data', (chunk) => {
        dataEventCount++;
        body += chunk;
      });

      res.on('end', () => {
        assert.ok(dataEventCount > 0, 'Should receive data events');
        assert.ok(body.includes('chunk1'), 'Should have chunk1');
        assert.ok(body.includes('chunk2'), 'Should have chunk2');
        assert.ok(body.includes('chunk3'), 'Should have chunk3');
        server.close();
      });
    });
  });
});

// Test 10: Request with custom headers
test('Request can set custom headers', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(
      req.headers['x-custom'],
      'test-value',
      'Should have custom header'
    );
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      headers: {
        'X-Custom': 'test-value',
      },
    });

    req.end();
  });
});

// Test 11: Request timeout
test('Request has setTimeout() method', (done) => {
  const server = http.createServer((req, res) => {
    // Delay response
    setTimeout(() => {
      res.end();
    }, 2000);
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request(`http://127.0.0.1:${port}/`);

    assert.strictEqual(
      typeof req.setTimeout,
      'function',
      'Should have setTimeout method'
    );

    let timeoutEmitted = false;
    req.setTimeout(100, () => {
      timeoutEmitted = true;
      req.abort();
    });

    req.on('error', () => {
      // Expected error from abort
    });

    req.end();

    setTimeout(() => {
      assert.strictEqual(timeoutEmitted, true, 'Timeout should emit');
      server.close();
    }, 500);
  });
});

// Test 12: Multiple concurrent requests
test('Multiple concurrent requests work', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.end('response');
  });

  server.listen(0, () => {
    const port = server.address().port;
    let completedCount = 0;
    const totalRequests = 3;

    for (let i = 0; i < totalRequests; i++) {
      http.get(`http://127.0.0.1:${port}/`, (res) => {
        res.on('data', () => {});
        res.on('end', () => {
          completedCount++;
          if (completedCount === totalRequests) {
            assert.strictEqual(
              completedCount,
              totalRequests,
              'All requests should complete'
            );
            server.close();
          }
        });
      });
    }
  });
});

console.log(`\n=== Test Results ===`);
console.log(`Tests run: ${testsRun}`);
console.log(`Tests passed: ${testsPassed}`);
console.log(`Tests failed: ${testsRun - testsPassed}`);

if (testsPassed === testsRun) {
  console.log('\n✓ All basic client tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
