// HTTP Module Loading Examples
// This file demonstrates how to use HTTP module loading in jsrt

console.log('=== HTTP Module Loading Examples ===');

console.log('\n1. Basic Usage:');
console.log('   Enable HTTP module loading by setting environment variables:');
console.log('   JSRT_HTTP_MODULES_ENABLED=1');
console.log('   JSRT_HTTP_MODULES_ALLOWED="cdn.skypack.dev,esm.sh,cdn.jsdelivr.net,unpkg.com"');

console.log('\n2. ES Module Import (when enabled):');
console.log('   import React from "https://esm.sh/react";');
console.log('   import { render } from "https://esm.sh/react-dom";');

console.log('\n3. CommonJS Require (when enabled):');
console.log('   const _ = require("https://cdn.skypack.dev/lodash");');
console.log('   const React = require("https://esm.sh/react");');

console.log('\n4. Supported CDNs:');
console.log('   - Skypack: https://cdn.skypack.dev/package-name');
console.log('   - esm.sh: https://esm.sh/package-name');  
console.log('   - jsDelivr: https://cdn.jsdelivr.net/npm/package-name');
console.log('   - unpkg: https://unpkg.com/package-name');

console.log('\n5. Configuration Options:');
console.log('   JSRT_HTTP_MODULES_ENABLED=1          # Enable HTTP module loading');
console.log('   JSRT_HTTP_MODULES_HTTPS_ONLY=1       # Require HTTPS (default)');
console.log('   JSRT_HTTP_MODULES_ALLOWED="..."      # Comma-separated allowed domains');
console.log('   JSRT_HTTP_MODULES_CACHE_SIZE=100     # Number of modules to cache');
console.log('   JSRT_HTTP_MODULES_CACHE_TTL=3600     # Cache time-to-live in seconds');
console.log('   JSRT_HTTP_MODULES_TIMEOUT=30         # Download timeout in seconds');
console.log('   JSRT_HTTP_MODULES_MAX_SIZE=10485760  # Max module size in bytes');

console.log('\n6. Security Features:');
console.log('   - Domain allowlist (only trusted CDNs)');
console.log('   - HTTPS-only mode by default');
console.log('   - Content-Type validation');
console.log('   - Size limits to prevent abuse');
console.log('   - Relative import resolution within same domain');

console.log('\n7. Performance Features:');
console.log('   - LRU cache with configurable size');
console.log('   - TTL-based cache expiration');
console.log('   - ETag and Last-Modified header support');
console.log('   - Efficient module compilation');

console.log('\n=== Try the Examples ===');

// Only try to load modules if HTTP loading is enabled
if (process.env.JSRT_HTTP_MODULES_ENABLED === '1') {
  console.log('\n🌐 HTTP module loading is enabled! Attempting to load a module...');
  
  try {
    // Try to load a simple utility module
    console.log('Loading a simple module from CDN...');
    
    // This is commented out because it requires network access
    // const _ = require('https://cdn.skypack.dev/lodash');
    // console.log('✅ Successfully loaded lodash from Skypack');
    // console.log('Available functions:', Object.keys(_).slice(0, 5).join(', '), '...');
    
    console.log('✅ HTTP module loading system is ready!');
    console.log('Uncomment the require() lines above to test with real CDNs.');
    
  } catch (error) {
    console.log('❌ Error loading HTTP module:', error.message);
    console.log('This is expected if network access is limited or domains are not in allowlist.');
  }
} else {
  console.log('\n⚠️  HTTP module loading is disabled.');
  console.log('To enable it, run this example with:');
  console.log('');
  console.log('JSRT_HTTP_MODULES_ENABLED=1 \\');
  console.log('JSRT_HTTP_MODULES_ALLOWED="cdn.skypack.dev,esm.sh,cdn.jsdelivr.net,unpkg.com" \\');
  console.log('./jsrt examples/modules/http_modules.js');
}

console.log('\n=== End of Examples ===');