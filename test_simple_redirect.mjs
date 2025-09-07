console.log('=== Simple HTTP to HTTPS Redirect Test ===');

async function testRedirect() {
  try {
    // Test HTTP to HTTPS redirect with a CommonJS compatible CDN
    console.log('Testing HTTP to HTTPS redirect with jsDelivr (CommonJS)...');
    const result = require('http://cdn.jsdelivr.net/npm/lodash@4.17.21/lodash.min.js');
    
    console.log('✅ SUCCESS: HTTP to HTTPS redirect worked!');
    console.log('Module type:', typeof result);
    console.log('Has VERSION property:', typeof result.VERSION !== 'undefined');
    
    if (result.VERSION) {
      console.log('Lodash VERSION:', result.VERSION);
    }
    
    console.log('🎯 HTTP to HTTPS redirect functionality is working perfectly!');
    return true;
  } catch (error) {
    console.log('❌ FAILED:', error.message);
    return false;
  }
}

testRedirect().then(success => {
  if (success) {
    console.log('\n=== Test Completed Successfully ===');
    process.exit(0);
  } else {
    process.exit(1);
  }
}).catch(error => {
  console.error('Test error:', error.message);
  process.exit(1);
});