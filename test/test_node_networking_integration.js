const assert = require('jsrt:assert');

// console.log('=== Node.js Networking Integration Test ===');
console.log('Testing node:net and node:http modules together...');

// Test both modules load
const net = require('node:net');
const http = require('node:http');

assert.ok(net, 'node:net module should load');
assert.ok(http, 'node:http module should load');
// console.log('✓ Both networking modules load successfully');

// Test that HTTP builds on net
console.log('\n🌐 Testing HTTP Server (built on net.Server):');

// Create HTTP server
const server = http.createServer((req, res) => {
  console.log(`  📥 Mock request: ${req.method} ${req.url}`);

  res.writeHead(200, 'OK', {
    'Content-Type': 'text/plain',
    'X-Test': 'jsrt-http',
  });

  res.write('Hello from jsrt HTTP server!');
  res.end(' Integration test complete.');
});

assert.ok(server, 'HTTP server should be created');
assert.ok(
  typeof server.listen === 'function',
  'HTTP server should have listen method'
);
// console.log('✓ HTTP server created with request handler');

// Test net.Socket creation
console.log('\n🔌 Testing TCP Socket:');
const socket = new net.Socket();
assert.ok(socket, 'TCP socket should be created');
assert.ok(
  typeof socket.connect === 'function',
  'Socket should have connect method'
);
assert.ok(
  typeof socket.write === 'function',
  'Socket should have write method'
);
assert.ok(typeof socket.end === 'function', 'Socket should have end method');
// console.log('✓ TCP socket created with all methods');

// Test net.Server creation
console.log('\n🏠 Testing TCP Server:');
const netServer = net.createServer((connection) => {
  console.log('  📡 Mock connection received');
  connection.write('Welcome to jsrt TCP server!');
  connection.end();
});

assert.ok(netServer, 'TCP server should be created');
assert.ok(
  typeof netServer.listen === 'function',
  'TCP server should have listen method'
);
// console.log('✓ TCP server created with connection handler');

// Test HTTP client request
console.log('\n📤 Testing HTTP Client:');
const clientReq = http.request('http://example.com/test');
assert.ok(clientReq, 'HTTP request should be created');
assert.strictEqual(
  clientReq.url,
  'http://example.com/test',
  'Request URL should be set correctly'
);
// console.log('✓ HTTP client request created');

// Test networking constants
console.log('\n📋 Testing networking constants:');
assert.ok(Array.isArray(http.METHODS), 'HTTP methods should be available');
assert.ok(http.METHODS.includes('GET'), 'HTTP methods should include GET');
assert.ok(http.METHODS.includes('POST'), 'HTTP methods should include POST');
assert.ok(
  typeof http.STATUS_CODES === 'object',
  'HTTP status codes should be available'
);
assert.strictEqual(
  http.STATUS_CODES['200'],
  'OK',
  'Status code 200 should be OK'
);
// console.log('✓ HTTP constants are properly defined');

// Test that HTTP and net modules work together conceptually
console.log('\n🔗 Testing HTTP/TCP Integration:');

// HTTP creates underlying net server
assert.ok(server, 'HTTP server exists');
// In a real scenario, we'd test actual networking, but for now we test the structure
// console.log('  ✓ HTTP server built on TCP foundation');
// console.log('  ✓ Both modules use EventEmitter pattern');
// console.log('  ✓ Consistent API design across modules');

// Clean up
socket.destroy();
netServer.close();
server.close();

// console.log('\n✅ All networking integration tests passed!');
console.log('📊 Node.js networking compatibility layer working!');
console.log('\nImplemented networking modules:');
// console.log('  ✅ node:net - TCP networking (Socket, Server)');
// console.log('  ✅ node:http - HTTP protocol (Server, Request, Response)');
console.log('\nFeatures working:');
// console.log('  ✅ EventEmitter inheritance for all networking objects');
// console.log('  ✅ Factory functions (createServer, connect, request)');
// console.log('  ✅ Constructor functions (Socket, Server, etc.)');
// console.log('  ✅ HTTP constants (METHODS, STATUS_CODES)');
// console.log('  ✅ CommonJS require() support');
// console.log('  ✅ Consistent API with Node.js networking modules');

console.log('\nReady for Phase 5: Advanced modules (fs, stream, crypto)');
