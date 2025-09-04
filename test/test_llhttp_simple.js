const assert = require('jsrt:assert');

console.log('=== Simple HTTP llhttp Test ===');

// Test basic local fetch
console.log('Testing simple GET request...');

// Test with a simple mock response first
fetch('http://httpbin.org/get')
  .then((response) => {
    console.log('Response received!');
    console.log('Status:', response.status);
    console.log('Status Text:', response.statusText);
    console.log('OK:', response.ok);
    console.log('Headers:', typeof response.headers);

    if (response.text) {
      console.log('Has text() method');
      return response.text();
    } else {
      console.log('No text() method available');
      return Promise.resolve('');
    }
  })
  .then((text) => {
    console.log('Body length:', text.length);
    console.log('Body preview:', text.substring(0, 100) + '...');
    console.log('✓ Simple test completed');
  })
  .catch((error) => {
    console.error('✗ Test failed:', error.message);
    console.error('Error stack:', error.stack);
  });

// Give time for async operation
setTimeout(() => {
  console.log('=== Test completed ===');
}, 3000);
