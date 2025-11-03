/**
 * Test Module Load Hooks
 *
 * Tests load hook execution with various source types and scenarios.
 */

const assert = require('assert');
const module = require('node:module');

// Test data
const testSourceCode = 'export const value = 42;';
const testBinaryData = new Uint8Array([0x65, 0x78, 0x70, 0x6f, 0x72, 0x74]); // "export" in ASCII

console.log('Testing module load hooks...');

// Test 1: Load hook with string source
console.log('Test 1: Load hook with string source');

let loadHookCalled = false;
let loadHookUrl = '';
let loadHookContext = null;

module.registerHooks({
  load(url, context, nextLoad) {
    loadHookCalled = true;
    loadHookUrl = url;
    loadHookContext = context;

    console.log(`Load hook called for URL: ${url}`);
    console.log(`Context format: ${context.format}`);
    console.log(`Context conditions: ${JSON.stringify(context.conditions)}`);

    // Return transformed source
    return {
      format: 'module',
      source: testSourceCode,
      shortCircuit: true,
    };
  },
});

// Try to load a module (this will trigger the load hook)
try {
  // Since we're testing load hooks, we need to attempt to require/import something
  // The actual loading might fail if the module doesn't exist, but we're testing hook execution
  require('./non-existent-module.js');
} catch (e) {
  // Expected to fail, but hook should have been called
  console.log('Expected error:', e.message);
}

assert(loadHookCalled, 'Load hook should have been called');
console.log('✓ Load hook executed with string source');

// Test 2: Load hook with binary source (Uint8Array)
console.log('\nTest 2: Load hook with binary source');

let binaryHookCalled = false;

module.registerHooks({
  load(url, context, nextLoad) {
    binaryHookCalled = true;

    console.log(`Binary load hook called for URL: ${url}`);

    // Return binary source as Uint8Array
    return {
      format: 'module',
      source: testBinaryData,
      shortCircuit: true,
    };
  },
});

try {
  require('./another-non-existent-module.js');
} catch (e) {
  console.log('Expected error:', e.message);
}

assert(binaryHookCalled, 'Binary load hook should have been called');
console.log('✓ Load hook executed with binary source');

// Test 3: Load hook with ArrayBuffer
console.log('\nTest 3: Load hook with ArrayBuffer');

let arrayBufferHookCalled = false;

module.registerHooks({
  load(url, context, nextLoad) {
    arrayBufferHookCalled = true;

    console.log(`ArrayBuffer load hook called for URL: ${url}`);

    // Return binary source as ArrayBuffer
    return {
      format: 'module',
      source: testBinaryData.buffer,
      shortCircuit: true,
    };
  },
});

try {
  require('./third-non-existent-module.js');
} catch (e) {
  console.log('Expected error:', e.message);
}

assert(arrayBufferHookCalled, 'ArrayBuffer load hook should have been called');
console.log('✓ Load hook executed with ArrayBuffer source');

// Test 4: Load hook with chaining (nextLoad)
console.log('\nTest 4: Load hook with chaining');

let firstHookCalled = false;
let secondHookCalled = false;
let nextLoadCalled = false;

module.registerHooks({
  load(url, context, nextLoad) {
    firstHookCalled = true;
    console.log(`First load hook called for URL: ${url}`);

    // Call nextLoad to continue chain
    const result = nextLoad(url, context);
    nextLoadCalled = true;

    return result;
  },
});

module.registerHooks({
  load(url, context, nextLoad) {
    secondHookCalled = true;
    console.log(`Second load hook called for URL: ${url}`);

    // Call nextLoad to continue chain
    const result = nextLoad(url, context);
    console.log(`Second hook nextLoad returned: ${typeof result}`);

    // Return our own result without short-circuiting
    return {
      format: 'module',
      source: testSourceCode,
      shortCircuit: false,
    };
  },
});

try {
  require('./fourth-non-existent-module.js');
} catch (e) {
  console.log('Expected error:', e.message);
}

