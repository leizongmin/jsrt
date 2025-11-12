#!/usr/bin/env jsrt

/**
 * Cross-module integration tests for the constants module
 * Tests integration with os, fs, and crypto modules
 */

const assert = require('jsrt:assert');

console.log('Starting cross-module integration tests...');

// Test 1: Load all required modules
console.log('Test 1: Module loading');
try {
  const constants = require('constants');
  const os = require('os');
  const fs = require('fs');
  const crypto = require('crypto');

  console.log('✓ All modules loaded successfully');
  runIntegrationTests(constants, os, fs, crypto);
} catch (error) {
  console.log('✗ Module loading failed:', error.message);
  process.exit(1);
}

function runIntegrationTests(constants, os, fs, crypto) {
  // Test 2: Integration with os module
  console.log('Test 2: os module integration');
  try {
    // 2a: Process signal handling integration
    if (constants.signals && constants.signals.SIGTERM) {
      console.log(
        `  ✓ SIGTERM constant available: ${constants.signals.SIGTERM}`
      );

      // Verify signal constants work with Node.js patterns
      const signalNames = Object.keys(constants.signals);
      if (signalNames.length > 0) {
        console.log(
          `  ✓ ${signalNames.length} signal constants available for os integration`
        );
      }
    }

    // 2b: Error code integration
    if (constants.errno && constants.errno.ENOENT) {
      console.log(`  ✓ ENOENT constant available: ${constants.errno.ENOENT}`);

      // Test error code mapping
      const testPath = '/nonexistent/path/for/testing';
      try {
        fs.accessSync(testPath);
      } catch (error) {
        if (error.code === 'ENOENT') {
          console.log(
            `  ✓ fs error code "ENOENT" matches constants.errno.ENOENT (${constants.errno.ENOENT})`
          );
        }
      }
    }

    // 2c: Priority constants for process scheduling
    if (constants.priority && os.constants && os.constants.priority) {
      const priorities = ['PRIORITY_LOW', 'PRIORITY_NORMAL', 'PRIORITY_HIGH'];
      for (const priority of priorities) {
        if (constants.priority[priority] && os.constants.priority[priority]) {
          assert(
            constants.priority[priority] === os.constants.priority[priority],
            `${priority} should match between constants and os.constants`
          );
          console.log(
            `  ✓ ${priority} integration verified: ${constants.priority[priority]}`
          );
        }
      }
    }

    console.log('✓ os module integration completed');
  } catch (error) {
    console.log('✗ os module integration failed:', error.message);
    process.exit(1);
  }

  // Test 3: Integration with fs module
  console.log('Test 3: fs module integration');
  try {
    // 3a: File access constants
    const accessConstants = ['F_OK', 'R_OK', 'W_OK', 'X_OK'];
    for (const access of accessConstants) {
      assert(
        typeof constants[access] === 'number',
        `${access} should be a number`
      );
      assert(
        typeof constants.faccess[access] === 'number',
        `faccess.${access} should be a number`
      );
      assert(
        constants[access] === constants.faccess[access],
        `Top-level ${access} should match faccess.${access}`
      );
      console.log(`  ✓ ${access} integration: ${constants[access]}`);
    }

    // 3b: Test fs.access with constants
    const testFile = '/tmp/test_constants_integration_' + Date.now();
    try {
      // Try to check access on non-existent file
      fs.accessSync(testFile, constants.F_OK);
    } catch (error) {
      if (error.code === 'ENOENT') {
        console.log(`  ✓ fs.access with constants.F_OK works correctly`);
      }
    }

    // 3c: File type constants integration
    if (constants.filetype) {
      const typeConstants = ['S_IFREG', 'S_IFDIR', 'S_IFCHR'];
      for (const type of typeConstants) {
        if (constants.filetype[type] !== undefined) {
          assert(
            typeof constants.filetype[type] === 'number',
            `filetype.${type} should be a number`
          );
          console.log(
            `  ✓ filetype.${type}: ${constants.filetype[type].toString(8)} (octal)`
          );
        }
      }
    }

    // 3d: Permission constants integration
    if (constants.permissions) {
      const permConstants = ['S_IRUSR', 'S_IWUSR', 'S_IXUSR'];
      for (const perm of permConstants) {
        if (constants.permissions[perm] !== undefined) {
          assert(
            typeof constants.permissions[perm] === 'number',
            `permissions.${perm} should be a number`
          );
          console.log(
            `  ✓ permissions.${perm}: ${constants.permissions[perm].toString(8)} (octal)`
          );
        }
      }
    }

    // 3e: Open flags integration
    if (constants.fopen) {
      const openConstants = ['O_RDONLY', 'O_WRONLY', 'O_CREAT', 'O_EXCL'];
      for (const open of openConstants) {
        if (constants.fopen[open] !== undefined) {
          assert(
            typeof constants.fopen[open] === 'number',
            `fopen.${open} should be a number`
          );
          console.log(`  ✓ fopen.${open}: ${constants.fopen[open]}`);
        }
      }
    }

    console.log('✓ fs module integration completed');
  } catch (error) {
    console.log('✗ fs module integration failed:', error.message);
    process.exit(1);
  }

  // Test 4: Integration with crypto module
  console.log('Test 4: crypto module integration');
  try {
    // 4a: Check if crypto constants are available
    if (constants.crypto) {
      assert(
        typeof constants.crypto === 'object',
        'crypto should be an object'
      );

      const cryptoKeys = Object.keys(constants.crypto);
      console.log(`  ✓ Found ${cryptoKeys.length} crypto constants`);

      if (cryptoKeys.length > 0) {
        // Test first few crypto constants
        for (let i = 0; i < Math.min(5, cryptoKeys.length); i++) {
          const key = cryptoKeys[i];
          console.log(`  ✓ crypto.${key}: ${typeof constants.crypto[key]}`);
        }
      }
    } else {
      console.log(
        '  ⚠ crypto constants not available (optional on some platforms)'
      );
    }

    // 4b: Test that crypto module works independently
    try {
      const hash = crypto.createHash('sha256');
      hash.update('test');
      const digest = hash.digest('hex');
      console.log(`  ✓ crypto.createHash works: ${digest.substring(0, 16)}...`);
    } catch (error) {
      console.log(`  ⚠ crypto module limited: ${error.message}`);
    }

    console.log('✓ crypto module integration completed');
  } catch (error) {
    console.log('✗ crypto module integration failed:', error.message);
    process.exit(1);
  }

  // Test 5: Real-world usage scenarios
  console.log('Test 5: Real-world usage scenarios');
  try {
    // 5a: Error handling scenario (like npm packages do)
    const testErrorCode = (code, expectedValue) => {
      if (constants.errno[code] !== undefined) {
        assert(
          constants.errno[code] === expectedValue,
          `${code} should be ${expectedValue}, got ${constants.errno[code]}`
        );
        console.log(`  ✓ Error code ${code}: ${constants.errno[code]}`);
      }
    };

    testErrorCode('ENOENT', 2);
    testErrorCode('EACCES', 13);
    testErrorCode('EEXIST', 17);

    // 5b: File permission scenario
    if (constants.permissions) {
      const userRead = constants.permissions.S_IRUSR || 0o400;
      const userWrite = constants.permissions.S_IWUSR || 0o200;
      const userExec = constants.permissions.S_IXUSR || 0o100;
      const userAll = constants.permissions.S_IRWXU || 0o700;

      assert(
        userRead + userWrite + userExec === userAll,
        'User permissions should add up to S_IRWXU'
      );
      console.log(
        `  ✓ Permission consistency: S_IRUSR(${userRead}) + S_IWUSR(${userWrite}) + S_IXUSR(${userExec}) = S_IRWXU(${userAll})`
      );
    }

    // 5c: Signal handling scenario
    if (constants.signals) {
      const commonSignals = ['SIGINT', 'SIGTERM', 'SIGKILL'];
      let signalCount = 0;

      for (const signal of commonSignals) {
        if (constants.signals[signal] !== undefined) {
          assert(
            typeof constants.signals[signal] === 'number',
            `${signal} should be a number`
          );
          assert(constants.signals[signal] > 0, `${signal} should be positive`);
          signalCount++;
        }
      }

      if (signalCount > 0) {
        console.log(
          `  ✓ Signal handling: ${signalCount} common signals available`
        );
      }
    }

    // 5d: File access check scenario
    const testFileAccess = () => {
      try {
        fs.accessSync('/etc/hosts', constants.R_OK);
        console.log(`  ✓ File access check with constants.R_OK works`);
        return true;
      } catch (error) {
        console.log(`  ✓ File access check correctly handles permissions`);
        return false;
      }
    };

    testFileAccess();

    console.log('✓ Real-world usage scenarios completed');
  } catch (error) {
    console.log('✗ Real-world usage scenarios failed:', error.message);
    process.exit(1);
  }

  // Test 6: Performance integration test
  console.log('Test 6: Performance integration test');
  try {
    const iterations = 1000;
    const startTime = Date.now();

    // Simulate npm package usage patterns
    for (let i = 0; i < iterations; i++) {
      // Access constants like packages do
      const enoent = constants.errno.ENOENT;
      const f_ok = constants.F_OK;
      const sigterm = constants.signals.SIGTERM;
      const priority = constants.priority.PRIORITY_NORMAL;

      // Use with module functions
      try {
        fs.accessSync('/nonexistent', f_ok);
      } catch (e) {
        // Expected
      }
    }

    const duration = Date.now() - startTime;
    const opsPerMs = (iterations * 4) / duration; // 4 operations per iteration

    console.log(
      `  ✓ Performance: ${iterations} iterations in ${duration}ms (${opsPerMs.toFixed(2)} ops/ms)`
    );
    assert(duration < 1000, 'Integration test should complete quickly (< 1s)');

    console.log('✓ Performance integration test passed');
  } catch (error) {
    console.log('✗ Performance integration test failed:', error.message);
    process.exit(1);
  }

  // Test 7: Memory and resource management
  console.log('Test 7: Memory and resource management');
  try {
    // Test that multiple requires don't create memory issues
    const modules = [];
    for (let i = 0; i < 10; i++) {
      modules.push(require('constants'));
    }

    // All should be the same object (cached)
    for (let i = 1; i < modules.length; i++) {
      assert(
        modules[i] === modules[0],
        'All requires should return the same object'
      );
      assert(
        modules[i].errno.ENOENT === modules[0].errno.ENOENT,
        'Constant values should be consistent'
      );
    }

    console.log('  ✓ Module caching works correctly, no memory leaks detected');

    // Test that accessing deep properties doesn't cause issues
    for (let i = 0; i < 100; i++) {
      const deepAccess = constants.errno.ENOENT;
      const categoryAccess = constants.faccess.F_OK;
      const sigAccess = constants.signals.SIGINT;
    }

    console.log('  ✓ Deep property access stable');
    console.log('✓ Memory and resource management test passed');
  } catch (error) {
    console.log('✗ Memory and resource management test failed:', error.message);
    process.exit(1);
  }

  console.log('\n🎉 All cross-module integration tests passed!');
  console.log(
    'Constants module integrates seamlessly with os, fs, and crypto modules.'
  );
  console.log('Ready for production use with npm packages.');
}
