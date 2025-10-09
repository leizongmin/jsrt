// Basic dgram module tests
const dgram = require('node:dgram');
const assert = require('jsrt:assert');

console.log('Testing dgram module...');

// Test 1: Module loads correctly
assert.strictEqual(typeof dgram, 'object', 'dgram module should be an object');
assert.strictEqual(
  typeof dgram.createSocket,
  'function',
  'dgram.createSocket should be a function'
);
assert.strictEqual(
  typeof dgram.Socket,
  'function',
  'dgram.Socket should be a constructor'
);

// Test 2: Create IPv4 socket
const socket1 = dgram.createSocket('udp4');
assert.ok(socket1, 'Should create IPv4 socket');
assert.strictEqual(
  typeof socket1.on,
  'function',
  'Socket should have on() method'
);
assert.strictEqual(
  typeof socket1.bind,
  'function',
  'Socket should have bind() method'
);
assert.strictEqual(
  typeof socket1.send,
  'function',
  'Socket should have send() method'
);
assert.strictEqual(
  typeof socket1.close,
  'function',
  'Socket should have close() method'
);
socket1.close();

// Test 3: Create IPv6 socket
const socket2 = dgram.createSocket('udp6');
assert.ok(socket2, 'Should create IPv6 socket');
socket2.close();

// Test 4: Create socket with options
const socket3 = dgram.createSocket({ type: 'udp4' });
assert.ok(socket3, 'Should create socket with options');
socket3.close();

// Test 5: Basic send/receive
let receivedMessage = false;
const sender = dgram.createSocket('udp4');
const receiver = dgram.createSocket('udp4');

receiver.on('message', (msg, rinfo) => {
  console.log('Received message:', msg.toString());
  assert.strictEqual(
    msg.toString(),
    'Hello UDP',
    'Should receive correct message'
  );
  assert.strictEqual(
    typeof rinfo.address,
    'string',
    'rinfo should have address'
  );
  assert.strictEqual(typeof rinfo.port, 'number', 'rinfo should have port');
  assert.strictEqual(typeof rinfo.family, 'string', 'rinfo should have family');
  receivedMessage = true;

  // Cleanup
  sender.close();
  receiver.close();
});

receiver.bind(41234, '127.0.0.1', () => {
  console.log('Receiver bound to port 41234');
  sender.send('Hello UDP', 41234, '127.0.0.1', (err) => {
    if (err) {
      console.error('Send error:', err);
      process.exit(1);
    }
    console.log('Message sent');
  });
});

// Test 6: Check destroyed property
const socket4 = dgram.createSocket('udp4');
assert.strictEqual(
  socket4.destroyed,
  false,
  'New socket should not be destroyed'
);
socket4.close();

// Give time for async operations
setTimeout(() => {
  if (receivedMessage) {
    console.log('All basic tests passed!');
  } else {
    console.error('Failed to receive message');
    process.exit(1);
  }
}, 1000);
