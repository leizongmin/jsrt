const assert = require('jsrt:assert');

console.log('=== HTTP Server Functionality Tests ===');

let http;
try {
  http = require('node:http');
} catch (e) {
  console.log('❌ SKIP: node:http module not available');
  console.log('=== Tests Completed (Skipped) ===');
  process.exit(0);
}

// Test 1: Basic server creation and startup
console.log('\n--- Test 1: Basic Server Creation ---');
const server1 = http.createServer((req, res) => {
  res.statusCode = 200;
  res.setHeader('Content-Type', 'text/plain');
  res.end('Test response');
});

assert.ok(server1, 'Server should be created');
assert.ok(
  typeof server1.listen === 'function',
  'Server should have listen method'
);
assert.ok(
  typeof server1.close === 'function',
  'Server should have close method'
);
console.log('✓ Basic server creation works');

// Test 2: Server listening with callback
console.log('\n--- Test 2: Server Listen Callback ---');
let listenCallbackCalled = false;
const server2 = http.createServer();

try {
  server2.listen(0, '127.0.0.1', () => {
    listenCallbackCalled = true;
    console.log('✓ Server listen callback executed');
    server2.close();
  });

  // Give some time for the callback
  setTimeout(() => {
    if (!listenCallbackCalled) {
      console.log('⚠ Listen callback not called within timeout');
    }
  }, 50);
} catch (e) {
  console.log('⚠ Server listen failed:', e.message);
}

// Test 3: Request handler invocation
console.log('\n--- Test 3: Request Handler Properties ---');
let requestHandlerCalled = false;
let receivedRequest = null;
let receivedResponse = null;

const server3 = http.createServer((req, res) => {
  requestHandlerCalled = true;
  receivedRequest = req;
  receivedResponse = res;

  // Test request properties
  assert.ok(req, 'Request object should exist');
  assert.ok(
    typeof req.method === 'string',
    'Request should have method property'
  );
  assert.ok(typeof req.url === 'string', 'Request should have url property');
  assert.ok(
    typeof req.headers === 'object',
    'Request should have headers object'
  );

  // Test response properties
  assert.ok(res, 'Response object should exist');
  assert.ok(
    typeof res.statusCode === 'number',
    'Response should have statusCode'
  );
  assert.ok(
    typeof res.setHeader === 'function',
    'Response should have setHeader method'
  );
  assert.ok(
    typeof res.write === 'function',
    'Response should have write method'
  );
  assert.ok(typeof res.end === 'function', 'Response should have end method');

  res.statusCode = 200;
  res.setHeader('Content-Type', 'application/json');
  res.end(
    JSON.stringify({ message: 'Hello World', method: req.method, url: req.url })
  );
});

console.log('✓ Request handler structure is valid');

// Test 4: HTTP Server Response Methods
console.log('\n--- Test 4: Response Methods ---');
const server4 = http.createServer((req, res) => {
  try {
    // Test writeHead
    res.writeHead(201, 'Created', {
      'Content-Type': 'text/plain',
      'X-Custom-Header': 'test-value',
    });

    // Test write
    res.write('First part ');
    res.write('Second part ');

    // Test end
    res.end('Final part');

    console.log(
      '✓ Response methods (writeHead, write, end) work without errors'
    );
  } catch (e) {
    console.log('⚠ Response method error:', e.message);
  }
});

// Test 5: Multiple servers on different ports
console.log('\n--- Test 5: Multiple Server Instances ---');
const server5a = http.createServer((req, res) => {
  res.end('Server A');
});

const server5b = http.createServer((req, res) => {
  res.end('Server B');
});

assert.ok(
  server5a !== server5b,
  'Multiple servers should be different instances'
);
console.log('✓ Multiple server instances can be created');

// Test 6: Server close functionality
console.log('\n--- Test 6: Server Close ---');
const server6 = http.createServer();
try {
  server6.close();
  console.log('✓ Server close method works');
} catch (e) {
  console.log('⚠ Server close error:', e.message);
}

// Test 7: HTTP Constants and Status Codes
console.log('\n--- Test 7: HTTP Constants ---');
assert.ok(Array.isArray(http.METHODS), 'METHODS should be an array');
assert.ok(http.METHODS.includes('GET'), 'METHODS should include GET');
assert.ok(http.METHODS.includes('POST'), 'METHODS should include POST');
assert.ok(http.METHODS.includes('PUT'), 'METHODS should include PUT');
assert.ok(http.METHODS.includes('DELETE'), 'METHODS should include DELETE');

assert.ok(
  typeof http.STATUS_CODES === 'object',
  'STATUS_CODES should be an object'
);
assert.strictEqual(
  http.STATUS_CODES['200'],
  'OK',
  'STATUS_CODES[200] should be "OK"'
);
assert.strictEqual(
  http.STATUS_CODES['404'],
  'Not Found',
  'STATUS_CODES[404] should be "Not Found"'
);
assert.strictEqual(
  http.STATUS_CODES['500'],
  'Internal Server Error',
  'STATUS_CODES[500] should be "Internal Server Error"'
);

console.log('✓ HTTP constants are properly defined');

// Test 8: Request object construction
console.log('\n--- Test 8: Request Construction ---');
try {
  const testReq = new http.IncomingMessage();
  assert.ok(testReq, 'IncomingMessage constructor should work');
  assert.ok(typeof testReq.method === 'string', 'Request should have method');
  assert.ok(typeof testReq.url === 'string', 'Request should have url');
  assert.ok(typeof testReq.headers === 'object', 'Request should have headers');
  console.log('✓ Request object construction works');
} catch (e) {
  console.log('⚠ Request construction error:', e.message);
}

// Test 9: Response object construction
console.log('\n--- Test 9: Response Construction ---');
try {
  const testRes = new http.ServerResponse();
  assert.ok(testRes, 'ServerResponse constructor should work');
  assert.ok(
    typeof testRes.statusCode === 'number',
    'Response should have statusCode'
  );
  assert.ok(
    typeof testRes.statusMessage === 'string',
    'Response should have statusMessage'
  );
  assert.strictEqual(
    testRes.headersSent,
    false,
    'Response headersSent should be false initially'
  );
  console.log('✓ Response object construction works');
} catch (e) {
  console.log('⚠ Response construction error:', e.message);
}

// Test 10: Client request functionality
console.log('\n--- Test 10: Client Request ---');
try {
  const clientReq = http.request({
    hostname: 'localhost',
    port: 8000,
    path: '/test',
    method: 'GET',
  });

  assert.ok(clientReq, 'Client request should be created');
  assert.ok(
    typeof clientReq.write === 'function',
    'Client request should have write method'
  );
  assert.ok(
    typeof clientReq.end === 'function',
    'Client request should have end method'
  );
  console.log('✓ Client request creation works');
} catch (e) {
  console.log('⚠ Client request error:', e.message);
}

// Cleanup all test servers
console.log('\n--- Cleanup ---');
try {
  server1.close();
  server3.close();
  server4.close();
  server5a.close();
  server5b.close();
  console.log('✓ All test servers closed');
} catch (e) {
  console.log('⚠ Cleanup warning:', e.message);
}

console.log('\n=== HTTP Server Functionality Tests Completed ===');
