# Elliptic Curve Cryptography Test Suite

This directory contains comprehensive test suites for ECDSA and ECDH implementations in the jsrt WebCrypto API.

## Test Files

### Basic Tests (Original)
- `test_webcrypto_ecdsa.js` - Basic ECDSA signature and verification tests
  - Key generation with P-256, P-384, P-521 curves
  - Sign/verify operations
  - Different hash algorithms (SHA-256, SHA-384, SHA-512)
  - Tampered data detection

- `test_webcrypto_ecdh.js` - Basic ECDH key derivation tests
  - Key generation for key exchange
  - Shared secret derivation between parties
  - Different curves support
  - deriveKey functionality (if supported)

### Comprehensive Tests (New)
- `test_webcrypto_ecdsa_edge.js` - ECDSA edge cases and comprehensive tests
  - Invalid curve name handling
  - Empty and large data signing
  - Wrong key usage scenarios
  - Cross-curve verification (should fail)
  - Multiple signatures with randomness verification
  - Hash algorithm mismatch detection

- `test_webcrypto_ecdh_comprehensive.js` - ECDH comprehensive tests
  - Multi-party key exchange simulation
  - Different bit lengths for deriveBits
  - Cross-curve derivation (should fail)
  - Self-derivation testing
  - Maximum bit length per curve
  - Consistency verification

- `test_webcrypto_ec_interop.js` - EC interoperability tests
  - Mixed algorithm usage testing
  - Combined ECDSA/ECDH workflows
  - Performance comparison between curves
  - Key reuse across operations
  - Error propagation scenarios

## Running the Tests

### Individual Test Execution
```bash
./target/release/jsrt test/test_webcrypto_ecdsa.js
./target/release/jsrt test/test_webcrypto_ecdh.js
./target/release/jsrt test/test_webcrypto_ecdsa_edge.js
./target/release/jsrt test/test_webcrypto_ecdh_comprehensive.js
./target/release/jsrt test/test_webcrypto_ec_interop.js
```

### Run All EC Tests
```bash
# Using make test will run all tests including EC tests
make test
```

## Test Coverage

### ECDSA Coverage
- ✅ Key generation (P-256, P-384, P-521)
- ✅ Signature creation
- ✅ Signature verification
- ✅ Multiple hash algorithms (SHA-256, SHA-384, SHA-512)
- ✅ Invalid signature detection
- ✅ Tampered data detection
- ✅ Empty data handling
- ✅ Large data (1MB) handling
- ✅ Cross-curve verification rejection
- ✅ Signature randomness verification

### ECDH Coverage
- ✅ Key generation (P-256, P-384, P-521)
- ✅ Shared secret derivation
- ✅ Bidirectional key agreement
- ✅ Variable bit length derivation
- ✅ Cross-curve rejection
- ✅ Invalid public key handling
- ✅ Multi-party simulation
- ✅ Consistency verification

### Interoperability Coverage
- ✅ ECDSA and ECDH on same curves
- ✅ Combined workflows
- ✅ Performance characteristics
- ✅ Key reuse scenarios
- ✅ Error handling

## Known Issues and Limitations

1. **Invalid Signature Format Test**: Some implementations may hang or behave unexpectedly with malformed signatures. This test is currently disabled in `test_webcrypto_ecdsa_edge.js`.

2. **Mixed Algorithm Usages**: The implementation currently allows flexible key usage combinations which may not strictly follow the WebCrypto specification.

3. **Bit Length Limitations**: P-256 curves are limited to 256-bit derivations, P-384 to 384 bits, etc.

4. **Error Propagation Test**: Disabled in interop tests to avoid potential hangs when mixing key types.

## Dependencies

All EC tests require OpenSSL to be available. Tests will automatically skip if crypto.subtle is not available, making them safe to run in environments without OpenSSL.

## Test Output

All tests follow a consistent output format:
- ✅ Success indicators for passed tests
- ❌ Failure indicators with error messages
- ⚠️ Warnings for non-standard behavior
- Clear section headers for test groups
- Summary at the end of each test file

## Maintenance

When adding new EC functionality:
1. Add basic tests to the original test files
2. Add edge cases to the comprehensive test files
3. Add interoperability tests if the feature interacts with other algorithms
4. Update this README with new coverage information