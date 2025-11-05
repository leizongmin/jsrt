// Test for node:net socket properties (Phase 1)
const net = require('node:net');

let testsPassed = 0;
let testsFailed = 0;
let pendingTests = 0;

function test(name, fn) {
  if (fn.length === 0) {
    // Synchronous test
    try {
      fn();
      testsPassed++;
    } catch (e) {
      console.log(`FAIL: ${name}`);
      console.log(`  ${e.message}`);
      testsFailed++;
    }
  } else {
    // Asynchronous test - has done callback
    pendingTests++;
    try {
      fn(() => {
        // done callback
        testsPassed++;
        pendingTests--;
      });
    } catch (e) {
      console.log(`FAIL: ${name}`);
      console.log(`  ${e.message}`);
      testsFailed++;
      pendingTests--;
    }
  }
}

function assert(condition, message) {
  if (!condition) {
    throw new Error(message || 'Assertion failed');
  }
}

// Test 1: Socket state properties before connection
test('Socket state properties before connection', () => {
  const socket = new net.Socket();

  assert(
    socket.connecting === false,
    'connecting should be false before connect()'
  );
  assert(socket.destroyed === false, 'destroyed should be false initially');
  assert(socket.pending === true, 'pending should be true before connection');
  assert(
    socket.readyState === 'closed',
    'readyState should be "closed" initially'
  );
  assert(socket.bytesRead === 0, 'bytesRead should be 0 initially');
  assert(socket.bytesWritten === 0, 'bytesWritten should be 0 initially');

  console.log('✓ Socket state properties correct before connection');
});

// Test 2: Socket properties during connection
test('Socket properties during connection', () => {
  const socket = new net.Socket();

  // Start connection
  socket.connect(9999, 'localhost');

  assert(
    socket.connecting === true,
    'connecting should be true after connect()'
  );
  assert(
    socket.readyState === 'opening',
    'readyState should be "opening" during connection'
  );

  console.log('✓ Socket properties correct during connection');

  // Clean up - destroy socket immediately
  socket.destroy();
});

// Test 3: Socket address properties after connection
test('Socket address/port properties after connection', (done) => {
  const server = net.createServer((socket) => {
    // Client connected - check server-side socket properties
    assert(socket.remoteAddress !== null, 'remoteAddress should not be null');
    assert(socket.remotePort !== null, 'remotePort should not be null');
    assert(
      socket.remoteFamily === 'IPv4' || socket.remoteFamily === 'IPv6',
      'remoteFamily should be IPv4 or IPv6'
    );

    assert(socket.localAddress !== null, 'localAddress should not be null');
    assert(socket.localPort !== null, 'localPort should not be null');
    assert(
      socket.localFamily === 'IPv4' || socket.localFamily === 'IPv6',
      'localFamily should be IPv4 or IPv6'
    );

    console.log(
      `✓ Server socket properties: ${socket.localAddress}:${socket.localPort} <- ${socket.remoteAddress}:${socket.remotePort}`
    );

    socket.end();
    server.close(() => {
      done();
    });
  });

  server.listen(0, 'localhost', () => {
    const port = server.address().port;
    const client = net.connect(port, 'localhost', () => {
      // Client connected - check client-side socket properties
      assert(
        client.connecting === false,
        'connecting should be false after connect event'
      );
      assert(
        client.destroyed === false,
        'destroyed should be false when connected'
      );
      assert(
        client.pending === false,
        'pending should be false when connected'
      );
      assert(
        client.readyState === 'open',
        'readyState should be "open" when connected'
      );

      assert(
        client.localAddress !== null,
        'client localAddress should not be null'
      );
      assert(client.localPort !== null, 'client localPort should not be null');
      assert(
        client.remoteAddress !== null,
        'client remoteAddress should not be null'
      );
      assert(client.remotePort === port, `client remotePort should be ${port}`);

      console.log(
        `✓ Client socket properties: ${client.localAddress}:${client.localPort} -> ${client.remoteAddress}:${client.remotePort}`
      );
    });
  });
});

// Test 4: bytesRead and bytesWritten tracking
test('bytesRead and bytesWritten tracking', (done) => {
  const server = net.createServer((socket) => {
    socket.on('data', (data) => {
      assert(
        socket.bytesRead > 0,
        'bytesRead should increase after reading data'
      );
      console.log(`✓ Server received ${socket.bytesRead} bytes`);

      socket.write('pong');

      setTimeout(() => {
        assert(
          socket.bytesWritten > 0,
          'bytesWritten should increase after writing data'
        );
        console.log(`✓ Server sent ${socket.bytesWritten} bytes`);
        socket.end();
        server.close(() => {
          done();
        });
      }, 10);
    });
  });

  server.listen(0, 'localhost', () => {
    const port = server.address().port;
    const client = net.connect(port, 'localhost', () => {
      const initialBytesWritten = client.bytesWritten;
      client.write('ping');

      setTimeout(() => {
        assert(
          client.bytesWritten > initialBytesWritten,
          'client bytesWritten should increase'
        );
        console.log(`✓ Client sent ${client.bytesWritten} bytes`);
      }, 10);
    });

    client.on('data', (data) => {
      assert(client.bytesRead > 0, 'client bytesRead should increase');
      console.log(`✓ Client received ${client.bytesRead} bytes`);
      client.end();
    });
  });
});

// Test 5: bufferSize property
test('bufferSize property', (done) => {
  let serverSocket = null;
  const server = net.createServer((socket) => {
    // Don't read data to cause buffering
    serverSocket = socket;
    socket.pause();
    socket.on('end', () => {
      socket.destroy();
    });
  });

  server.listen(0, 'localhost', () => {
    const port = server.address().port;
    const client = net.connect(port, 'localhost', () => {
      const initialBufferSize = client.bufferSize;

      // Write data
      client.write('x'.repeat(1000));

      setTimeout(() => {
        // Buffer size should be available (may be 0 if write completed immediately)
        const bufferSize = client.bufferSize;
        assert(typeof bufferSize === 'number', 'bufferSize should be a number');
        console.log(`✓ Client bufferSize: ${bufferSize} bytes`);

        client.end();
        if (serverSocket && !serverSocket.destroyed) {
          serverSocket.destroy();
        }
        server.close(() => {
          done();
        });
      }, 10);
    });
  });
});

// Test 6: destroyed property after destroy()
test('destroyed property after destroy()', () => {
  const socket = new net.Socket();
  assert(socket.destroyed === false, 'destroyed should be false initially');

  socket.destroy();
  assert(socket.destroyed === true, 'destroyed should be true after destroy()');
  assert(
    socket.readyState === 'closed',
    'readyState should be "closed" after destroy()'
  );

  console.log('✓ destroyed property correct after destroy()');
});

// Give async tests time to complete
function checkTestsComplete() {
  if (pendingTests === 0) {
    console.log(`\nTest Results: ${testsPassed} passed, ${testsFailed} failed`);
    process.exit(testsFailed > 0 ? 1 : 0);
  } else {
    setTimeout(checkTestsComplete, 100);
  }
}

setTimeout(checkTestsComplete, 100);