assert(firstHookCalled, 'First load hook should have been called');
assert(secondHookCalled, 'Second load hook should have been called');
assert(nextLoadCalled, 'nextLoad should have been called');
console.log('✓ Load hook chaining works correctly');

// Test 5: Load hook without shortCircuit (should continue)
console.log('\nTest 5: Load hook without shortCircuit');

let noShortCircuitHookCalled = false;
let fallbackHookCalled = false;

module.registerHooks({
  load(url, context, nextLoad) {
    noShortCircuitHookCalled = true;
    console.log(`No short-circuit load hook called for URL: ${url}`);

    return {
      format: 'module',
      source: testSourceCode,
      shortCircuit: false, // Don't short-circuit
    };
  },
});

module.registerHooks({
  load(url, context, nextLoad) {
    fallbackHookCalled = true;
    console.log(`Fallback load hook called for URL: ${url}`);

    return {
      format: 'module',
      source: 'export const fallback = true;',
      shortCircuit: true,
    };
  },
});

try {
  require('./fifth-non-existent-module.js');
} catch (e) {
  console.log('Expected error:', e.message);
}

// Based on LIFO order, the fallback hook (registered second) is called first
// Since it returns shortCircuit: true, the no-shortCircuit hook should NOT be called
assert(
  !noShortCircuitHookCalled,
  'No short-circuit load hook should NOT have been called (short-circuited)'
);
assert(fallbackHookCalled, 'Fallback load hook should have been called');
console.log('✓ Load hook shortCircuit behavior works correctly');

// Test 6: Load hook with different formats
console.log('\nTest 6: Load hook with different formats');

let formatHookCalled = false;
let receivedFormat = '';

module.registerHooks({
  load(url, context, nextLoad) {
    formatHookCalled = true;
    receivedFormat = context.format || 'undefined';

    console.log(`Format load hook called for URL: ${url}`);
    console.log(`Received format: ${receivedFormat}`);

    return {
      format: 'commonjs',
      source: 'module.exports = { format: "commonjs" };',
      shortCircuit: true,
    };
  },
});

try {
  require('./sixth-non-existent-module.js');
} catch (e) {
  console.log('Expected error:', e.message);
}

assert(formatHookCalled, 'Format load hook should have been called');
console.log(`✓ Load hook received format: ${receivedFormat}`);

// Test 7: Load hook with conditions
console.log('\nTest 7: Load hook with conditions');

let conditionsHookCalled = false;
let receivedConditions = [];

module.registerHooks({
  load(url, context, nextLoad) {
    conditionsHookCalled = true;
    receivedConditions = context.conditions || [];

    console.log(`Conditions load hook called for URL: ${url}`);
    console.log(`Received conditions: ${JSON.stringify(receivedConditions)}`);

    return {
      format: 'module',
      source: testSourceCode,
      shortCircuit: true,
    };
  },
});

try {
  require('./seventh-non-existent-module.js');
} catch (e) {
  console.log('Expected error:', e.message);
}

assert(conditionsHookCalled, 'Conditions load hook should have been called');
console.log(
  `✓ Load hook received conditions: ${JSON.stringify(receivedConditions)}`
);

console.log('\n=== All load hook tests passed! ===');

// Test 8: Legacy string result compatibility
console.log('\nTest 8: Legacy string result compatibility');

let legacyHookCalled = false;

module.registerHooks({
  load(url, context, nextLoad) {
    legacyHookCalled = true;
    console.log(`Legacy load hook called for URL: ${url}`);

    // Return simple string (legacy format)
    return 'export const legacy = true;';
  },
});

try {
  require('./eighth-non-existent-module.js');
} catch (e) {
  console.log('Expected error:', e.message);
}

assert(legacyHookCalled, 'Legacy load hook should have been called');
console.log('✓ Legacy string result compatibility works');

console.log('\n🎉 All load hook tests completed successfully!');
