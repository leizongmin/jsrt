#!/usr/bin/env jsrt
// Test: Node.js crypto KDF functions (Phase 6)
// Tests pbkdf2, pbkdf2Sync, hkdf, hkdfSync, scrypt, scryptSync

const crypto = require('node:crypto');

// Check if OpenSSL KDF functions are available (required on Windows)
try {
  crypto.pbkdf2Sync('test', 'salt', 1, 16, 'sha256');
} catch (e) {
  if (e.message && (e.message.includes('PBKDF2 key derivation failed') || 
                     e.message.includes('HKDF key derivation failed'))) {
    console.log('Error: OpenSSL symmetric functions not available');
    process.exit(0);
  }
  // Re-throw other errors
  throw e;
}

let testsRun = 0;
let testsPassed = 0;
let testsFailed = 0;

function assert(condition, message) {
  testsRun++;
  if (condition) {
    testsPassed++;
  } else {
    testsFailed++;
    console.log(`FAIL: ${message}`);
  }
}

function assertEquals(actual, expected, message) {
  testsRun++;
  if (actual === expected) {
    testsPassed++;
  } else {
    testsFailed++;
    console.log(`FAIL: ${message}`);
    console.log(`  Expected: ${expected}`);
    console.log(`  Actual: ${actual}`);
  }
}

function assertArrayEquals(actual, expected, message) {
  testsRun++;
  if (actual.length !== expected.length) {
    testsFailed++;
    console.log(`FAIL: ${message} (length mismatch)`);
    console.log(`  Expected length: ${expected.length}`);
    console.log(`  Actual length: ${actual.length}`);
    return;
  }
  for (let i = 0; i < actual.length; i++) {
    if (actual[i] !== expected[i]) {
      testsFailed++;
      console.log(`FAIL: ${message} (element ${i} mismatch)`);
      console.log(`  Expected[${i}]: ${expected[i]}`);
      console.log(`  Actual[${i}]: ${actual[i]}`);
      return;
    }
  }
  testsPassed++;
}

function bufferToHex(buffer) {
  return Array.from(buffer)
    .map((b) => b.toString(16).padStart(2, '0'))
    .join('');
}

console.log('=== Testing PBKDF2 ===');

// Test 1: PBKDF2 Sync - SHA-256
try {
  const password = 'password';
  const salt = 'salt';
  const iterations = 1000;
  const keylen = 32;
  const digest = 'sha256';

  const key = crypto.pbkdf2Sync(password, salt, iterations, keylen, digest);
  assert(key instanceof Uint8Array, 'pbkdf2Sync returns Uint8Array');
  assertEquals(key.length, keylen, 'pbkdf2Sync returns correct key length');

  // Verify it's not all zeros (indicates derivation actually happened)
  let hasNonZero = false;
  for (let i = 0; i < key.length; i++) {
    if (key[i] !== 0) {
      hasNonZero = true;
      break;
    }
  }
  assert(hasNonZero, 'pbkdf2Sync generates non-zero key');

  console.log('PASS: pbkdf2Sync with SHA-256');
} catch (e) {
  testsFailed++;
  console.log('FAIL: pbkdf2Sync with SHA-256:', e.message);
}

// Test 2: PBKDF2 Sync - SHA-512
try {
  const password = 'mypassword';
  const salt = 'mysalt';
  const iterations = 500;
  const keylen = 64;
  const digest = 'sha512';

  const key = crypto.pbkdf2Sync(password, salt, iterations, keylen, digest);
  assertEquals(
    key.length,
    keylen,
    'pbkdf2Sync SHA-512 returns correct key length'
  );

  console.log('PASS: pbkdf2Sync with SHA-512');
} catch (e) {
  testsFailed++;
  console.log('FAIL: pbkdf2Sync with SHA-512:', e.message);
}

// Test 3: PBKDF2 Sync - Buffer inputs
try {
  const password = new Uint8Array([112, 97, 115, 115]); // "pass"
  const salt = new Uint8Array([115, 97, 108, 116]); // "salt"
  const iterations = 100;
  const keylen = 16;
  const digest = 'sha256';

  const key = crypto.pbkdf2Sync(password, salt, iterations, keylen, digest);
  assertEquals(
    key.length,
    keylen,
    'pbkdf2Sync with Buffer inputs returns correct length'
  );

  console.log('PASS: pbkdf2Sync with Buffer inputs');
} catch (e) {
  testsFailed++;
  console.log('FAIL: pbkdf2Sync with Buffer inputs:', e.message);
}

