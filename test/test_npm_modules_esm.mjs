// Test npm ES module support
console.log('=== NPM ES Module Loading Tests ===');

// Test 1: Import from npm package (ESM)
console.log('\nTest 1: Basic npm package with import');
const { greetFromESMPackage, esmPackageData } = await import('test-package');
if (typeof greetFromESMPackage !== 'function') {
  throw new Error('greetFromESMPackage should be a function');
}
if (typeof esmPackageData !== 'object') {
  throw new Error('esmPackageData should be an object');
}
if (esmPackageData.version !== '1.0.0') {
  throw new Error('version should be 1.0.0');
}
console.log('✓ Basic npm ESM package imported successfully');
console.log('  greet result:', greetFromESMPackage('esm-npm'));

// Test 2: Dynamic import
console.log('\nTest 2: Dynamic import');
const dynamicMod = await import('test-package');
if (typeof dynamicMod.greetFromESMPackage !== 'function') {
  throw new Error('Dynamic import should work');
}
console.log('✓ Dynamic import works correctly');
console.log('  dynamic greet result:', dynamicMod.greetFromESMPackage('dynamic'));

// Test 3: Non-existent package
console.log('\nTest 3: Non-existent ESM package handling');
try {
  await import('non-existent-esm-package');
  throw new Error('Should have thrown an error for non-existent package');
} catch (error) {
  if (!error.message.includes('could not load module') && !error.message.includes('Cannot find module')) {
    throw error;
  }
  console.log('✓ Non-existent ESM package handled correctly');
}

console.log('\n=== All npm ES module tests passed ===');