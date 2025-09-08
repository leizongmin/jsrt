// HTTP Module Loading Examples
// This file demonstrates actual HTTP module loading in jsrt

console.log('=== HTTP Module Loading Examples ===');

console.log('\n🌐 Testing real module imports from different CDNs...');

// Test 1: Load lodash from cdn.skypack.dev
console.log('\n--- Test 1: Loading lodash from Skypack ---');
try {
  const _ = require('https://cdn.skypack.dev/lodash');
  console.log('✅ Successfully loaded lodash from Skypack');
  console.log('Lodash version:', _.VERSION || 'unknown');
  console.log('Sample functions:', Object.keys(_).slice(0, 10).join(', '));
  console.log(
    'Testing _.chunk([1,2,3,4], 2):',
    JSON.stringify(_.chunk([1, 2, 3, 4], 2))
  );
} catch (error) {
  console.log('❌ Failed to load lodash from Skypack:', error.message);
}

// Test 2: Load lodash from esm.sh
console.log('\n--- Test 2: Loading lodash from esm.sh ---');
try {
  const lodashEsm = require('https://esm.sh/lodash');
  console.log('✅ Successfully loaded lodash from esm.sh');
  console.log('Lodash version:', lodashEsm.VERSION || 'unknown');
  console.log(
    'Testing lodashEsm.pick({a:1,b:2,c:3}, ["a","c"]):',
    JSON.stringify(lodashEsm.pick({ a: 1, b: 2, c: 3 }, ['a', 'c']))
  );
} catch (error) {
  console.log('❌ Failed to load lodash from esm.sh:', error.message);
}

// Test 3: Load lodash from jsDelivr
console.log('\n--- Test 3: Loading lodash from jsDelivr ---');
try {
  const lodashJsd = require('https://cdn.jsdelivr.net/npm/lodash@4.17.21/lodash.min.js');
  console.log('✅ Successfully loaded lodash from jsDelivr');
  console.log('Lodash version:', lodashJsd.VERSION || 'unknown');
  console.log(
    'Testing lodashJsd.uniq([1,2,2,3,3,4]):',
    JSON.stringify(lodashJsd.uniq([1, 2, 2, 3, 3, 4]))
  );
} catch (error) {
  console.log('❌ Failed to load lodash from jsDelivr:', error.message);
}

// Test 4: Load lodash from unpkg
console.log('\n--- Test 4: Loading lodash from unpkg ---');
try {
  const lodashUnpkg = require('https://unpkg.com/lodash@4.17.21/lodash.min.js');
  console.log('✅ Successfully loaded lodash from unpkg');
  console.log('Lodash version:', lodashUnpkg.VERSION || 'unknown');
  console.log(
    'Testing lodashUnpkg.range(1, 5):',
    JSON.stringify(lodashUnpkg.range(1, 5))
  );
} catch (error) {
  console.log('❌ Failed to load lodash from unpkg:', error.message);
}

// Test 5: Load React from esm.sh
console.log('\n--- Test 5: Loading React from esm.sh ---');
try {
  const React = require('https://esm.sh/react@18');
  console.log('✅ Successfully loaded React from esm.sh');
  console.log('React version:', React.version || 'unknown');
  console.log('React object keys:', Object.keys(React).slice(0, 10).join(', '));

  // Test creating a simple element
  if (React.createElement) {
    const element = React.createElement(
      'div',
      { className: 'test' },
      'Hello from HTTP-loaded React!'
    );
    console.log('Created React element:', element.type, element.props);
  }
} catch (error) {
  console.log('❌ Failed to load React from esm.sh:', error.message);
}

// Test 6: Load React from esm.run
console.log('\n--- Test 6: Loading React from esm.run ---');
try {
  const ReactRun = require('https://esm.run/react@18');
  console.log('✅ Successfully loaded React from esm.run');
  console.log('React version:', ReactRun.version || 'unknown');
  console.log(
    'React object keys:',
    Object.keys(ReactRun).slice(0, 10).join(', ')
  );
} catch (error) {
  console.log('❌ Failed to load React from esm.run:', error.message);
}

console.log('\n=== HTTP Module Loading Test Complete ===');
console.log('🎉 All supported CDN domains tested!');
console.log('   - cdn.skypack.dev ✓');
console.log('   - esm.sh ✓');
console.log('   - cdn.jsdelivr.net ✓');
console.log('   - unpkg.com ✓');
console.log('   - esm.run ✓');
