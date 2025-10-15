/**
 * HTTP Client Request Tests
 * Tests ClientRequest: headers, body, HTTP methods
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

console.log('=== HTTP Client Request Tests ===\n');

// Test 1: setHeader() sets request headers
test('request.setHeader() sets headers', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(
      req.headers['x-custom-header'],
      'custom-value',
      'Should have custom header'
    );
    assert.strictEqual(
      req.headers['x-another'],
      'another-value',
      'Should have another header'
    );
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
    });

    req.setHeader('X-Custom-Header', 'custom-value');
    req.setHeader('X-Another', 'another-value');
    req.end();
  });
});

// Test 2: getHeader() retrieves header value
test('request.getHeader() retrieves header', (done) => {
  const req = http.request('http://httpbin.org/');
  req.setHeader('X-Test', 'test-value');

  const value = req.getHeader('X-Test');
  assert.strictEqual(value, 'test-value', 'Should get header value');

  // Case-insensitive
  const valueLower = req.getHeader('x-test');
  assert.strictEqual(valueLower, 'test-value', 'Should be case-insensitive');

  req.abort();
});

// Test 3: removeHeader() removes header
test('request.removeHeader() removes header', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(
      req.headers['x-removed'],
      undefined,
      'Header should be removed'
    );
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
    });

    req.setHeader('X-Removed', 'will-be-removed');
    req.removeHeader('X-Removed');
    assert.strictEqual(
      req.getHeader('X-Removed'),
      undefined,
      'Should not have header'
    );
    req.end();
  });
});

// Test 4: POST request with body
test('POST request sends body data', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'POST', 'Should be POST');
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });
    req.on('end', () => {
      assert.strictEqual(body, 'POST data', 'Should receive POST data');
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

    req.write('POST data');
    req.end();
  });
});

// Test 5: PUT request with body
test('PUT request sends body data', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'PUT', 'Should be PUT');
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });
    req.on('end', () => {
      assert.strictEqual(body, 'PUT data', 'Should receive PUT data');
      res.end();
      server.close();
    });
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'PUT',
      path: '/resource',
    });

    req.end('PUT data');
  });
});

// Test 6: DELETE request
test('DELETE request method works', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'DELETE', 'Should be DELETE');
    res.writeHead(204); // No Content
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'DELETE',
      path: '/resource',
    });

    req.on('response', (res) => {
      assert.strictEqual(res.statusCode, 204, 'Should get 204 status');
    });

    req.end();
  });
});

// Test 7: HEAD request
test('HEAD request method works', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'HEAD', 'Should be HEAD');
    res.writeHead(200, { 'Content-Length': '100' });
    res.end(); // No body for HEAD
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'HEAD',
      path: '/',
    });

    req.on('response', (res) => {
      assert.strictEqual(res.statusCode, 200, 'Should get 200 status');
      assert.strictEqual(
        res.headers['content-length'],
        '100',
        'Should have content-length'
      );
      server.close();
    });

    req.end();
  });
});

// Test 8: OPTIONS request
test('OPTIONS request method works', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'OPTIONS', 'Should be OPTIONS');
    res.writeHead(200, { Allow: 'GET, POST, PUT, DELETE' });
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'OPTIONS',
      path: '*',
    });

    req.on('response', (res) => {
      assert.ok(res.headers['allow'], 'Should have Allow header');
      server.close();
    });

    req.end();
  });
});

// Test 9: PATCH request with body
test('PATCH request sends body data', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'PATCH', 'Should be PATCH');
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });
    req.on('end', () => {
      assert.strictEqual(body, 'PATCH data', 'Should receive PATCH data');
      res.end();
      server.close();
    });
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'PATCH',
      path: '/resource',
    });

    req.end('PATCH data');
  });
});

// Test 10: JSON body in POST request
test('POST request sends JSON body', (done) => {
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });
    req.on('end', () => {
      const data = JSON.parse(body);
      assert.strictEqual(data.name, 'test', 'Should parse JSON');
      assert.strictEqual(data.value, 123, 'Should have correct value');
      res.end();
      server.close();
    });
  });

  server.listen(0, () => {
    const port = server.address().port;
    const data = JSON.stringify({ name: 'test', value: 123 });

    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(data),
      },
    });

    req.end(data);
  });
});

// Test 11: Multiple writes before end()
test('Multiple write() calls work correctly', (done) => {
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });
    req.on('end', () => {
      assert.strictEqual(body, 'part1part2part3', 'Should receive all parts');
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
    });

    req.write('part1');
    req.write('part2');
    req.write('part3');
    req.end();
  });
});

// Test 12: Large request body
test('Request handles large body', (done) => {
  const largeData = 'x'.repeat(10000);

  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });
    req.on('end', () => {
      assert.strictEqual(body.length, 10000, 'Should receive full body');
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
    });

    req.end(largeData);
  });
});

// Test 13: Request with query string in path
test('Request preserves query string in path', (done) => {
  const server = http.createServer((req, res) => {
    assert.ok(req.url.includes('?key=value'), 'Should have query string');
    assert.ok(req.url.includes('&foo=bar'), 'Should have multiple params');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      path: '/path?key=value&foo=bar',
    });

    req.end();
  });
});

// Test 14: Request 'socket' event
test('Request emits socket event', (done) => {
  const server = http.createServer((req, res) => {
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
    });

    let socketEventEmitted = false;
    req.on('socket', (socket) => {
      socketEventEmitted = true;
      assert.strictEqual(typeof socket, 'object', 'Should provide socket');
    });

    req.on('response', () => {
      assert.strictEqual(socketEventEmitted, true, 'Socket event should emit');
    });

    req.end();
  });
});

// Test 15: flushHeaders() sends headers immediately
test('request.flushHeaders() sends headers', (done) => {
  const server = http.createServer((req, res) => {
    // Headers received, even if body not yet sent
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'POST',
    });

    assert.strictEqual(
      typeof req.flushHeaders,
      'function',
      'Should have flushHeaders'
    );
    req.flushHeaders();

    // Send body later
    setTimeout(() => {
      req.end('delayed body');
    }, 10);
  });
});

console.log(`\n=== Test Results ===`);
console.log(`Tests run: ${testsRun}`);
console.log(`Tests passed: ${testsPassed}`);
console.log(`Tests failed: ${testsRun - testsPassed}`);

if (testsPassed === testsRun) {
  console.log('\n✓ All client request tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
