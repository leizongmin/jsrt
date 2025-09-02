# jsrt WebCrypto API 完整实现计划

## 项目概述

本文档详细规划jsrt JavaScript运行时对完整WebCrypto API的支持实现。目标是提供符合W3C Web Cryptography API规范的加密功能，同时保持可选的OpenSSL依赖和优雅的降级机制。

## 当前实现状态

### ✅ 已完成的基础功能

#### 1. 基础随机数生成
- **已实现**：`crypto.getRandomValues(typedArray)`
- **已实现**：`crypto.randomUUID()`
- **支持平台**：Windows、macOS、Linux
- **OpenSSL支持**：动态加载，支持多版本检测
- **降级机制**：系统随机数生成器备用方案
- **位置**：`src/std/crypto.c`, `src/std/crypto.h`

#### 2. SubtleCrypto基础架构
- **已实现**：`crypto.subtle` 对象和完整方法框架
- **已实现**：Promise-based异步API架构
- **已实现**：WebCrypto标准错误处理机制
- **已实现**：算法解析和验证系统
- **已实现**：TypedArray和ArrayBuffer输入处理
- **位置**：`src/std/crypto_subtle.c`, `src/std/crypto_subtle.h`

#### 3. 消息摘要算法 (完整实现)
- **已实现**：`crypto.subtle.digest(algorithm, data)`
- **支持算法**：SHA-1, SHA-256, SHA-384, SHA-512
- **已实现**：OpenSSL EVP API集成
- **已实现**：完整的Promise包装和错误处理
- **已实现**：测试向量验证
- **位置**：`src/std/crypto_digest.c`, `src/std/crypto_digest.h`

#### 4. 对称加密算法 (完整实现)
- **已实现**：`crypto.subtle.encrypt(algorithm, key, data)` 和 `crypto.subtle.decrypt(algorithm, key, data)`
- **支持算法**：AES-CBC (128/256位), AES-GCM (128/256位)
- **已实现**：IV/随机数处理，认证标签管理
- **已实现**：Additional Authenticated Data (AAD) 支持
- **已实现**：密钥生成 `crypto.subtle.generateKey()`
- **位置**：`src/std/crypto_symmetric.c`, `src/std/crypto_symmetric.h`

#### 5. 消息认证码 (完整实现)
- **已实现**：`crypto.subtle.sign(algorithm, key, data)` 和 `crypto.subtle.verify(algorithm, key, signature, data)`
- **支持算法**：HMAC-SHA-1, HMAC-SHA-256, HMAC-SHA-384, HMAC-SHA-512
- **已实现**：常数时间签名验证 (防时间攻击)
- **已实现**：HMAC密钥生成
- **位置**：`src/std/crypto_hmac.c`, `src/std/crypto_hmac.h`

#### 6. 运行时集成
- **已实现**：`process.versions.openssl` - 显示OpenSSL版本信息  
- **已实现**：进程对象全局注册
- **已实现**：完整的单元测试覆盖
- **已实现**：构建系统集成

## 📋 完整WebCrypto API实现规划

### 第一阶段：核心加密算法 (优先级：高)

#### 1.1 SubtleCrypto 接口基础架构
**目标**：`crypto.subtle` 对象和基础方法框架

**需要实现**：
- `crypto.subtle` 对象创建
- 基础的 Promise-based API 架构
- 错误处理和异常机制
- CryptoKey 对象模型

**技术实现**：
```c
typedef struct {
  JSValue promise_resolve;
  JSValue promise_reject; 
  // 异步操作状态
} CryptoOperation;

typedef struct {
  char* algorithm;
  char* type;  // "public", "private", "secret"
  bool extractable;
  JSValue usages;  // array of strings
  void* key_data;  // OpenSSL key structure
} CryptoKey;
```

**API接口**：
- `crypto.subtle.encrypt(algorithm, key, data)`
- `crypto.subtle.decrypt(algorithm, key, data)`
- `crypto.subtle.sign(algorithm, key, data)`
- `crypto.subtle.verify(algorithm, key, signature, data)`
- `crypto.subtle.digest(algorithm, data)`
- `crypto.subtle.generateKey(algorithm, extractable, keyUsages)`
- `crypto.subtle.importKey(format, keyData, algorithm, extractable, keyUsages)`
- `crypto.subtle.exportKey(format, key)`

**实现复杂度**：🔴 困难  
**预估工作量**：3-4 天  
**依赖**：OpenSSL EVP API, libuv事件循环集成

#### 1.2 消息摘要算法
**目标**：实现常用的哈希算法

**需要实现**：
- SHA-1 (已弃用，仅为兼容性)
- SHA-256
- SHA-384  
- SHA-512

**OpenSSL映射**：
- `EVP_sha1()`, `EVP_sha256()`, `EVP_sha384()`, `EVP_sha512()`

