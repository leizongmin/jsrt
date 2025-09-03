// WebCrypto SubtleCrypto.digest tests
const assert = require('jsrt:assert');
console.log('=== Starting WebCrypto SubtleCrypto Digest Tests ===');

// Test 1: crypto.subtle object existence
console.log('Test 1: crypto.subtle object existence');
if (typeof crypto === 'undefined') {
  console.log('❌ SKIP: crypto object not available (OpenSSL not found)');
  console.log('=== WebCrypto Digest Tests Completed (Skipped) ===');
} else if (typeof crypto.subtle === 'undefined') {
  console.log('❌ FAIL: crypto.subtle not available');
  console.log('=== WebCrypto Digest Tests Completed (Failed) ===');
} else {
  console.log('✅ PASS: crypto.subtle is available');

  // Test 2: digest function existence
  console.log('Test 2: crypto.subtle.digest function existence');
  if (typeof crypto.subtle.digest === 'function') {
    console.log('✅ PASS: crypto.subtle.digest is a function');
  } else {
    console.log('❌ FAIL: crypto.subtle.digest should be a function');
  }

  // Test 3: Basic SHA-256 digest test
  console.log('Test 3: Basic SHA-256 digest test');
  try {
    const data = new TextEncoder().encode('hello');
    console.log('Input data:', Array.from(data));

    const result = crypto.subtle.digest('SHA-256', data);
    console.log('Digest result type:', typeof result);

    if (result && typeof result.then === 'function') {
      console.log('✅ PASS: Returns promise-like object');

      result
        .then((hash) => {
          console.log('SHA-256 hash received');
          const hashArray = new Uint8Array(hash);
          console.log('Hash length:', hashArray.length);
          console.log(
            'Hash (first 8 bytes):',
            Array.from(hashArray.slice(0, 8))
          );

          // Expected SHA-256 of "hello": 2cf24dba4f21d4288dfcb1eb908e6ba18a40d8e14a03c5cdf2df5659cfd4d2cf
          const expected_start = [
            0x2c, 0xf2, 0x4d, 0xba, 0x4f, 0x21, 0xd4, 0x28,
          ];
          const actual_start = Array.from(hashArray.slice(0, 8));

          let matches = true;
          for (let i = 0; i < 8; i++) {
            if (actual_start[i] !== expected_start[i]) {
              matches = false;
              break;
            }
          }

          if (matches) {
            console.log('✅ PASS: SHA-256 hash matches expected value');
          } else {
            console.log('❌ FAIL: SHA-256 hash does not match expected value');
            console.log('Expected:', expected_start);
            console.log('Actual:', actual_start);
          }

          // Test 4: Different algorithms
          console.log('Test 4: Different hash algorithms');
          testOtherAlgorithms();
        })
        .catch((error) => {
          console.error('❌ FAIL: Promise rejected:', error);
        });
    } else {
      console.log('❌ FAIL: Should return a promise, got:', result);
    }
  } catch (error) {
    console.error('❌ FAIL: Exception thrown:', error.name, error.message);
  }
}

function testOtherAlgorithms() {
  const algorithms = ['SHA-1', 'SHA-256', 'SHA-384', 'SHA-512'];
  const data = new TextEncoder().encode('test');

  algorithms.forEach((alg, index) => {
    console.log(`Test 4.${index + 1}: ${alg} algorithm`);
    try {
      const result = crypto.subtle.digest(alg, data);
      if (result && typeof result.then === 'function') {
        result
          .then((hash) => {
            const hashArray = new Uint8Array(hash);
            console.log(
              `✅ PASS: ${alg} digest successful (${hashArray.length} bytes)`
            );

            if (index === algorithms.length - 1) {
              console.log(
                '=== WebCrypto Digest Tests Completed Successfully ==='
              );
            }
          })
          .catch((error) => {
            console.error(`❌ FAIL: ${alg} digest failed:`, error);
          });
      } else {
        console.log(`❌ FAIL: ${alg} should return a promise`);
      }
    } catch (error) {
      console.error(
        `❌ FAIL: ${alg} threw exception:`,
        error.name,
        error.message
      );
    }
  });
}
