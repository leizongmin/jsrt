/**
 * WPT Test Harness Adapter for jsrt
 * Provides a minimal WPT-compatible test harness that uses jsrt's built-in assert module
 */

// Import jsrt's assert module
const assert = require('jsrt:assert');

// Global test state
let tests = [];
let currentTest = null;
let testCounter = 0;

// Test result types
const TEST_PASS = 'PASS';
const TEST_FAIL = 'FAIL';
const TEST_TIMEOUT = 'TIMEOUT';

// WPT-compatible assertion functions
function assert_true(actual, description) {
    try {
        assert.strictEqual(!!actual, true, description || 'assert_true failed');
    } catch (e) {
        throw new Error(description || e.message);
    }
}

function assert_false(actual, description) {
    try {
        assert.strictEqual(!!actual, false, description || 'assert_false failed');
    } catch (e) {
        throw new Error(description || e.message);
    }
}

function assert_equals(actual, expected, description) {
    try {
        assert.strictEqual(actual, expected, description || `assert_equals failed: expected ${expected}, got ${actual}`);
    } catch (e) {
        throw new Error(description || e.message);
    }
}

function assert_not_equals(actual, expected, description) {
    try {
        assert.notStrictEqual(actual, expected, description || 'assert_not_equals failed');
    } catch (e) {
        throw new Error(description || e.message);
    }
}

function assert_array_equals(actual, expected, description) {
    try {
        if (!Array.isArray(actual)) {
            throw new Error('actual is not an array');
        }
        if (!Array.isArray(expected)) {
            throw new Error('expected is not an array');
        }
        if (actual.length !== expected.length) {
            throw new Error(`Array length mismatch: expected ${expected.length}, got ${actual.length}`);
        }
        for (let i = 0; i < actual.length; i++) {
            if (actual[i] !== expected[i]) {
                throw new Error(`Array element ${i} mismatch: expected ${expected[i]}, got ${actual[i]}`);
            }
        }
    } catch (e) {
        throw new Error(description || e.message);
    }
}

function assert_throws(errorType, func, description) {
    try {
        if (typeof errorType === 'string') {
            // If first param is string, it's actually the function and description
            description = func;
            func = errorType;
            errorType = Error;
        }
        
        let threw = false;
        let caughtError = null;
        
        try {
            func();
        } catch (e) {
            threw = true;
            caughtError = e;
        }
        
        if (!threw) {
            throw new Error('Expected function to throw but it did not');
        }
        
        if (errorType && !(caughtError instanceof errorType)) {
            throw new Error(`Expected error of type ${errorType.name} but got ${caughtError.constructor.name}`);
        }
    } catch (e) {
        throw new Error(description || e.message);
    }
}

function assert_unreached(description) {
    throw new Error(description || 'Reached unreachable code');
}

// Test management functions
function test(func, name) {
    testCounter++;
    const testName = name || `Test ${testCounter}`;
    
    const testObj = {
        name: testName,
        func: func,
        status: null,
        message: null
    };
    
    tests.push(testObj);
    
    // Run the test immediately
    currentTest = testObj;
    
    try {
        func();
        testObj.status = TEST_PASS;
        testObj.message = 'OK';
        console.log(`✅ ${testName}`);
    } catch (e) {
        testObj.status = TEST_FAIL;
        testObj.message = e.message;
        console.log(`❌ ${testName}: ${e.message}`);
    } finally {
        currentTest = null;
    }
}

function async_test(func, name) {
    testCounter++;
    const testName = name || `Async Test ${testCounter}`;
    
    let completed = false;
    const testObj = {
        name: testName,
        func: func,
        status: null,
        message: null
    };
    
    tests.push(testObj);
    currentTest = testObj;
    
    const t = {
        step: function(stepFunc, ...args) {
            if (completed) return;
            try {
                return stepFunc.apply(this, args);
            } catch (e) {
                this.step_func = null;
                throw e;
            }
        },
        
        step_func: function(stepFunc, context) {
            return (...args) => {
                return t.step(() => stepFunc.apply(context || this, args));
            };
        },
        
        done: function() {
            if (completed) return;
            completed = true;
            testObj.status = TEST_PASS;
            testObj.message = 'OK';
            console.log(`✅ ${testName}`);
            currentTest = null;
        }
    };
    
    try {
        func(t);
    } catch (e) {
        if (!completed) {
            completed = true;
            testObj.status = TEST_FAIL;
            testObj.message = e.message;
            console.log(`❌ ${testName}: ${e.message}`);
            currentTest = null;
        }
    }
    
    return t;
}

function promise_test(func, name) {
    testCounter++;
    const testName = name || `Promise Test ${testCounter}`;
    
    const testObj = {
        name: testName,
        func: func,
        status: null,
        message: null
    };
    
    tests.push(testObj);
    currentTest = testObj;
    
    try {
        const result = func();
        if (result && typeof result.then === 'function') {
            // It's a promise
            result.then(() => {
                testObj.status = TEST_PASS;
                testObj.message = 'OK';
                console.log(`✅ ${testName}`);
            }).catch(e => {
                testObj.status = TEST_FAIL;
                testObj.message = e.message;
                console.log(`❌ ${testName}: ${e.message}`);
            }).finally(() => {
                currentTest = null;
            });
        } else {
            // Sync function that returned
            testObj.status = TEST_PASS;
            testObj.message = 'OK';
            console.log(`✅ ${testName}`);
            currentTest = null;
        }
    } catch (e) {
        testObj.status = TEST_FAIL;
        testObj.message = e.message;
        console.log(`❌ ${testName}: ${e.message}`);
        currentTest = null;
    }
}

// Setup function (no-op for jsrt)
function setup() {
    // No-op in jsrt environment
}

// Make functions global
globalThis.assert_true = assert_true;
globalThis.assert_false = assert_false;
globalThis.assert_equals = assert_equals;
globalThis.assert_not_equals = assert_not_equals;
globalThis.assert_array_equals = assert_array_equals;
globalThis.assert_throws = assert_throws;
globalThis.assert_unreached = assert_unreached;
globalThis.test = test;
globalThis.async_test = async_test;
globalThis.promise_test = promise_test;
globalThis.setup = setup;

// For compatibility with different global objects
if (typeof self === 'undefined') {
    globalThis.self = globalThis;
}

// Test completion summary - print at the end since jsrt doesn't have process.on('exit')
function printTestSummary() {
    const passed = tests.filter(t => t.status === TEST_PASS).length;
    const failed = tests.filter(t => t.status === TEST_FAIL).length;
    const total = tests.length;
    
    if (total > 0) {
        console.log(`\n=== Test Results ===`);
        console.log(`Total: ${total}, Passed: ${passed}, Failed: ${failed}`);
        
        if (failed > 0) {
            console.log('\nFailed tests:');
            tests.filter(t => t.status === TEST_FAIL).forEach(t => {
                console.log(`  ❌ ${t.name}: ${t.message}`);
            });
        }
    }
}

// Set up automatic summary printing with a timeout
setTimeout(() => {
    printTestSummary();
}, 100);

// Export for CommonJS compatibility
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        assert_true,
        assert_false,
        assert_equals,
        assert_not_equals,
        assert_array_equals,
        assert_throws,
        assert_unreached,
        test,
        async_test,
        promise_test,
        setup
    };
}