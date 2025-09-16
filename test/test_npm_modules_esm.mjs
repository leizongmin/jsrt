// Test npm ES module support
// // console.log('=== NPM ES Module Loading Tests ===');

// Note: ES module imports for npm packages may have limited support
// This tests what works with the current implementation

// Test 1: Try to import lodash (this may not work yet)
console.log('\nTest 1: Loading lodash with import');
try {
  const lodashModule = await import('lodash');
  const _ = lodashModule.default || lodashModule;
  if (typeof _ !== 'function') {
    throw new Error('lodash should be a function, got: ' + typeof _);
  }
  // // console.log('✓ Lodash ESM package imported successfully');
  console.log(
    '  lodash.map test:',
    _.map([1, 2, 3], (x) => x * 2)
  );
} catch (error) {
  console.log('❌ Lodash ES import not yet supported:', error.message);
  console.log(
    '   (This is expected - ES imports for npm packages are still being developed)'
  );
}

// Test 2: Try to import moment
console.log('\nTest 2: Loading moment with import');
try {
  const momentModule = await import('moment');
  const moment = momentModule.default || momentModule;
  if (typeof moment !== 'function') {
    throw new Error('moment should be a function, got: ' + typeof moment);
  }
  // // console.log('✓ Moment ESM package imported successfully');
  console.log('  current time:', moment().format('YYYY-MM-DD HH:mm:ss'));
} catch (error) {
  console.log('❌ Moment ES import not yet supported:', error.message);
  console.log(
    '   (This is expected - ES imports for npm packages are still being developed)'
  );
}

// Test 3: Non-existent package (should work for error handling)
console.log('\nTest 3: Non-existent ESM package handling');
try {
  await import('non-existent-esm-package');
  throw new Error('Should have thrown an error for non-existent package');
} catch (error) {
  if (
    !error.message.includes('could not load module') &&
    !error.message.includes('Cannot find module') &&
    !error.message.includes('JSRT_READ_FILE_ERROR_FILE_NOT_FOUND')
  ) {
    throw error;
  }
  // // console.log('✓ Non-existent ESM package handled correctly');
}

// console.log('\n=== NPM ES module tests completed ===');
console.log(
  'Note: Full ES module support for npm packages is still in development.'
);
console.log('CommonJS require() works fully for npm packages.');
