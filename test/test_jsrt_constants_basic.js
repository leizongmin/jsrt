#!/usr/bin/env jsrt

/**
 * Basic functionality tests for the Node.js constants module
 * Tests all constant categories and basic module operations
 */

const assert = require('jsrt:assert');

console.log('Starting Node.js constants basic functionality tests...');

function runTests() {
  // Test 1: Module loading - CommonJS
  console.log('Test 1: Module loading (CommonJS)');
  try {
    const constants = require('constants');
    assert(typeof constants === 'object', 'constants should be an object');
    console.log("✓ CommonJS require('constants') works");

    return constants;
  } catch (error) {
    console.log('✗ CommonJS require failed:', error.message);
    process.exit(1);
  }
}

function testConstants(constants) {
  console.log('Running comprehensive constant category tests...');

  // Test 2: errno constants
  console.log('Test 2: errno constants');
  try {
    assert(typeof constants.errno === 'object', 'errno should be an object');

    // Test essential errno constants
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
    ];

    for (const errno of essentialErrno) {
      if (constants.errno[errno] !== undefined) {
        assert(
          typeof constants.errno[errno] === 'number',
          `${errno} should be a number`
        );
        assert(constants.errno[errno] > 0, `${errno} should be positive`);
        console.log(`  ✓ ${errno} = ${constants.errno[errno]}`);
      } else {
        console.log(`  ⚠ ${errno} not available on this platform`);
      }
    }

    console.log('✓ errno constants test passed');
  } catch (error) {
    console.log('✗ errno constants test failed:', error.message);
    process.exit(1);
  }

  // Test 3: signals constants
  console.log('Test 3: signals constants');
  try {
    assert(
      typeof constants.signals === 'object',
      'signals should be an object'
    );

    // Test essential signal constants (Unix-only)
    const essentialSignals = [
      'SIGHUP',
      'SIGINT',
      'SIGQUIT',
      'SIGILL',
      'SIGTRAP',
      'SIGABRT',
      'SIGBUS',
      'SIGFPE',
      'SIGKILL',
      'SIGUSR1',
      'SIGSEGV',
      'SIGUSR2',
      'SIGPIPE',
      'SIGALRM',
      'SIGTERM',
      'SIGCHLD',
      'SIGCONT',
      'SIGSTOP',
      'SIGTSTP',
      'SIGTTIN',
      'SIGTTOU',
      'SIGURG',
      'SIGXCPU',
      'SIGXFSZ',
      'SIGVTALRM',
      'SIGPROF',
      'SIGWINCH',
      'SIGIO',
      'SIGSYS',
    ];

    let signalsFound = 0;
    for (const signal of essentialSignals) {
      if (constants.signals[signal] !== undefined) {
        assert(
          typeof constants.signals[signal] === 'number',
          `${signal} should be a number`
        );
        assert(constants.signals[signal] > 0, `${signal} should be positive`);
        console.log(`  ✓ ${signal} = ${constants.signals[signal]}`);
        signalsFound++;
      } else {
        console.log(`  ⚠ ${signal} not available on this platform`);
      }
    }

    if (signalsFound > 0) {
      console.log('✓ signals constants test passed');
    } else {
      console.log('⚠ No signal constants available (likely Windows platform)');
    }
  } catch (error) {
    console.log('✗ signals constants test failed:', error.message);
    process.exit(1);
  }

  // Test 4: priority constants
  console.log('Test 4: priority constants');
  try {
    assert(
      typeof constants.priority === 'object',
      'priority should be an object'
    );

    const expectedPriorities = [
      'PRIORITY_LOW',
      'PRIORITY_BELOW_NORMAL',
      'PRIORITY_NORMAL',
      'PRIORITY_ABOVE_NORMAL',
      'PRIORITY_HIGH',
      'PRIORITY_HIGHEST',
    ];

    for (const priority of expectedPriorities) {
      assert(
        typeof constants.priority[priority] === 'number',
        `${priority} should be a number`
      );
      console.log(`  ✓ ${priority} = ${constants.priority[priority]}`);
    }

    // Verify priority ordering
    assert(
      constants.priority.PRIORITY_LOW > constants.priority.PRIORITY_NORMAL,
      'PRIORITY_LOW should be greater than PRIORITY_NORMAL'
    );
    assert(
      constants.priority.PRIORITY_HIGH < constants.priority.PRIORITY_NORMAL,
      'PRIORITY_HIGH should be less than PRIORITY_NORMAL'
    );
    assert(
      constants.priority.PRIORITY_HIGHEST < constants.priority.PRIORITY_HIGH,
      'PRIORITY_HIGHEST should be less than PRIORITY_HIGH'
    );

    console.log('✓ priority constants test passed');
  } catch (error) {
    console.log('✗ priority constants test failed:', error.message);
    process.exit(1);
  }

  // Test 5: File access constants (backward compatibility)
  console.log('Test 5: File access constants (backward compatibility)');
  try {
    const accessConstants = ['F_OK', 'R_OK', 'W_OK', 'X_OK'];

    for (const access of accessConstants) {
      assert(
        typeof constants[access] === 'number',
        `${access} should be a number`
      );
      assert(constants[access] >= 0, `${access} should be non-negative`);
      console.log(`  ✓ ${access} = ${constants[access]}`);
    }

    console.log('✓ File access constants test passed');
  } catch (error) {
    console.log('✗ File access constants test failed:', error.message);
    process.exit(1);
  }

  // Test 6: faccess category constants
  console.log('Test 6: faccess category constants');
  try {
    assert(
      typeof constants.faccess === 'object',
      'faccess should be an object'
    );

    const accessConstants = ['F_OK', 'R_OK', 'W_OK', 'X_OK'];

    for (const access of accessConstants) {
      assert(
        typeof constants.faccess[access] === 'number',
        `faccess.${access} should be a number`
      );
      console.log(`  ✓ faccess.${access} = ${constants.faccess[access]}`);

      // Verify consistency with top-level constants
      assert(
        constants.faccess[access] === constants[access],
        `faccess.${access} should match constants.${access}`
      );
    }

    console.log('✓ faccess category constants test passed');
  } catch (error) {
    console.log('✗ faccess category constants test failed:', error.message);
    process.exit(1);
  }

  // Test 7: fopen category constants
  console.log('Test 7: fopen category constants');
  try {
    assert(typeof constants.fopen === 'object', 'fopen should be an object');

    const openConstants = [
      'O_RDONLY',
      'O_WRONLY',
      'O_RDWR',
      'O_CREAT',
      'O_EXCL',
      'O_TRUNC',
      'O_APPEND',
      'O_NONBLOCK',
    ];

    for (const open of openConstants) {
      if (constants.fopen[open] !== undefined) {
        assert(
          typeof constants.fopen[open] === 'number',
          `fopen.${open} should be a number`
        );
        console.log(`  ✓ fopen.${open} = ${constants.fopen[open]}`);
      } else {
        console.log(`  ⚠ fopen.${open} not available on this platform`);
      }
    }

    console.log('✓ fopen category constants test passed');
  } catch (error) {
    console.log('✗ fopen category constants test failed:', error.message);
    process.exit(1);
  }

  // Test 8: filetype category constants
  console.log('Test 8: filetype category constants');
  try {
    assert(
      typeof constants.filetype === 'object',
      'filetype should be an object'
    );

    const typeConstants = [
      'S_IFMT',
      'S_IFREG',
      'S_IFDIR',
      'S_IFCHR',
      'S_IFBLK',
      'S_IFIFO',
      'S_IFLNK',
      'S_IFSOCK',
    ];

    for (const type of typeConstants) {
      if (constants.filetype[type] !== undefined) {
        assert(
          typeof constants.filetype[type] === 'number',
          `filetype.${type} should be a number`
        );
        console.log(
          `  ✓ filetype.${type} = ${constants.filetype[type].toString(8).padStart(6, '0')} (octal)`
        );
      } else {
        console.log(`  ⚠ filetype.${type} not available on this platform`);
      }
    }

    console.log('✓ filetype category constants test passed');
  } catch (error) {
    console.log('✗ filetype category constants test failed:', error.message);
    process.exit(1);
  }

  // Test 9: permissions category constants
  console.log('Test 9: permissions category constants');
  try {
    assert(
      typeof constants.permissions === 'object',
      'permissions should be an object'
    );

    const permConstants = [
      'S_IRWXU',
      'S_IRUSR',
      'S_IWUSR',
      'S_IXUSR',
      'S_IRWXG',
      'S_IRGRP',
      'S_IWGRP',
      'S_IXGRP',
      'S_IRWXO',
      'S_IROTH',
      'S_IWOTH',
      'S_IXOTH',
    ];

    for (const perm of permConstants) {
      if (constants.permissions[perm] !== undefined) {
        assert(
          typeof constants.permissions[perm] === 'number',
          `permissions.${perm} should be a number`
        );
        console.log(
          `  ✓ permissions.${perm} = ${constants.permissions[perm].toString(8).padStart(3, '0')} (octal)`
        );
      } else {
        console.log(`  ⚠ permissions.${perm} not available on this platform`);
      }
    }

    console.log('✓ permissions category constants test passed');
  } catch (error) {
    console.log('✗ permissions category constants test failed:', error.message);
    process.exit(1);
  }

  // Test 10: crypto constants (if available)
  console.log('Test 10: crypto constants');
  try {
    if (constants.crypto) {
      assert(
        typeof constants.crypto === 'object',
        'crypto should be an object'
      );
      console.log('  ✓ crypto constants object available');

      // Test some common crypto constants if they exist
      const cryptoKeys = Object.keys(constants.crypto);
      if (cryptoKeys.length > 0) {
        console.log(
          `  Found ${cryptoKeys.length} crypto constants: ${cryptoKeys.slice(0, 5).join(', ')}${cryptoKeys.length > 5 ? '...' : ''}`
        );
      }
    } else {
      console.log('  ⚠ crypto constants not available');
    }

    console.log('✓ crypto constants test passed');
  } catch (error) {
    console.log('✗ crypto constants test failed:', error.message);
    process.exit(1);
  }

  // Test 11: LibUV-specific constants
  console.log('Test 11: LibUV-specific constants');
  try {
    const uvConstants = [
      'UV_FS_O_APPEND',
      'UV_FS_O_CREAT',
      'UV_FS_O_EXCL',
      'UV_FS_O_RDONLY',
      'UV_FS_O_RDWR',
      'UV_FS_O_TRUNC',
      'UV_FS_O_WRONLY',
    ];

    for (const uv of uvConstants) {
      assert(typeof constants[uv] === 'number', `${uv} should be a number`);
      console.log(`  ✓ ${uv} = ${constants[uv]}`);
    }

    console.log('✓ LibUV-specific constants test passed');
  } catch (error) {
    console.log('✗ LibUV-specific constants test failed:', error.message);
    process.exit(1);
  }

  // Test 12: Performance test
  console.log('Test 12: Performance test');
  try {
    const startTime = Date.now();

    // Access constants multiple times
    for (let i = 0; i < 10000; i++) {
      const enoent = constants.errno.ENOENT;
      const sigint = constants.signals.SIGINT;
      const f_ok = constants.F_OK;
      const priority = constants.priority.PRIORITY_NORMAL;
    }

    const endTime = Date.now();
    const duration = endTime - startTime;

    console.log(`  ✓ 10,000 constant accesses completed in ${duration}ms`);
    assert(
      duration < 100,
      'Constant access should be fast (< 100ms for 10k accesses)'
    );

    console.log('✓ Performance test passed');
  } catch (error) {
    console.log('✗ Performance test failed:', error.message);
    process.exit(1);
  }

  console.log('\n🎉 All basic functionality tests passed!');
  console.log('Node.js constants module is working correctly.');
}

// Main execution
try {
  const constants = runTests();
  testConstants(constants);
} catch (error) {
  console.log('Test execution failed:', error.message);
  process.exit(1);
}