// Test 4: PBKDF2 Sync - Different iterations
try {
  const password = 'test';
  const salt = 'test';
  const digest = 'sha256';
  const keylen = 32;

  const key1 = crypto.pbkdf2Sync(password, salt, 100, keylen, digest);
  const key2 = crypto.pbkdf2Sync(password, salt, 200, keylen, digest);

  // Keys should be different with different iterations
  let different = false;
  for (let i = 0; i < keylen; i++) {
    if (key1[i] !== key2[i]) {
      different = true;
      break;
    }
  }
  assert(
    different,
    'pbkdf2Sync generates different keys for different iterations'
  );

  console.log('PASS: pbkdf2Sync different iterations');
} catch (e) {
  testsFailed++;
  console.log('FAIL: pbkdf2Sync different iterations:', e.message);
}

// Test 5: PBKDF2 Sync - SHA-1
try {
  const password = 'password';
  const salt = 'salt';
  const iterations = 100;
  const keylen = 20;
  const digest = 'sha1';

  const key = crypto.pbkdf2Sync(password, salt, iterations, keylen, digest);
  assertEquals(
    key.length,
    keylen,
    'pbkdf2Sync SHA-1 returns correct key length'
  );

  console.log('PASS: pbkdf2Sync with SHA-1');
} catch (e) {
  testsFailed++;
  console.log('FAIL: pbkdf2Sync with SHA-1:', e.message);
}

// Test 6: PBKDF2 Sync - SHA-384
try {
  const password = 'password';
  const salt = 'salt';
  const iterations = 100;
  const keylen = 48;
  const digest = 'sha384';

  const key = crypto.pbkdf2Sync(password, salt, iterations, keylen, digest);
  assertEquals(
    key.length,
    keylen,
    'pbkdf2Sync SHA-384 returns correct key length'
  );

  console.log('PASS: pbkdf2Sync with SHA-384');
} catch (e) {
  testsFailed++;
  console.log('FAIL: pbkdf2Sync with SHA-384:', e.message);
}

// Test 7: PBKDF2 Async - callback version
try {
  const password = 'password';
  const salt = 'salt';
  const iterations = 100;
  const keylen = 32;
  const digest = 'sha256';

  let callbackCalled = false;
  crypto.pbkdf2(password, salt, iterations, keylen, digest, (err, key) => {
    callbackCalled = true;
    if (err) {
      testsFailed++;
      console.log('FAIL: pbkdf2 async callback received error:', err.message);
    } else {
      assert(
        key instanceof Uint8Array,
        'pbkdf2 async returns Uint8Array in callback'
      );
      assertEquals(
        key.length,
        keylen,
        'pbkdf2 async returns correct key length'
      );
      console.log('PASS: pbkdf2 async with callback');
    }
  });

  // Give it time to execute
  setTimeout(() => {
    if (!callbackCalled) {
      testsFailed++;
      console.log('FAIL: pbkdf2 async callback was never called');
    }
  }, 100);
} catch (e) {
  testsFailed++;
  console.log('FAIL: pbkdf2 async:', e.message);
}

console.log('\n=== Testing HKDF ===');

// Test 8: HKDF Sync - SHA-256
try {
  const digest = 'sha256';
  const ikm = 'input key material';
  const salt = 'salt';
  const info = 'info';
  const keylen = 32;

  const key = crypto.hkdfSync(digest, ikm, salt, info, keylen);
  assert(key instanceof Uint8Array, 'hkdfSync returns Uint8Array');
  assertEquals(key.length, keylen, 'hkdfSync returns correct key length');

  // Verify it's not all zeros
  let hasNonZero = false;
  for (let i = 0; i < key.length; i++) {
    if (key[i] !== 0) {
      hasNonZero = true;
      break;
    }
  }
  assert(hasNonZero, 'hkdfSync generates non-zero key');

  console.log('PASS: hkdfSync with SHA-256');
} catch (e) {
  testsFailed++;
  console.log('FAIL: hkdfSync with SHA-256:', e.message);
}

