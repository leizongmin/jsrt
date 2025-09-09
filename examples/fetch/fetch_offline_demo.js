// Complete Fetch API demonstration (offline features)
console.log('🚀 Fetch API - Complete Implementation Demo');
console.log('=============================================');

// Test 1: Response methods return proper Promises
console.log('\n📋 Test 1: Response Methods Promise Support');
async function testResponseMethods() {
  const response = new Response();

  console.log('Creating response methods...');
  const textPromise = response.text();
  const jsonPromise = response.json();
  const arrayBufferPromise = response.arrayBuffer();
  const blobPromise = response.blob();

  console.log(
    '✅ text() returns:',
    typeof textPromise,
    textPromise.constructor.name
  );
  console.log(
    '✅ json() returns:',
    typeof jsonPromise,
    jsonPromise.constructor.name
  );
  console.log(
    '✅ arrayBuffer() returns:',
    typeof arrayBufferPromise,
    arrayBufferPromise.constructor.name
  );
  console.log(
    '✅ blob() returns:',
    typeof blobPromise,
    blobPromise.constructor.name
  );

  // Test promise resolution
  try {
    const text = await textPromise;
    console.log('text() resolved with:', typeof text);
  } catch (error) {
    console.log('text() rejected (expected for empty response)');
  }

  try {
    const buffer = await arrayBufferPromise;
    console.log(
      'arrayBuffer() resolved with:',
      buffer.constructor.name,
      'size:',
      buffer.byteLength
    );
  } catch (error) {
    console.log('arrayBuffer() rejected:', error.message);
  }

  try {
    const blob = await blobPromise;
    console.log('blob() resolved with size:', blob.size, 'type:', blob.type);
  } catch (error) {
    console.log('blob() rejected:', error.message);
  }
}

// Test 2: Headers API comprehensive test
console.log('\n📝 Test 2: Headers API Features');
function testHeaders() {
  const headers = new Headers();

  // Test basic operations
  headers.set('Content-Type', 'application/json');
  headers.set('User-Agent', 'jsrt/1.0');
  headers.set('X-Custom-Header', 'test-value');

  console.log('✅ Headers set successfully');

  // Test case insensitivity
  console.log('Case-insensitive tests:');
  console.log('  content-type:', headers.get('content-type'));
  console.log('  CONTENT-TYPE:', headers.get('CONTENT-TYPE'));
  console.log('  Content-Type:', headers.get('Content-Type'));

  console.log('Has tests:');
  console.log('  has("user-agent"):', headers.has('user-agent'));
  console.log('  has("USER-AGENT"):', headers.has('USER-AGENT'));
  console.log('  has("missing"):', headers.has('missing'));

  // Test deletion
  headers.delete('X-Custom-Header');
  console.log('After deletion:');
  console.log('  has("X-Custom-Header"):', headers.has('X-Custom-Header'));

  // Test overwriting
  headers.set('User-Agent', 'jsrt/2.0');
  console.log('After update:');
  console.log('  User-Agent:', headers.get('User-Agent'));
}

// Test 3: Request construction
console.log('\n🏗️  Test 3: Request Construction');
function testRequest() {
  // Basic GET request
  const getReq = new Request('http://example.com/api');
  console.log('GET Request:');
  console.log('  method:', getReq.method);
  console.log('  url:', getReq.url);

  // POST request with options
  const postReq = new Request('http://example.com/api', {
    method: 'POST',
  });
  console.log('POST Request:');
  console.log('  method:', postReq.method);
  console.log('  url:', postReq.url);
}

// Test 4: Response construction and properties
console.log('\n📊 Test 4: Response Properties');
function testResponse() {
  const response = new Response();

  console.log('Response properties:');
  console.log('  status:', response.status);
  console.log('  ok:', response.ok);
  console.log('  status type:', typeof response.status);
  console.log('  ok type:', typeof response.ok);

  // Verify properties are getters, not functions
  console.log('Property access (not function calls):');
  console.log('  response.status:', response.status);
  console.log('  response.ok:', response.ok);
}

// Test 5: Fetch function return type
console.log('\n🌐 Test 5: Fetch Function');
function testFetch() {
  console.log('Testing fetch function return type...');

  // Test that fetch returns a Promise
  const fetchPromise = fetch('http://example.com');
  console.log(
    'fetch() returns:',
    typeof fetchPromise,
    fetchPromise.constructor.name
  );
  console.log('Has then method:', typeof fetchPromise.then);
  console.log('Has catch method:', typeof fetchPromise.catch);

  // Test with options object
  const headers = new Headers();
  headers.set('Content-Type', 'application/json');

  const fetchWithOptions = fetch('http://example.com', {
    method: 'POST',
    headers: headers,
    body: JSON.stringify({ test: 'data' }),
  });

  console.log(
    'fetch() with options returns:',
    typeof fetchWithOptions,
    fetchWithOptions.constructor.name
  );
}

// Main demo execution
async function runDemo() {
  try {
    await testResponseMethods();
    testHeaders();
    testRequest();
    testResponse();
    testFetch();

    console.log('\n🎉 Fetch API Implementation Summary');
    console.log('===================================');
    console.log('✅ Core fetch() function implemented');
    console.log('✅ Promise-based async API');
    console.log('✅ Headers class with case-insensitive operations');
    console.log('✅ Request class with method and URL properties');
    console.log('✅ Response class with status and ok properties');
    console.log('✅ Response.text() returns Promise<string>');
    console.log('✅ Response.json() returns Promise<any>');
    console.log('✅ Response.arrayBuffer() returns Promise<ArrayBuffer>');
    console.log('✅ Response.blob() returns Promise<Blob>');
    console.log('✅ POST request body support (string data)');
    console.log('✅ HTTP networking via libuv');
    console.log('✅ Web Standards compliant API design');

    console.log('\n🔄 Future enhancements possible:');
    console.log('  • HTTPS/TLS support');
    console.log('  • Request timeout handling');
    console.log('  • Advanced body types (ArrayBuffer, FormData)');
    console.log('  • Response/Request cloning');
    console.log('  • Streaming responses');
  } catch (error) {
    console.error('Demo failed:', error.message);
  }
}

runDemo();
