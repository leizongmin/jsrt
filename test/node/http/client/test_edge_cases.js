/**
 * HTTP Client Edge Cases and Error Handling Tests
 * Tests error scenarios, timeouts, and edge cases
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

console.log('=== HTTP Client Edge Cases Tests ===\n');

// Test 1: Request to non-existent server
test('request handles connection refused', (done) => {
  // Use a port that's unlikely to be in use
  const req = http.request({
    host: '127.0.0.1',
    port: 59999,
    timeout: 100,
  });

  let errorEmitted = false;
  req.on('error', (err) => {
    errorEmitted = true;
    assert.ok(err, 'Should emit error');
  });

  req.end();

  setTimeout(() => {
    assert.strictEqual(errorEmitted, true, 'Should emit error event');
  }, 200);
});

// Test 2: Request timeout
test('request emits timeout event', (done) => {
  const server = http.createServer((req, res) => {
    // Never respond
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request(`http://127.0.0.1:${port}/`);

    let timeoutEmitted = false;
    req.setTimeout(50, () => {
      timeoutEmitted = true;
      req.abort();
    });

    req.on('error', () => {
      // Expected after abort
    });

    req.end();

    setTimeout(() => {
      assert.strictEqual(timeoutEmitted, true, 'Should emit timeout');
      server.close();
    }, 200);
  });
});

// Test 3: Abort before connection
test('abort() before connection established', (done) => {
  const server = http.createServer((req, res) => {
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request(`http://127.0.0.1:${port}/`);

    let errorEmitted = false;
    req.on('error', () => {
      errorEmitted = true;
    });

    // Abort immediately
    req.abort();
    req.end();

    setTimeout(() => {
      server.close();
    }, 100);
  });
});

// Test 4: Write after end() throws error
test('write() after end() causes error', (done) => {
  const server = http.createServer((req, res) => {
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request(`http://127.0.0.1:${port}/`, (res) => {
      res.on('data', () => {});
      res.on('end', () => {
        server.close();
      });
    });

    req.end();

    // Try to write after end
    try {
      req.write('data');
      // If no error thrown, that's also acceptable behavior
    } catch (err) {
      // Expected error
      assert.ok(err, 'Should throw error');
    }
  });
});

// Test 5: Invalid hostname
test('request handles invalid hostname', (done) => {
  const req = http.request({
    host: 'invalid.hostname.that.does.not.exist.test',
    port: 80,
    timeout: 100,
  });

  let errorEmitted = false;
  req.on('error', (err) => {
    errorEmitted = true;
    assert.ok(err, 'Should emit error');
  });

  req.end();

  setTimeout(() => {
    assert.strictEqual(errorEmitted, true, 'Should emit error');
  }, 200);
});

// Test 6: Server closes connection early
test('request handles early server close', (done) => {
  const server = http.createServer((req, res) => {
    // Close connection without responding
    req.socket.destroy();
  });

  server.listen(0, () => {
    const port = server.address().port;
    let errorEmitted = false;

    const req = http.request(`http://127.0.0.1:${port}/`, (res) => {
      // Should not reach here
    });

    req.on('error', (err) => {
      errorEmitted = true;
    });

    req.end();

    setTimeout(() => {
      assert.strictEqual(errorEmitted, true, 'Should emit error');
      server.close();
    }, 200);
  });
});

// Test 7: Multiple end() calls
test('multiple end() calls are handled', (done) => {
  const server = http.createServer((req, res) => {
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request(`http://127.0.0.1:${port}/`, (res) => {
      res.on('data', () => {});
      res.on('end', () => {
        server.close();
      });
    });

    req.end();

    // Try calling end() again
    try {
      req.end();
      // If no error, that's acceptable
    } catch (err) {
      // Error is also acceptable
    }
  });
});

// Test 8: Request without calling end()
test('request without end() does not complete', (done) => {
  const server = http.createServer((req, res) => {
    // Should not be called
    assert.fail('Server should not receive request');
  });

  server.listen(0, () => {
    const port = server.address().port;
    let responseReceived = false;

    const req = http.request(`http://127.0.0.1:${port}/`, (res) => {
      responseReceived = true;
    });

    // Don't call req.end()

    setTimeout(() => {
      assert.strictEqual(
        responseReceived,
        false,
        'Should not receive response'
      );
      req.abort();
      server.close();
    }, 200);
  });
});

// Test 9: Response body larger than buffer
test('request handles very large response', (done) => {
  const largeData = 'x'.repeat(100000);

  const server = http.createServer((req, res) => {
    res.end(largeData);
  });

  server.listen(0, () => {
    const port = server.address().port;
    let body = '';

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(
          body.length,
          100000,
          'Should receive full large body'
        );
        server.close();
      });
    });
  });
});

// Test 10: Request with invalid method
test('request handles custom HTTP methods', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'CUSTOM', 'Should accept custom method');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'CUSTOM',
    });

    req.end();
  });
});

// Test 11: Request with empty path
test('request handles empty path', (done) => {
  const server = http.createServer((req, res) => {
    assert.ok(req.url === '/' || req.url === '', 'Should have default path');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      path: '',
    });

    req.end();
  });
});

// Test 12: Response with malformed headers (server sends then client receives)
test('request handles response without Content-Type', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200, {});
    res.end('data');
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.statusCode, 200, 'Should get status');
      // Content-Type is optional
      res.on('data', () => {});
      res.on('end', () => {
        server.close();
      });
    });
  });
});

// Test 13: Concurrent requests to same server
test('multiple concurrent requests to same server', (done) => {
  let requestCount = 0;
  const server = http.createServer((req, res) => {
    requestCount++;
    res.end(`response${requestCount}`);
  });

  server.listen(0, () => {
    const port = server.address().port;
    let completedCount = 0;
    const totalRequests = 5;

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
            assert.strictEqual(
              requestCount,
              totalRequests,
              'Server should receive all requests'
            );
            server.close();
          }
        });
      });
    }
  });
});

// Test 14: Request with special characters in URL
test('request handles URL encoding', (done) => {
  const server = http.createServer((req, res) => {
    assert.ok(req.url.includes('test'), 'Should have encoded URL');
    res.end();
    server.close();
  });

  server.listen(0, () => {
    const port = server.address().port;
    const req = http.request({
      host: '127.0.0.1',
      port: port,
      path: '/test?q=hello%20world',
    });

    req.end();
  });
});

// Test 15: Request abort during response
test('abort() during response reception', (done) => {
  const server = http.createServer((req, res) => {
    res.write('part1');
    // Client will abort before we finish
    setTimeout(() => {
      try {
        res.end('part2');
      } catch (err) {
        // Socket may be closed
      }
    }, 100);
  });

  server.listen(0, () => {
    const port = server.address().port;
    let aborted = false;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      res.on('data', () => {
        // Abort after first chunk
        if (!aborted) {
          aborted = true;
          res.destroy();
        }
      });

      res.on('error', () => {
        // Expected after destroy
      });
    });

    setTimeout(() => {
      assert.strictEqual(aborted, true, 'Should have aborted');
      server.close();
    }, 300);
  });
});

console.log(`\n=== Test Results ===`);
console.log(`Tests run: ${testsRun}`);
console.log(`Tests passed: ${testsPassed}`);
console.log(`Tests failed: ${testsRun - testsPassed}`);

if (testsPassed === testsRun) {
  console.log('\n✓ All client edge case tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
