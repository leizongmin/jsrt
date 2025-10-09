---
Created: 2025-10-09T12:30:00Z
Last Updated: 2025-10-09T12:30:00Z
Status: 🟡 PLANNING
Overall Progress: 0/168 tasks (0%)
API Coverage: 3/85+ methods (3.5%)
---

# Node.js crypto Module Implementation Plan

## 📋 Executive Summary

### Objective
Implement a comprehensive Node.js-compatible `node:crypto` module in jsrt by maximally reusing the existing WebCrypto implementation (~99% complete). Provide stream-based APIs and Node.js compatibility layer on top of the robust WebCrypto foundation.

### Current Status
- ✅ **WebCrypto foundation** - Complete implementation in `src/crypto/` (18 files, ~400KB code)
- ✅ **Basic node:crypto** - Minimal implementation with randomBytes, randomUUID, constants
- 🟡 **Full node:crypto API** - 3/85+ methods implemented (3.5% coverage)
- 🎯 **Target**: 100% API coverage with maximum code reuse from WebCrypto

### Strategy: Maximum Code Reuse
**Critical Success Factor**: Reuse existing WebCrypto implementation (~99% complete)

| WebCrypto Component | Node.js APIs Enabled | Reuse % |
|---------------------|---------------------|---------|
| `crypto_digest.c/h` | createHash, hash.update/digest | 95% |
| `crypto_hmac.c/h` | createHmac, hmac.update/digest | 95% |
| `crypto_symmetric.c/h` | createCipheriv, createDecipheriv | 90% |
| `crypto_rsa.c/h` | createSign, createVerify (RSA) | 85% |
| `crypto_ec.c/h` | createSign, createVerify (ECDSA), createECDH | 85% |
| `crypto_kdf.c/h` | pbkdf2, pbkdf2Sync, scrypt, hkdf | 90% |
| `crypto_subtle.c` | KeyObject import/export, generateKey | 80% |

**Implementation Pattern**: Thin Node.js API wrapper → WebCrypto backend functions

### Implementation Phases (Prioritized by API Usage)

**Phase 1: Hash & HMAC** (Highest Priority - Most Used)
- createHash, createHmac (stream APIs)
- Direct mapping to crypto_digest.c and crypto_hmac.c

**Phase 2: Random & Utilities** (Quick Wins)
- Enhance existing randomBytes, randomUUID
- Add randomInt, randomFill, timingSafeEqual

**Phase 3: Cipher Operations** (High Priority)
- createCipheriv, createDecipheriv (stream APIs)
- Map to crypto_symmetric.c (AES-CBC/GCM/CTR)

**Phase 4: Sign/Verify** (Medium Priority)
- createSign, createVerify (stream APIs)
- Map to crypto_rsa.c and crypto_ec.c

**Phase 5: Key Management** (Complex but Essential)
- KeyObject class (createPublicKey, createPrivateKey, createSecretKey)
- generateKeyPair, generateKeyPairSync
- Leverage crypto_subtle.c key import/export

**Phase 6: Key Derivation** (Medium Priority)
- pbkdf2, pbkdf2Sync, scrypt, scryptSync
- Map to crypto_kdf.c (PBKDF2/HKDF)

**Phase 7: Diffie-Hellman** (Lower Priority)
- createDiffieHellman, createDiffieHellmanGroup
- createECDH (reuse crypto_ec.c ECDH)

**Phase 8: Advanced Features** (Future)
- Certificate class, webcrypto integration
- Additional utilities and constants

### File Organization Strategy

**Current**: Single file `src/node/node_crypto.c` (161 lines)

**Target Structure** (when exceeding 500 lines):
```
src/node/crypto/
├── node_crypto_internal.h     # Shared types, macros, utilities
├── node_crypto_hash.c         # Hash and HMAC (Phase 1)
├── node_crypto_cipher.c       # Cipher/Decipher (Phase 3)
├── node_crypto_sign.c         # Sign/Verify (Phase 4)
├── node_crypto_keys.c         # KeyObject, key generation (Phase 5)
├── node_crypto_kdf.c          # PBKDF2, scrypt, HKDF (Phase 6)
├── node_crypto_dh.c           # DiffieHellman, ECDH (Phase 7)
├── node_crypto_random.c       # Random generators (Phase 2)
├── node_crypto_util.c         # Utilities (timingSafeEqual, etc.)
└── node_crypto_module.c       # Module registration & exports
```

**Refactoring Trigger**: When node_crypto.c exceeds 500 lines (estimated: after Phase 2)

### Testing Strategy

**Test Organization**:
```
test/node/crypto/
├── test_crypto_hash.js        # Phase 1: Hash & HMAC tests
├── test_crypto_random.js      # Phase 2: Random generation tests
├── test_crypto_cipher.js      # Phase 3: Cipher/Decipher tests
├── test_crypto_sign.js        # Phase 4: Sign/Verify tests
├── test_crypto_keys.js        # Phase 5: KeyObject tests
├── test_crypto_kdf.js         # Phase 6: Key derivation tests
├── test_crypto_dh.js          # Phase 7: Diffie-Hellman tests
└── test_crypto_compat.js      # Cross-phase compatibility tests
```

**Testing Requirements** (MANDATORY per jsrt rules):
1. Run baseline tests BEFORE modifications: `make test && make wpt`
2. Test IMMEDIATELY after each phase completion
3. Minimum 80% code coverage per phase
4. All tests MUST pass before moving to next phase
5. ASAN validation: `make jsrt_m` for memory leak detection

---

## 📊 L0 Main Task

### Requirement
Implement complete Node.js `node:crypto` module API compatible with Node.js v20+ specification, maximally reusing existing WebCrypto implementation in jsrt.

### Success Criteria
- ✅ 85+ Node.js crypto API methods implemented
- ✅ All stream-based APIs (Hash, Hmac, Cipher, Decipher, Sign, Verify) functional
- ✅ Full KeyObject class with import/export support
- ✅ All random generation methods implemented
- ✅ Key derivation functions (PBKDF2, scrypt, HKDF) working
- ✅ Diffie-Hellman key exchange operational
- ✅ 100% code reuse from existing WebCrypto where applicable
- ✅ All tests pass: `make test && make wpt` (no regressions)
- ✅ ASAN clean: `make jsrt_m` (no memory leaks)
- ✅ Comprehensive test coverage (80%+ per module)

### Constraints
- **MUST reuse** existing WebCrypto implementation (src/crypto/)
- **MUST follow** jsrt development rules (CLAUDE.md)
- **MUST maintain** backward compatibility with existing code
- **MUST NOT** modify WebCrypto API (only add Node.js wrapper layer)
- **MUST support** both CommonJS and ES module exports
- **MUST handle** both callback and Promise-based APIs

---

## 🏗️ L1 Epic Phases

### Phase 0: Research & Architecture Setup [S][R:LOW][C:SIMPLE]
**Goal**: Analyze WebCrypto code, design Node.js API mapping, establish architecture
**Duration**: ~30 minutes | **Status**: ⏳ PENDING