// Test 9: HKDF Sync - SHA-512
try {
  const digest = 'sha512';
  const ikm = 'my input key material';
  const salt = 'my salt';
  const info = 'my info';
  const keylen = 64;

  const key = crypto.hkdfSync(digest, ikm, salt, info, keylen);
  assertEquals(
    key.length,
    keylen,
    'hkdfSync SHA-512 returns correct key length'
  );

  console.log('PASS: hkdfSync with SHA-512');
} catch (e) {
  testsFailed++;
  console.log('FAIL: hkdfSync with SHA-512:', e.message);
}

// Test 10: HKDF Sync - Buffer inputs
try {
  const digest = 'sha256';
  const ikm = new Uint8Array([1, 2, 3, 4, 5]);
  const salt = new Uint8Array([6, 7, 8]);
  const info = new Uint8Array([9, 10]);
  const keylen = 16;

  const key = crypto.hkdfSync(digest, ikm, salt, info, keylen);
  assertEquals(
    key.length,
    keylen,
    'hkdfSync with Buffer inputs returns correct length'
  );

  console.log('PASS: hkdfSync with Buffer inputs');
} catch (e) {
  testsFailed++;
  console.log('FAIL: hkdfSync with Buffer inputs:', e.message);
}

// Test 11: HKDF Sync - Empty salt and info
try {
  const digest = 'sha256';
  const ikm = 'input key material';
  const salt = '';
  const info = '';
  const keylen = 32;

  const key = crypto.hkdfSync(digest, ikm, salt, info, keylen);
  assertEquals(
    key.length,
    keylen,
    'hkdfSync with empty salt/info returns correct length'
  );

  console.log('PASS: hkdfSync with empty salt and info');
} catch (e) {
  testsFailed++;
  console.log('FAIL: hkdfSync with empty salt and info:', e.message);
}

// Test 12: HKDF Sync - Different key lengths
try {
  const digest = 'sha256';
  const ikm = 'test';
  const salt = 'salt';
  const info = 'info';

  const key16 = crypto.hkdfSync(digest, ikm, salt, info, 16);
  const key32 = crypto.hkdfSync(digest, ikm, salt, info, 32);
  const key64 = crypto.hkdfSync(digest, ikm, salt, info, 64);

  assertEquals(key16.length, 16, 'hkdfSync returns 16-byte key');
  assertEquals(key32.length, 32, 'hkdfSync returns 32-byte key');
  assertEquals(key64.length, 64, 'hkdfSync returns 64-byte key');

  console.log('PASS: hkdfSync different key lengths');
} catch (e) {
  testsFailed++;
  console.log('FAIL: hkdfSync different key lengths:', e.message);
}

// Test 13: HKDF Sync - SHA-1
try {
  const digest = 'sha1';
  const ikm = 'input';
  const salt = 'salt';
  const info = 'info';
  const keylen = 20;

  const key = crypto.hkdfSync(digest, ikm, salt, info, keylen);
  assertEquals(key.length, keylen, 'hkdfSync SHA-1 returns correct key length');

  console.log('PASS: hkdfSync with SHA-1');
} catch (e) {
  testsFailed++;
  console.log('FAIL: hkdfSync with SHA-1:', e.message);
}

// Test 14: HKDF Async - callback version
try {
  const digest = 'sha256';
  const ikm = 'input key material';
  const salt = 'salt';
  const info = 'info';
  const keylen = 32;

  let callbackCalled = false;
  crypto.hkdf(digest, ikm, salt, info, keylen, (err, key) => {
    callbackCalled = true;
    if (err) {
      testsFailed++;
      console.log('FAIL: hkdf async callback received error:', err.message);
    } else {
      assert(
        key instanceof Uint8Array,
        'hkdf async returns Uint8Array in callback'
      );
      assertEquals(key.length, keylen, 'hkdf async returns correct key length');
      console.log('PASS: hkdf async with callback');
    }
  });

  // Give it time to execute
  setTimeout(() => {
    if (!callbackCalled) {
      testsFailed++;
      console.log('FAIL: hkdf async callback was never called');
    }
  }, 100);
} catch (e) {
  testsFailed++;
  console.log('FAIL: hkdf async:', e.message);
}

console.log('\n=== Testing Scrypt (Expected: Not Implemented) ===');

