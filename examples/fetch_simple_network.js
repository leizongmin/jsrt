// Simple network test
console.log('Testing simple network request...');

const promise = fetch('http://httpbin.org/get');

promise
  .then((response) => {
    console.log('✅ Response received!');
    console.log('Status:', response.status);
    console.log('OK:', response.ok);
    return response.text();
  })
  .then((text) => {
    console.log('Response length:', text.length, 'characters');
    console.log('Test completed successfully!');
  })
  .catch((error) => {
    console.log('❌ Error:', error.message);
  });

console.log('Request initiated, waiting for response...');