### Phase 1: Hash & HMAC Implementation [S][R:LOW][C:MEDIUM][D:0]
**Goal**: Implement createHash and createHmac with stream API
**Duration**: ~1.5 hours | **Status**: ⏳ PENDING

### Phase 2: Random & Utilities [P][R:LOW][C:SIMPLE][D:0]
**Goal**: Enhance random generators, add utility functions
**Duration**: ~1 hour | **Status**: ⏳ PENDING

### Phase 3: Cipher Operations [S][R:MED][C:COMPLEX][D:1]
**Goal**: Implement createCipheriv and createDecipheriv with stream API
**Duration**: ~2 hours | **Status**: ⏳ PENDING

### Phase 4: Sign/Verify Operations [S][R:MED][C:COMPLEX][D:1,3]
**Goal**: Implement createSign and createVerify with stream API
**Duration**: ~2 hours | **Status**: ⏳ PENDING

### Phase 5: Key Management [S][R:HIGH][C:COMPLEX][D:1,3,4]
**Goal**: Implement KeyObject class and key generation APIs
**Duration**: ~3 hours | **Status**: ⏳ PENDING

### Phase 6: Key Derivation Functions [P][R:LOW][C:MEDIUM][D:1]
**Goal**: Implement pbkdf2, scrypt, hkdf (sync & async)
**Duration**: ~1.5 hours | **Status**: ⏳ PENDING

### Phase 7: Diffie-Hellman [S][R:MED][C:COMPLEX][D:5]
**Goal**: Implement DH and ECDH key exchange
**Duration**: ~2 hours | **Status**: ⏳ PENDING

### Phase 8: Integration & Testing [S][R:MED][C:MEDIUM][D:1,2,3,4,5,6,7]
**Goal**: Comprehensive testing, documentation, final validation
**Duration**: ~2 hours | **Status**: ⏳ PENDING

---

## 📝 Detailed Task Breakdown

### Phase 0: Research & Architecture Setup (15 tasks)

**0.1 Code Analysis & Architecture** [S][R:LOW][C:SIMPLE] - 8 tasks
- 0.1.1 [S] Analyze crypto_digest.c/h API surface (see @docs/plan/node-crypto-plan/webcrypto-analysis.md)
- 0.1.2 [S] Analyze crypto_hmac.c/h API surface [D:0.1.1]
- 0.1.3 [S] Analyze crypto_symmetric.c/h API surface [D:0.1.1]
- 0.1.4 [S] Analyze crypto_rsa.c/h API surface [D:0.1.1]
- 0.1.5 [S] Analyze crypto_ec.c/h API surface [D:0.1.1]
- 0.1.6 [S] Analyze crypto_kdf.c/h API surface [D:0.1.1]
- 0.1.7 [S] Analyze crypto_subtle.c key management [D:0.1.1]
- 0.1.8 [S] Document API mapping matrix (see @docs/plan/node-crypto-plan/api-mapping.md) [D:0.1.1-0.1.7]

**0.2 Architecture Design** [S][R:LOW][C:SIMPLE][D:0.1] - 7 tasks
- 0.2.1 [S] Design stream API architecture (update/final pattern)
- 0.2.2 [S] Design callback/Promise wrapper pattern
- 0.2.3 [S] Design error handling strategy (Node.js errors vs WebCrypto)
- 0.2.4 [S] Design memory management pattern (JS object lifecycle)
- 0.2.5 [S] Design KeyObject class structure (reuse CryptoKey)
- 0.2.6 [S] Plan file organization structure (when to refactor)
- 0.2.7 [S] Create internal header template (node_crypto_internal.h stub)

### Phase 1: Hash & HMAC Implementation (32 tasks)

**1.1 Hash Class Implementation** [S][R:LOW][C:MEDIUM] - 12 tasks
- 1.1.1 [S] Create JSDgramHash opaque structure (holds digest context)
- 1.1.2 [S] Implement createHash(algorithm, options) factory function
- 1.1.3 [S] Map algorithm names (sha256, sha512, etc.) to WebCrypto enums
- 1.1.4 [S] Initialize digest context (reuse jsrt_get_digest_impl) [D:1.1.3]
- 1.1.5 [S] Implement hash.update(data, inputEncoding) - call digest->update()
- 1.1.6 [S] Handle encoding conversions (utf8, hex, base64, buffer)
- 1.1.7 [S] Implement hash.digest(outputEncoding) - call digest->final()
- 1.1.8 [S] Handle output encoding (hex, base64, buffer) [D:1.1.7]
- 1.1.9 [S] Implement hash.copy() method (clone digest context)
- 1.1.10 [S] Add finalizer for digest context cleanup
- 1.1.11 [S] Add property getters (hash.hashSize, etc.)
- 1.1.12 [S] Export Hash class constructor

**1.2 HMAC Class Implementation** [S][R:LOW][C:MEDIUM][D:1.1] - 13 tasks
- 1.2.1 [S] Create JSDgramHmac opaque structure (holds HMAC context)
- 1.2.2 [S] Implement createHmac(algorithm, key, options) factory
- 1.2.3 [S] Map algorithm names to jsrt_hmac_algorithm_t
- 1.2.4 [S] Handle key input (string, Buffer, TypedArray, KeyObject)
- 1.2.5 [S] Initialize HMAC context (allocate jsrt_hmac_params_t)
- 1.2.6 [S] Implement streaming HMAC update mechanism
- 1.2.7 [S] Implement hmac.update(data, inputEncoding)
- 1.2.8 [S] Handle encoding conversions (same as Hash)
- 1.2.9 [S] Implement hmac.digest(outputEncoding) - call jsrt_crypto_hmac_sign
- 1.2.10 [S] Handle output encoding [D:1.2.9]
- 1.2.11 [S] Add finalizer for HMAC context cleanup
- 1.2.12 [S] Add property getters (hmac.hashSize, etc.)
- 1.2.13 [S] Export Hmac class constructor

**1.3 Testing & Validation** [S][R:LOW][C:SIMPLE][D:1.1,1.2] - 7 tasks
- 1.3.1 [S] Create test/node/crypto/test_crypto_hash.js
- 1.3.2 [S] Test createHash with all algorithms (SHA-256, SHA-384, SHA-512)
- 1.3.3 [S] Test hash.update() with multiple calls (streaming)
- 1.3.4 [S] Test hash.digest() with encodings (hex, base64, buffer)
- 1.3.5 [S] Test createHmac with all algorithms
- 1.3.6 [S] Test hmac.update() and hmac.digest() with encodings
- 1.3.7 [S] Run tests: `make test` - verify no regressions [D:1.3.1-1.3.6]

### Phase 2: Random & Utilities (24 tasks)

