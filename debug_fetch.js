console.log('=== Debug Fetch Test ===');

// Test single request with debugging
console.log('Testing single GET request...');
fetch('http://httpbin.org/get')
  .then((response) => {
    console.log('✓ Response received');
    console.log('Status:', response.status);
    console.log('StatusText:', response.statusText);
    console.log('OK:', response.ok);
    console.log('Headers type:', typeof response.headers);
    
    return response.text();
  })
  .then((text) => {
    console.log('✓ Text received, length:', text.length);
    console.log('First 100 chars:', text.substring(0, 100));
    console.log('=== Debug test completed successfully ===');
  })
  .catch((error) => {
    console.error('✗ Debug test failed:', error.message);
    console.error('Stack:', error.stack);
  });

// Shorter timeout for debug
setTimeout(() => {
  console.log('=== Debug timeout reached ===');
}, 2000);