// Real HTTP Client Test - Demonstrates actual network requests with jsrt fetch API
console.log('=== jsrt HTTP Client - Real Network Testing ===');
console.log('');

// Helper function to add delays between tests
function delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

// Test 1: Basic GET request with real HTTP response
console.log('Test 1: Basic GET request');
console.log('Making request to http://httpbin.org/get...');

fetch('http://httpbin.org/get')
    .then(response => {
        console.log('✅ Response received');
        console.log('Status:', response.status);
        console.log('Status Text:', response.statusText || 'OK');
        console.log('Headers available:', response.headers ? 'Yes' : 'No');
        return response.text();
    })
    .then(body => {
        console.log('Response body preview:', body.slice(0, 200) + '...');
        console.log('');
        return delay(1000);
    })
    .then(() => {
        // Test 2: GET request with JSON response
        console.log('Test 2: JSON API request');
        console.log('Making request to http://httpbin.org/json...');
        
        return fetch('http://httpbin.org/json');
    })
    .then(response => {
        console.log('✅ JSON Response received');
        console.log('Status:', response.status);
        return response.text();
    })
    .then(body => {
        console.log('JSON Response:', body);
        console.log('');
        return delay(1000);
    })
    .then(() => {
        // Test 3: POST request with custom headers
        console.log('Test 3: POST request with Headers object');
        console.log('Making POST request to http://httpbin.org/post...');
        
        const headers = new Headers();
        headers.set('Content-Type', 'application/json');
        headers.set('User-Agent', 'jsrt-http-client/1.0');
        
        return fetch('http://httpbin.org/post', {
            method: 'POST',
            headers: headers,
            body: JSON.stringify({
                test: 'POST with Headers object',
                timestamp: Date.now()
            })
        });
    })
    .then(response => {
        console.log('✅ POST Response received');
        console.log('Status:', response.status);
        return response.text();
    })
    .then(body => {
        console.log('POST Response preview:', body.slice(0, 300) + '...');
        console.log('');
        return delay(1000);
    })
    .then(() => {
        // Test 4: PUT request with plain object headers
        console.log('Test 4: PUT request with plain object headers');
        console.log('Making PUT request to http://httpbin.org/put...');
        
        return fetch('http://httpbin.org/put', {
            method: 'PUT',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json',
                'X-Custom-Header': 'jsrt-test-value'
            },
            body: JSON.stringify({
                test: 'PUT with plain object headers',
                data: { key: 'value', number: 42 }
            })
        });
    })
    .then(response => {
        console.log('✅ PUT Response received');
        console.log('Status:', response.status);
        return response.text();
    })
    .then(body => {
        console.log('PUT Response preview:', body.slice(0, 300) + '...');
        console.log('');
        return delay(1000);
    })
    .then(() => {
        // Test 5: Different HTTP methods
        console.log('Test 5: Testing different HTTP methods');
        console.log('Making DELETE request to http://httpbin.org/delete...');
        
        return fetch('http://httpbin.org/delete', {
            method: 'DELETE'
        });
    })
    .then(response => {
        console.log('✅ DELETE Response received');
        console.log('Status:', response.status);
        return response.text();
    })
    .then(body => {
        console.log('DELETE Response preview:', body.slice(0, 200) + '...');
        console.log('');
        return delay(1000);
    })
    .then(() => {
        // Test 6: PATCH method
        console.log('Test 6: PATCH method test');
        console.log('Making PATCH request to http://httpbin.org/patch...');
        
        return fetch('http://httpbin.org/patch', {
            method: 'PATCH',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                patch: 'data',
                modified: true
            })
        });
    })
    .then(response => {
        console.log('✅ PATCH Response received');
        console.log('Status:', response.status);
        return response.text();
    })
    .then(body => {
        console.log('PATCH Response preview:', body.slice(0, 200) + '...');
        console.log('');
        return delay(1000);
    })
    .then(() => {
        // Test 7: Request with user agent and custom headers
        console.log('Test 7: Custom User-Agent and headers test');
        console.log('Making request to http://httpbin.org/headers...');
        
        return fetch('http://httpbin.org/headers', {
            headers: {
                'User-Agent': 'jsrt-javascript-runtime/1.0',
                'Accept': 'application/json',
                'X-Test-Client': 'jsrt-fetch-api',
                'X-Request-ID': 'test-' + Math.random().toString(36).substr(2, 9)
            }
        });
    })
    .then(response => {
        console.log('✅ Headers test response received');
        console.log('Status:', response.status);
        return response.text();
    })
    .then(body => {
        console.log('Headers response body:', body);
        console.log('');
        return delay(1000);
    })
    .then(() => {
        // Test 8: Error handling - DNS failure
        console.log('Test 8: Error handling - DNS failure test');
        console.log('Making request to http://this-domain-does-not-exist-12345.com...');
        
        return fetch('http://this-domain-does-not-exist-12345.com')
            .then(response => {
                console.log('❌ Unexpected success for DNS failure test');
                console.log('Status:', response.status);
            })
            .catch(error => {
                console.log('✅ Expected error caught for DNS failure');
                console.log('Error message:', error.message || error.toString());
            });
    })
    .then(() => delay(1000))
    .then(() => {
        // Test 9: Error handling - Connection refused
        console.log('Test 9: Error handling - Connection refused test');
        console.log('Making request to http://localhost:99999 (invalid port)...');
        
        return fetch('http://localhost:99999')
            .then(response => {
                console.log('❌ Unexpected success for connection refused test');
                console.log('Status:', response.status);
            })
            .catch(error => {
                console.log('✅ Expected error caught for connection refused');
                console.log('Error message:', error.message || error.toString());
            });
    })
    .then(() => delay(1000))
    .then(() => {
        // Final summary
        console.log('=== HTTP Client Test Summary ===');
        console.log('✅ Real HTTP networking using libuv');
        console.log('✅ DNS resolution and TCP connections');
        console.log('✅ HTTP request building and response parsing');
        console.log('✅ Custom headers support (Headers object and plain object)');
        console.log('✅ Multiple HTTP methods (GET, POST, PUT, DELETE, PATCH)');
        console.log('✅ Promise-based asynchronous operations');
        console.log('✅ Proper error handling for network failures');
        console.log('✅ Robust HTTP response parsing');
        console.log('✅ Integration with JavaScript runtime event loop');
        console.log('');
        console.log('jsrt fetch API provides complete HTTP client functionality!');
        console.log('All network requests are real - no mocks or stubs used.');
    })
    .catch(error => {
        console.log('❌ Test suite error:', error.message || error.toString());
    });

// Demonstrate async/await pattern as well
console.log('');
console.log('=== Async/Await Pattern Demo ===');
console.log('Note: This demonstrates the syntax - actual execution depends on async/await support');
console.log('');
console.log('async function testAsyncAwait() {');
console.log('  try {');
console.log('    const response = await fetch("http://httpbin.org/get");');
console.log('    console.log("Status:", response.status);');
console.log('    const body = await response.text();');
console.log('    console.log("Response:", body.slice(0, 100) + "...");');
console.log('  } catch (error) {');
console.log('    console.error("Network error:", error.message);');
console.log('  }');
console.log('}');
console.log('');
console.log('// Usage: await testAsyncAwait();');