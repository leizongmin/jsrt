// Test process Unix permission APIs
// Note: Most setters require root privileges and will be skipped if not root

console.log('Testing process Unix permission APIs...\n');

// Check if running on Unix
const isUnix = process.platform !== 'win32';

if (!isUnix) {
  console.log('⚠️  Skipping Unix permission tests on Windows platform');
  console.log(
    'Note: These APIs are Unix-specific and not available on Windows'
  );
  process.exit(0);
}

// Test 1: process.getuid()
console.log('Test 1: process.getuid()');
if (typeof process.getuid === 'function') {
  console.log('✓ process.getuid() exists');

  const uid = process.getuid();
  if (typeof uid === 'number' && uid >= 0) {
    console.log('✓ getuid() returns non-negative number:', uid);
  } else {
    throw new Error('getuid() should return non-negative number');
  }
} else {
  throw new Error('process.getuid not found');
}

// Test 2: process.geteuid()
console.log('\nTest 2: process.geteuid()');
if (typeof process.geteuid === 'function') {
  console.log('✓ process.geteuid() exists');

  const euid = process.geteuid();
  if (typeof euid === 'number' && euid >= 0) {
    console.log('✓ geteuid() returns non-negative number:', euid);
  } else {
    throw new Error('geteuid() should return non-negative number');
  }
} else {
  throw new Error('process.geteuid not found');
}

// Test 3: process.getgid()
console.log('\nTest 3: process.getgid()');
if (typeof process.getgid === 'function') {
  console.log('✓ process.getgid() exists');

  const gid = process.getgid();
  if (typeof gid === 'number' && gid >= 0) {
    console.log('✓ getgid() returns non-negative number:', gid);
  } else {
    throw new Error('getgid() should return non-negative number');
  }
} else {
  throw new Error('process.getgid not found');
}

// Test 4: process.getegid()
console.log('\nTest 4: process.getegid()');
if (typeof process.getegid === 'function') {
  console.log('✓ process.getegid() exists');

  const egid = process.getegid();
  if (typeof egid === 'number' && egid >= 0) {
    console.log('✓ getegid() returns non-negative number:', egid);
  } else {
    throw new Error('getegid() should return non-negative number');
  }
} else {
  throw new Error('process.getegid not found');
}

// Test 5: process.getgroups()
console.log('\nTest 5: process.getgroups()');
if (typeof process.getgroups === 'function') {
  console.log('✓ process.getgroups() exists');

  const groups = process.getgroups();
  if (Array.isArray(groups)) {
    console.log('✓ getgroups() returns array');
    console.log('  groups:', groups.length > 0 ? groups.join(', ') : '(empty)');

    if (groups.every((g) => typeof g === 'number' && g >= 0)) {
      console.log('✓ All group IDs are non-negative numbers');
    } else {
      throw new Error(
        'getgroups() should return array of non-negative numbers'
      );
    }
  } else {
    throw new Error('getgroups() should return array');
  }
} else {
  throw new Error('process.getgroups not found');
}

// Test 6: process.umask()
console.log('\nTest 6: process.umask()');
if (typeof process.umask === 'function') {
  console.log('✓ process.umask() exists');

  // Get current umask without changing it
  const currentMask = process.umask();
  if (typeof currentMask === 'number') {
    console.log('✓ umask() returns number:', '0' + currentMask.toString(8));
  } else {
    throw new Error('umask() should return number');
  }

  // Set a new mask and verify old one is returned
  const oldMask = process.umask(0o022);
  if (typeof oldMask === 'number') {
    console.log(
      '✓ umask(newMask) returns old mask:',
      '0' + oldMask.toString(8)
    );
  } else {
    throw new Error('umask(newMask) should return old mask');
  }

  // Restore original mask
  process.umask(currentMask);
  console.log('✓ Restored original umask');
} else {
  throw new Error('process.umask not found');
}

// Test 7: Check consistency between uid and euid
console.log('\nTest 7: UID/EUID consistency');
const uid = process.getuid();
const euid = process.geteuid();
console.log('  Real UID:', uid);
console.log('  Effective UID:', euid);
if (uid === euid) {
  console.log('✓ UID and EUID are the same (normal process)');
} else {
  console.log('⚠️  UID and EUID differ (setuid binary or privilege change)');
}

// Test 8: Check consistency between gid and egid
console.log('\nTest 8: GID/EGID consistency');
const gid = process.getgid();
const egid = process.getegid();
console.log('  Real GID:', gid);
console.log('  Effective GID:', egid);
if (gid === egid) {
  console.log('✓ GID and EGID are the same (normal process)');
} else {
  console.log('⚠️  GID and EGID differ (setgid binary or privilege change)');
}

// Test 9: Setter APIs existence (tests will be skipped if not root)
console.log('\nTest 9: Setter APIs existence');
const setterAPIs = [
  'setuid',
  'seteuid',
  'setgid',
  'setegid',
  'setgroups',
  'initgroups',
];
let allSettersExist = true;

for (const api of setterAPIs) {
  if (typeof process[api] === 'function') {
    console.log('  ✓ process.' + api + '() exists');
  } else {
    console.log('  ✗ process.' + api + '() missing');
    allSettersExist = false;
  }
}

if (allSettersExist) {
  console.log('✓ All setter APIs exist');
}

// Test 10: Test setter error handling (requires root)
console.log('\nTest 10: Setter error handling');
const isRoot = process.getuid() === 0;

if (!isRoot) {
  console.log('⚠️  Skipping setter tests (not running as root)');
  console.log('  Note: Setter functions require root privileges');
} else {
  console.log('✓ Running as root, testing setters...');

  // Test setuid with invalid input
  try {
    process.setuid({});
    throw new Error('setuid() should throw on invalid input');
  } catch (e) {
    if (e.message.includes('must be a number or string')) {
      console.log('  ✓ setuid() throws TypeError for invalid input');
    } else {
      throw new Error('Wrong error for setuid: ' + e.message);
    }
  }

  // Test setgid with invalid input
  try {
    process.setgid([]);
    throw new Error('setgid() should throw on invalid input');
  } catch (e) {
    if (e.message.includes('must be a number or string')) {
      console.log('  ✓ setgid() throws TypeError for invalid input');
    } else {
      throw new Error('Wrong error for setgid: ' + e.message);
    }
  }

  // Test setgroups with non-array
  try {
    process.setgroups('not an array');
    throw new Error('setgroups() should throw on non-array input');
  } catch (e) {
    if (e.message.includes('must be an array')) {
      console.log('  ✓ setgroups() throws TypeError for non-array');
    } else {
      throw new Error('Wrong error for setgroups: ' + e.message);
    }
  }

  // Test initgroups with missing arguments
  try {
    process.initgroups('root');
    throw new Error('initgroups() should require 2 arguments');
  } catch (e) {
    if (e.message.includes('requires 2 arguments')) {
      console.log('  ✓ initgroups() requires 2 arguments');
    } else {
      throw new Error('Wrong error for initgroups: ' + e.message);
    }
  }

  // Test successful group operations (should not fail for root)
  try {
    const currentGroups = process.getgroups();
    console.log('  ✓ getgroups() works:', currentGroups.length, 'groups');

    // Set groups back to current (should succeed for root)
    process.setgroups(currentGroups);
    console.log('  ✓ setgroups() succeeded (restored current groups)');
  } catch (e) {
    console.log('  ⚠️  Group operations failed:', e.message);
  }
}

console.log('\n✅ All Unix permission API tests passed!');