**API示例**：
```javascript
const hash = await crypto.subtle.digest('SHA-256', data);
```

**实现复杂度**：🟡 中等  
**预估工作量**：1-2 天

#### 1.3 对称加密算法
**目标**：实现AES系列对称加密

**需要实现**：
- AES-CBC (128, 192, 256位密钥)
- AES-GCM (128, 192, 256位密钥) 
- AES-CTR (128, 192, 256位密钥)

**OpenSSL映射**：
- `EVP_aes_128_cbc()`, `EVP_aes_256_cbc()`
- `EVP_aes_128_gcm()`, `EVP_aes_256_gcm()`
- `EVP_aes_128_ctr()`, `EVP_aes_256_ctr()`

**技术挑战**：
- IV/Nonce 管理
- GCM模式的认证标签处理
- 填充模式处理

**API示例**：
```javascript
const key = await crypto.subtle.generateKey(
  {name: "AES-GCM", length: 256},
  true, 
  ["encrypt", "decrypt"]
);
const encrypted = await crypto.subtle.encrypt(
  {name: "AES-GCM", iv: iv}, 
  key, 
  data
);
```

**实现复杂度**：🔴 困难  
**预估工作量**：4-5 天

### 第二阶段：非对称加密算法 (优先级：高)

#### 2.1 RSA算法族 - ✅ 核心功能完成
**目标**：实现RSA加密和签名

**已完成功能** ✅：
- ✅ RSA-OAEP 密钥对生成 (1024-4096位)
- ✅ RSA算法基础架构和OpenSSL集成
- ✅ WebCrypto API集成 (generateKey/encrypt/decrypt方法)
- ✅ 哈希算法支持 (SHA-256/384/512，排除SHA-1)
- ✅ RSA-OAEP 加密/解密核心实现
- ✅ 测试框架兼容性修复 (async/await → Promise chains)
- ✅ 内存管理和EVP_PKEY生命周期管理

**关键技术突破** 🔧：
- 解决了QuickJS运行时async/await兼容性问题
- 实现了OpenSSL EVP_PKEY的DER序列化/反序列化
- 修复了segmentation fault和测试基础设施问题
- 完成了RSA密钥在WebCrypto对象中的存储和检索机制

**待完成功能**：
- 🚧 RSA-OAEP 加密/解密操作完整测试验证
- 🚧 RSA-PKCS1-v1_5 (加密/解密)  
- 🚧 RSASSA-PKCS1-v1_5 (签名/验证)
- 🚧 RSA-PSS (签名/验证)

**密钥长度支持**：1024, 2048, 3072, 4096位 ✅

**技术实现** ✅：
- **文件**: `src/std/crypto_rsa.h`, `src/std/crypto_rsa.c`
- **OpenSSL映射**:
  - `EVP_PKEY_RSA` ✅
  - `EVP_PKEY_CTX_new_id()`, `EVP_PKEY_keygen()` ✅
  - `EVP_PKEY_encrypt()`, `EVP_PKEY_decrypt()` (基础架构已完成)
  - `EVP_PKEY_sign()`, `EVP_PKEY_verify()` (基础架构已完成)

**成功测试** ✅：
```javascript
const keyPair = await crypto.subtle.generateKey(
  {
    name: "RSA-OAEP",
    modulusLength: 2048,
    publicExponent: new Uint8Array([1, 0, 1]),
    hash: "SHA-256"
  },
  true,
  ["encrypt", "decrypt"]
);
// ✅ 测试通过：生成2048位RSA-OAEP密钥对成功
```

**关键突破** 🎉：
- 解决OpenSSL控制常量问题：`EVP_PKEY_CTRL_RSA_KEYGEN_BITS = (EVP_PKEY_ALG_CTRL + 3)`
- 实现灵活的哈希算法解析 (支持字符串和对象格式)
- 完整的动态库加载和内存管理

**实际工作量**：2天 (预估5-7天)  
**实现复杂度**：🔴 非常困难 → ✅ 已突破核心难点

#### 2.2 椭圆曲线算法 (ECDSA/ECDH)
**目标**：实现椭圆曲线加密和签名

**需要实现**：
- ECDSA (签名/验证)
- ECDH (密钥交换)

**支持曲线**：
- P-256 (secp256r1)
- P-384 (secp384r1)  
- P-521 (secp521r1)

**OpenSSL映射**：
- `EVP_PKEY_EC`
- `NID_X9_62_prime256v1`, `NID_secp384r1`, `NID_secp521r1`

**API示例**：
```javascript
const keyPair = await crypto.subtle.generateKey(
  {name: "ECDSA", namedCurve: "P-256"},
  true,
  ["sign", "verify"]
);
```

**实现复杂度**：🔴 非常困难  
**预估工作量**：4-6 天

### 第三阶段：密钥管理和派生 (优先级：中)

