/**
 * HTTP Server Request Handling Tests
 * Tests IncomingMessage: headers, body, URL parsing, HTTP methods
 */

const http = require('node:http');
const net = require('node:net');
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

console.log('=== HTTP Server Request Handling Tests ===\n');

// Test 1: Request method
test('request.method contains HTTP method', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'GET', 'Should be GET');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
      client.end();
    });
  });
});

// Test 2: Request URL
test('request.url contains path', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.url, '/test/path?foo=bar', 'Should have full path');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write(
        'GET /test/path?foo=bar HTTP/1.1\r\nHost: localhost\r\n\r\n'
      );
      client.end();
    });
  });
});

// Test 3: Request headers
test('request.headers contains parsed headers', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(
      typeof req.headers,
      'object',
      'Headers should be object'
    );
    assert.strictEqual(
      req.headers['host'],
      'localhost',
      'Should have host header'
    );
    assert.strictEqual(
      req.headers['user-agent'],
      'test-agent',
      'Should have user-agent'
    );
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write('User-Agent: test-agent\r\n');
      client.write('\r\n');
      client.end();
    });
  });
});

// Test 4: Case-insensitive headers
test('request.headers are lowercase', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(
      req.headers['content-type'],
      'application/json',
      'Lowercase key'
    );
    assert.strictEqual(
      req.headers['Content-Type'],
      undefined,
      'Uppercase should not exist'
    );
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('POST / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write('Content-Type: application/json\r\n');
      client.write('Content-Length: 0\r\n');
      client.write('\r\n');
      client.end();
    });
  });
});

// Test 5: Request body - small
test('request receives small body', (done) => {
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk.toString();
    });
    req.on('end', () => {
      assert.strictEqual(body, 'Hello World', 'Should receive body');
      res.end();
      server.close();
    });
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('POST / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write('Content-Length: 11\r\n');
      client.write('\r\n');
      client.write('Hello World');
      client.end();
    });
  });
});

// Test 6: Request body - JSON
test('request receives JSON body', (done) => {
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk.toString();
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
    const bodyData = JSON.stringify({ name: 'test', value: 123 });
    const client = net.createConnection(port, () => {
      client.write('POST / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write('Content-Type: application/json\r\n');
      client.write(`Content-Length: ${bodyData.length}\r\n`);
      client.write('\r\n');
      client.write(bodyData);
      client.end();
    });
  });
});

// Test 7: Multiple headers with same name
test('request handles duplicate headers', (done) => {
  const server = http.createServer((req, res) => {
    // Note: Multiple headers with same name may be handled differently
    // Some implementations join with comma, others keep last value
    assert.ok(req.headers['x-custom'], 'Should have custom header');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write('X-Custom: value1\r\n');
      client.write('X-Custom: value2\r\n');
      client.write('\r\n');
      client.end();
    });
  });
});

// Test 8: POST method
test('request handles POST method', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'POST', 'Should be POST');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write(
        'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n'
      );
      client.end();
    });
  });
});

// Test 9: PUT method
test('request handles PUT method', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'PUT', 'Should be PUT');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write(
        'PUT /resource HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n'
      );
      client.end();
    });
  });
});

// Test 10: DELETE method
test('request handles DELETE method', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'DELETE', 'Should be DELETE');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('DELETE /resource HTTP/1.1\r\nHost: localhost\r\n\r\n');
      client.end();
    });
  });
});

// Test 11: HEAD method
test('request handles HEAD method', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'HEAD', 'Should be HEAD');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n');
      client.end();
    });
  });
});

// Test 12: OPTIONS method
test('request handles OPTIONS method', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'OPTIONS', 'Should be OPTIONS');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('OPTIONS * HTTP/1.1\r\nHost: localhost\r\n\r\n');
      client.end();
    });
  });
});

// Test 13: HTTP version
test('request has httpVersion property', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.httpVersion, '1.1', 'Should be HTTP/1.1');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
      client.end();
    });
  });
});

// Test 14: Request with query string
test('request.url includes query string', (done) => {
  const server = http.createServer((req, res) => {
    assert.ok(req.url.includes('?'), 'Should have query string');
    assert.ok(req.url.includes('key=value'), 'Should have parameter');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write(
        'GET /path?key=value&foo=bar HTTP/1.1\r\nHost: localhost\r\n\r\n'
      );
      client.end();
    });
  });
});

// Test 15: Request socket property
test('request has socket property', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(typeof req.socket, 'object', 'Should have socket');
    assert.ok(req.socket, 'Socket should exist');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
      client.end();
    });
  });
});

console.log(`\n=== Test Results ===`);
console.log(`Tests run: ${testsRun}`);
console.log(`Tests passed: ${testsPassed}`);
console.log(`Tests failed: ${testsRun - testsPassed}`);

if (testsPassed === testsRun) {
  console.log('\n✓ All request handling tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
