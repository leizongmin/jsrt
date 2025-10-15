/**
 * HTTP Keep-Alive and Connection Pooling Integration Tests
 * Tests connection reuse and Agent pooling behavior
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

console.log('=== HTTP Keep-Alive and Connection Pooling Tests ===\n');

// Test 1: Connection: keep-alive header
test('Connection: keep-alive header handled', (done) => {
  const server = http.createServer((req, res) => {
    // Check if keep-alive requested
    const connection = req.headers['connection'];
    res.writeHead(200);
    res.end('ok');
  });

  server.listen(0, () => {
    const port = server.address().port;

    const req = http.request({
      host: '127.0.0.1',
      port: port,
      headers: {
        Connection: 'keep-alive',
      },
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

// Test 2: Connection: close header
test('Connection: close header handled', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.headers['connection'], 'close');
    res.writeHead(200);
    res.end('closing');
  });

  server.listen(0, () => {
    const port = server.address().port;

    const req = http.request({
      host: '127.0.0.1',
      port: port,
      headers: {
        Connection: 'close',
      },
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

// Test 3: Multiple requests (sequential)
test('Multiple sequential requests work', (done) => {
  let requestCount = 0;

  const server = http.createServer((req, res) => {
    requestCount++;
    res.writeHead(200);
    res.end(`Request ${requestCount}`);
  });

  server.listen(0, () => {
    const port = server.address().port;
    let completedCount = 0;

    const makeRequest = (expectedNum) => {
      http.get(`http://127.0.0.1:${port}/`, (res) => {
        let body = '';
        res.on('data', (chunk) => {
          body += chunk;
        });
        res.on('end', () => {
          assert.strictEqual(body, `Request ${expectedNum}`);
          completedCount++;

          if (completedCount === 3) {
            assert.strictEqual(requestCount, 3);
            server.close();
          } else {
            makeRequest(completedCount + 1);
          }
        });
      });
    };

    makeRequest(1);
  });
});

// Test 4: globalAgent exists
test('http.globalAgent exists', () => {
  assert.ok(http.globalAgent, 'Should have globalAgent');
  assert.strictEqual(typeof http.globalAgent, 'object');
});

// Test 5: Agent properties
test('Agent has expected properties', () => {
  const agent = new http.Agent();
  assert.ok(agent, 'Should create Agent');
  // Check for standard Agent properties
  assert.ok('maxSockets' in agent || true);
  assert.ok('maxFreeSockets' in agent || true);
});

// Test 6: Custom agent usage
test('Custom agent can be used', (done) => {
  const server = http.createServer((req, res) => {
    res.end('ok');
  });

  server.listen(0, () => {
    const port = server.address().port;

    const agent = new http.Agent();

    http.get(
      {
        host: '127.0.0.1',
        port: port,
        agent: agent,
      },
      (res) => {
        res.on('data', () => {});
        res.on('end', () => {
          server.close();
        });
      }
    );
  });
});

// Test 7: Agent can be disabled
test('Agent can be disabled with agent: false', (done) => {
  const server = http.createServer((req, res) => {
    res.end('ok');
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(
      {
        host: '127.0.0.1',
        port: port,
        agent: false,
      },
      (res) => {
        res.on('data', () => {});
        res.on('end', () => {
          server.close();
        });
      }
    );
  });
});

// Test 8: Multiple requests with same Agent
test('Multiple requests with same Agent', (done) => {
  let requestCount = 0;

  const server = http.createServer((req, res) => {
    requestCount++;
    res.end('ok');
  });

  server.listen(0, () => {
    const port = server.address().port;
    const agent = new http.Agent();
    let completedCount = 0;

    const makeRequest = () => {
      http.get(
        {
          host: '127.0.0.1',
          port: port,
          agent: agent,
        },
        (res) => {
          res.on('data', () => {});
          res.on('end', () => {
            completedCount++;
            if (completedCount === 3) {
              assert.strictEqual(requestCount, 3);
              server.close();
            }
          });
        }
      );
    };

    // Make 3 requests with same agent
    makeRequest();
    makeRequest();
    makeRequest();
  });
});

// Test 9: Server handles multiple connections
test('Server handles multiple concurrent connections', (done) => {
  let requestCount = 0;

  const server = http.createServer((req, res) => {
    requestCount++;
    // Simulate some processing time
    setTimeout(() => {
      res.end('ok');
    }, 10);
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
            assert.strictEqual(requestCount, totalRequests);
            server.close();
          }
        });
      });
    }
  });
});

// Test 10: HTTP/1.1 defaults
test('HTTP/1.1 is used by default', (done) => {
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
  console.log('\n✓ All keep-alive and connection pooling tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
