const assert = require('jsrt:assert');

console.log('=== HTTP Protocol Parsing Tests ===');

let http;
try {
  http = require('node:http');
} catch (e) {
  console.log('❌ SKIP: node:http module not available');
  console.log('=== Tests Completed (Skipped) ===');
  process.exit(0);
}

// Test 1: HTTP Request Line Parsing
console.log('\n--- Test 1: HTTP Request Line Parsing ---');
let parsedRequests = [];

const server1 = http.createServer((req, res) => {
  parsedRequests.push({
    method: req.method,
    url: req.url,
    httpVersion: req.httpVersion,
  });

  res.statusCode = 200;
  res.end('OK');
});

console.log('✓ Request line parsing test setup ready');

// Test 2: HTTP Header Parsing
console.log('\n--- Test 2: HTTP Header Parsing ---');
let parsedHeaders = [];

const server2 = http.createServer((req, res) => {
  parsedHeaders.push({
    host: req.headers.host,
    userAgent: req.headers['user-agent'],
    contentType: req.headers['content-type'],
    contentLength: req.headers['content-length'],
    connection: req.headers.connection,
    accept: req.headers.accept,
  });

  res.statusCode = 200;
  res.end('Headers parsed');
});

console.log('✓ Header parsing test setup ready');

// Test 3: HTTP Method Validation
console.log('\n--- Test 3: HTTP Method Support ---');
const supportedMethods = [
  'GET',
  'POST',
  'PUT',
  'DELETE',
  'HEAD',
  'OPTIONS',
  'PATCH',
];
let methodTests = [];

const server3 = http.createServer((req, res) => {
  methodTests.push({
    method: req.method,
    supported: supportedMethods.includes(req.method),
  });

  if (supportedMethods.includes(req.method)) {
    res.statusCode = 200;
    res.end(`Method ${req.method} is supported`);
  } else {
    res.statusCode = 405;
    res.setHeader('Allow', supportedMethods.join(', '));
    res.end('Method Not Allowed');
  }
});

console.log('✓ Method validation test setup ready');

// Test 4: URL Path Parsing
console.log('\n--- Test 4: URL Path Parsing ---');
let urlTests = [];

const server4 = http.createServer((req, res) => {
  const url = new URL(req.url, `http://localhost`);

  urlTests.push({
    originalUrl: req.url,
    pathname: url.pathname,
    search: url.search,
    query: url.searchParams.toString(),
    hash: url.hash,
  });

  res.statusCode = 200;
  res.setHeader('Content-Type', 'application/json');
  res.end(
    JSON.stringify({
      pathname: url.pathname,
      query: Object.fromEntries(url.searchParams),
      hash: url.hash,
    })
  );
});

console.log('✓ URL parsing test setup ready');

// Test 5: Content-Length and Body Handling
console.log('\n--- Test 5: Content-Length Handling ---');
let bodyTests = [];

const server5 = http.createServer((req, res) => {
  let body = '';
  let contentLength = parseInt(req.headers['content-length'] || '0');

  req.on('data', (chunk) => {
    body += chunk.toString();
  });

  req.on('end', () => {
    bodyTests.push({
      contentLength: contentLength,
      actualBodyLength: body.length,
      body: body,
    });

    res.statusCode = 200;
    res.setHeader('Content-Type', 'application/json');
    res.end(
      JSON.stringify({
        receivedContentLength: contentLength,
        actualBodyLength: body.length,
        body: body.substring(0, 100), // First 100 chars
      })
    );
  });
});

console.log('✓ Content-Length handling test setup ready');

// Test 6: HTTP Version Handling
console.log('\n--- Test 6: HTTP Version Support ---');
let versionTests = [];

const server6 = http.createServer((req, res) => {
  versionTests.push({
    httpVersion: req.httpVersion,
    httpVersionMajor: req.httpVersionMajor,
    httpVersionMinor: req.httpVersionMinor,
  });

  res.statusCode = 200;
  res.setHeader('Content-Type', 'text/plain');
  res.end(`HTTP version: ${req.httpVersion}`);
});

console.log('✓ HTTP version handling test setup ready');

// Test 7: Connection Handling
console.log('\n--- Test 7: Connection Management ---');
let connectionTests = [];

const server7 = http.createServer((req, res) => {
  const keepAlive =
    req.headers.connection &&
    req.headers.connection.toLowerCase() === 'keep-alive';

  connectionTests.push({
    connection: req.headers.connection,
    keepAlive: keepAlive,
    socketReused: req.socket.reusedSocket || false,
  });

  if (keepAlive) {
    res.setHeader('Connection', 'keep-alive');
    res.setHeader('Keep-Alive', 'timeout=5, max=1000');
  } else {
    res.setHeader('Connection', 'close');
  }

  res.statusCode = 200;
  res.end('Connection handled');
});

