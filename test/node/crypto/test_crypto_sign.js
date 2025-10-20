// Test node:crypto Sign and Verify implementation

const crypto = require('node:crypto');
const webcrypto = globalThis.crypto;

let testsPassed = 0;
let testsFailed = 0;

function test(name, fn) {
  try {
    fn();
    console.log(`✓ ${name}`);
    testsPassed++;
  } catch (e) {
    console.log(`✗ ${name}`);
    console.log(`  Error: ${e.message}`);
    if (e.stack) {
      console.log(`  Stack: ${e.stack}`);
    }
    testsFailed++;
  }
}

function assert(condition, message) {
  if (!condition) {
    throw new Error(message || 'Assertion failed');
  }
}

async function asyncTest(name, fn) {
  try {
    await fn();
    console.log(`✓ ${name}`);
    testsPassed++;
  } catch (e) {
    console.log(`✗ ${name}`);
    console.log(`  Error: ${e.message}`);
    if (e.stack) {
      console.log(`  Stack: ${e.stack}`);
    }
    testsFailed++;
  }
}

// Helper to generate RSA key pair using WebCrypto
async function generateRSAKeyPair() {
  const keyPair = await webcrypto.subtle.generateKey(
    {
      name: 'RSASSA-PKCS1-v1_5',
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
    },
    true,
    ['sign', 'verify']
  );
  return keyPair;
}

// Helper to generate ECDSA key pair using WebCrypto
async function generateECDSAKeyPair() {
  const keyPair = await webcrypto.subtle.generateKey(
    {
      name: 'ECDSA',
      namedCurve: 'P-256',
    },
    true,
    ['sign', 'verify']
  );
  return keyPair;
}

