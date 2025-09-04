// Comprehensive JWK format test suite
const assert = require('jsrt:assert');

console.log('=== Comprehensive JWK Format Test Suite ===');

async function testJWKComprehensive() {
  let passed = 0;
  let failed = 0;
  let skipped = 0;
  let total = 0;

  // Test 1: RSA public key JWK import (RFC 7517 example)
  total++;
  console.log('Test 1: RSA public key JWK import');
  try {
    const rsaPublicJwk = {
      kty: 'RSA',
      n: '0vx7agoebGcQSuuPiLJXZptN9nndrQmbXEps2aiAFbWhM78LhWx4cbbfAAtVT86zwu1RK7aPFFxuhDR1L6tSoc_BJECPebWKRXjBZCiFV4n3oknjhMstn64tZ_2W-5JsGY4Hc5n9yBXArwl93lqt7_RN5w6Cf0h4QyQ5v-65YGjQR0_FDW2QvzqY368QQMicAtaSqzs8KJZgnYb9c7d0zgdAZHzu6qMQvRL5hajrn1n91CbOpbISD08qNLyrdkt-bFTWhAI4vMQFh6WeZu0fM4lFd2NcRwr3XPksINHaQ-G_xBniIqbw0Ls1jF44-csFCur-kEgU8awapJzKnqDKgw',
      e: 'AQAB',
      alg: 'RS256',
      use: 'sig',
    };

    const importedKey = await crypto.subtle.importKey(
      'jwk',
      rsaPublicJwk,
      {
        name: 'RSASSA-PKCS1-v1_5',
        hash: 'SHA-256',
      },
      true,
      ['verify']
    );

    assert.ok(importedKey, 'Imported key should exist');
    assert.strictEqual(
      importedKey.type,
      'public',
      "Key type should be 'public'"
    );
    assert.strictEqual(
      importedKey.algorithm.name,
      'RSASSA-PKCS1-v1_5',
      'Algorithm should match'
    );
    console.log('✅ PASS: RSA public key JWK import');
    passed++;
  } catch (error) {
    console.log('❌ FAIL: RSA public key JWK import - ' + error.message);
    failed++;
  }

  // Test 2: RSA key with RSA-OAEP algorithm
  total++;
  console.log('Test 2: RSA JWK with RSA-OAEP algorithm');
  try {
    const rsaOaepJwk = {
      kty: 'RSA',
      n: 'sRJjz2msKJXWK8oBu2nLJGx1k6VoqkqbfnI-q6oKG-0AXGh_lNOcf3lzz6qbKq7EbHmfSJl5rr1MX5zBZLSb9g',
      e: 'AQAB',
    };

    const importedKey = await crypto.subtle.importKey(
      'jwk',
      rsaOaepJwk,
      {
        name: 'RSA-OAEP',
        hash: 'SHA-256',
      },
      true,
      ['encrypt']
    );

    assert.strictEqual(
      importedKey.algorithm.name,
      'RSA-OAEP',
      'Algorithm should be RSA-OAEP'
    );
    console.log('✅ PASS: RSA JWK with RSA-OAEP algorithm');
    passed++;
  } catch (error) {
    console.log('❌ FAIL: RSA JWK with RSA-OAEP algorithm - ' + error.message);
    failed++;
  }

  // Test 3: Invalid JWK - missing kty
  total++;
  console.log('Test 3: Invalid JWK - missing kty');
  try {
    const invalidJwk = {
      n: 'invalid',
      e: 'AQAB',
    };

    try {
      await crypto.subtle.importKey(
        'jwk',
        invalidJwk,
        { name: 'RSA-OAEP', hash: 'SHA-256' },
        true,
        ['encrypt']
      );
      console.log('❌ FAIL: Should have thrown error for missing kty');
      failed++;
    } catch (expectedError) {
      assert.ok(
        expectedError.message.includes('kty'),
        'Error should mention kty parameter'
      );
      console.log('✅ PASS: Correctly rejected JWK missing kty');
      passed++;
    }
  } catch (error) {
    console.log('❌ FAIL: Invalid JWK test setup - ' + error.message);
    failed++;
  }

  // Test 4: Invalid JWK - wrong algorithm
  total++;
  console.log('Test 4: Algorithm mismatch error');
  try {
    const rsaJwk = {
      kty: 'RSA',
      n: 'sRJjz2msKJXWK8oBu2nLJGx1k6VoqkqbfnI-q6oKG-0AXGh_lNOcf3lzz6qbKq7EbHmfSJl5rr1MX5zBZLSb9g',
      e: 'AQAB',
    };

    try {
      await crypto.subtle.importKey(
        'jwk',
        rsaJwk,
        { name: 'HMAC', hash: 'SHA-256' }, // Wrong algorithm
        true,
        ['sign']
      );
      console.log('❌ FAIL: Should have thrown error for algorithm mismatch');
      failed++;
    } catch (expectedError) {
      assert.ok(
        expectedError.message.includes('Algorithm mismatch'),
        'Error should mention algorithm mismatch'
      );
      console.log('✅ PASS: Correctly rejected algorithm mismatch');
      passed++;
    }
  } catch (error) {
    console.log('❌ FAIL: Algorithm mismatch test setup - ' + error.message);
    failed++;
  }

  // Test 5: ECDSA JWK import (P-256)
  total++;
  console.log('Test 5: ECDSA P-256 JWK import');
  try {
    const ecJwk = {
      kty: 'EC',
      crv: 'P-256',
      x: 'MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4',
      y: '4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM',
    };

    const importedKey = await crypto.subtle.importKey(
      'jwk',
      ecJwk,
      { name: 'ECDSA', namedCurve: 'P-256' },
      true,
      ['verify']
    );

    assert.ok(importedKey, 'Imported EC key should exist');
    assert.strictEqual(
      importedKey.type,
      'public',
      "EC key type should be 'public'"
    );
    assert.strictEqual(
      importedKey.algorithm.name,
      'ECDSA',
      'Algorithm should be ECDSA'
    );
    assert.strictEqual(
      importedKey.algorithm.namedCurve,
      'P-256',
      'Named curve should be P-256'
    );
    console.log('✅ PASS: ECDSA P-256 JWK import');
    passed++;
  } catch (error) {
    console.log('❌ FAIL: ECDSA P-256 JWK import - ' + error.message);
    failed++;
  }

  // Test 6: Symmetric (oct) JWK import for HMAC
  total++;
  console.log('Test 6: Symmetric HMAC JWK import');
  try {
    const octJwk = {
      kty: 'oct',
      k: 'GawgguFyGrWKav7AX4VKUg', // Base64URL encoded 16-byte key
    };

    const importedKey = await crypto.subtle.importKey(
      'jwk',
      octJwk,
      { name: 'HMAC', hash: 'SHA-256' },
      true,
      ['sign', 'verify']
    );

    assert.ok(importedKey, 'Imported HMAC key should exist');
    assert.strictEqual(
      importedKey.type,
      'secret',
      "HMAC key type should be 'secret'"
    );
    assert.strictEqual(
      importedKey.algorithm.name,
      'HMAC',
      'Algorithm should be HMAC'
    );
    console.log('✅ PASS: Symmetric HMAC JWK import');
    passed++;
  } catch (error) {
    console.log('❌ FAIL: Symmetric HMAC JWK import - ' + error.message);
    failed++;
  }

  // Test 7: AES-256 symmetric key JWK import
  total++;
  console.log('Test 7: AES-256 JWK import');
  try {
    const aesJwk = {
      kty: 'oct',
      k: 'AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8', // 32-byte key for AES-256
    };

    const importedKey = await crypto.subtle.importKey(
      'jwk',
      aesJwk,
      { name: 'AES-GCM', length: 256 },
      true,
      ['encrypt', 'decrypt']
    );

    assert.ok(importedKey, 'Imported AES key should exist');
    assert.strictEqual(
      importedKey.type,
      'secret',
      "AES key type should be 'secret'"
    );
    assert.strictEqual(
      importedKey.algorithm.name,
      'AES-GCM',
      'Algorithm should be AES-GCM'
    );
    console.log('✅ PASS: AES-256 JWK import');
    passed++;
  } catch (error) {
    console.log('❌ FAIL: AES-256 JWK import - ' + error.message);
    failed++;
  }

  // Test 8: ECDSA P-384 JWK import
  total++;
  console.log('Test 8: ECDSA P-384 JWK import');
  try {
    const ecP384Jwk = {
      kty: 'EC',
      crv: 'P-384',
      x: 'usWxHK2PmfnHKwXPS54m0kTcGJ90UiglWiGahtagnv8',
      y: 'IBOL-C3BtVOrsHat1gE1-T0d-Iicn9_rp4AfPQ6yqtE',
    };

    const importedKey = await crypto.subtle.importKey(
      'jwk',
      ecP384Jwk,
      { name: 'ECDSA', namedCurve: 'P-384' },
      true,
      ['verify']
    );

    assert.ok(importedKey, 'Imported P-384 EC key should exist');
    assert.strictEqual(
      importedKey.algorithm.namedCurve,
      'P-384',
      'Named curve should be P-384'
    );
    console.log('✅ PASS: ECDSA P-384 JWK import');
    passed++;
  } catch (error) {
    console.log('❌ FAIL: ECDSA P-384 JWK import - ' + error.message);
    failed++;
  }

  // Test 9: Invalid EC curve
  total++;
  console.log('Test 9: Invalid EC curve in JWK');
  try {
    const invalidCurveJwk = {
      kty: 'EC',
      crv: 'P-192', // Unsupported curve
      x: 'MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4',
      y: '4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM',
    };

    try {
      await crypto.subtle.importKey(
        'jwk',
        invalidCurveJwk,
        { name: 'ECDSA', namedCurve: 'P-192' },
        true,
        ['verify']
      );
      console.log('❌ FAIL: Should have thrown error for unsupported curve');
      failed++;
    } catch (expectedError) {
      assert.ok(
        expectedError.message.includes('Unsupported curve'),
        'Error should mention unsupported curve'
      );
      console.log('✅ PASS: Correctly rejected unsupported curve');
      passed++;
    }
  } catch (error) {
    console.log('❌ FAIL: Invalid curve test setup - ' + error.message);
    failed++;
  }

  // Test 10: HMAC key too short
  total++;
  console.log('Test 10: HMAC key too short');
  try {
    const shortHmacJwk = {
      kty: 'oct',
      k: 'c2hvcnQ', // "short" - only 5 bytes
    };

    try {
      await crypto.subtle.importKey(
        'jwk',
        shortHmacJwk,
        { name: 'HMAC', hash: 'SHA-256' },
        true,
        ['sign', 'verify']
      );
      console.log('❌ FAIL: Should have thrown error for short HMAC key');
      failed++;
    } catch (expectedError) {
      assert.ok(
        expectedError.message.includes('too short'),
        'Error should mention key too short'
      );
      console.log('✅ PASS: Correctly rejected short HMAC key');
      passed++;
    }
  } catch (error) {
    console.log('❌ FAIL: Short HMAC key test setup - ' + error.message);
    failed++;
  }

  // Test 11: Invalid base64url encoding
  total++;
  console.log('Test 11: Invalid base64url encoding in JWK');
  try {
    const invalidEncodingJwk = {
      kty: 'RSA',
      n: 'invalid_base64url_@#$%', // Invalid characters
      e: 'AQAB',
    };

    try {
      await crypto.subtle.importKey(
        'jwk',
        invalidEncodingJwk,
        { name: 'RSA-OAEP', hash: 'SHA-256' },
        true,
        ['encrypt']
      );
      console.log('❌ FAIL: Should have thrown error for invalid base64url');
      failed++;
    } catch (expectedError) {
      assert.ok(
        expectedError.message.includes('base64url'),
        'Error should mention base64url encoding'
      );
      console.log('✅ PASS: Correctly rejected invalid base64url');
      passed++;
    }
  } catch (error) {
    console.log('❌ FAIL: Invalid encoding test setup - ' + error.message);
    failed++;
  }

  console.log('\n=== JWK Comprehensive Test Summary ===');
  console.log('Total Tests:', total);
  console.log('Passed:', passed);
  console.log('Failed:', failed);
  console.log('Skipped:', skipped);
  console.log('=== Tests Completed ===');

  return { total, passed, failed, skipped };
}

// Check if crypto.subtle is available
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== Tests Completed (Skipped) ===');
} else {
  testJWKComprehensive().catch(console.error);
}