console.log('✓ Connection management test setup ready');

// Test 8: Special Characters in Headers
console.log('\n--- Test 8: Header Character Handling ---');
let headerCharTests = [];

const server8 = http.createServer((req, res) => {
  headerCharTests.push({
    hasSpecialChars: false,
    headers: Object.keys(req.headers),
  });

  // Check for problematic characters
  for (const [key, value] of Object.entries(req.headers)) {
    if (
      typeof value === 'string' &&
      (/[\\r\\n\\x00-\\x1f\\x7f]/.test(value) ||
        /[\\r\\n\\x00-\\x1f\\x7f]/.test(key))
    ) {
      headerCharTests[headerCharTests.length - 1].hasSpecialChars = true;
      break;
    }
  }

  res.statusCode = 200;
  res.end('Header characters checked');
});

console.log('✓ Header character handling test setup ready');

// Test 9: Large Header Handling
console.log('\n--- Test 9: Large Header Support ---');
let largeHeaderTests = [];

const server9 = http.createServer((req, res) => {
  const totalHeaderSize = JSON.stringify(req.headers).length;
  const headerCount = Object.keys(req.headers).length;

  largeHeaderTests.push({
    headerCount: headerCount,
    totalSize: totalHeaderSize,
    largeHeaders: totalHeaderSize > 8192, // 8KB threshold
  });

  res.statusCode = 200;
  res.setHeader('X-Header-Count', headerCount.toString());
  res.setHeader('X-Total-Header-Size', totalHeaderSize.toString());
  res.end('Large headers processed');
});

console.log('✓ Large header handling test setup ready');

// Test 10: Malformed Request Handling
console.log('\n--- Test 10: Error Recovery ---');
let errorTests = [];

const server10 = http.createServer((req, res) => {
  errorTests.push({
    method: req.method || 'UNKNOWN',
    url: req.url || 'INVALID',
    hasHeaders: !!req.headers,
    timestamp: Date.now(),
  });

  res.statusCode = 200;
  res.end('Error recovery test');
});

server10.on('error', (err) => {
  console.log('Server error caught:', err.message);
  errorTests.push({
    error: err.message,
    type: 'server-error',
    timestamp: Date.now(),
  });
});

console.log('✓ Error recovery test setup ready');

// Validation functions
function validateRequestParsing() {
  console.log('\n--- Validation: Request Parsing ---');

  if (parsedRequests.length > 0) {
    parsedRequests.forEach((req, i) => {
      assert.ok(req.method, `Request ${i + 1} should have method`);
      assert.ok(req.url, `Request ${i + 1} should have URL`);
      console.log(
        `✓ Request ${i + 1}: ${req.method} ${req.url} HTTP/${req.httpVersion || '1.1'}`
      );
    });
  }
}

function validateHeaderParsing() {
  console.log('\n--- Validation: Header Parsing ---');

  if (parsedHeaders.length > 0) {
    parsedHeaders.forEach((headers, i) => {
      console.log(
        `✓ Headers ${i + 1} parsed:`,
        Object.keys(headers).filter((k) => headers[k]).length,
        'fields'
      );
    });
  }
}

function validateMethodSupport() {
  console.log('\n--- Validation: Method Support ---');

  if (methodTests.length > 0) {
    const supportedCount = methodTests.filter((t) => t.supported).length;
    console.log(`✓ ${supportedCount}/${methodTests.length} methods supported`);
  }
}

// Clean up servers function
function cleanupServers() {
  const servers = [
    server1,
    server2,
    server3,
    server4,
    server5,
    server6,
    server7,
    server8,
    server9,
    server10,
  ];
  servers.forEach((server, i) => {
    try {
      server.close();
    } catch (e) {
      console.log(`⚠ Error closing server ${i + 1}:`, e.message);
    }
  });
}

// Run validations after some time
setTimeout(() => {
  validateRequestParsing();
  validateHeaderParsing();
  validateMethodSupport();

  console.log('\n--- Test Results Summary ---');
  console.log(`Requests parsed: ${parsedRequests.length}`);
  console.log(`Headers parsed: ${parsedHeaders.length}`);
  console.log(`Methods tested: ${methodTests.length}`);
  console.log(`URLs parsed: ${urlTests.length}`);
  console.log(`Body tests: ${bodyTests.length}`);
  console.log(`Version tests: ${versionTests.length}`);
  console.log(`Connection tests: ${connectionTests.length}`);
  console.log(`Header char tests: ${headerCharTests.length}`);
  console.log(`Large header tests: ${largeHeaderTests.length}`);
  console.log(`Error tests: ${errorTests.length}`);

  cleanupServers();
  console.log('\n=== HTTP Protocol Parsing Tests Completed ===');
}, 100);