#### 3.1 密钥导入/导出
**目标**：支持多种密钥格式

**需要实现**：
- **格式支持**：
  - "raw" - 原始字节
  - "pkcs8" - PKCS#8私钥格式
  - "spki" - SubjectPublicKeyInfo公钥格式  
  - "jwk" - JSON Web Key格式

**技术实现**：
- PEM/DER格式解析
- JWK JSON处理
- OpenSSL格式转换

**API示例**：
```javascript
const key = await crypto.subtle.importKey(
  "jwk",
  jwkKey,
  {name: "RSA-OAEP", hash: "SHA-256"},
  true,
  ["encrypt"]
);
```

**实现复杂度**：🔴 困难  
**预估工作量**：3-4 天

#### 3.2 密钥派生函数 (KDF)
**目标**：实现密钥派生算法

**需要实现**：
- PBKDF2 (Password-Based Key Derivation Function 2)
- HKDF (HMAC-based Key Derivation Function)

**OpenSSL映射**：
- `PKCS5_PBKDF2_HMAC()`
- `EVP_PKEY_derive()` for HKDF

**API示例**：
```javascript
const derivedKey = await crypto.subtle.deriveKey(
  {
    name: "PBKDF2",
    salt: salt,
    iterations: 100000,
    hash: "SHA-256"
  },
  baseKey,
  {name: "AES-GCM", length: 256},
  false,
  ["encrypt", "decrypt"]
);
```

**实现复杂度**：🟡 中等  
**预估工作量**：2-3 天

### 第四阶段：消息认证码 (优先级：中)

#### 4.1 HMAC算法
**目标**：实现HMAC消息认证码

**需要实现**：
- HMAC-SHA-1
- HMAC-SHA-256
- HMAC-SHA-384
- HMAC-SHA-512

**OpenSSL映射**：
- `HMAC()` function
- `EVP_sha1()`, `EVP_sha256()`, etc.

**API示例**：
```javascript
const key = await crypto.subtle.generateKey(
  {name: "HMAC", hash: "SHA-256"},
  true,
  ["sign", "verify"]
);
const signature = await crypto.subtle.sign("HMAC", key, data);
```

**实现复杂度**：🟡 中等  
**预估工作量**：1-2 天

### 第五阶段：高级功能和优化 (优先级：低)

#### 5.1 Ed25519签名算法
**目标**：实现现代椭圆曲线签名

**需要实现**：
- Ed25519签名和验证
- X25519密钥交换 (可选)

**OpenSSL版本要求**：OpenSSL 1.1.1+

**实现复杂度**：🔴 困难  
**预估工作量**：3-4 天

#### 5.2 性能优化
**目标**：提升加密操作性能

**优化方向**：
- 多线程异步处理
- 内存池管理
- OpenSSL引擎优化
- 硬件加速支持

**实现复杂度**：🔴 困难  
**预估工作量**：3-5 天

## 🏗️ 技术架构设计

### 1. 模块组织结构
```
src/std/crypto/
├── crypto.c              // 主入口和基础功能
├── crypto.h              // 公共头文件
├── subtle.c              // SubtleCrypto实现
├── subtle.h              // SubtleCrypto头文件
├── algorithms/           // 算法实现目录
│   ├── digest.c          // 摘要算法
│   ├── symmetric.c       // 对称加密
│   ├── asymmetric.c      // 非对称加密
│   ├── kdf.c            // 密钥派生
│   └── hmac.c           // HMAC
├── keys/                // 密钥管理
│   ├── key_mgmt.c       // 密钥生成/导入/导出
│   ├── key_formats.c    // 格式转换
│   └── jwk.c           // JSON Web Key支持
└── utils/               // 工具函数
    ├── openssl_loader.c // OpenSSL动态加载
    ├── async.c         // 异步操作支持
    └── errors.c        // 错误处理
```

### 2. 异步操作模型
```c
typedef struct {
  JSContext *ctx;
  JSValue promise;
  JSValue resolve_func;
  JSValue reject_func;
  uv_work_t work_req;
  // 操作特定数据
  union {
    struct encrypt_data encrypt;
    struct digest_data digest;
    struct keygen_data keygen;
  } operation_data;
} CryptoAsyncOperation;
```

### 3. 错误处理机制
```c
typedef enum {
  JSRT_CRYPTO_ERROR_NONE = 0,
  JSRT_CRYPTO_ERROR_NOT_SUPPORTED,
  JSRT_CRYPTO_ERROR_INVALID_ALGORITHM,
  JSRT_CRYPTO_ERROR_INVALID_KEY,
  JSRT_CRYPTO_ERROR_OPENSSL_ERROR,
  JSRT_CRYPTO_ERROR_OPERATION_ERROR
} jsrt_crypto_error_t;

JSValue jsrt_crypto_throw_error(JSContext *ctx, 
                                jsrt_crypto_error_t error, 
                                const char *message);
```

