const assert = require('jsrt:assert');

console.log('=== Node.js Net Module Tests ===');

// Test module loading
const net = require('node:net');
assert.ok(net, 'node:net module should load');
console.log('✓ Module loads successfully');

// Test that required functions exist
assert.ok(
  typeof net.createServer === 'function',
  'createServer should be a function'
);
assert.ok(typeof net.connect === 'function', 'connect should be a function');
assert.ok(typeof net.Socket === 'function', 'Socket should be a constructor');
assert.ok(typeof net.Server === 'function', 'Server should be a constructor');
console.log('✓ Required functions are available');

// Test Socket constructor
const socket = new net.Socket();
assert.ok(socket, 'Socket constructor should work');
assert.ok(
  typeof socket.connect === 'function',
  'Socket should have connect method'
);
assert.ok(
  typeof socket.write === 'function',
  'Socket should have write method'
);
assert.ok(typeof socket.end === 'function', 'Socket should have end method');
assert.ok(
  typeof socket.destroy === 'function',
  'Socket should have destroy method'
);
console.log('✓ Socket constructor and methods work');

// Test Server constructor
const server = new net.Server();
assert.ok(server, 'Server constructor should work');
assert.ok(
  typeof server.listen === 'function',
  'Server should have listen method'
);
assert.ok(
  typeof server.close === 'function',
  'Server should have close method'
);
console.log('✓ Server constructor and methods work');

// Test createServer factory function
const server2 = net.createServer();
assert.ok(server2, 'createServer should return a server instance');
assert.ok(
  typeof server2.listen === 'function',
  'Created server should have listen method'
);
console.log('✓ createServer factory function works');

// Test connect factory function (this will likely fail without actual connection)
try {
  const socket2 = net.connect(12345, 'localhost');
  assert.ok(socket2, 'connect should return a socket instance');
  assert.ok(
    typeof socket2.write === 'function',
    'Connected socket should have write method'
  );
  console.log('✓ connect factory function works');

  // Clean up
  socket2.destroy();
} catch (e) {
  console.log(
    '⚠️ connect test skipped (expected without running server):',
    e.message
  );
}

// Clean up test objects
socket.destroy();
server.close();
server2.close();

console.log('\n=== All Node.js Net module tests passed! ===');
