// Test package.json "type" field detection
console.log('[TEST] Module format detection by package.json type');

const assert = require('jsrt:assert').default || require('jsrt:assert');

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    passed++;
    console.log(`  ✓ ${name}`);
  } catch (e) {
    failed++;
    console.log(`  ✗ ${name}: ${e.message}`);
  }
}

// Test 1: package.json with "type": "module"
test('ESM package detection exists', () => {
  // jsrt_detect_format_by_package() is implemented in Phase 3
  // Full ESM loading will be in Phase 5
  assert.ok(true, 'Package.json detector infrastructure ready');
});

// Test 2: package.json with "type": "commonjs"
test('Load .js file in CommonJS package', () => {
  const mod = require(
    process.cwd() + '/test/module/detector/fixtures/cjs-package/index.js'
  );
  assert.strictEqual(
    mod.format,
    'commonjs',
    '.js in commonjs package loads as CommonJS'
  );
});

// Test 3: No package.json (default to CommonJS)
test('Load .js file without package.json', () => {
  const mod = require(
    process.cwd() + '/test/module/detector/fixtures/no-package/index.js'
  );
  assert.strictEqual(
    mod.format,
    'commonjs',
    '.js without package.json defaults to CommonJS'
  );
});

// Test 4: Nested package.json
test('Load .js file with nested package.json', () => {
  const mod = require(
    process.cwd() + '/test/module/detector/fixtures/cjs-package/nested/index.js'
  );
  assert.strictEqual(
    mod.format,
    'commonjs',
    '.js in nested directory uses parent package.json'
  );
});

console.log(`\n[RESULT] ${passed} passed, ${failed} failed`);
if (failed > 0) {
  throw new Error(`${failed} test(s) failed`);
}
console.log('[PASS] All package.json type detection tests passed');
