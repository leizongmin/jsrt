const assert = require('jsrt:assert');

// Test module loading
const http = require('node:http');
assert.ok(http, 'node:http module should load');

// Test that required functions exist
assert.ok(
  typeof http.createServer === 'function',
  'createServer should be a function'
);
assert.ok(typeof http.request === 'function', 'request should be a function');
assert.ok(typeof http.Server === 'function', 'Server should be a constructor');
assert.ok(
  typeof http.ServerResponse === 'function',
  'ServerResponse should be a constructor'
);
assert.ok(
  typeof http.IncomingMessage === 'function',
  'IncomingMessage should be a constructor'
);

// Test HTTP constants
assert.ok(Array.isArray(http.METHODS), 'METHODS should be an array');
assert.ok(http.METHODS.length > 0, 'METHODS should contain HTTP methods');
assert.ok(http.METHODS.includes('GET'), 'METHODS should include GET');
assert.ok(http.METHODS.includes('POST'), 'METHODS should include POST');

assert.ok(
  typeof http.STATUS_CODES === 'object',
  'STATUS_CODES should be an object'
);
assert.strictEqual(
  http.STATUS_CODES['200'],
  'OK',
  'STATUS_CODES should include 200 OK'
);
assert.strictEqual(
  http.STATUS_CODES['404'],
  'Not Found',
  'STATUS_CODES should include 404 Not Found'
);

// Test Server constructor
const server = new http.Server();
assert.ok(server, 'Server constructor should work');
assert.ok(
  typeof server.listen === 'function',
  'Server should have listen method'
);
assert.ok(
  typeof server.close === 'function',
  'Server should have close method'
);

// Test createServer factory function
const server2 = http.createServer();
assert.ok(server2, 'createServer should return a server instance');
assert.ok(
  typeof server2.listen === 'function',
  'Created server should have listen method'
);

// Test createServer with callback
let callbackCalled = false;
const server3 = http.createServer((req, res) => {
  callbackCalled = true;
});
assert.ok(
  server3,
  'createServer with callback should return a server instance'
);
// Note: callback won't actually be called without real request

// Test ServerResponse constructor
const res = new http.ServerResponse();
assert.ok(res, 'ServerResponse constructor should work');
assert.ok(
  typeof res.writeHead === 'function',
  'ServerResponse should have writeHead method'
);
assert.ok(
  typeof res.write === 'function',
  'ServerResponse should have write method'
);
assert.ok(
  typeof res.end === 'function',
  'ServerResponse should have end method'
);
assert.strictEqual(
  res.statusCode,
  200,
  'ServerResponse should have default statusCode 200'
);
assert.strictEqual(
  res.statusMessage,
  'OK',
  'ServerResponse should have default statusMessage OK'
);
assert.strictEqual(
  res.headersSent,
  false,
  'ServerResponse should have headersSent false by default'
);

// Test IncomingMessage constructor
const req = new http.IncomingMessage();
assert.ok(req, 'IncomingMessage constructor should work');
assert.strictEqual(
  req.method,
  'GET',
  'IncomingMessage should have default method GET'
);
assert.strictEqual(req.url, '/', 'IncomingMessage should have default url /');
assert.strictEqual(
  req.httpVersion,
  '1.1',
  'IncomingMessage should have default httpVersion 1.1'
);
assert.ok(
  typeof req.headers === 'object',
  'IncomingMessage should have headers object'
);

// Test ServerResponse methods
res.writeHead(404, 'Not Found', { 'Content-Type': 'text/plain' });
assert.ok(true, 'writeHead should work without errors');

res.write('Hello ');
assert.ok(true, 'write should work without errors');

res.end('World!');
assert.ok(true, 'end should work without errors');

// Test request function
const clientReq = http.request('http://example.com/test');
assert.ok(clientReq, 'request should return a request object');
assert.strictEqual(
  clientReq.url,
  'http://example.com/test',
  'request should set URL properly'
);

// Clean up test objects
server.close();
server2.close();
server3.close();
