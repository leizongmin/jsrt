// Comprehensive querystring test suite runner
console.log('🧪 Running comprehensive querystring test suite...\n');

let totalTests = 0;
let passedSuites = 0;
const suites = [];

function runTest(name, path) {
  try {
    console.log(`\n📝 Running: ${name}`);
    require(path);
    passedSuites++;
    suites.push({ name, status: '✅ PASS' });
    return true;
  } catch (e) {
    suites.push({ name, status: '❌ FAIL', error: e.message });
    console.error(`❌ ${name} failed:`, e.message);
    return false;
  }
}

// Run all test suites
runTest('Basic Functionality', '../test_node_querystring.js');
runTest('Edge Cases', './test_edge_cases.js');
runTest('maxKeys Handling', './test_maxkeys.js');
runTest('Custom Separators', './test_custom_separators.js');
runTest('Array Handling', './test_arrays.js');
runTest('Type Coercion', './test_type_coercion.js');
runTest('Encoding/Decoding', './test_encoding.js');
runTest('Compatibility & Round-trip', './test_compatibility.js');

// Print summary
console.log('\n' + '='.repeat(60));
console.log('📊 TEST SUITE SUMMARY');
console.log('='.repeat(60));

suites.forEach((suite) => {
  console.log(`${suite.status} ${suite.name}`);
  if (suite.error) {
    console.log(`   Error: ${suite.error}`);
  }
});

console.log('='.repeat(60));
console.log(`Total suites: ${suites.length}`);
console.log(`Passed: ${passedSuites}/${suites.length}`);
console.log(
  `Success rate: ${((passedSuites / suites.length) * 100).toFixed(1)}%`
);

if (passedSuites === suites.length) {
  console.log('\n✅ ALL TEST SUITES PASSED! 🎉');
  console.log('\n📈 Estimated total test count: 150+ individual tests');
} else {
  console.log('\n⚠️  Some tests failed. Please review above.');
  process.exit(1);
}