**2.1 Random Generation Enhancement** [P][R:LOW][C:SIMPLE] - 10 tasks
- 2.1.1 [S] Enhance randomBytes to support large sizes (remove 64KB limit)
- 2.1.2 [S] Implement randomBytes async (callback-based) properly
- 2.1.3 [S] Implement randomFill(buffer, [offset], [size], [callback])
- 2.1.4 [S] Implement randomFillSync(buffer, [offset], [size])
- 2.1.5 [S] Implement randomInt([min,] max, [callback])
- 2.1.6 [S] Validate randomInt range parameters
- 2.1.7 [S] Implement uniform distribution for randomInt
- 2.1.8 [S] Enhance randomUUID to use crypto.getRandomValues
- 2.1.9 [S] Add options support for randomUUID (disableEntropyCache)
- 2.1.10 [S] Document random API compatibility notes

**2.2 Utility Functions** [P][R:LOW][C:SIMPLE] - 7 tasks
- 2.2.1 [S] Implement timingSafeEqual(a, b) - constant-time comparison
- 2.2.2 [S] Use OpenSSL CRYPTO_memcmp or manual implementation
- 2.2.3 [S] Validate buffer inputs (same length requirement)
- 2.2.4 [S] Implement getRandomValues(typedArray) alias
- 2.2.5 [S] Implement getCiphers() - return supported cipher list
- 2.2.6 [S] Implement getHashes() - return supported hash list
- 2.2.7 [S] Implement getCurves() - return supported curve list

**2.3 Constants Enhancement** [P][R:LOW][C:SIMPLE] - 7 tasks
- 2.3.1 [S] Expand crypto.constants with engine constants
- 2.3.2 [S] Add TLS/SSL version constants (complete set)
- 2.3.3 [S] Add cipher suite constants
- 2.3.4 [S] Add point conversion constants (EC curves)
- 2.3.5 [S] Add default encoding constants
- 2.3.6 [S] Create constants reference documentation
- 2.3.7 [S] Run tests: `make test` - verify constants accessible

### Phase 3: Cipher Operations (34 tasks)

**3.1 Cipher Class Implementation** [S][R:MED][C:COMPLEX] - 14 tasks
- 3.1.1 [S] Create JSNodeCipher opaque structure (holds cipher context)
- 3.1.2 [S] Implement createCipheriv(algorithm, key, iv, options)
- 3.1.3 [S] Parse cipher algorithm (aes-128-cbc, aes-256-gcm, etc.)
- 3.1.4 [S] Map to jsrt_symmetric_algorithm_t and key length
- 3.1.5 [S] Validate key and IV lengths per algorithm
- 3.1.6 [S] Initialize cipher context (allocate jsrt_symmetric_params_t)
- 3.1.7 [S] Implement streaming encryption (buffering mechanism)
- 3.1.8 [S] Implement cipher.update(data, inputEncoding, outputEncoding)
- 3.1.9 [S] Handle block alignment for CBC mode
- 3.1.10 [S] Implement cipher.final(outputEncoding) - flush remaining data
- 3.1.11 [S] For GCM: Implement cipher.setAAD(buffer, options)
- 3.1.12 [S] For GCM: Implement cipher.getAuthTag() - return tag after final
- 3.1.13 [S] Add finalizer for cipher context cleanup
- 3.1.14 [S] Export Cipher class constructor

**3.2 Decipher Class Implementation** [S][R:MED][C:COMPLEX][D:3.1] - 13 tasks
- 3.2.1 [S] Create JSNodeDecipher opaque structure
- 3.2.2 [S] Implement createDecipheriv(algorithm, key, iv, options)
- 3.2.3 [S] Reuse algorithm parsing from Cipher [D:3.1.4]
- 3.2.4 [S] Initialize decipher context (similar to Cipher)
- 3.2.5 [S] Implement streaming decryption (buffering mechanism)
- 3.2.6 [S] Implement decipher.update(data, inputEncoding, outputEncoding)
- 3.2.7 [S] Handle block alignment for CBC mode decryption
- 3.2.8 [S] Implement decipher.final(outputEncoding) - verify padding
- 3.2.9 [S] For GCM: Implement decipher.setAAD(buffer, options)
- 3.2.10 [S] For GCM: Implement decipher.setAuthTag(buffer) - before final
- 3.2.11 [S] Validate auth tag in final() for GCM
- 3.2.12 [S] Add finalizer for decipher context cleanup
- 3.2.13 [S] Export Decipher class constructor

**3.3 Testing & Validation** [S][R:MED][C:MEDIUM][D:3.1,3.2] - 7 tasks
- 3.3.1 [S] Create test/node/crypto/test_crypto_cipher.js
- 3.3.2 [S] Test AES-128-CBC encrypt/decrypt (stream API)
- 3.3.3 [S] Test AES-256-GCM with AAD and auth tag
- 3.3.4 [S] Test encoding conversions (hex, base64, buffer)
- 3.3.5 [S] Test error cases (wrong key length, invalid tag)
- 3.3.6 [S] Test round-trip compatibility (encrypt → decrypt)
- 3.3.7 [S] Run tests: `make test` - verify cipher operations [D:3.3.1-3.3.6]

### Phase 4: Sign/Verify Operations (33 tasks)

**4.1 Sign Class Implementation** [S][R:MED][C:COMPLEX] - 13 tasks
- 4.1.1 [S] Create JSNodeSign opaque structure (holds sign context)
- 4.1.2 [S] Implement createSign(algorithm, options) factory
- 4.1.3 [S] Parse algorithm name (RSA-SHA256, ecdsa-with-SHA256, etc.)
- 4.1.4 [S] Initialize sign context (buffer for streaming data)
- 4.1.5 [S] Implement sign.update(data, inputEncoding) - append to buffer
- 4.1.6 [S] Handle encoding conversions
- 4.1.7 [S] Implement sign.sign(privateKey, outputEncoding)
- 4.1.8 [S] Extract key from KeyObject or parse PEM/DER
- 4.1.9 [S] Determine algorithm (RSA vs ECDSA) from key type
- 4.1.10 [S] Call jsrt_crypto_rsa_sign or jsrt_ec_sign
- 4.1.11 [S] Handle output encoding (buffer, hex, base64)
- 4.1.12 [S] Add finalizer for sign context cleanup
- 4.1.13 [S] Export Sign class constructor

**4.2 Verify Class Implementation** [S][R:MED][C:COMPLEX][D:4.1] - 13 tasks
- 4.2.1 [S] Create JSNodeVerify opaque structure
- 4.2.2 [S] Implement createVerify(algorithm, options) factory
- 4.2.3 [S] Reuse algorithm parsing from Sign [D:4.1.3]
- 4.2.4 [S] Initialize verify context (buffer for streaming data)
- 4.2.5 [S] Implement verify.update(data, inputEncoding)
- 4.2.6 [S] Handle encoding conversions
- 4.2.7 [S] Implement verify.verify(publicKey, signature, signatureEncoding)
- 4.2.8 [S] Extract key from KeyObject or parse PEM/DER
- 4.2.9 [S] Determine algorithm from key type
- 4.2.10 [S] Call jsrt_crypto_rsa_verify or jsrt_ec_verify
- 4.2.11 [S] Return boolean (true/false) verification result
- 4.2.12 [S] Add finalizer for verify context cleanup
- 4.2.13 [S] Export Verify class constructor