// Test 15: Scrypt Sync - should throw not implemented
try {
  const password = 'password';
  const salt = 'salt';
  const keylen = 32;

  try {
    const key = crypto.scryptSync(password, salt, keylen);
    testsFailed++;
    console.log('FAIL: scryptSync should throw not implemented error');
  } catch (e) {
    if (e.message && e.message.includes('not yet implemented')) {
      testsPassed++;
      console.log('PASS: scryptSync throws not implemented error');
    } else {
      testsFailed++;
      console.log('FAIL: scryptSync throws wrong error:', e.message);
    }
  }
} catch (e) {
  testsFailed++;
  console.log('FAIL: scryptSync test error:', e.message);
}

// Test 16: Scrypt Async - should call callback with error
try {
  const password = 'password';
  const salt = 'salt';
  const keylen = 32;

  let callbackCalled = false;
  crypto.scrypt(password, salt, keylen, {}, (err, key) => {
    callbackCalled = true;
    if (err && err.message && err.message.includes('not yet implemented')) {
      testsPassed++;
      console.log('PASS: scrypt async callback receives not implemented error');
    } else if (!err) {
      testsFailed++;
      console.log('FAIL: scrypt async should return error');
    } else {
      testsFailed++;
      console.log('FAIL: scrypt async wrong error:', err.message);
    }
  });

  setTimeout(() => {
    if (!callbackCalled) {
      testsFailed++;
      console.log('FAIL: scrypt async callback was never called');
    }
  }, 100);
} catch (e) {
  testsFailed++;
  console.log('FAIL: scrypt async test error:', e.message);
}

// Test 17: PBKDF2 with known test vector (RFC 6070)
try {
  // RFC 6070 Test Vector 1
  const password = 'password';
  const salt = 'salt';
  const iterations = 1;
  const keylen = 20;
  const digest = 'sha1';

  const key = crypto.pbkdf2Sync(password, salt, iterations, keylen, digest);
  const expected = [
    0x0c, 0x60, 0xc8, 0x0f, 0x96, 0x1f, 0x0e, 0x71, 0xf3, 0xa9, 0xb5, 0x24,
    0xaf, 0x60, 0x12, 0x06, 0x2f, 0xe0, 0x37, 0xa6,
  ];

  assertArrayEquals(Array.from(key), expected, 'PBKDF2 RFC 6070 test vector 1');
  console.log('PASS: PBKDF2 RFC 6070 test vector');
} catch (e) {
  testsFailed++;
  console.log('FAIL: PBKDF2 RFC 6070 test vector:', e.message);
}

// Test 18: HKDF deterministic output
try {
  const digest = 'sha256';
  const ikm = 'test input';
  const salt = 'test salt';
  const info = 'test info';
  const keylen = 32;

  const key1 = crypto.hkdfSync(digest, ikm, salt, info, keylen);
  const key2 = crypto.hkdfSync(digest, ikm, salt, info, keylen);

  assertArrayEquals(
    Array.from(key1),
    Array.from(key2),
    'HKDF produces deterministic output'
  );
  console.log('PASS: HKDF deterministic output');
} catch (e) {
  testsFailed++;
  console.log('FAIL: HKDF deterministic output:', e.message);
}

// Test 19: PBKDF2 deterministic output
try {
  const password = 'test';
  const salt = 'salt';
  const iterations = 100;
  const keylen = 32;
  const digest = 'sha256';

  const key1 = crypto.pbkdf2Sync(password, salt, iterations, keylen, digest);
  const key2 = crypto.pbkdf2Sync(password, salt, iterations, keylen, digest);

  assertArrayEquals(
    Array.from(key1),
    Array.from(key2),
    'PBKDF2 produces deterministic output'
  );
  console.log('PASS: PBKDF2 deterministic output');
} catch (e) {
  testsFailed++;
  console.log('FAIL: PBKDF2 deterministic output:', e.message);
}

// Wait for async tests to complete
setTimeout(() => {
  console.log('\n=== Test Summary ===');
  console.log(`Total tests: ${testsRun}`);
  console.log(`Passed: ${testsPassed}`);
  console.log(`Failed: ${testsFailed}`);

  if (testsFailed === 0) {
    console.log('\n✓ All tests passed!');
  } else {
    console.log(`\n✗ ${testsFailed} test(s) failed`);
    process.exit(1);
  }
}, 500);
