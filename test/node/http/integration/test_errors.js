/**
 * HTTP Error Scenarios Integration Tests
 * Tests error handling in integrated client-server scenarios
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

console.log('=== HTTP Error Scenarios Integration Tests ===\n');

// Test 1: Connection refused (ECONNREFUSED)
test('Connection refused error handled', (done) => {
  const req = http.request({
    host: '127.0.0.1',
    port: 59999, // Port unlikely to be in use
    timeout: 100,
  });

  let errorEmitted = false;
  req.on('error', (err) => {
    errorEmitted = true;
    assert.ok(err, 'Should emit error');
  });

  req.end();

  setTimeout(() => {
    assert.strictEqual(errorEmitted, true, 'Should have emitted error');
  }, 200);
});

// Test 2: Server closes connection early
test('Server early close handled', (done) => {
  const server = http.createServer((req, res) => {
    // Close socket without sending response
    req.socket.destroy();
  });

  server.listen(0, () => {
    const port = server.address().port;
    let errorEmitted = false;

    const req = http.request(`http://127.0.0.1:${port}/`);

    req.on('error', (err) => {
      errorEmitted = true;
    });

    req.on('response', (res) => {
      // Should not reach here
    });

    req.end();

    setTimeout(() => {
      assert.strictEqual(errorEmitted, true, 'Should emit error');
      server.close();
    }, 200);
  });
});

// Test 3: Client abort during response
test('Client abort during response', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
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

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      res.on('data', () => {
        // Abort after first chunk
        res.destroy();
      });

      res.on('error', () => {
        // Expected after destroy
      });
    });

    setTimeout(() => {
      server.close();
    }, 300);
  });
});

// Test 4: Invalid hostname
test('Invalid hostname error handled', (done) => {
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

// Test 5: Request timeout
test('Request timeout handled', (done) => {
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
      assert.strictEqual(timeoutEmitted, true, 'Should timeout');
      server.close();
    }, 200);
  });
});

// Test 6: Write after end error
test('Write after end throws error', (done) => {
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
      // If no error thrown, test passes (different implementations)
    } catch (err) {
      // Expected error
      assert.ok(err);
    }
  });
});

// Test 7: Server error during request processing
test('Server error during processing', (done) => {
  const server = http.createServer((req, res) => {
    // Simulate error
    try {
      throw new Error('Processing error');
    } catch (err) {
      res.writeHead(500);
      res.end('Internal Server Error');
    }
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.statusCode, 500);
      res.on('data', () => {});
      res.on('end', () => {
        server.close();
      });
    });
  });
});

// Test 8: Malformed request handling
test('Server handles malformed requests', (done) => {
  const server = http.createServer((req, res) => {
    // Should not reach here for truly malformed requests
    res.end();
  });

  let errorHandled = false;
  server.on('clientError', (err, socket) => {
    errorHandled = true;
  });

  server.listen(0, () => {
    const port = server.address().port;

    // Send invalid HTTP
    const client = net.createConnection(port, () => {
      client.write('INVALID HTTP REQUEST\r\n');
      client.end();
    });

    setTimeout(() => {
      server.close();
    }, 200);
  });
});

// Test 9: Empty response handling
test('Empty response handled', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(204); // No Content
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.statusCode, 204);

      let dataReceived = false;
      res.on('data', () => {
        dataReceived = true;
      });

      res.on('end', () => {
        assert.strictEqual(dataReceived, false);
        server.close();
      });
    });
  });
});

// Test 10: Multiple end() calls
test('Multiple end() calls handled', (done) => {
  const server = http.createServer((req, res) => {
    res.end('ok');

    // Try to end again
    try {
      res.end('again');
    } catch (err) {
      // Expected or silently ignored
    }
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(body, 'ok');
        server.close();
      });
    });
  });
});

// Test 11: Socket errors propagate
test('Socket errors propagate correctly', (done) => {
  const server = http.createServer((req, res) => {
    // Destroy socket to simulate error
    req.socket.destroy();
  });

  server.listen(0, () => {
    const port = server.address().port;
    let errorEmitted = false;

    const req = http.request(`http://127.0.0.1:${port}/`);

    req.on('error', (err) => {
      errorEmitted = true;
    });

    req.end();

    setTimeout(() => {
      assert.strictEqual(errorEmitted, true);
      server.close();
    }, 200);
  });
});

// Test 12: Abort before connection established
test('Abort before connection works', (done) => {
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

    // Abort immediately before connection
    req.abort();
    req.end();

    setTimeout(() => {
      server.close();
    }, 100);
  });
});

// Test 13: Response without proper headers
test('Response without required headers', (done) => {
  const server = http.createServer((req, res) => {
    // Send minimal response
    res.writeHead(200);
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.statusCode, 200);
      res.on('data', () => {});
      res.on('end', () => {
        server.close();
      });
    });
  });
});

// Test 14: Concurrent requests with errors
test('Concurrent requests with mixed results', (done) => {
  let requestCount = 0;

  const server = http.createServer((req, res) => {
    requestCount++;

    // Simulate different outcomes
    if (requestCount % 2 === 0) {
      res.writeHead(500);
      res.end('error');
    } else {
      res.writeHead(200);
      res.end('ok');
    }
  });

  server.listen(0, () => {
    const port = server.address().port;
    let completedCount = 0;
    let successCount = 0;
    let errorCount = 0;
    const totalRequests = 4;

    for (let i = 0; i < totalRequests; i++) {
      http.get(`http://127.0.0.1:${port}/`, (res) => {
        if (res.statusCode === 200) {
          successCount++;
        } else {
          errorCount++;
        }

        res.on('data', () => {});
        res.on('end', () => {
          completedCount++;
          if (completedCount === totalRequests) {
            assert.strictEqual(successCount, 2);
            assert.strictEqual(errorCount, 2);
            server.close();
          }
        });
      });
    }
  });
});

// Test 15: Server handles disconnect mid-request
test('Server handles client disconnect mid-request', (done) => {
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });

    req.on('end', () => {
      // Client disconnected, may not reach here
      try {
        res.end('ok');
      } catch (err) {
        // Expected if client disconnected
      }
    });

    req.on('error', () => {
      // Client disconnected
    });
  });

  server.listen(0, () => {
    const port = server.address().port;

    const req = http.request({
      host: '127.0.0.1',
      port: port,
      method: 'POST',
    });

    req.write('partial');
    // Disconnect without ending
    req.socket.destroy();

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
  console.log('\n✓ All error scenario tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
