// Advanced HTTP Module Loading Example
// Demonstrates relative imports and mixed module systems

console.log('=== Advanced HTTP Module Loading ===');

console.log('\n1. Relative Imports:');
console.log('   When loading a module from HTTP, relative imports work within the same domain:');
console.log('   Base: https://esm.sh/my-package/index.js');
console.log('   Relative: import "./utils/helper.js"');
console.log('   Resolved: https://esm.sh/my-package/utils/helper.js');

console.log('\n2. Mixed Module Systems:');
console.log('   You can mix local and HTTP modules:');
console.log('   const localUtil = require("./local-utils.js");');
console.log('   const httpUtil = require("https://cdn.skypack.dev/date-fns");');

console.log('\n3. Error Handling:');
console.log('   try {');
console.log('     const module = require("https://cdn.skypack.dev/nonexistent");');
console.log('   } catch (error) {');
console.log('     if (error.message.includes("Security validation failed")) {');
console.log('       console.log("Domain not allowed");');
console.log('     } else if (error.message.includes("Network error")) {');
console.log('       console.log("Network issue, try fallback");');
console.log('     }');
console.log('   }');

console.log('\n4. Cache Benefits:');
console.log('   - First load: Downloads from CDN');
console.log('   - Subsequent loads: Served from memory cache');
console.log('   - Cache respects TTL and HTTP headers');

// Example function to demonstrate mixed usage
function demonstrateMixedModules() {
  console.log('\n=== Mixed Module Example ===');
  
  // Local jsrt modules always work
  const assert = require('jsrt:assert');
  console.log('✓ Loaded jsrt:assert module');
  
  // Built-in Node.js compatibility modules
  try {
    const path = require('node:path');
    console.log('✓ Loaded node:path module');
  } catch (e) {
    console.log('⚠️ Node.js modules not available');
  }
  
  // HTTP modules (if enabled)
  if (process.env.JSRT_HTTP_MODULES_ENABLED === '1') {
    console.log('✓ HTTP modules are enabled - ready for CDN imports');
    
    // Example of how you would use HTTP modules:
    console.log('Example usage:');
    console.log('  const _ = require("https://cdn.skypack.dev/lodash");');
    console.log('  const React = require("https://esm.sh/react");');
    
  } else {
    console.log('✓ HTTP modules are disabled - using local modules only');
  }
}

demonstrateMixedModules();

console.log('\n=== Performance Tips ===');
console.log('1. Pin versions in URLs for better caching:');
console.log('   ✓ https://cdn.skypack.dev/lodash@4.17.21');
console.log('   ❌ https://cdn.skypack.dev/lodash');

console.log('\n2. Preload critical modules at startup:');
console.log('   const criticalModules = [');
console.log('     "https://esm.sh/react",');
console.log('     "https://cdn.skypack.dev/lodash"');
console.log('   ];');
console.log('   // Load them early to warm cache');

console.log('\n3. Use fallback strategies:');
console.log('   let dateLib;');
console.log('   try {');
console.log('     dateLib = require("https://esm.sh/date-fns");');
console.log('   } catch {');
console.log('     dateLib = require("./local-date-utils.js");');
console.log('   }');

console.log('\n=== Security Best Practices ===');
console.log('1. Always use HTTPS URLs');
console.log('2. Only allow trusted CDN domains');
console.log('3. Set reasonable size limits');
console.log('4. Monitor cache usage');
console.log('5. Use version pinning in production');

console.log('\n=== Advanced Example Complete ===');