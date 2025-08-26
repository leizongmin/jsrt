// Test Fetch API implementation
console.log('=== Fetch API Tests ===');

// Test 1: Headers class
console.log('Test 1: Headers class');
const headers = new Headers();
console.log('Headers created:', typeof headers);

// Test headers methods
headers.set('Content-Type', 'application/json');
headers.set('User-Agent', 'jsrt/1.0');

console.log('Has Content-Type:', headers.has('Content-Type'));
console.log('Get Content-Type:', headers.get('Content-Type'));
console.log('Has Authorization:', headers.has('Authorization'));
console.log('Get non-existent header:', headers.get('Authorization'));

// Test case insensitive headers
console.log('Get content-type (lowercase):', headers.get('content-type'));
console.log('Has user-agent (lowercase):', headers.has('user-agent'));

// Test header deletion
headers.delete('User-Agent');
console.log('After delete - Has User-Agent:', headers.has('User-Agent'));

// Test 2: Request class
console.log('\nTest 2: Request class');
const request1 = new Request('https://api.example.com/data');
console.log('Request created:', typeof request1);
console.log('Request method:', request1.method);
console.log('Request URL:', request1.url);

// Test request with options
const request2 = new Request('https://api.example.com/data', {
  method: 'POST'
});
console.log('POST request created:', typeof request2);
console.log('POST request method:', request2.method);

// Test 3: Response class
console.log('\nTest 3: Response class');
const response = new Response();
console.log('Response created:', typeof response);
console.log('Response status:', response.status);
console.log('Response ok:', response.ok);

// Test 4: Basic fetch function with text response
console.log('\nTest 4: Basic fetch function');
try {
  const textResponse = fetch('https://httpbin.org/plain');
  console.log('Text fetch result:', typeof textResponse);
  console.log('Text response status:', textResponse.status);
  console.log('Text response ok:', textResponse.ok);
  console.log('Text response body:', textResponse.text());
  
  // Test JSON response
  const jsonResponse = fetch('https://httpbin.org/json');
  console.log('JSON fetch result:', typeof jsonResponse);
  console.log('JSON response status:', jsonResponse.status);
  console.log('JSON response text:', jsonResponse.text());
  
  // Test JSON parsing
  const jsonData = jsonResponse.json();
  console.log('Parsed JSON:', JSON.stringify(jsonData));
  console.log('JSON message:', jsonData.message);
  
} catch (error) {
  console.log('Fetch error:', error.message);
}

console.log('\n=== All Fetch API tests completed ===');