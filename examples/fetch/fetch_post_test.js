// Test POST request with body
console.log('🧪 Testing POST request...');

const data = {
  message: 'Hello from jsrt!',
  timestamp: Date.now(),
  test: true,
};

console.log('Making POST request...');
fetch('http://httpbin.org/post', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
    'User-Agent': 'jsrt-test/1.0',
  },
  body: JSON.stringify(data),
})
  .then((response) => {
    console.log('✅ POST Success! Status:', response.status);
    return response.json();
  })
  .then((responseData) => {
    console.log('✅ JSON received!');
    console.log('Echoed data:', responseData.json.message);
    console.log('Request headers were:', responseData.headers['Content-Type']);
  })
  .catch((error) => {
    console.log('❌ POST Error:', error.message);
  });

console.log('✅ POST test setup complete');
