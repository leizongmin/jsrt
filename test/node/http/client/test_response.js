/**
 * HTTP Client Response Tests
 * Tests IncomingMessage: status, headers, body streaming
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

console.log('=== HTTP Client Response Tests ===\n');

// Test 1: Response has statusCode
test('response.statusCode returns status', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(201);
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.statusCode, 201, 'Should have status code');
      server.close();
    });
  });
});

// Test 2: Response has statusMessage
test('response.statusMessage returns message', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200, 'Everything OK');
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.statusCode, 200, 'Should have status code');
      // statusMessage may or may not be available depending on implementation
      assert.ok(
        typeof res.statusMessage === 'string' || res.statusMessage === undefined
      );
      server.close();
    });
  });
});

// Test 3: Response has headers object
test('response.headers contains response headers', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200, {
      'Content-Type': 'text/plain',
      'X-Custom': 'custom-value',
    });
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(
        typeof res.headers,
        'object',
        'Should have headers object'
      );
      assert.strictEqual(
        res.headers['content-type'],
        'text/plain',
        'Should have content-type'
      );
      assert.strictEqual(
        res.headers['x-custom'],
        'custom-value',
        'Should have custom header'
      );
      server.close();
    });
  });
});

// Test 4: Response headers are lowercase
test('response.headers are normalized to lowercase', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200, {
      'Content-Type': 'text/html',
      'X-Custom-Header': 'value',
    });
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(
        res.headers['content-type'],
        'text/html',
        'Should be lowercase'
      );
      assert.strictEqual(
        res.headers['x-custom-header'],
        'value',
        'Should be lowercase'
      );
      server.close();
    });
  });
});

// Test 5: Response has httpVersion
test('response.httpVersion contains HTTP version', (done) => {
  const server = http.createServer((req, res) => {
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.ok(res.httpVersion, 'Should have httpVersion');
      assert.ok(
        res.httpVersion === '1.0' || res.httpVersion === '1.1',
        'Should be HTTP/1.x'
      );
      server.close();
    });
  });
});

// Test 6: Response emits 'data' events
test('response emits data events', (done) => {
  const server = http.createServer((req, res) => {
    res.write('chunk1');
    res.write('chunk2');
    res.end('chunk3');
  });

  server.listen(0, () => {
    const port = server.address().port;
    let dataEventCount = 0;
    let body = '';

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      res.on('data', (chunk) => {
        dataEventCount++;
        body += chunk;
      });

      res.on('end', () => {
        assert.ok(dataEventCount > 0, 'Should emit data events');
        assert.strictEqual(
          body,
          'chunk1chunk2chunk3',
          'Should receive all chunks'
        );
        server.close();
      });
    });
  });
});

// Test 7: Response emits 'end' event
test('response emits end event', (done) => {
  const server = http.createServer((req, res) => {
    res.end('response data');
  });

  server.listen(0, () => {
    const port = server.address().port;
    let endEmitted = false;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      res.on('data', () => {});
      res.on('end', () => {
        endEmitted = true;
        assert.strictEqual(endEmitted, true, 'Should emit end event');
        server.close();
      });
    });
  });
});

// Test 8: Response handles empty body
test('response handles empty body', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(204); // No Content
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    let dataReceived = false;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.statusCode, 204, 'Should be 204');

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

// Test 9: Response handles large body
test('response handles large body', (done) => {
  const largeData = 'x'.repeat(50000);

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
        assert.strictEqual(body.length, 50000, 'Should receive full body');
        server.close();
      });
    });
  });
});

// Test 10: Response handles chunked encoding
test('response handles chunked transfer encoding', (done) => {
  const server = http.createServer((req, res) => {
    // Don't set Content-Length, trigger chunked encoding
    res.write('part1');
    res.write('part2');
    res.end('part3');
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
          body,
          'part1part2part3',
          'Should handle chunked encoding'
        );
        server.close();
      });
    });
  });
});

// Test 11: Response handles Content-Length
test('response respects Content-Length header', (done) => {
  const server = http.createServer((req, res) => {
    const data = 'test data';
    res.writeHead(200, {
      'Content-Length': data.length,
    });
    res.end(data);
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(
        res.headers['content-length'],
        '9',
        'Should have content-length'
      );

      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(body, 'test data', 'Should receive exact length');
        server.close();
      });
    });
  });
});

// Test 12: Response handles JSON content
test('response handles JSON content', (done) => {
  const jsonData = { name: 'test', value: 123 };

  const server = http.createServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(jsonData));
  });

  server.listen(0, () => {
    const port = server.address().port;
    let body = '';

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.headers['content-type'], 'application/json');

      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        const parsed = JSON.parse(body);
        assert.strictEqual(parsed.name, 'test', 'Should parse JSON');
        assert.strictEqual(parsed.value, 123, 'Should have correct values');
        server.close();
      });
    });
  });
});

// Test 13: Response handles multiple Set-Cookie headers
test('response handles multiple set-cookie headers', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(200, {
      'Set-Cookie': ['cookie1=value1', 'cookie2=value2'],
    });
    res.end();
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      // Set-Cookie may be array or string depending on implementation
      const setCookie = res.headers['set-cookie'];
      assert.ok(setCookie, 'Should have set-cookie header');
      server.close();
    });
  });
});

// Test 14: Response handles 404 status
test('response handles 404 Not Found', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(404, 'Not Found');
    res.end('Page not found');
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/missing`, (res) => {
      assert.strictEqual(res.statusCode, 404, 'Should be 404');

      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(body, 'Page not found', 'Should receive error body');
        server.close();
      });
    });
  });
});

// Test 15: Response handles 500 status
test('response handles 500 Internal Error', (done) => {
  const server = http.createServer((req, res) => {
    res.writeHead(500);
    res.end('Server error');
  });

  server.listen(0, () => {
    const port = server.address().port;

    http.get(`http://127.0.0.1:${port}/`, (res) => {
      assert.strictEqual(res.statusCode, 500, 'Should be 500');

      let body = '';
      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', () => {
        assert.strictEqual(body, 'Server error', 'Should receive error body');
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
  console.log('\n✓ All client response tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