## 📊 实施优先级和时间规划

### Phase 1: 基础架构 ✅ **已完成** (1-2周)
1. ✅ **基础随机数** - 已完成
2. ✅ **SubtleCrypto架构** - 已完成 (Promise-based API)
3. ✅ **消息摘要** - 已完成 (SHA-1, SHA-256, SHA-384, SHA-512)
4. ✅ **错误处理机制** - 已完成 (WebCrypto标准错误类型)
5. ✅ **TypedArray处理** - 已完成 (支持ArrayBuffer和TypedArray输入)
6. ✅ **OpenSSL集成** - 已完成 (动态加载，多平台支持)

**实际完成时间**：2天  
**核心成果**：完整的SubtleCrypto.digest实现，为后续算法奠定坚实基础

### Phase 2: 对称加密 ✅ **已完成** (1周)
5. ✅ **AES-CBC/GCM** - 已完成 (支持128/256位密钥)
6. ✅ **HMAC** - 已完成 (支持SHA-1/256/384/512)
7. ✅ **单元测试** - 已完成 (全面测试覆盖)

**实际完成时间**：3天  
**核心成果**：完整的对称加密和消息认证码实现，包括AES-CBC、AES-GCM和HMAC算法

### Phase 3: 非对称加密 (2-3周)
8. **RSA算法族** - 5-7天
9. **椭圆曲线** - 4-6天  
10. **密钥管理** - 3-4天

### Phase 4: 高级功能 (1-2周)
11. **密钥派生** - 2-3天
12. **密钥格式** - 3-4天
13. **性能优化** - 3-5天

**总预估工作量**：6-9周 (30-45个工作日)

## 🧪 测试和验证策略

### 1. 单元测试
- 每个算法独立测试
- 边界条件测试
- 错误处理测试
- 性能基准测试

### 2. 兼容性测试  
- 与Node.js WebCrypto API对比
- 与浏览器实现对比
- 跨平台测试 (Windows/macOS/Linux)
- 多版本OpenSSL测试

### 3. 安全性验证
- 已知测试向量验证
- 密码学正确性验证  
- 内存安全检查
- 侧信道攻击防护

## 🔧 构建和依赖管理

### OpenSSL版本支持策略
- **最低要求**：OpenSSL 1.1.1 (LTS)
- **推荐版本**：OpenSSL 3.x
- **降级策略**：核心算法支持，高级功能可选

### 构建配置选项
```cmake
option(JSRT_CRYPTO_ENABLE_ALL "Enable all crypto algorithms" ON)
option(JSRT_CRYPTO_ENABLE_RSA "Enable RSA algorithms" ON) 
option(JSRT_CRYPTO_ENABLE_ECC "Enable elliptic curve algorithms" ON)
option(JSRT_CRYPTO_ENABLE_ED25519 "Enable Ed25519" OFF)
option(JSRT_CRYPTO_REQUIRE_OPENSSL "Require OpenSSL (fail if not found)" OFF)
```

## 📚 参考文档和标准

1. [W3C Web Cryptography API](https://www.w3.org/TR/WebCryptoAPI/)
2. [RFC 3447 - PKCS #1: RSA Cryptography Specifications](https://tools.ietf.org/html/rfc3447)
3. [RFC 5652 - Cryptographic Message Syntax (CMS)](https://tools.ietf.org/html/rfc5652)
4. [RFC 7515 - JSON Web Signature (JWS)](https://tools.ietf.org/html/rfc7515)
5. [RFC 7517 - JSON Web Key (JWK)](https://tools.ietf.org/html/rfc7517)
6. [OpenSSL Documentation](https://www.openssl.org/docs/)
7. [Node.js crypto module](https://nodejs.org/api/crypto.html)

## 🎯 成功标准

### 功能完整性
- [x] 支持所有主要对称加密算法 (AES-CBC, AES-GCM, HMAC)
- [ ] 支持所有主要非对称加密算法  
- [x] 支持基础密钥生命周期管理 (generateKey)
- [x] 通过核心对称加密W3C测试用例

### 性能指标
- [ ] 加密操作延迟 < 10ms (AES-256, 1KB数据)
- [ ] RSA-2048签名 < 50ms
- [ ] 内存占用增长 < 50MB (相比基础运行时)

### 兼容性
- [ ] 与Node.js WebCrypto API 100%兼容
- [ ] 支持OpenSSL 1.1.1+ 所有版本
- [ ] 跨平台功能一致性

### 安全性
- [ ] 通过所有已知向量测试
- [ ] 内存安全检查通过  
- [ ] 无密钥泄露风险
- [ ] 符合FIPS 140-2标准 (可选)

---

*本计划将根据实际实施进度和技术挑战进行动态调整和细化。*