**4.3 Testing & Validation** [S][R:MED][C:MEDIUM][D:4.1,4.2] - 7 tasks
- 4.3.1 [S] Create test/node/crypto/test_crypto_sign.js
- 4.3.2 [S] Test RSA-SHA256 sign/verify with PEM keys
- 4.3.3 [S] Test ECDSA-SHA256 sign/verify (P-256 curve)
- 4.3.4 [S] Test streaming API (multiple update calls)
- 4.3.5 [S] Test encoding conversions (signature formats)
- 4.3.6 [S] Test verification failure cases (wrong key, corrupted signature)
- 4.3.7 [S] Run tests: `make test` - verify sign/verify [D:4.3.1-4.3.6]

### Phase 5: Key Management (38 tasks)

**5.1 KeyObject Class Foundation** [S][R:HIGH][C:COMPLEX] - 12 tasks
- 5.1.1 [S] Create JSNodeKeyObject opaque structure (wraps CryptoKey)
- 5.1.2 [S] Define KeyObject type enum (secret, public, private)
- 5.1.3 [S] Implement KeyObject constructor (internal use only)
- 5.1.4 [S] Implement keyObject.type getter (return 'secret'/'public'/'private')
- 5.1.5 [S] Implement keyObject.asymmetricKeyType getter (rsa, ec, etc.)
- 5.1.6 [S] Implement keyObject.asymmetricKeyDetails getter (modulus length, etc.)
- 5.1.7 [S] Implement keyObject.symmetricKeySize getter
- 5.1.8 [S] Implement keyObject.export(options) - format: pem/der/jwk
- 5.1.9 [S] Map to crypto_subtle.c exportKey functionality
- 5.1.10 [S] Handle encoding conversions (PEM string, DER buffer)
- 5.1.11 [S] Add finalizer for KeyObject cleanup
- 5.1.12 [S] Export KeyObject class constructor

**5.2 KeyObject Factory Functions** [S][R:HIGH][C:COMPLEX][D:5.1] - 14 tasks
- 5.2.1 [S] Implement createSecretKey(key, encoding)
- 5.2.2 [S] Validate key input (Buffer, string with encoding)
- 5.2.3 [S] Wrap raw key data in KeyObject (type: secret)
- 5.2.4 [S] Implement createPublicKey(key)
- 5.2.5 [S] Parse key input (KeyObject, PEM string, DER buffer, JWK)
- 5.2.6 [S] Map to crypto_subtle.c importKey (spki format)
- 5.2.7 [S] Wrap imported key in KeyObject (type: public)
- 5.2.8 [S] Implement createPrivateKey(key)
- 5.2.9 [S] Parse key input (KeyObject, PEM string, DER buffer, JWK)
- 5.2.10 [S] Map to crypto_subtle.c importKey (pkcs8 format)
- 5.2.11 [S] Wrap imported key in KeyObject (type: private)
- 5.2.12 [S] Handle passphrase decryption for encrypted PEM keys
- 5.2.13 [S] Implement key format detection (PEM vs DER vs JWK)
- 5.2.14 [S] Error handling for malformed keys

**5.3 Key Generation** [S][R:HIGH][C:COMPLEX][D:5.1,5.2] - 12 tasks
- 5.3.1 [S] Implement generateKeyPairSync(type, options)
- 5.3.2 [S] Support type: 'rsa' - map to jsrt_crypto_generate_rsa_keypair
- 5.3.3 [S] Support type: 'ec' - map to jsrt_ec_generate_key
- 5.3.4 [S] Parse options (modulusLength, publicExponent, namedCurve, etc.)
- 5.3.5 [S] Wrap generated keys in KeyObject (public, private pair)
- 5.3.6 [S] Handle export format options (pem, der, jwk)
- 5.3.7 [S] Implement generateKeyPair(type, options, callback) - async version
- 5.3.8 [S] Run key generation in libuv thread pool
- 5.3.9 [S] Invoke callback with (err, publicKey, privateKey)
- 5.3.10 [S] Implement generateKey(type, options, callback) - symmetric keys
- 5.3.11 [S] Support type: 'hmac', 'aes' - map to WebCrypto generate
- 5.3.12 [S] Run tests: `make test` - verify key generation [D:5.3.1-5.3.11]

### Phase 6: Key Derivation Functions (25 tasks)

**6.1 PBKDF2 Implementation** [P][R:LOW][C:MEDIUM] - 9 tasks
- 6.1.1 [S] Implement pbkdf2Sync(password, salt, iterations, keylen, digest)
- 6.1.2 [S] Validate parameters (iterations > 0, keylen > 0)
- 6.1.3 [S] Map digest name to jsrt_crypto_algorithm_t
- 6.1.4 [S] Call jsrt_crypto_pbkdf2_derive_key (blocking)
- 6.1.5 [S] Return derived key as Buffer
- 6.1.6 [S] Implement pbkdf2(password, salt, iterations, keylen, digest, callback)
- 6.1.7 [S] Run PBKDF2 in libuv thread pool (async work)
- 6.1.8 [S] Invoke callback with (err, derivedKey)
- 6.1.9 [S] Handle encoding conversions for password/salt

**6.2 Scrypt Implementation** [P][R:MED][C:COMPLEX] - 9 tasks
- 6.2.1 [S] Research scrypt OpenSSL 3.x API availability
- 6.2.2 [S] Implement scryptSync(password, salt, keylen, options) if available
- 6.2.3 [S] Validate options (N, r, p, maxmem)
- 6.2.4 [S] Call OpenSSL EVP_PBE_scrypt (blocking)
- 6.2.5 [S] Return derived key as Buffer
- 6.2.6 [S] Implement scrypt(password, salt, keylen, options, callback)
- 6.2.7 [S] Run scrypt in libuv thread pool
- 6.2.8 [S] Invoke callback with (err, derivedKey)
- 6.2.9 [S] OR: Stub with "not implemented" if OpenSSL lacks scrypt

**6.3 HKDF Implementation** [P][R:LOW][C:MEDIUM] - 7 tasks
- 6.3.1 [S] Implement hkdfSync(digest, ikm, salt, info, keylen)
- 6.3.2 [S] Map digest to jsrt_crypto_algorithm_t
- 6.3.3 [S] Call jsrt_crypto_hkdf_derive_key (blocking)
- 6.3.4 [S] Return derived key as Buffer
- 6.3.5 [S] Implement hkdf(digest, ikm, salt, info, keylen, callback)
- 6.3.6 [S] Run HKDF in libuv thread pool
- 6.3.7 [S] Run tests: `make test` - verify KDF functions [D:6.1,6.2,6.3]

### Phase 7: Diffie-Hellman (27 tasks)

