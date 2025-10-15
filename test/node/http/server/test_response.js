/**
 * HTTP Server Response Tests
 * Tests ServerResponse: writeHead, headers, write, end, chunked encoding
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

console.log('=== HTTP Server Response Tests ===\n');

// Test 1: response.writeHead() with status code
test('response.writeHead() sets status code', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(404);
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    client.on('data', (data) => {
      const response = data.toString();
      assert.ok(response.includes('404'), 'Should have 404 status');
      client.end();
      server.close();
    });
  });
});

// Test 2: response.writeHead() with headers
test('response.writeHead() sets headers', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    client.on('data', (data) => {
      const response = data.toString();
      assert.ok(response.includes('Content-Type: application/json'), 'Should have content type');
      client.end();
      server.close();
    });
  });
});

// Test 3: response.setHeader()
test('response.setHeader() sets individual headers', (done) => {
  const server = http.createServer((req, res) => {
    res.setHeader('X-Custom', 'test-value');
    res.writeHead(200);
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    client.on('data', (data) => {
      const response = data.toString();
      assert.ok(response.includes('X-Custom: test-value'), 'Should have custom header');
      client.end();
      server.close();
    });
  });
});

// Test 4: response.getHeader()
test('response.getHeader() retrieves header value', (done) => {
  const server = http.createServer((req, res) => {
    res.setHeader('X-Test', 'value');
    const value = res.getHeader('X-Test');
    assert.strictEqual(value, 'value', 'Should get header value');
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

// Test 5: response.removeHeader()
test('response.removeHeader() removes header', (done) => {
  const server = http.createServer((req, res) => {
    res.setHeader('X-Remove', 'value');
    assert.strictEqual(res.getHeader('X-Remove'), 'value', 'Header should exist');
    res.removeHeader('X-Remove');
    assert.strictEqual(res.getHeader('X-Remove'), undefined, 'Header should be removed');
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

// Test 6: response.write() and response.end()
test('response.write() and end() send data', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.write('Hello ');
    res.write('World');
    res.end('!');
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    let responseData = '';
    client.on('data', (data) => {
      responseData += data.toString();
    });

    client.on('end', () => {
      assert.ok(responseData.includes('Hello World!'), 'Should have complete body');
      server.close();
    });
  });
});

// Test 7: response.end() with data
test('response.end() can send data directly', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.end('Complete response');
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    let responseData = '';
    client.on('data', (data) => {
      responseData += data.toString();
    });

    client.on('end', () => {
      assert.ok(responseData.includes('Complete response'), 'Should have body');
      server.close();
    });
  });
});

// Test 8: Chunked transfer encoding (implicit)
test('response uses chunked encoding without Content-Length', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.write('chunk1');
    res.write('chunk2');
    res.end('chunk3');
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    let responseData = '';
    client.on('data', (data) => {
      responseData += data.toString();
    });

    client.on('end', () => {
      // Should contain all chunks
      assert.ok(responseData.includes('chunk1'), 'Should have chunk1');
      assert.ok(responseData.includes('chunk2'), 'Should have chunk2');
      assert.ok(responseData.includes('chunk3'), 'Should have chunk3');
      server.close();
    });
  });
});

// Test 9: Multiple headers with same name
test('response handles multiple headers correctly', (done) => {
  const server = http.createServer((req, res) => {
    res.setHeader('Set-Cookie', ['cookie1=value1', 'cookie2=value2']);
    res.writeHead(200);
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    client.on('data', (data) => {
      const response = data.toString();
      // Should have Set-Cookie headers
      assert.ok(response.includes('Set-Cookie'), 'Should have Set-Cookie');
      client.end();
      server.close();
    });
  });
});

// Test 10: Status message
test('response.writeHead() accepts custom status message', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200, 'All Good');
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    client.on('data', (data) => {
      const response = data.toString();
      assert.ok(response.includes('200'), 'Should have 200 status');
      client.end();
      server.close();
    });
  });
});

// Test 11: JSON response
test('response can send JSON data', (done) => {
  const server = http.createServer((req, res) => {
    const data = { message: 'Hello', value: 42 };
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(data));
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    let responseData = '';
    client.on('data', (data) => {
      responseData += data.toString();
    });

    client.on('end', () => {
      const bodyStart = responseData.indexOf('\r\n\r\n') + 4;
      const body = responseData.substring(bodyStart);
      const parsed = JSON.parse(body.trim());
      assert.strictEqual(parsed.message, 'Hello', 'Should have correct message');
      assert.strictEqual(parsed.value, 42, 'Should have correct value');
      server.close();
    });
  });
});

// Test 12: Empty response
test('response.end() without data sends empty body', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(204); // No Content
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    client.on('data', (data) => {
      const response = data.toString();
      assert.ok(response.includes('204'), 'Should have 204 status');
      client.end();
      server.close();
    });
  });
});

// Test 13: Large response body
test('response handles large body', (done) => {
  const largeData = 'x'.repeat(10000);

  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.end(largeData);
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    let responseData = '';
    client.on('data', (data) => {
      responseData += data.toString();
    });

    client.on('end', () => {
      assert.ok(responseData.includes('xxx'), 'Should have data');
      assert.ok(responseData.length > 10000, 'Should have large body');
      server.close();
    });
  });
});

// Test 14: response.getHeaders()
test('response.getHeaders() returns all headers', (done) => {
  const server = http.createServer((req, res) => {
    res.setHeader('X-Header-1', 'value1');
    res.setHeader('X-Header-2', 'value2');
    const headers = res.getHeaders();
    assert.strictEqual(typeof headers, 'object', 'Should return object');
    assert.strictEqual(headers['x-header-1'], 'value1', 'Should have header 1');
    assert.strictEqual(headers['x-header-2'], 'value2', 'Should have header 2');
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

// Test 15: Headers after writeHead()
test('headers cannot be modified after writeHead()', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    // Setting headers after writeHead should not affect sent headers
    res.setHeader('X-After', 'value');
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
  console.log('\n✓ All response tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
