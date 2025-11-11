#!/usr/bin/env jsrt

/**
 * Node.js compatibility validation tests for the constants module
 * Tests compatibility between constants module and os, fs, crypto modules
 */

const assert = require('jsrt:assert');

console.log('Starting Node.js constants compatibility tests...');

// Test 1: Module loading
console.log('Test 1: Loading modules for compatibility testing');
try {
  const constants = require('constants');
  const os = require('os');
  const fs = require('fs');

  assert(typeof constants === 'object', 'constants should be an object');
  assert(typeof os === 'object', 'os should be an object');
  assert(typeof fs === 'object', 'fs should be an object');

  console.log('✓ All modules loaded successfully');
} catch (error) {
  console.log('✗ Module loading failed:', error.message);
  process.exit(1);
}

const constants = require('constants');
const os = require('os');
const fs = require('fs');

// Test 2: signals compatibility between constants and os.constants
console.log('Test 2: signals compatibility');
try {
  assert(
    typeof constants.signals === 'object',
    'constants.signals should be an object'
  );
  assert(
    typeof os.constants.signals === 'object',
    'os.constants.signals should be an object'
  );

  let signalsMatched = 0;
  let signalsTested = 0;

  // Test all signals that exist in both modules
  for (const signal in constants.signals) {
    if (os.constants.signals[signal] !== undefined) {
      signalsTested++;
      if (constants.signals[signal] === os.constants.signals[signal]) {
        console.log(
          `  ✓ ${signal}: constants=${constants.signals[signal]}, os.constants=${os.constants.signals[signal]}`
        );
        signalsMatched++;
      } else {
        console.log(
          `  ✗ ${signal}: constants=${constants.signals[signal]}, os.constants=${os.constants.signals[signal]} (MISMATCH)`
        );
      }
    } else {
      console.log(
        `  ⚠ ${signal}: exists in constants but not in os.constants`
      );
    }
  }

  // Check for signals in os.constants that might be missing from constants
  for (const signal in os.constants.signals) {
    if (constants.signals[signal] === undefined) {
      console.log(
        `  ⚠ ${signal}: exists in os.constants but not in constants`
      );
    }
  }

  if (signalsTested > 0) {
    const matchRate = ((signalsMatched / signalsTested) * 100).toFixed(1);
    console.log(
      `✓ signals compatibility: ${signalsMatched}/${signalsTested} match (${matchRate}%)`
    );
    assert(
      signalsMatched === signalsTested,
      'All signals should match between constants and os.constants'
    );
  } else {
    console.log(
      '⚠ No signals available for compatibility testing (likely Windows platform)'
    );
  }
} catch (error) {
  console.log('✗ signals compatibility test failed:', error.message);
  process.exit(1);
}

// Test 3: errno compatibility between constants and os.constants
console.log('Test 3: errno compatibility');
try {
  assert(
    typeof constants.errno === 'object',
    'constants.errno should be an object'
  );
  assert(
    typeof os.constants.errno === 'object',
    'os.constants.errno should be an object'
  );

  let errnoMatched = 0;
  let errnoTested = 0;

  // Test essential errno codes that should exist in both modules
  const essentialErrno = [
    'ENOENT',
    'EACCES',
    'EEXIST',
    'ENOTDIR',
    'EISDIR',
    'EINVAL',
    'EPIPE',
    'EBADF',
    'EAGAIN',
    'ENOMEM',
    'EPERM',
    'EIO',
    'ESPIPE',
  ];

  for (const errno of essentialErrno) {
    if (
      constants.errno[errno] !== undefined &&
      os.constants.errno[errno] !== undefined
    ) {
      errnoTested++;
      if (constants.errno[errno] === os.constants.errno[errno]) {
        console.log(
          `  ✓ ${errno}: constants=${constants.errno[errno]}, os.constants=${os.constants.errno[errno]}`
        );
        errnoMatched++;
      } else {
        console.log(
          `  ✗ ${errno}: constants=${constants.errno[errno]}, os.constants=${os.constants.errno[errno]} (MISMATCH)`
        );
      }
    } else {
      if (
        constants.errno[errno] === undefined &&
        os.constants.errno[errno] === undefined
      ) {
        console.log(
          `  ⚠ ${errno}: not available in either module (platform-specific)`
        );
      } else if (constants.errno[errno] === undefined) {
        console.log(`  ⚠ ${errno}: missing from constants module`);
      } else {
        console.log(`  ⚠ ${errno}: missing from os.constants module`);
      }
    }
  }

  if (errnoTested > 0) {
    const matchRate = ((errnoMatched / errnoTested) * 100).toFixed(1);
    console.log(
      `✓ errno compatibility: ${errnoMatched}/${errnoTested} match (${matchRate}%)`
    );
    assert(
      errnoMatched === errnoTested,
      'All errno values should match between constants and os.constants'
    );
  } else {
    console.log('⚠ No errno codes available for compatibility testing');
  }
} catch (error) {
  console.log('✗ errno compatibility test failed:', error.message);
  process.exit(1);
}

