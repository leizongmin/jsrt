// Working fetch test
console.log('🧪 Testing working fetch...');

console.log('Creating fetch request...');
const promise = fetch('http://httpbin.org/get');

console.log('Adding handlers...');
promise
  .then(response => {
    console.log('✅ Success! Status:', response.status);
    console.log('Response OK:', response.ok);
    return response.text();
  })
  .then(text => {
    console.log('✅ Text received, length:', text.length);
    console.log('Sample:', text.substring(0, 100) + '...');
  })
  .catch(error => {
    console.log('❌ Error:', error.message);
  });

console.log('✅ Test setup complete - waiting for response...');