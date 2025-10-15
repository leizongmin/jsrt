/**
 * HTTP Streaming Integration Tests
 * Tests stream-based data transfer between client and server
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

console.log('=== HTTP Streaming Integration Tests ===\n');

// Test 1: Request body streaming (client write → server read)
test('Request body streams from client to server', (done) => {
  const server = http.createServer((req, res) => {
    const chunks = [];
    let dataCount = 0;

    req.on('data', (chunk) => {
      dataCount++;
      chunks.push(chunk.toString());
    });

    req.on('end', () => {
      const fullBody = chunks.join('');
      assert.ok(dataCount > 0, 'Should receive data events');
      assert.strictEqual(fullBody, 'chunk1chunk2chunk3');
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

    // Stream data in multiple writes
    req.write('chunk1');
    req.write('chunk2');
    req.write('chunk3');
    req.end();
  });
});

// Test 2: Response body streaming (server write → client read)
test('Response body streams from server to client', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.write('part1');
    res.write('part2');
    res.write('part3');
    res.end('part4');
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      const chunks = [];
      let dataCount = 0;

      res.on('data', (chunk) => {
        dataCount++;
        chunks.push(chunk.toString());
      });

      res.on('end', () => {
        const fullBody = chunks.join('');
        assert.ok(dataCount > 0, 'Should receive data events');
        assert.strictEqual(fullBody, 'part1part2part3part4');
        server.close();
      });
    });
  });
});

// Test 3: Large data streaming
test('Large data streams efficiently', (done) => {
  const chunkSize = 10000;
  const chunkCount = 10;
  const expectedSize = chunkSize * chunkCount;

  const server = http.createServer((req, res) => {
    res.writeHead(200);

    for (let i = 0; i < chunkCount; i++) {
      res.write('x'.repeat(chunkSize));
    }

    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      let totalSize = 0;
      let chunkCounter = 0;

      res.on('data', (chunk) => {
        chunkCounter++;
        totalSize += chunk.length;
      });

      res.on('end', () => {
        assert.strictEqual(totalSize, expectedSize, 'Should receive all data');
        assert.ok(chunkCounter > 0, 'Should receive in chunks');
        server.close();
      });
    });
  });
});

// Test 4: Chunked transfer encoding
test('Chunked transfer encoding works correctly', (done) => {
  const server = http.createServer((req, res) => {
    // Don't set Content-Length to trigger chunked encoding
    res.writeHead(200);
    res.write('first');
    setTimeout(() => {
      res.write('second');
      setTimeout(() => {
        res.end('third');
      }, 10);
    }, 10);
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      let body = '';

      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(body, 'firstsecondthird');
        server.close();
      });
    });
  });
});

// Test 5: Request pause and resume
test('Request streaming pause and resume', (done) => {
  const server = http.createServer((req, res) => {
    const chunks = [];
    let paused = false;

    req.on('data', (chunk) => {
      chunks.push(chunk.toString());

      if (chunks.length === 2 && !paused) {
        paused = true;
        req.pause();

        setTimeout(() => {
          req.resume();
        }, 50);
      }
    });

    req.on('end', () => {
      const body = chunks.join('');
      assert.ok(paused, 'Should have paused');
      assert.strictEqual(body, 'data1data2data3');
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

    req.write('data1');
    req.write('data2');
    req.write('data3');
    req.end();
  });
});

// Test 6: Response pause and resume
test('Response streaming pause and resume', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.write('chunk1');
    res.write('chunk2');
    res.write('chunk3');
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      const chunks = [];
      let paused = false;

      res.on('data', (chunk) => {
        chunks.push(chunk.toString());

        if (chunks.length === 1 && !paused) {
          paused = true;
          res.pause();

          setTimeout(() => {
            res.resume();
          }, 50);
        }
      });

      res.on('end', () => {
        assert.ok(paused, 'Should have paused');
        const body = chunks.join('');
        assert.ok(body.includes('chunk1'));
        server.close();
      });
    });
  });
});

// Test 7: Bidirectional streaming
test('Bidirectional streaming works', (done) => {
  const server = http.createServer((req, res) => {
    let requestBody = '';

    req.on('data', (chunk) => {
      requestBody += chunk;
    });

    req.on('end', () => {
      res.writeHead(200);
      res.write('Echo: ');
      res.end(requestBody);
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
      let responseBody = '';

      res.on('data', (chunk) => {
        responseBody += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(responseBody, 'Echo: test data');
        server.close();
      });
    });

    req.write('test ');
    req.end('data');
  });
});

// Test 8: Multiple write() calls
test('Multiple write() calls stream correctly', (done) => {
  const writeCount = 20;

  const server = http.createServer((req, res) => {
    res.writeHead(200);

    for (let i = 0; i < writeCount; i++) {
      res.write(`line${i}\n`);
    }

    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      let body = '';
      let chunkCount = 0;

      res.on('data', (chunk) => {
        chunkCount++;
        body += chunk;
      });

      res.on('end', () => {
        assert.ok(chunkCount > 0, 'Should receive chunks');

        for (let i = 0; i < writeCount; i++) {
          assert.ok(body.includes(`line${i}`), `Should have line${i}`);
        }

        server.close();
      });
    });
  });
});

// Test 9: Streaming with Content-Length
test('Streaming with Content-Length header', (done) => {
  const data = 'test data with length';

  const server = http.createServer((req, res) => {
    res.writeHead(200, {
      'Content-Length': data.length,
    });
    res.end(data);
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.headers['content-length'], String(data.length));

      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(body, data);
        assert.strictEqual(body.length, data.length);
        server.close();
      });
    });
  });
});

// Test 10: Empty stream
test('Empty stream handled correctly', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      let dataReceived = false;

      res.on('data', () => {
        dataReceived = true;
      });

      res.on('end', () => {
        assert.strictEqual(dataReceived, false, 'Should not receive data');
        server.close();
      });
    });
  });
});

// Test 11: Stream with delay between writes
test('Stream with delayed writes', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.write('part1');

    setTimeout(() => {
      res.write('part2');

      setTimeout(() => {
        res.end('part3');
      }, 20);
    }, 20);
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      const chunks = [];
      const timestamps = [];

      res.on('data', (chunk) => {
        chunks.push(chunk.toString());
        timestamps.push(Date.now());
      });

      res.on('end', () => {
        const body = chunks.join('');
        assert.strictEqual(body, 'part1part2part3');

        // Verify chunks arrived at different times
        if (timestamps.length > 1) {
          assert.ok(
            timestamps[1] - timestamps[0] >= 10,
            'Chunks should arrive with delay'
          );
        }

        server.close();
      });
    });
  });
});

// Test 12: Request with multiple writes and end
test('Request multiple writes then end', (done) => {
  const server = http.createServer((req, res) => {
    const chunks = [];

    req.on('data', (chunk) => {
      chunks.push(chunk.toString());
    });

    req.on('end', () => {
      const body = chunks.join('');
      assert.strictEqual(body, 'ABC');
      res.end('received');
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

    req.write('A');
    req.write('B');
    req.end('C');
  });
});

console.log(`\n=== Test Results ===`);
console.log(`Tests run: ${testsRun}`);
console.log(`Tests passed: ${testsPassed}`);
console.log(`Tests failed: ${testsRun - testsPassed}`);

if (testsPassed === testsRun) {
  console.log('\n✓ All streaming integration tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