**7.1 DiffieHellman Class** [S][R:MED][C:COMPLEX] - 13 tasks
- 7.1.1 [S] Research OpenSSL DH API (DH_new, DH_generate_parameters, etc.)
- 7.1.2 [S] Create JSNodeDH opaque structure (wraps OpenSSL DH*)
- 7.1.3 [S] Implement createDiffieHellman(primeLength, generator)
- 7.1.4 [S] Generate DH parameters using OpenSSL
- 7.1.5 [S] Implement createDiffieHellman(prime, primeEncoding, generator, generatorEncoding)
- 7.1.6 [S] Parse prime and generator from encoding
- 7.1.7 [S] Implement dh.generateKeys() - generate key pair
- 7.1.8 [S] Implement dh.computeSecret(otherPublicKey, inputEncoding, outputEncoding)
- 7.1.9 [S] Implement dh.getPublicKey(encoding) getter
- 7.1.10 [S] Implement dh.getPrivateKey(encoding) getter
- 7.1.11 [S] Implement dh.getPrime(encoding) getter
- 7.1.12 [S] Implement dh.getGenerator(encoding) getter
- 7.1.13 [S] Add finalizer for DH cleanup

**7.2 ECDH Class** [S][R:MED][C:MEDIUM][D:7.1] - 14 tasks
- 7.2.1 [S] Create JSNodeECDH opaque structure (wraps crypto_ec.c context)
- 7.2.2 [S] Implement createECDH(curveName)
- 7.2.3 [S] Validate curve name (prime256v1, secp384r1, secp521r1)
- 7.2.4 [S] Map to jsrt_ec_curve_t
- 7.2.5 [S] Generate EC key pair using jsrt_ec_generate_key
- 7.2.6 [S] Implement ecdh.generateKeys() - generate key pair
- 7.2.7 [S] Implement ecdh.computeSecret(otherPublicKey, inputEncoding, outputEncoding)
- 7.2.8 [S] Map to jsrt_ec_derive_bits (ECDH derivation)
- 7.2.9 [S] Implement ecdh.getPublicKey(encoding, format)
- 7.2.10 [S] Support format: 'compressed', 'uncompressed', 'hybrid'
- 7.2.11 [S] Implement ecdh.getPrivateKey(encoding) getter
- 7.2.12 [S] Implement ecdh.setPrivateKey(privateKey, encoding)
- 7.2.13 [S] Implement ecdh.setPublicKey(publicKey, encoding) - deprecated but support
- 7.2.14 [S] Run tests: `make test` - verify DH/ECDH [D:7.1,7.2]

### Phase 8: Integration & Testing (15 tasks)