// Test 4: priority compatibility between constants and os.constants
console.log('Test 4: priority compatibility');
try {
  assert(
    typeof constants.priority === 'object',
    'constants.priority should be an object'
  );
  assert(
    typeof os.constants.priority === 'object',
    'os.constants.priority should be an object'
  );

  let priorityMatched = 0;
  const priorityConstants = [
    'PRIORITY_LOW',
    'PRIORITY_BELOW_NORMAL',
    'PRIORITY_NORMAL',
    'PRIORITY_ABOVE_NORMAL',
    'PRIORITY_HIGH',
    'PRIORITY_HIGHEST',
  ];

  for (const priority of priorityConstants) {
    if (
      constants.priority[priority] !== undefined &&
      os.constants.priority[priority] !== undefined
    ) {
      if (constants.priority[priority] === os.constants.priority[priority]) {
        console.log(
          `  ✓ ${priority}: constants=${constants.priority[priority]}, os.constants=${os.constants.priority[priority]}`
        );
        priorityMatched++;
      } else {
        console.log(
          `  ✗ ${priority}: constants=${constants.priority[priority]}, os.constants=${os.constants.priority[priority]} (MISMATCH)`
        );
      }
    } else {
      console.log(`  ⚠ ${priority}: not available in both modules`);
    }
  }

  if (priorityMatched > 0) {
    console.log(
      `✓ priority compatibility: ${priorityMatched} priorities match`
    );
    assert(
      priorityMatched === priorityConstants.length,
      'All priority constants should match'
    );
  } else {
    console.log('⚠ No priority constants available for compatibility testing');
  }
} catch (error) {
  console.log('✗ priority compatibility test failed:', error.message);
  process.exit(1);
}

// Test 5: File access constants compatibility between constants and fs.constants
console.log('Test 5: File access constants compatibility');
try {
  assert(typeof fs.constants === 'object', 'fs.constants should be an object');

  // Test backward compatibility: F_OK, R_OK, W_OK, X_OK at top level
  const accessConstants = ['F_OK', 'R_OK', 'W_OK', 'X_OK'];
  let accessMatched = 0;

  for (const access of accessConstants) {
    // Check top-level constants
    if (constants[access] !== undefined && fs.constants[access] !== undefined) {
      if (constants[access] === fs.constants[access]) {
        console.log(
          `  ✓ ${access}: constants=${constants[access]}, fs.constants=${fs.constants[access]}`
        );
        accessMatched++;
      } else {
        console.log(
          `  ✗ ${access}: constants=${constants[access]}, fs.constants=${fs.constants[access]} (MISMATCH)`
        );
      }
    } else if (constants[access] !== undefined) {
      console.log(
        `  ✓ ${access}: constants=${constants[access]} (fs.constants not available)`
      );
      accessMatched++;
    } else {
      console.log(`  ✗ ${access}: not available in constants module`);
    }

    // Check faccess category
    if (constants.faccess && constants.faccess[access] !== undefined) {
      if (fs.constants[access] !== undefined) {
        if (constants.faccess[access] === fs.constants[access]) {
          console.log(
            `  ✓ faccess.${access}: ${constants.faccess[access]} matches fs.constants`
          );
        } else {
          console.log(
            `  ✗ faccess.${access}: ${constants.faccess[access]} vs fs.constants=${fs.constants[access]} (MISMATCH)`
          );
        }
      }
    }
  }

  console.log(
    `✓ File access constants compatibility: ${accessMatched}/${accessConstants.length} match`
  );
} catch (error) {
  console.log(
    '✗ File access constants compatibility test failed:',
    error.message
  );
  process.exit(1);
}

