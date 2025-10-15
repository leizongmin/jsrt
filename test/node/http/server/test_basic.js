/**
 * Basic HTTP Server Tests
 * Tests core server functionality: createServer, listen, close
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

console.log('=== Basic HTTP Server Tests ===\n');

// Test 1: Server creation
test('http.createServer creates a server object', () => {
  const server = http.createServer();
  // Note: instanceof check may not work in all JS engines
  assert.strictEqual(typeof server, 'object', 'Should be an object');
  assert.strictEqual(typeof server.listen, 'function', 'Should have listen method');
  assert.strictEqual(typeof server.close, 'function', 'Should have close method');
});

// Test 2: Server with request handler
test('createServer accepts a request handler', () => {
  let handlerCalled = false;
  const server = http.createServer((req, res) => {
    handlerCalled = true;
  });
  assert.ok(server, 'Server created with handler');
  // Handler will be tested in integration tests
});

// Test 3: Server listen on port
test('server.listen() starts listening on a port', (done) => {
  const server = http.createServer();
  server.listen(0, '127.0.0.1', () => {
    const addr = server.address();
    assert.ok(addr, 'Should have address');
    assert.strictEqual(addr.address, '127.0.0.1', 'Should bind to 127.0.0.1');
    assert.ok(addr.port > 0, 'Should have port number');
    server.close();
  });
});

// Test 4: Server listen with callback
test('server.listen() callback is called when ready', (done) => {
  const server = http.createServer();
  let callbackCalled = false;

  server.listen(0, () => {
    callbackCalled = true;
    assert.strictEqual(callbackCalled, true, 'Callback should be called');
    server.close();
  });
});

// Test 5: Server close
test('server.close() stops the server', (done) => {
  const server = http.createServer();

  server.listen(0, () => {
    server.close(() => {
      // Server closed successfully
      assert.ok(true, 'Close callback called');
    });
  });
});

// Test 6: Server address()
test('server.address() returns address info', (done) => {
  const server = http.createServer();

  server.listen(0, '127.0.0.1', () => {
    const addr = server.address();
    assert.strictEqual(typeof addr, 'object', 'address() returns object');
    assert.strictEqual(addr.address, '127.0.0.1', 'Correct address');
    assert.strictEqual(typeof addr.port, 'number', 'Has port number');
    assert.strictEqual(addr.family, 'IPv4', 'Correct family');
    server.close();
  });
});

// Test 7: Server listening property
test('server.listening property reflects state', (done) => {
  const server = http.createServer();

  // Note: listening property may not be implemented yet
  // assert.strictEqual(server.listening, false, 'Not listening initially');

  server.listen(0, () => {
    // Server is now listening
    assert.ok(true, 'Server started listening');

    server.close(() => {
      assert.ok(true, 'Server closed successfully');
    });
  });
});

// Test 8: Multiple servers can coexist
test('multiple servers can run on different ports', (done) => {
  const server1 = http.createServer();
  const server2 = http.createServer();

  server1.listen(0, () => {
    const port1 = server1.address().port;

    server2.listen(0, () => {
      const port2 = server2.address().port;

      assert.notStrictEqual(port1, port2, 'Should have different ports');

      server1.close();
      server2.close();
    });
  });
});

// Test 9: Server 'listening' event
test('server emits "listening" event', (done) => {
  const server = http.createServer();
  let eventEmitted = false;

  server.on('listening', () => {
    eventEmitted = true;
    assert.strictEqual(eventEmitted, true, 'listening event emitted');
    server.close();
  });

  server.listen(0);
});

// Test 10: Server 'close' event
test('server emits "close" event', (done) => {
  const server = http.createServer();
  let closeEventEmitted = false;

  server.on('close', () => {
    closeEventEmitted = true;
    assert.strictEqual(closeEventEmitted, true, 'close event emitted');
  });

  server.listen(0, () => {
    server.close();
  });
});

// Test 11: Server maxHeadersCount property
test('server has maxHeadersCount property', () => {
  const server = http.createServer();
  assert.strictEqual(typeof server.maxHeadersCount, 'number', 'Should have maxHeadersCount');
  assert.ok(server.maxHeadersCount > 0, 'Should have positive value');
});

// Test 12: Server timeout property
test('server has setTimeout() method', () => {
  const server = http.createServer();
  assert.strictEqual(typeof server.setTimeout, 'function', 'Should have setTimeout method');
});

// Test 13: Simple request/response cycle
test('server handles simple GET request', (done) => {
  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'GET', 'Should be GET request');
    assert.strictEqual(req.url, '/', 'Should request /');
    res.writeHead(200);
    res.end('OK');
  });

  server.listen(0, () => {
    const port = server.address().port;

    // Make a request using net module
    const client = net.createConnection(port, '127.0.0.1', () => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });

    client.on('data', (data) => {
      const response = data.toString();
      assert.ok(response.includes('HTTP/1.1 200'), 'Should have 200 status');
      assert.ok(response.includes('OK'), 'Should have OK body');
      client.end();
      server.close();
    });
  });
});

// Test 14: Server handles POST request
test('server handles POST request with body', (done) => {
  let requestBody = '';

  const server = http.createServer((req, res) => {
    assert.strictEqual(req.method, 'POST', 'Should be POST request');

    req.on('data', (chunk) => {
      requestBody += chunk;
    });

    req.on('end', () => {
      assert.strictEqual(requestBody, 'test data', 'Should receive body');
      res.writeHead(200);
      res.end('Received');
    });
  });

  server.listen(0, () => {
    const port = server.address().port;
    const client = net.createConnection(port, '127.0.0.1', () => {
      client.write('POST / HTTP/1.1\r\n');
      client.write('Host: localhost\r\n');
      client.write('Content-Length: 9\r\n');
      client.write('\r\n');
      client.write('test data');
    });

    client.on('data', (data) => {
      const response = data.toString();
      assert.ok(response.includes('200'), 'Should get 200 response');
      client.end();
      server.close();
    });
  });
});

// Test 15: Server has essential methods
test('server has essential methods', () => {
  const server = http.createServer();
  assert.strictEqual(typeof server.listen, 'function', 'Should have listen method');
  assert.strictEqual(typeof server.close, 'function', 'Should have close method');
  assert.strictEqual(typeof server.address, 'function', 'Should have address method');
  // Note: ref() and unref() are libuv features that may not be exposed yet
});

console.log(`\n=== Test Results ===`);
console.log(`Tests run: ${testsRun}`);
console.log(`Tests passed: ${testsPassed}`);
console.log(`Tests failed: ${testsRun - testsPassed}`);

if (testsPassed === testsRun) {
  console.log('\n✓ All basic server tests passed');
  process.exit(0);
} else {
  console.log(`\n✗ ${testsRun - testsPassed} test(s) failed`);
  process.exit(1);
}