**8.1 File Organization Refactoring** [S][R:MED][C:MEDIUM] - 5 tasks
- 8.1.1 [S] Check node_crypto.c line count - if > 500, refactor
- 8.1.2 [S] Create src/node/crypto/ subdirectory structure
- 8.1.3 [S] Split code into modular files (hash, cipher, sign, keys, etc.)
- 8.1.4 [S] Create node_crypto_internal.h with shared utilities
- 8.1.5 [S] Update Makefile to compile all crypto/*.c files

**8.2 Module Export Consolidation** [S][R:LOW][C:SIMPLE][D:8.1] - 5 tasks
- 8.2.1 [S] Review all exported functions (CommonJS exports)
- 8.2.2 [S] Ensure ES module exports match CommonJS
- 8.2.3 [S] Add crypto.webcrypto property (reference global.crypto)
- 8.2.4 [S] Add crypto.subtle alias (reference crypto.webcrypto.subtle)
- 8.2.5 [S] Verify module can be required and imported

**8.3 Comprehensive Testing** [S][R:MED][C:MEDIUM][D:1,2,3,4,5,6,7] - 5 tasks
- 8.3.1 [S] Create test/node/crypto/test_crypto_compat.js (cross-API tests)
- 8.3.2 [S] Test Hash → HMAC → Cipher integration
- 8.3.3 [S] Test KeyObject with Sign/Verify
- 8.3.4 [S] Test PBKDF2 with Cipher (derived key usage)
- 8.3.5 [S] Run full test suite: `make format && make test && make wpt`

---

## 📊 Task Execution Tracker

### Phase 0: Research & Architecture (15 tasks)
| ID | Task | Status | Deps | Risk | Complexity |
|----|------|--------|------|------|------------|
| 0.1.1 | Analyze crypto_digest.c/h API | ⏳ | None | LOW | SIMPLE |
| 0.1.2 | Analyze crypto_hmac.c/h API | ⏳ | 0.1.1 | LOW | SIMPLE |
| 0.1.3 | Analyze crypto_symmetric.c/h API | ⏳ | 0.1.1 | LOW | SIMPLE |
| 0.1.4 | Analyze crypto_rsa.c/h API | ⏳ | 0.1.1 | LOW | SIMPLE |
| 0.1.5 | Analyze crypto_ec.c/h API | ⏳ | 0.1.1 | LOW | SIMPLE |
| 0.1.6 | Analyze crypto_kdf.c/h API | ⏳ | 0.1.1 | LOW | SIMPLE |
| 0.1.7 | Analyze crypto_subtle.c keys | ⏳ | 0.1.1 | LOW | SIMPLE |
| 0.1.8 | Document API mapping matrix | ⏳ | 0.1.1-7 | LOW | SIMPLE |
| 0.2.1 | Design stream API architecture | ⏳ | 0.1 | LOW | SIMPLE |
| 0.2.2 | Design callback/Promise pattern | ⏳ | 0.1 | LOW | SIMPLE |
| 0.2.3 | Design error handling strategy | ⏳ | 0.1 | LOW | SIMPLE |
| 0.2.4 | Design memory management | ⏳ | 0.1 | LOW | SIMPLE |
| 0.2.5 | Design KeyObject structure | ⏳ | 0.1 | LOW | SIMPLE |
| 0.2.6 | Plan file organization | ⏳ | 0.1 | LOW | SIMPLE |
| 0.2.7 | Create internal header stub | ⏳ | 0.1 | LOW | SIMPLE |

### Phase 1: Hash & HMAC (32 tasks)
| ID | Task | Status | Deps | Risk | Complexity |
|----|------|--------|------|------|------------|
| 1.1.1 | Create JSDgramHash structure | ⏳ | 0 | LOW | MEDIUM |
| 1.1.2 | Implement createHash factory | ⏳ | 0 | LOW | MEDIUM |
| ... | (30 more tasks - see details above) | ⏳ | ... | ... | ... |

### Phase 2: Random & Utilities (24 tasks)
*See detailed breakdown above*

### Phase 3: Cipher Operations (34 tasks)
*See detailed breakdown above*

### Phase 4: Sign/Verify (33 tasks)
*See detailed breakdown above*

### Phase 5: Key Management (38 tasks)
*See detailed breakdown above*

### Phase 6: Key Derivation (25 tasks)
*See detailed breakdown above*

### Phase 7: Diffie-Hellman (27 tasks)
*See detailed breakdown above*

### Phase 8: Integration & Testing (15 tasks)
*See detailed breakdown above*

---

## 🚀 Execution Dashboard

### Current Status
- **Current Phase**: Phase 0 - Research & Architecture Setup
- **Progress**: 0/168 tasks (0%)
- **Active Task**: Not started
- **Next Tasks**: 0.1.1 Analyze crypto_digest.c/h API surface

### Phase Progress Summary
| Phase | Tasks | Completed | Status | Blocking Issues |
|-------|-------|-----------|--------|-----------------|
| Phase 0 | 15 | 0 | ⏳ PENDING | None |
| Phase 1 | 32 | 0 | ⏳ PENDING | Waiting on Phase 0 |
| Phase 2 | 24 | 0 | ⏳ PENDING | Can start after Phase 0 |
| Phase 3 | 34 | 0 | ⏳ PENDING | Waiting on Phase 1 |
| Phase 4 | 33 | 0 | ⏳ PENDING | Waiting on Phase 1,3 |
| Phase 5 | 38 | 0 | ⏳ PENDING | Waiting on Phase 1,3,4 |
| Phase 6 | 25 | 0 | ⏳ PENDING | Can start after Phase 1 |
| Phase 7 | 27 | 0 | ⏳ PENDING | Waiting on Phase 5 |
| Phase 8 | 15 | 0 | ⏳ PENDING | Waiting on all phases |

### Parallel Execution Opportunities
**Phases that can run in parallel** (after Phase 0 completes):
- Phase 1 (Hash/HMAC) [S] - Sequential foundation
- Phase 2 (Random/Utilities) [P] - Can run parallel to Phase 1
- Phase 6 (KDF) [P] - Can run parallel after Phase 1 completes

**Phases that must run sequentially**:
- Phase 0 → Phase 1 (foundation required)
- Phase 1 → Phase 3 (hash required for cipher validation)
- Phase 3 → Phase 4 (cipher context pattern reuse)
- Phase 4 → Phase 5 (sign/verify requires key management)
- Phase 5 → Phase 7 (DH requires KeyObject)

---

## 📜 History Log

| Timestamp | Action | Details |
|-----------|--------|---------|
| 2025-10-09T12:30:00Z | Plan created | Initial task breakdown completed |
| 2025-10-09T12:30:00Z | Status | 🟡 PLANNING - Ready to begin Phase 0 |

---

## 🗺️ API Mapping Reference

### WebCrypto → Node.js API Mapping

**crypto_digest.c/h → Node.js Hash API**
```
jsrt_get_digest_impl() → createHash() backend
digest->init() → Hash constructor
digest->update() → hash.update()
digest->final() → hash.digest()
```

**crypto_hmac.c/h → Node.js HMAC API**
```
jsrt_crypto_hmac_sign() → createHmac() + hmac.digest() backend
jsrt_hmac_params_t → Internal HMAC context
```

**crypto_symmetric.c/h → Node.js Cipher API**
```
jsrt_crypto_aes_encrypt() → createCipheriv() + cipher.final() backend
jsrt_crypto_aes_decrypt() → createDecipheriv() + decipher.final() backend
jsrt_symmetric_params_t → Internal cipher context
```

**crypto_rsa.c/h + crypto_ec.c/h → Node.js Sign/Verify API**
```
jsrt_crypto_rsa_sign() → RSA sign.sign() backend
jsrt_crypto_rsa_verify() → RSA verify.verify() backend
jsrt_ec_sign() → ECDSA sign.sign() backend
jsrt_ec_verify() → ECDSA verify.verify() backend
```

**crypto_kdf.c/h → Node.js KDF API**
```
jsrt_crypto_pbkdf2_derive_key() → pbkdf2/pbkdf2Sync backend
jsrt_crypto_hkdf_derive_key() → hkdf/hkdfSync backend
```

**crypto_subtle.c → Node.js KeyObject API**
```
crypto.subtle.importKey() → createPublicKey/createPrivateKey backend
crypto.subtle.exportKey() → keyObject.export() backend
crypto.subtle.generateKey() → generateKeyPair backend
```

### Complete API Coverage Matrix

**Implemented (3/85+)**:
- ✅ crypto.randomBytes(size, [callback])
- ✅ crypto.randomUUID([options])
- ✅ crypto.constants

**Planned (82+)**:

*Phase 1 (Hash & HMAC - 6 methods)*:
- ⏳ crypto.createHash(algorithm, [options])
- ⏳ hash.update(data, [inputEncoding])
- ⏳ hash.digest([encoding])
- ⏳ crypto.createHmac(algorithm, key, [options])
- ⏳ hmac.update(data, [inputEncoding])
- ⏳ hmac.digest([encoding])

*Phase 2 (Random & Utilities - 10 methods)*:
- ⏳ crypto.randomFill(buffer, [offset], [size], [callback])
- ⏳ crypto.randomFillSync(buffer, [offset], [size])
- ⏳ crypto.randomInt([min,] max, [callback])
- ⏳ crypto.timingSafeEqual(a, b)
- ⏳ crypto.getRandomValues(typedArray)
- ⏳ crypto.getCiphers()
- ⏳ crypto.getHashes()
- ⏳ crypto.getCurves()
- ⏳ (Enhanced randomBytes)
- ⏳ (Enhanced randomUUID)

*Phase 3 (Cipher - 8 methods)*:
- ⏳ crypto.createCipheriv(algorithm, key, iv, [options])
- ⏳ cipher.update(data, [inputEncoding], [outputEncoding])
- ⏳ cipher.final([outputEncoding])
- ⏳ cipher.setAAD(buffer, [options]) - GCM only
- ⏳ cipher.getAuthTag() - GCM only
- ⏳ crypto.createDecipheriv(algorithm, key, iv, [options])
- ⏳ decipher.update(data, [inputEncoding], [outputEncoding])
- ⏳ decipher.final([outputEncoding])

*Phase 4 (Sign/Verify - 6 methods)*:
- ⏳ crypto.createSign(algorithm, [options])
- ⏳ sign.update(data, [inputEncoding])
- ⏳ sign.sign(privateKey, [outputEncoding])
- ⏳ crypto.createVerify(algorithm, [options])
- ⏳ verify.update(data, [inputEncoding])
- ⏳ verify.verify(object, signature, [signatureEncoding])

*Phase 5 (KeyObject - 18 methods)*:
- ⏳ crypto.createPublicKey(key)
- ⏳ crypto.createPrivateKey(key)
- ⏳ crypto.createSecretKey(key, [encoding])
- ⏳ crypto.generateKeyPair(type, options, callback)
- ⏳ crypto.generateKeyPairSync(type, options)
- ⏳ crypto.generateKey(type, options, callback)
- ⏳ crypto.generateKeySync(type, options)
- ⏳ keyObject.type (getter)
- ⏳ keyObject.asymmetricKeyType (getter)
- ⏳ keyObject.asymmetricKeyDetails (getter)
- ⏳ keyObject.symmetricKeySize (getter)
- ⏳ keyObject.export(options)
- ⏳ (Plus 6 more KeyObject utility methods)

*Phase 6 (KDF - 6 methods)*:
- ⏳ crypto.pbkdf2(password, salt, iterations, keylen, digest, callback)
- ⏳ crypto.pbkdf2Sync(password, salt, iterations, keylen, digest)
- ⏳ crypto.scrypt(password, salt, keylen, [options], callback)
- ⏳ crypto.scryptSync(password, salt, keylen, [options])
- ⏳ crypto.hkdf(digest, ikm, salt, info, keylen, callback)
- ⏳ crypto.hkdfSync(digest, ikm, salt, info, keylen)

*Phase 7 (DH - 20+ methods)*:
- ⏳ crypto.createDiffieHellman(primeLength, [generator])
- ⏳ crypto.createDiffieHellman(prime, [primeEncoding], [generator], [generatorEncoding])
- ⏳ crypto.createDiffieHellmanGroup(name)
- ⏳ crypto.createECDH(curveName)
- ⏳ (Plus 16+ DH/ECDH instance methods)

---

## 🎯 Success Metrics & Validation

### Completion Criteria (ALL MUST PASS)

**Functional Requirements**:
- ✅ 85+ Node.js crypto APIs implemented
- ✅ Stream-based APIs (Hash, Hmac, Cipher, Decipher, Sign, Verify) fully functional
- ✅ KeyObject class with complete import/export support
- ✅ All encoding conversions working (hex, base64, buffer, pem, der, jwk)
- ✅ Both sync and async variants implemented (where applicable)
- ✅ Both callback and Promise APIs supported (where applicable)

**Code Quality Requirements**:
- ✅ `make format` - Clean (no formatting issues)
- ✅ `make test` - 100% pass rate (no failures)
- ✅ `make wpt` - No regressions (failures ≤ baseline)
- ✅ `make jsrt_m` - ASAN clean (no memory leaks)
- ✅ Code coverage ≥ 80% per module
- ✅ Maximum code reuse from WebCrypto (target: 90%+)

**Documentation Requirements**:
- ✅ API compatibility notes documented
- ✅ Known limitations documented
- ✅ Usage examples for each major API category
- ✅ Migration guide from WebCrypto to Node.js crypto
- ✅ Performance benchmarks completed

### Risk Mitigation Strategies

**High-Risk Areas**:

1. **Phase 5: KeyObject Class** [R:HIGH]
   - *Risk*: Complex key format handling (PEM, DER, JWK, passphrase decryption)
   - *Mitigation*: Incremental implementation, start with raw formats first
   - *Fallback*: Implement subset of formats, document unsupported cases

2. **Phase 7: Diffie-Hellman** [R:MED]
   - *Risk*: OpenSSL DH API may vary across versions
   - *Mitigation*: Runtime version detection, conditional compilation
   - *Fallback*: Implement ECDH only (reuses crypto_ec.c), defer classic DH

3. **Phase 3/4: Stream API Design** [R:MED]
   - *Risk*: Complex buffering and state management
   - *Mitigation*: Study Node.js crypto source code, reuse patterns
   - *Fallback*: Implement non-streaming variants first, add streaming later

**Rollback Strategy**:
- Each phase is independently testable
- Failed phase does not block other phases (except dependencies)
- Can ship partial implementation (e.g., skip scrypt if OpenSSL lacks support)
- Document unsupported APIs clearly

---

## 📚 Implementation Notes

### Memory Management Patterns

**Pattern 1: Stream Object Lifecycle**
```c
// Hash/Hmac/Cipher/Decipher/Sign/Verify follow this pattern:
typedef struct {
  JSContext* ctx;
  void* native_context;  // OpenSSL EVP_MD_CTX*, HMAC_CTX*, etc.
  uint8_t* buffer;       // Accumulation buffer for streaming
  size_t buffer_len;
  bool finalized;        // Prevent double-finalization
} JSNodeStreamContext;

// Finalizer called by GC
static void stream_finalizer(JSRuntime* rt, JSValue val) {
  JSNodeStreamContext* ctx = JS_GetOpaque(val, class_id);
  if (ctx) {
    if (ctx->native_context) {
      // Free OpenSSL context
    }
    if (ctx->buffer) {
      js_free(ctx->ctx, ctx->buffer);
    }
    js_free(ctx->ctx, ctx);
  }
}
```

**Pattern 2: Async Work with libuv**
```c
// For async APIs (pbkdf2, scrypt, generateKeyPair, etc.)
typedef struct {
  uv_work_t work;
  JSContext* ctx;
  JSValue callback;
  // Input parameters
  // Output results
  int error_code;
} JSNodeCryptoWork;

// Work function (runs in thread pool)
static void crypto_work_cb(uv_work_t* req) {
  JSNodeCryptoWork* work = req->data;
  // Call blocking WebCrypto function
  work->error_code = jsrt_crypto_xxx(...);
}

// After function (runs in main thread)
static void crypto_after_cb(uv_work_t* req, int status) {
  JSNodeCryptoWork* work = req->data;
  // Invoke JavaScript callback with results
  // Free work structure
}
```

### Error Handling Strategy

**Node.js Error Codes to WebCrypto Error Mapping**:
```
Node.js ERR_CRYPTO_INVALID_DIGEST → WebCrypto algorithm validation
Node.js ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE → KeyObject type mismatch
Node.js ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH → Buffer length check
Node.js ERR_OSSL_* → OpenSSL error codes (preserve original)
```

**Error Propagation**:
- Sync APIs: Throw JavaScript exception immediately
- Async APIs (callback): Invoke callback(err, null)
- Async APIs (Promise): Reject promise with error object

### Encoding Conversion Utilities

**Shared utility functions** (in node_crypto_internal.h):
```c
// Convert Node.js encoding name to implementation
typedef enum {
  NODE_ENC_BUFFER,  // Default: Buffer/Uint8Array
  NODE_ENC_HEX,
  NODE_ENC_BASE64,
  NODE_ENC_BASE64URL,
  NODE_ENC_UTF8,
  NODE_ENC_ASCII,
  NODE_ENC_LATIN1,
  NODE_ENC_BINARY,  // Alias for latin1
} node_encoding_t;

node_encoding_t parse_encoding(const char* enc_str);
JSValue encode_output(JSContext* ctx, uint8_t* data, size_t len, node_encoding_t enc);
int decode_input(JSContext* ctx, JSValue val, node_encoding_t enc, uint8_t** out, size_t* out_len);
```

---

## 🔍 Testing Plan Details

### Test Coverage Requirements

**Per-Phase Test Files**:
- Phase 1: `test/node/crypto/test_crypto_hash.js` (~200 lines)
  - All hash algorithms (SHA-256, SHA-384, SHA-512)
  - All HMAC algorithms
  - Streaming (multiple updates)
  - Encoding conversions
  - Edge cases (empty input, large input)

- Phase 2: `test/node/crypto/test_crypto_random.js` (~150 lines)
  - randomBytes (various sizes)
  - randomFill (buffers, offsets)
  - randomInt (ranges, distribution)
  - randomUUID (format validation)
  - timingSafeEqual (positive, negative, errors)

- Phase 3: `test/node/crypto/test_crypto_cipher.js` (~250 lines)
  - AES-128/192/256-CBC (encrypt/decrypt round-trip)
  - AES-128/192/256-GCM (with AAD and auth tag)
  - AES-128/192/256-CTR (counter mode)
  - Encoding conversions
  - Error cases (wrong key length, invalid tag)

- Phase 4: `test/node/crypto/test_crypto_sign.js` (~200 lines)
  - RSA-SHA256/384/512 (sign/verify)
  - ECDSA-SHA256/384/512 (P-256, P-384, P-521)
  - Streaming (multiple updates)
  - PEM/DER key formats
  - Verification failure cases

- Phase 5: `test/node/crypto/test_crypto_keys.js` (~300 lines)
  - createSecretKey (various inputs)
  - createPublicKey (PEM, DER, JWK)
  - createPrivateKey (PEM, DER, JWK, encrypted PEM)
  - generateKeyPairSync (RSA, EC)
  - generateKeyPair async (callback)
  - KeyObject.export (all formats)
  - KeyObject getters (type, asymmetricKeyType, etc.)

- Phase 6: `test/node/crypto/test_crypto_kdf.js` (~150 lines)
  - pbkdf2Sync (various iterations, key lengths)
  - pbkdf2 async (callback)
  - scryptSync (if available)
  - scrypt async (if available)
  - hkdfSync
  - hkdf async

- Phase 7: `test/node/crypto/test_crypto_dh.js` (~200 lines)
  - createDiffieHellman (generate parameters)
  - createDiffieHellman (fixed prime)
  - DH key exchange (Alice/Bob scenario)
  - createECDH (all curves)
  - ECDH key exchange
  - Public/private key getters

- Phase 8: `test/node/crypto/test_crypto_compat.js` (~200 lines)
  - Hash → HMAC key → HMAC
  - PBKDF2 → Cipher key → Cipher
  - generateKeyPair → Sign → Verify
  - ECDH → derive key → Cipher
  - Node.js crypto interop tests (if possible)

### WPT Compliance

**Web Platform Tests - Crypto API**:
- Ensure no regressions in existing WPT crypto tests
- Node.js crypto is separate from WebCrypto, should not affect WPT
- Validate crypto.webcrypto alias points to existing implementation

### Performance Benchmarks

**Benchmark Suite** (optional, Phase 8):
```javascript
// target/tmp/bench_crypto.js
const crypto = require('node:crypto');
const iterations = 10000;

// Hash performance
console.time('Hash SHA-256 x10000');
for (let i = 0; i < iterations; i++) {
  crypto.createHash('sha256').update('test data').digest();
}
console.timeEnd('Hash SHA-256 x10000');

// HMAC performance
// Cipher performance
// Sign/Verify performance
// Key generation performance
```

---

## 🔄 Iterative Development Process

### Development Cycle (Per Phase)

**Step 1: Implementation** (~60-70% of phase time)
1. Write C code in src/node/node_crypto.c (or subdirectory)
2. Follow existing WebCrypto patterns
3. Implement minimal viable API first
4. Add error handling
5. Run `make format`

**Step 2: Unit Testing** (~20-25% of phase time)
1. Create/update test file in test/node/crypto/
2. Write positive test cases (happy path)
3. Write negative test cases (error handling)
4. Run `make test` - verify new tests pass
5. Run `make wpt` - verify no regressions

**Step 3: Validation** (~10-15% of phase time)
1. Run ASAN build: `make jsrt_m`
2. Run tests with ASAN: `./target/debug/jsrt_m test/node/crypto/test_crypto_xxx.js`
3. Fix any memory leaks detected
4. Update plan document with completion status
5. Commit changes with descriptive message

### Incremental Approach

**Start Simple, Add Complexity**:
- Phase 1: Implement Hash first (simpler), then HMAC (reuses Hash patterns)
- Phase 3: Implement Cipher with CBC first (simpler), then GCM (AAD/tag)
- Phase 5: Implement createSecretKey first (simple wrapper), then createPublicKey (PEM parsing)

**Defer Complex Features**:
- Encrypted PEM key decryption (Phase 5) - implement after basic PEM works
- Scrypt (Phase 6) - skip if OpenSSL version lacks support
- Classic DH (Phase 7) - implement ECDH first (reuses crypto_ec.c)

---

## 📖 Documentation Deliverables

### Required Documentation (Phase 8)

**1. API Compatibility Matrix** (docs/node-crypto-compatibility.md)
- List all 85+ Node.js crypto APIs
- Mark each as: ✅ Fully supported | ⚠️ Partial | ❌ Not supported
- Document differences from Node.js (if any)
- List unsupported features with rationale

**2. Migration Guide** (docs/node-crypto-migration.md)
- How to use Node.js crypto instead of WebCrypto
- Code examples for common scenarios
- Performance comparison (if benchmarks completed)

**3. Developer Guide** (docs/node-crypto-development.md)
- How to add new crypto APIs
- WebCrypto reuse patterns
- Memory management guidelines
- Testing requirements

**4. Known Limitations** (in main plan document)
- OpenSSL version-specific features
- Unsupported algorithms
- Performance characteristics
- Platform-specific notes

---

## ✅ Definition of Done

### Phase Completion Checklist

**For EACH phase, ALL items must be checked**:
- ✅ All tasks in phase completed
- ✅ Code written and formatted (`make format`)
- ✅ Tests written and passing (`make test`)
- ✅ No WPT regressions (`make wpt`)
- ✅ ASAN clean (`make jsrt_m` + test run)
- ✅ Plan document updated (status, progress %)
- ✅ Code committed with descriptive message
- ✅ Ready to proceed to next phase

### Overall Project Completion Checklist

- ✅ All 8 phases completed
- ✅ File organization refactored (if needed)
- ✅ All 85+ APIs implemented (or documented as unsupported)
- ✅ Comprehensive test suite passing
- ✅ Documentation complete (compatibility, migration, development guides)
- ✅ Performance benchmarks completed (optional)
- ✅ Final validation: `make clean && make format && make test && make wpt`
- ✅ Final ASAN check: `make jsrt_m` + full test suite
- ✅ Plan document marked as ✅ COMPLETED

---

## 📎 Appendix

### Sub-Documents (Created on Demand)

When main plan approaches 1500 lines, create sub-documents:

**@docs/plan/node-crypto-plan/webcrypto-analysis.md**
- Detailed analysis of each WebCrypto component
- Function signatures and parameters
- Reusability assessment

**@docs/plan/node-crypto-plan/api-mapping.md**
- Complete mapping table (Node.js API → WebCrypto function)
- Implementation notes per API
- Code examples

**@docs/plan/node-crypto-plan/implementation-details.md**
- Detailed implementation notes per phase
- Code snippets and patterns
- Troubleshooting notes

### External References

**Node.js Official Documentation**:
- https://nodejs.org/docs/latest/api/crypto.html

**OpenSSL Documentation**:
- https://www.openssl.org/docs/man3.0/

**jsrt Internal References**:
- `src/crypto/` - WebCrypto implementation
- `src/node/node_modules.c` - Module registration pattern
- `.claude/CLAUDE.md` - Development guidelines
- `.claude/docs/testing.md` - Testing best practices

---

**Plan Status**: 🟡 PLANNING - Ready to begin implementation
**Next Action**: Start Phase 0, Task 0.1.1 - Analyze crypto_digest.c/h API surface
**Estimated Total Duration**: ~15-20 hours (with task-breakdown agent parallel execution)
