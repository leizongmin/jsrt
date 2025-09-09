// Simple fetch network demo
console.log('🌐 Fetch Network Demo');
console.log('=====================');

console.log('1. Making GET request to httpbin.org...');
const promise = fetch('http://httpbin.org/get');

promise
  .then((response) => {
    console.log('✅ Response received!');
    console.log('   Status:', response.status);
    console.log('   OK:', response.ok);

    console.log('2. Getting response JSON...');
    return response.json();
  })
  .then((data) => {
    console.log('✅ JSON parsed successfully!');
    console.log('   Your IP:', data.origin);
    console.log('   User-Agent:', data.headers['User-Agent']);
    console.log('\n🎉 Network demonstration completed!');
  })
  .catch((error) => {
    console.log('❌ Error:', error.message);
  });

console.log('Network demonstration initiated...');