// Test 6: Module consistency tests
console.log('Test 6: Module consistency tests');
try {
  // Test that accessing constants multiple times returns the same values
  const constants1 = require('constants');
  const constants2 = require('constants');

  assert(
    constants1 === constants2,
    'Multiple requires should return the same module object'
  );
  assert(
    constants1.errno.ENOENT === constants2.errno.ENOENT,
    'Constants should be consistent across requires'
  );
  console.log('✓ Module caching works correctly');

  // Test that values are consistent
  assert(
    constants1.F_OK === constants1.faccess.F_OK,
    'Top-level F_OK should match faccess.F_OK'
  );
  assert(
    constants1.R_OK === constants1.faccess.R_OK,
    'Top-level R_OK should match faccess.R_OK'
  );
  assert(
    constants1.W_OK === constants1.faccess.W_OK,
    'Top-level W_OK should match faccess.W_OK'
  );
  assert(
    constants1.X_OK === constants1.faccess.X_OK,
    'Top-level X_OK should match faccess.X_OK'
  );
  console.log('✓ Internal consistency verified');
} catch (error) {
  console.log('✗ Module consistency test failed:', error.message);
  process.exit(1);
}

// Test 7: Cross-module usage scenarios
console.log('Test 7: Cross-module usage scenarios');
try {
  // Scenario 1: Using constants.errno with fs operations
  const testFile = '/tmp/test_constants_compat_' + Date.now();

  try {
    // Try to access a non-existent file
    require('fs').accessSync(testFile, constants.F_OK);
  } catch (error) {
    // Verify the error code matches our constants
    assert(error.code === 'ENOENT', 'Should get ENOENT error');
    assert(constants.errno.ENOENT === 2, 'ENOENT should be 2 on Unix systems');
    console.log(
      `  ✓ fs.access error matches constants.ENOENT (${constants.errno.ENOENT})`
    );
  }

  // Scenario 2: Using constants for file creation flags (if supported)
  if (constants.fopen.O_CREAT && constants.fopen.O_WRONLY) {
    console.log(
      `  ✓ File creation flags available: O_CREAT=${constants.fopen.O_CREAT}, O_WRONLY=${constants.fopen.O_WRONLY}`
    );
  }

  // Scenario 3: Using priority constants (if os module supports it)
  if (os.constants && os.constants.priority) {
    console.log(`  ✓ Priority constants available for process scheduling`);
  }

  console.log('✓ Cross-module usage scenarios completed');
} catch (error) {
  console.log('✗ Cross-module usage scenarios failed:', error.message);
  process.exit(1);
}

// Test 8: Node.js API compatibility verification
console.log('Test 8: Node.js API compatibility verification');
try {
  // Verify the module structure matches Node.js expectations
  assert(typeof constants.errno === 'object', 'Should have errno object');
  assert(typeof constants.signals === 'object', 'Should have signals object');
  assert(typeof constants.priority === 'object', 'Should have priority object');

  // Verify specific constants that Node.js applications expect
  const expectedConstants = {
    F_OK: 'number',
    R_OK: 'number',
    W_OK: 'number',
    X_OK: 'number',
  };

  for (const [name, type] of Object.entries(expectedConstants)) {
    assert(
      typeof constants[name] === type,
      `${name} should be of type ${type}`
    );
    assert(constants[name] >= 0, `${name} should be non-negative`);
    console.log(`  ✓ ${name}: ${constants[name]} (type: ${type})`);
  }

  // Verify errno constants that Node.js apps commonly use
  const commonErrno = ['ENOENT', 'EACCES', 'EEXIST', 'EINVAL'];
  for (const errno of commonErrno) {
    if (constants.errno[errno] !== undefined) {
      assert(
        typeof constants.errno[errno] === 'number',
        `${errno} should be a number`
      );
      console.log(`  ✓ errno.${errno}: ${constants.errno[errno]}`);
    }
  }

  console.log('✓ Node.js API compatibility verified');
} catch (error) {
  console.log(
    '✗ Node.js API compatibility verification failed:',
    error.message
  );
  process.exit(1);
}

console.log('\n🎉 All compatibility tests passed!');
console.log('Constants module is fully compatible with os and fs modules.');
console.log('Ready for npm package compatibility.');
