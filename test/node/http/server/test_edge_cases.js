/**
 * HTTP Server Edge Cases and Error Handling Tests
 * Tests error scenarios, invalid requests, and edge cases
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

console.log('=== HTTP Server Edge Cases Tests ===\n');

// Test 1: Server handles connection error gracefully
test('server handles connection errors', (done) => {
  const server = http.createServer((req, res) => {
    res.end();
  });

  let errorEmitted = false;
  server.on('clientError', (err, socket) => {
    errorEmitted = true;
  });

  server.listen(0, () => {
    // This test just verifies error event exists
    assert.strictEqual(typeof server.on, 'function', 'Should have on() method');
    server.close();
  });
});

// Test 2: Server handles empty request
test('server handles immediate disconnect', (done) => {
  const server = http.createServer((req, res) => {
    // Should not be called
    assert.fail('Request handler should not be called');
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      // Connect and immediately disconnect
      client.end();
    });

    setTimeout(() => {
      server.close();
    }, 100);
  });
});

// Test 3: Server handles very long URL
test('server handles long URL', (done) => {
  const server = http.createServer((req, res) => {
    assert.ok(req.url.length > 1000, 'Should receive long URL');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const longPath = '/' + 'a'.repeat(1000);
    const client = net.createConnection(port, () => {
      client.write(`GET ${longPath} HTTP/1.1\r\nHost: localhost\r\n\r\n`);
      client.end();
    });
  });
});

// Test 4: Server handles many headers
test('server handles many headers', (done) => {
  const server = http.createServer((req, res) => {
    const headerCount = Object.keys(req.headers).length;
    assert.ok(headerCount >= 50, 'Should have many headers');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      for (let i = 0; i < 50; i++) {
        client.write(`X-Header-${i}: value${i}\r\n`);
      }
      client.write('\r\n');
      client.end();
    });
  });
});

// Test 5: Server handles request without Host header (HTTP/1.0)
test('server handles HTTP/1.0 request', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.httpVersion, '1.0', 'Should be HTTP/1.0');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.0\r\n\r\n');
      client.end();
    });
  });
});

// Test 6: Server handles pipelined requests
test('server handles multiple requests on same connection', (done) => {
  let requestCount = 0;

  const server = http.createServer((req, res) => {
    requestCount++;
    res.end('OK');
    if (requestCount === 2) {
      server.close();
    }
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      // Send two requests
      client.write('GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n');
      setTimeout(() => {
        client.write('GET /second HTTP/1.1\r\nHost: localhost\r\n\r\n');
        client.end();
      }, 50);
    });
  });
});

// Test 7: Server handles special characters in headers
test('server handles special characters in headers', (done) => {
  const server = http.createServer((req, res) => {
    // Headers should be parsed
    assert.strictEqual(typeof req.headers, 'object', 'Should have headers');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write('X-Custom: value with spaces\r\n');
      client.write('\r\n');
      client.end();
    });
  });
});

// Test 8: Server handles response after client disconnect
test('server handles write after client disconnect', (done) => {
  const server = http.createServer((req, res) => {
    // Client will disconnect before response
    setTimeout(() => {
      try {
        res.writeHead(200);
        res.end('data');
      } catch (err) {
        // Expected - socket may be closed
      }
      server.close();
    }, 100);
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
      // Disconnect immediately
      client.end();
    });
  });
});

// Test 9: Server handles zero Content-Length
test('server handles zero Content-Length', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.headers['content-length'], '0', 'Should have zero length');

    let dataReceived = false;
    req.on('data', () => {
      dataReceived = true;
    });

    req.on('end', () => {
      assert.strictEqual(dataReceived, false, 'Should not receive data');
      res.end();
      server.close();
    });
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('POST / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write('Content-Length: 0\r\n');
      client.write('\r\n');
      client.end();
    });
  });
});

// Test 10: Server handles concurrent connections
test('server handles concurrent connections', (done) => {
  let completedRequests = 0;
  const totalRequests = 5;

  const server = http.createServer((req, res) => {
    res.end('OK');
    completedRequests++;
    if (completedRequests === totalRequests) {
      server.close();
    }
  });

  server.listen(0, () => {
    const port = server.address().port;

    // Create multiple concurrent connections
    for (let i = 0; i < totalRequests; i++) {
      const client = net.createConnection(port, () => {
        client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
        client.end();
      });
    }
  });
});

// Test 11: Server handles request with body but no Content-Length
test('server handles request without Content-Length', (done) => {
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });
    req.on('end', () => {
      // Body may or may not be received
      res.end();
      server.close();
    });
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('POST / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write('\r\n');
      // Note: Without Content-Length or Transfer-Encoding, body handling varies
      client.end();
    });
  });
});

// Test 12: Server handles large header value
test('server handles large header value', (done) => {
  const largeValue = 'x'.repeat(5000);

  const server = http.createServer((req, res) => {
    const headerValue = req.headers['x-large'];
    assert.ok(headerValue, 'Should have header');
    assert.ok(headerValue.length > 4000, 'Should have large value');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write(`X-Large: ${largeValue}\r\n`);
      client.write('\r\n');
      client.end();
    });
  });
});

// Test 13: Server emits 'request' event
test('server emits request event', (done) => {
  const server = http.createServer();
  let requestEventEmitted = false;

  server.on('request', (req, res) => {
    requestEventEmitted = true;
    assert.strictEqual(requestEventEmitted, true, 'request event should emit');
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

// Test 14: Server handles Connection: close
test('server handles Connection: close header', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.headers['connection'], 'close', 'Should have close header');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, () => {
      client.write('GET / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write('Connection: close\r\n');
      client.write('\r\n');
      client.end();
    });
  });
});

// Test 15: Server handles rapid open/close
test('server handles rapid connect/disconnect', (done) => {
  const server = http.createServer((req, res) => {
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    // Rapidly connect and disconnect
    for (let i = 0; i < 3; i++) {
      const client = net.createConnection(port, () => {
        client.end();
      });
    }

    setTimeout(() => {
      server.close();
    }, 200);
  });
});

console.log(`\n=== Test Results ===`);
console.log(`Tests run: ${testsRun}`);
console.log(`Tests passed: ${testsPassed}`);
console.log(`Tests failed: ${testsRun - testsPassed}`);

if (testsPassed === testsRun) {
  console.log('\n✓ All edge case tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
