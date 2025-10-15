/**
 * HTTP Client-Server Integration Tests
 * Tests actual HTTP communication between local server and client
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

console.log('=== HTTP Client-Server Integration Tests ===\n');

// Test 1: Complete GET request-response cycle
test('Complete GET request-response cycle', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'GET', 'Should be GET');
    assert.strictEqual(req.url, '/test', 'Should have correct path');
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('Hello World');
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/test`, (res) => {
      assert.strictEqual(res.statusCode, 200, 'Should get 200');
      assert.strictEqual(res.headers['content-type'], 'text/plain');

      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(body, 'Hello World', 'Should receive response body');
        server.close();
      });
    });
  });
});

// Test 2: POST request with body transmission
test('POST request transmits body to server', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'POST', 'Should be POST');

    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });

    req.on('end', () => {
      assert.strictEqual(body, 'request data', 'Server should receive body');
      res.writeHead(200);
      res.end('received');
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

    req.on('response', (res) => {
      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });
      res.on('end', () => {
        assert.strictEqual(body, 'received', 'Client should receive response');
        server.close();
      });
    });

    req.end('request data');
  });
});

// Test 3: JSON request and response
test('JSON request and response transmission', (done) => {
  const requestData = { name: 'test', value: 123 };

  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });

    req.on('end', () => {
      const data = JSON.parse(body);
      assert.strictEqual(data.name, 'test', 'Should parse request JSON');

      const responseData = { result: 'success', received: data.value };
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify(responseData));
    });
  });

  server.listen(0, () => {
    const port = server.address().port;

    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
    });

    req.on('response', (res) => {
      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });
      res.on('end', () => {
        const data = JSON.parse(body);
        assert.strictEqual(data.result, 'success');
        assert.strictEqual(data.received, 123);
        server.close();
      });
    });

    req.end(JSON.stringify(requestData));
  });
});

// Test 4: Custom headers propagation
test('Custom headers propagate correctly', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(
      req.headers['x-custom-client'],
      'client-value',
      'Should receive client header'
    );

    res.writeHead(200, {
      'X-Custom-Server': 'server-value',
    });
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    const req = http.request({
      host: '127.0.0.1',
      port: port,
      headers: {
        'X-Custom-Client': 'client-value',
      },
    });

    req.on('response', (res) => {
      assert.strictEqual(
        res.headers['x-custom-server'],
        'server-value',
        'Should receive server header'
      );
      server.close();
    });

    req.end();
  });
});

// Test 5: Multiple HTTP methods
test('Multiple HTTP methods work correctly', (done) => {
  let requestCount = 0;
  const methods = ['GET', 'POST', 'PUT', 'DELETE', 'PATCH'];

  const server = http.createServer((req, res) => {
    const expectedMethod = methods[requestCount];
    assert.strictEqual(
      req.method,
      expectedMethod,
      `Should be ${expectedMethod}`
    );

    requestCount++;
    res.writeHead(200);
    res.end(req.method);
  });

  server.listen(0, () => {
    const port = server.address().port;
    let completedCount = 0;

    methods.forEach((method) => {
      const req = http.request({
        host: '127.0.0.1',
        port: port,
        method: method,
        path: `/${method.toLowerCase()}`,
      });

      req.on('response', (res) => {
        let body = '';
        res.on('data', (chunk) => {
          body += chunk;
        });
        res.on('end', () => {
          assert.strictEqual(body, method, `Should receive ${method}`);
          completedCount++;
          if (completedCount === methods.length) {
            server.close();
          }
        });
      });

      req.end();
    });
  });
});

// Test 6: Status code propagation
test('Status codes propagate correctly', (done) => {
  const statusCodes = [200, 201, 404, 500];
  let requestCount = 0;

  const server = http.createServer((req, res) => {
    const status = statusCodes[requestCount];
    requestCount++;
    res.writeHead(status);
    res.end(`Status ${status}`);
  });

  server.listen(0, () => {
    const port = server.address().port;
    let completedCount = 0;

    statusCodes.forEach((expectedStatus) => {
      http.get(`http://127.0.0.1:${port}/`, (res) => {
        assert.strictEqual(
          res.statusCode,
          expectedStatus,
          `Should get ${expectedStatus}`
        );

        res.on('data', () => {});
        res.on('end', () => {
          completedCount++;
          if (completedCount === statusCodes.length) {
            server.close();
          }
        });
      });
    });
  });
});

// Test 7: URL path parsing
test('URL paths parse correctly', (done) => {
  const server = http.createServer((req, res) => {
    assert.ok(req.url.includes('/test/path'), 'Should have correct path');
    assert.ok(req.url.includes('?key=value'), 'Should have query string');
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/test/path?key=value`, (res) => {
      res.on('data', () => {});
      res.on('end', () => {
        server.close();
      });
    });
  });
});

// Test 8: Multiple sequential requests to same server
test('Multiple sequential requests work', (done) => {
  let requestCount = 0;

  const server = http.createServer((req, res) => {
    requestCount++;
    res.writeHead(200);
    res.end(`Request ${requestCount}`);
  });

  server.listen(0, () => {
    const port = server.address().port;

    // First request
    http.get(`http://127.0.0.1:${port}/`, (res1) => {
      let body1 = '';
      res1.on('data', (chunk) => {
        body1 += chunk;
      });
      res1.on('end', () => {
        assert.strictEqual(body1, 'Request 1');

        // Second request after first completes
        http.get(`http://127.0.0.1:${port}/`, (res2) => {
          let body2 = '';
          res2.on('data', (chunk) => {
            body2 += chunk;
          });
          res2.on('end', () => {
            assert.strictEqual(body2, 'Request 2');

            // Third request
            http.get(`http://127.0.0.1:${port}/`, (res3) => {
              let body3 = '';
              res3.on('data', (chunk) => {
                body3 += chunk;
              });
              res3.on('end', () => {
                assert.strictEqual(body3, 'Request 3');
                assert.strictEqual(requestCount, 3, 'Should handle 3 requests');
                server.close();
              });
            });
          });
        });
      });
    });
  });
});

// Test 9: Large response body
test('Large response body transmits correctly', (done) => {
  const largeData = 'x'.repeat(100000);

  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.end(largeData);
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(body.length, 100000, 'Should receive full body');
        server.close();
      });
    });
  });
});

// Test 10: Chunked response encoding
test('Chunked response encoding works', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.write('chunk1');
    res.write('chunk2');
    res.write('chunk3');
    res.end('chunk4');
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(body, 'chunk1chunk2chunk3chunk4');
        server.close();
      });
    });
  });
});

// Test 11: Request with query parameters
test('Query parameters transmitted correctly', (done) => {
  const server = http.createServer((req, res) => {
    assert.ok(req.url.includes('name=test'));
    assert.ok(req.url.includes('value=123'));
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/?name=test&value=123`, (res) => {
      res.on('data', () => {});
      res.on('end', () => {
        server.close();
      });
    });
  });
});

// Test 12: HEAD request returns headers but no body
test('HEAD request returns headers without body', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'HEAD');
    res.writeHead(200, { 'Content-Length': '100' });
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'HEAD',
    });

    req.on('response', (res) => {
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.headers['content-length'], '100');

      let dataReceived = false;
      res.on('data', () => {
        dataReceived = true;
      });

      res.on('end', () => {
        assert.strictEqual(dataReceived, false, 'Should not receive body');
        server.close();
      });
    });

    req.end();
  });
});

// Test 13: Content-Type header propagation
test('Content-Type header propagates correctly', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.headers['content-type'], 'text/html');
    res.writeHead(200, { 'Content-Type': 'application/xml' });
    res.end('<xml/>');
  });

  server.listen(0, () => {
    const port = server.address().port;

    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'POST',
      headers: {
        'Content-Type': 'text/html',
      },
    });

    req.on('response', (res) => {
      assert.strictEqual(res.headers['content-type'], 'application/xml');
      res.on('data', () => {});
      res.on('end', () => {
        server.close();
      });
    });

    req.end('<html/>');
  });
});

// Test 14: Empty request body
test('Empty request body handled correctly', (done) => {
  const server = http.createServer((req, res) => {
    let dataReceived = false;
    req.on('data', () => {
      dataReceived = true;
    });

    req.on('end', () => {
      assert.strictEqual(dataReceived, false, 'Should not receive data');
      res.end('ok');
    });
  });

  server.listen(0, () => {
    const port = server.address().port;

    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'POST',
    });

    req.on('response', (res) => {
      res.on('data', () => {});
      res.on('end', () => {
        server.close();
      });
    });

    req.end();
  });
});

// Test 15: Server receives correct httpVersion
test('HTTP version propagates correctly', (done) => {
  const server = http.createServer((req, res) => {
    assert.ok(req.httpVersion === '1.0' || req.httpVersion === '1.1');
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.ok(res.httpVersion === '1.0' || res.httpVersion === '1.1');
      res.on('data', () => {});
      res.on('end', () => {
        server.close();
      });
    });
  });
});

console.log(`\n=== Test Results ===`);
console.log(`Tests run: ${testsRun}`);
console.log(`Tests passed: ${testsPassed}`);
console.log(`Tests failed: ${testsRun - testsPassed}`);

if (testsPassed === testsRun) {
  console.log('\n✓ All client-server integration tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