// Run async tests
(async () => {
  // Check if crypto key generation is available (required on Windows)
  try {
    // Test RSA key generation
    await generateRSAKeyPair();
    // Test ECDSA key generation
    await generateECDSAKeyPair();
  } catch (e) {
    const errMsg = e && e.message ? e.message : String(e);
    if (errMsg.includes('Failed to initialize') || 
        errMsg.includes('not available') ||
        errMsg.includes('OpenSSL')) {
      console.log('Error: OpenSSL symmetric functions not available');
      process.exit(0);
    }
    // Re-throw other errors
    throw e;
  }

  console.log('Testing node:crypto Sign and Verify implementation...\n');
  // Test 1: createSign returns a Sign object
  await asyncTest('createSign returns a Sign object', async () => {
    const sign = crypto.createSign('RSA-SHA256');
    assert(sign !== null, 'Sign object should not be null');
    assert(typeof sign.update === 'function', 'Sign should have update method');
    assert(typeof sign.sign === 'function', 'Sign should have sign method');
  });

  // Test 2: createVerify returns a Verify object
  await asyncTest('createVerify returns a Verify object', async () => {
    const verify = crypto.createVerify('RSA-SHA256');
    assert(verify !== null, 'Verify object should not be null');
    assert(
      typeof verify.update === 'function',
      'Verify should have update method'
    );
    assert(
      typeof verify.verify === 'function',
      'Verify should have verify method'
    );
  });

  // Test 3: RSA-SHA256 sign/verify basic functionality
  await asyncTest('RSA-SHA256 sign and verify', async () => {
    const keyPair = await generateRSAKeyPair();

    const data = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]); // "hello"

    // Sign
    const sign = crypto.createSign('RSA-SHA256');
    sign.update(data);
    const signature = sign.sign(keyPair.privateKey);

    assert(signature instanceof Uint8Array, 'Signature should be Uint8Array');
    assert(signature.length > 0, 'Signature should not be empty');

    // Verify
    const verify = crypto.createVerify('RSA-SHA256');
    verify.update(data);
    const result = verify.verify(keyPair.publicKey, signature);

    assert(result === true, 'Verification should succeed');
  });

  // Test 4: RSA sign with hex output encoding
  await asyncTest('RSA sign with hex encoding', async () => {
    const keyPair = await generateRSAKeyPair();
    const data = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]);

    const sign = crypto.createSign('RSA-SHA256');
    sign.update(data);
    const signature = sign.sign(keyPair.privateKey, 'hex');

    assert(typeof signature === 'string', 'Hex signature should be a string');
    assert(signature.length > 0, 'Hex signature should not be empty');
    assert(/^[0-9a-f]+$/.test(signature), 'Should be valid hex string');
  });

  // Test 5: RSA sign with base64 output encoding
  await asyncTest('RSA sign with base64 encoding', async () => {
    const keyPair = await generateRSAKeyPair();
    const data = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]);

    const sign = crypto.createSign('RSA-SHA256');
    sign.update(data);
    const signature = sign.sign(keyPair.privateKey, 'base64');

    assert(
      typeof signature === 'string',
      'Base64 signature should be a string'
    );
    assert(signature.length > 0, 'Base64 signature should not be empty');
  });

  // Test 6: Sign/Verify with multiple updates
  await asyncTest('Sign and verify with multiple updates', async () => {
    const keyPair = await generateRSAKeyPair();

    // Sign with multiple updates
    const sign = crypto.createSign('RSA-SHA256');
    sign.update(new Uint8Array([0x68, 0x65])); // "he"
    sign.update(new Uint8Array([0x6c, 0x6c])); // "ll"
    sign.update(new Uint8Array([0x6f])); // "o"
    const signature = sign.sign(keyPair.privateKey);

    // Verify with same data
    const verify = crypto.createVerify('RSA-SHA256');
    verify.update(new Uint8Array([0x68, 0x65]));
    verify.update(new Uint8Array([0x6c, 0x6c]));
    verify.update(new Uint8Array([0x6f]));
    const result = verify.verify(keyPair.publicKey, signature);

    assert(
      result === true,
      'Verification with multiple updates should succeed'
    );
  });

  // Test 7: Verification should fail with wrong data
  await asyncTest('Verification fails with wrong data', async () => {
    const keyPair = await generateRSAKeyPair();

    const sign = crypto.createSign('RSA-SHA256');
    sign.update(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f])); // "hello"
    const signature = sign.sign(keyPair.privateKey);

    const verify = crypto.createVerify('RSA-SHA256');
    verify.update(new Uint8Array([0x77, 0x6f, 0x72, 0x6c, 0x64])); // "world"
    const result = verify.verify(keyPair.publicKey, signature);

    assert(result === false, 'Verification should fail with wrong data');
  });

  // Test 8: Verification should fail with wrong key
  await asyncTest('Verification fails with wrong key', async () => {
    const keyPair1 = await generateRSAKeyPair();
    const keyPair2 = await generateRSAKeyPair();

    const data = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]);

    const sign = crypto.createSign('RSA-SHA256');
    sign.update(data);
    const signature = sign.sign(keyPair1.privateKey);

    const verify = crypto.createVerify('RSA-SHA256');
    verify.update(data);
    const result = verify.verify(keyPair2.publicKey, signature);

    assert(result === false, 'Verification should fail with wrong key');
  });

  // Test 9: ECDSA-SHA256 sign/verify
  await asyncTest('ECDSA-SHA256 sign and verify', async () => {
    const keyPair = await generateECDSAKeyPair();

    const data = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]);

    // Sign
    const sign = crypto.createSign('ecdsa-with-SHA256');
    sign.update(data);
    const signature = sign.sign(keyPair.privateKey);

    assert(
      signature instanceof Uint8Array,
      'ECDSA signature should be Uint8Array'
    );
    assert(signature.length > 0, 'ECDSA signature should not be empty');

    // Verify
    const verify = crypto.createVerify('ecdsa-with-SHA256');
    verify.update(data);
    const result = verify.verify(keyPair.publicKey, signature);

    assert(result === true, 'ECDSA verification should succeed');
  });

  // Test 10: RSA-SHA384
  await asyncTest('RSA-SHA384 sign and verify', async () => {
    const keyPair = await webcrypto.subtle.generateKey(
      {
        name: 'RSASSA-PKCS1-v1_5',
        modulusLength: 2048,
        publicExponent: new Uint8Array([1, 0, 1]),
        hash: 'SHA-384',
      },
      true,
      ['sign', 'verify']
    );

    const data = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]);

    const sign = crypto.createSign('RSA-SHA384');
    sign.update(data);
    const signature = sign.sign(keyPair.privateKey);

    const verify = crypto.createVerify('RSA-SHA384');
    verify.update(data);
    const result = verify.verify(keyPair.publicKey, signature);

    assert(result === true, 'RSA-SHA384 verification should succeed');
  });

  // Test 11: RSA-SHA512
  await asyncTest('RSA-SHA512 sign and verify', async () => {
    const keyPair = await webcrypto.subtle.generateKey(
      {
        name: 'RSASSA-PKCS1-v1_5',
        modulusLength: 2048,
        publicExponent: new Uint8Array([1, 0, 1]),
        hash: 'SHA-512',
      },
      true,
      ['sign', 'verify']
    );

    const data = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]);

    const sign = crypto.createSign('RSA-SHA512');
    sign.update(data);
    const signature = sign.sign(keyPair.privateKey);

    const verify = crypto.createVerify('RSA-SHA512');
    verify.update(data);
    const result = verify.verify(keyPair.publicKey, signature);

    assert(result === true, 'RSA-SHA512 verification should succeed');
  });

  // Test 12: Error - sign called twice
  await asyncTest('Error when sign called twice', async () => {
    const keyPair = await generateRSAKeyPair();

    const sign = crypto.createSign('RSA-SHA256');
    sign.update(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]));
    sign.sign(keyPair.privateKey);

    let errorThrown = false;
    try {
      sign.sign(keyPair.privateKey);
    } catch (e) {
      errorThrown = true;
    }
    assert(errorThrown, 'Should throw error when sign called twice');
  });

  // Test 13: Error - verify called twice
  await asyncTest('Error when verify called twice', async () => {
    const keyPair = await generateRSAKeyPair();

    const data = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]);

    const sign = crypto.createSign('RSA-SHA256');
    sign.update(data);
    const signature = sign.sign(keyPair.privateKey);

    const verify = crypto.createVerify('RSA-SHA256');
    verify.update(data);
    verify.verify(keyPair.publicKey, signature);

    let errorThrown = false;
    try {
      verify.verify(keyPair.publicKey, signature);
    } catch (e) {
      errorThrown = true;
    }
    assert(errorThrown, 'Should throw error when verify called twice');
  });

  console.log(`\n${testsPassed} tests passed, ${testsFailed} tests failed`);

  if (testsFailed > 0) {
    throw new Error('Some tests failed');
  }
})().catch((e) => {
  console.error('Test execution failed:', e);
  process.exit(1);
});
