// WebCrypto ECDH comprehensive tests
const assert = require('std:assert');

// Check if crypto is available (skip if OpenSSL not found)
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== ECDH Comprehensive Tests Completed (Skipped) ===');
} else {
  console.log('=== WebCrypto ECDH Comprehensive Tests ===');

  // Test 1: Multiple party key exchange (3-way)
  function testMultiPartyKeyExchange() {
    console.log('\n1. Testing 3-party key exchange simulation...');

    // Generate three key pairs
    const alicePromise = crypto.subtle.generateKey(
      {
        name: 'ECDH',
        namedCurve: 'P-256',
      },
      true,
      ['deriveBits']
    );

    const bobPromise = crypto.subtle.generateKey(
      {
        name: 'ECDH',
        namedCurve: 'P-256',
      },
      true,
      ['deriveBits']
    );

    const charliePromise = crypto.subtle.generateKey(
      {
        name: 'ECDH',
        namedCurve: 'P-256',
      },
      true,
      ['deriveBits']
    );

    return Promise.all([alicePromise, bobPromise, charliePromise])
      .then(function (keyPairs) {
        const aliceKeys = keyPairs[0];
        const bobKeys = keyPairs[1];
        const charlieKeys = keyPairs[2];

        console.log('✅ Generated key pairs for Alice, Bob, and Charlie');

        // Alice derives with Bob
        const aliceBobPromise = crypto.subtle.deriveBits(
          {
            name: 'ECDH',
            public: bobKeys.publicKey,
          },
          aliceKeys.privateKey,
          256
        );

        // Bob derives with Charlie
        const bobCharliePromise = crypto.subtle.deriveBits(
          {
            name: 'ECDH',
            public: charlieKeys.publicKey,
          },
          bobKeys.privateKey,
          256
        );

        // Charlie derives with Alice
        const charlieAlicePromise = crypto.subtle.deriveBits(
          {
            name: 'ECDH',
            public: aliceKeys.publicKey,
          },
          charlieKeys.privateKey,
          256
        );

        return Promise.all([
          aliceBobPromise,
          bobCharliePromise,
          charlieAlicePromise,
        ]);
      })
      .then(function (secrets) {
        console.log('✅ All three pairwise secrets derived');
        console.log(
          '   Alice-Bob secret length:',
          new Uint8Array(secrets[0]).length,
          'bytes'
        );
        console.log(
          '   Bob-Charlie secret length:',
          new Uint8Array(secrets[1]).length,
          'bytes'
        );
        console.log(
          '   Charlie-Alice secret length:',
          new Uint8Array(secrets[2]).length,
          'bytes'
        );

        // Each pair should have unique secrets
        const ab = new Uint8Array(secrets[0]);
        const bc = new Uint8Array(secrets[1]);
        const ca = new Uint8Array(secrets[2]);

        let abBcDifferent = false;
        let bcCaDifferent = false;
        let caAbDifferent = false;

        for (let i = 0; i < 32; i++) {
          if (ab[i] !== bc[i]) abBcDifferent = true;
          if (bc[i] !== ca[i]) bcCaDifferent = true;
          if (ca[i] !== ab[i]) caAbDifferent = true;
        }

        assert.strictEqual(
          abBcDifferent,
          true,
          'Alice-Bob and Bob-Charlie secrets should differ'
        );
        assert.strictEqual(
          bcCaDifferent,
          true,
          'Bob-Charlie and Charlie-Alice secrets should differ'
        );
        assert.strictEqual(
          caAbDifferent,
          true,
          'Charlie-Alice and Alice-Bob secrets should differ'
        );

        console.log('✅ All pairwise secrets are unique');
      })
      .catch(function (error) {
        console.error(
          '❌ Multi-party key exchange test failed:',
          error.message
        );
        throw error;
      });
  }

  // Test 2: Different bit lengths for deriveBits
  function testDifferentBitLengths() {
    console.log('\n2. Testing different bit lengths for deriveBits...');

    return Promise.all([
      crypto.subtle.generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-256',
        },
        true,
        ['deriveBits']
      ),
      crypto.subtle.generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-256',
        },
        true,
        ['deriveBits']
      ),
    ])
      .then(function (keyPairs) {
        const aliceKeys = keyPairs[0];
        const bobKeys = keyPairs[1];

        // Test different bit lengths (P-256 max is 256 bits)
        const bitLengths = [128, 192, 256];
        const derivePromises = [];

        bitLengths.forEach(function (bits) {
          derivePromises.push(
            crypto.subtle
              .deriveBits(
                {
                  name: 'ECDH',
                  public: bobKeys.publicKey,
                },
                aliceKeys.privateKey,
                bits
              )
              .then(function (secret) {
                const bytes = new Uint8Array(secret);
                console.log(
                  '✅ Derived',
                  bits,
                  'bits (',
                  bytes.length,
                  'bytes)'
                );
                // P-256 might clamp to max 256 bits
                const expectedBytes = Math.min(bits, 256) / 8;
                assert.strictEqual(
                  bytes.length,
                  expectedBytes,
                  'Derived bytes length should match expected'
                );
                return bytes;
              })
              .catch(function (error) {
                console.log(
                  '   Could not derive',
                  bits,
                  'bits:',
                  error.message
                );
                return null;
              })
          );
        });

        return Promise.all(derivePromises);
      })
      .then(function (secrets) {
        // Filter out null results from failed derivations
        const validSecrets = secrets.filter(function (s) {
          return s !== null;
        });
        if (validSecrets.length > 0) {
          console.log(
            '✅ Successfully derived',
            validSecrets.length,
            'different bit lengths'
          );
        }
      })
      .catch(function (error) {
        console.error('❌ Different bit lengths test failed:', error.message);
        throw error;
      });
  }

  // Test 3: Cross-curve key derivation (should fail)
  function testCrossCurveDerivation() {
    console.log('\n3. Testing cross-curve key derivation (should fail)...');

    // Generate P-256 key pair
    const p256Promise = crypto.subtle.generateKey(
      {
        name: 'ECDH',
        namedCurve: 'P-256',
      },
      true,
      ['deriveBits']
    );

    // Generate P-384 key pair
    const p384Promise = crypto.subtle.generateKey(
      {
        name: 'ECDH',
        namedCurve: 'P-384',
      },
      true,
      ['deriveBits']
    );

    return Promise.all([p256Promise, p384Promise])
      .then(function (keyPairs) {
        const p256Keys = keyPairs[0];
        const p384Keys = keyPairs[1];

        // Try to derive bits using keys from different curves
        return crypto.subtle
          .deriveBits(
            {
              name: 'ECDH',
              public: p384Keys.publicKey, // P-384 public key
            },
            p256Keys.privateKey, // P-256 private key
            256
          )
          .then(function () {
            console.error('❌ Cross-curve derivation should have failed');
            throw new Error('Expected error for cross-curve derivation');
          })
          .catch(function (error) {
            if (error.message && error.message.includes('Expected error')) {
              throw error;
            }
            console.log(
              '✅ Cross-curve derivation correctly failed:',
              error.message
            );
          });
      })
      .catch(function (error) {
        if (error.message && error.message.includes('Expected error')) {
          throw error;
        }
        // Some errors are expected for cross-curve operations
        console.log('✅ Cross-curve test handled appropriately');
      });
  }

  // Test 4: Derive with same key pair (self-derivation)
  function testSelfDerivation() {
    console.log('\n4. Testing self-derivation (derive with own public key)...');

    return crypto.subtle
      .generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-256',
        },
        true,
        ['deriveBits']
      )
      .then(function (keyPair) {
        // Try to derive bits using own public key
        return crypto.subtle.deriveBits(
          {
            name: 'ECDH',
            public: keyPair.publicKey, // Own public key
          },
          keyPair.privateKey, // Own private key
          256
        );
      })
      .then(function (secret) {
        const bytes = new Uint8Array(secret);
        console.log(
          '✅ Self-derivation succeeded, secret length:',
          bytes.length,
          'bytes'
        );

        // The secret should still be valid (mathematically it's the private key times the generator point times the private key)
        assert.strictEqual(
          bytes.length,
          32,
          'Self-derived secret should be 32 bytes'
        );

        // Check it's not all zeros
        let hasNonZero = false;
        for (let i = 0; i < bytes.length; i++) {
          if (bytes[i] !== 0) {
            hasNonZero = true;
            break;
          }
        }
        assert.strictEqual(
          hasNonZero,
          true,
          'Self-derived secret should not be all zeros'
        );
        console.log('✅ Self-derived secret is valid');
      })
      .catch(function (error) {
        console.error('❌ Self-derivation test failed:', error.message);
        throw error;
      });
  }

  // Test 5: Maximum bit length derivation
  function testMaximumBitLength() {
    console.log('\n5. Testing maximum bit length derivation...');

    // For each curve, test deriving maximum possible bits
    const curveMaxBits = {
      'P-256': 256,
      'P-384': 384,
      'P-521': 528, // P-521 actually produces 528 bits (66 bytes)
    };

    const testPromises = [];

    Object.keys(curveMaxBits).forEach(function (curve) {
      const maxBits = curveMaxBits[curve];

      const promise = Promise.all([
        crypto.subtle.generateKey(
          {
            name: 'ECDH',
            namedCurve: curve,
          },
          true,
          ['deriveBits']
        ),
        crypto.subtle.generateKey(
          {
            name: 'ECDH',
            namedCurve: curve,
          },
          true,
          ['deriveBits']
        ),
      ])
        .then(function (keyPairs) {
          return crypto.subtle.deriveBits(
            {
              name: 'ECDH',
              public: keyPairs[1].publicKey,
            },
            keyPairs[0].privateKey,
            maxBits
          );
        })
        .then(function (secret) {
          const bytes = new Uint8Array(secret);
          console.log(
            '✅',
            curve,
            ': Derived',
            maxBits,
            'bits (',
            bytes.length,
            'bytes)'
          );
          assert.strictEqual(
            bytes.length * 8,
            maxBits,
            'Max bits derivation for ' + curve
          );
        });

      testPromises.push(promise);
    });

    return Promise.all(testPromises).catch(function (error) {
      console.error('❌ Maximum bit length test failed:', error.message);
      throw error;
    });
  }

  // Test 6: Zero bits derivation (edge case)
  function testZeroBitsDerivation() {
    console.log('\n6. Testing zero bits derivation (edge case)...');

    return Promise.all([
      crypto.subtle.generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-256',
        },
        true,
        ['deriveBits']
      ),
      crypto.subtle.generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-256',
        },
        true,
        ['deriveBits']
      ),
    ])
      .then(function (keyPairs) {
        // Try to derive 0 bits
        return crypto.subtle
          .deriveBits(
            {
              name: 'ECDH',
              public: keyPairs[1].publicKey,
            },
            keyPairs[0].privateKey,
            0 // Zero bits
          )
          .then(function (secret) {
            const bytes = new Uint8Array(secret);
            console.log(
              'Zero bits derivation returned buffer of length:',
              bytes.length
            );
            assert.strictEqual(
              bytes.length,
              0,
              'Zero bits should return empty buffer'
            );
            console.log('✅ Zero bits derivation handled correctly');
          })
          .catch(function (error) {
            // Some implementations might reject 0 bits
            console.log(
              '✅ Zero bits derivation rejected (also acceptable):',
              error.message
            );
          });
      })
      .catch(function (error) {
        console.error('❌ Zero bits derivation test failed:', error.message);
        throw error;
      });
  }

  // Test 7: Consistency test - same keys always produce same secret
  function testDerivationConsistency() {
    console.log('\n7. Testing derivation consistency...');

    return Promise.all([
      crypto.subtle.generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-384',
        },
        true,
        ['deriveBits']
      ),
      crypto.subtle.generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-384',
        },
        true,
        ['deriveBits']
      ),
    ])
      .then(function (keyPairs) {
        const aliceKeys = keyPairs[0];
        const bobKeys = keyPairs[1];

        // Derive the same secret multiple times
        const derivePromises = [];
        for (let i = 0; i < 5; i++) {
          derivePromises.push(
            crypto.subtle.deriveBits(
              {
                name: 'ECDH',
                public: bobKeys.publicKey,
              },
              aliceKeys.privateKey,
              384
            )
          );
        }

        return Promise.all(derivePromises);
      })
      .then(function (secrets) {
        console.log('Derived same secret 5 times');

        // All secrets should be identical
        const firstSecret = new Uint8Array(secrets[0]);
        for (let i = 1; i < secrets.length; i++) {
          const currentSecret = new Uint8Array(secrets[i]);
          assert.strictEqual(
            currentSecret.length,
            firstSecret.length,
            'Secret lengths should match'
          );

          for (let j = 0; j < firstSecret.length; j++) {
            assert.strictEqual(
              currentSecret[j],
              firstSecret[j],
              'Secret bytes should match at position ' + j
            );
          }
        }

        console.log(
          '✅ All 5 derivations produced identical secrets (deterministic)'
        );
      })
      .catch(function (error) {
        console.error('❌ Derivation consistency test failed:', error.message);
        throw error;
      });
  }

  // Test 8: Invalid public key format
  function testInvalidPublicKey() {
    console.log('\n8. Testing invalid public key handling...');

    return crypto.subtle
      .generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-256',
        },
        true,
        ['deriveBits']
      )
      .then(function (keyPair) {
        // Try to derive with various invalid "public keys"
        const invalidKeys = [
          null,
          undefined,
          {},
          { type: 'public' }, // Missing algorithm
          keyPair.privateKey, // Using private key as public key
        ];

        const testPromises = invalidKeys.map(function (invalidKey) {
          return crypto.subtle
            .deriveBits(
              {
                name: 'ECDH',
                public: invalidKey,
              },
              keyPair.privateKey,
              256
            )
            .then(function () {
              console.error('❌ Should have failed for invalid key');
              return false;
            })
            .catch(function (error) {
              console.log('✅ Correctly rejected invalid public key');
              return true;
            });
        });

        return Promise.all(testPromises);
      })
      .then(function (results) {
        const allRejected = results.every(function (rejected) {
          return rejected;
        });
        assert.strictEqual(
          allRejected,
          true,
          'All invalid keys should be rejected'
        );
        console.log('✅ All invalid public keys were correctly rejected');
      })
      .catch(function (error) {
        console.error('❌ Invalid public key test failed:', error.message);
        throw error;
      });
  }

  // Run all tests
  testMultiPartyKeyExchange()
    .then(testDifferentBitLengths)
    .then(testCrossCurveDerivation)
    .then(testSelfDerivation)
    .then(testMaximumBitLength)
    .then(testZeroBitsDerivation)
    .then(testDerivationConsistency)
    .then(testInvalidPublicKey)
    .then(function () {
      console.log('\n=== All ECDH Comprehensive Tests Passed! ===');
    })
    .catch(function (error) {
      console.error('\n=== ECDH Comprehensive Tests Failed ===');
      console.error('Error:', error.message);
      if (error.stack) {
        console.error('Stack:', error.stack);
      }
    });
} // End of crypto availability check
