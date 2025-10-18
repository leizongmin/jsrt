<div align="center">
  <h1>🚀 JSRT</h1>
  <p><strong>A lightweight, fast JavaScript runtime built on QuickJS and libuv</strong></p>
  <p><em>Version 0.1.0 • WinterCG Compatible • 90.6% WPT Pass Rate</em></p>

  [![CI](https://github.com/leizongmin/jsrt/actions/workflows/ci.yml/badge.svg)](https://github.com/leizongmin/jsrt/actions/workflows/ci.yml)
  [![Coverage](https://img.shields.io/badge/coverage-70.8%25-yellow)](https://github.com/leizongmin/jsrt/actions/workflows/coverage.yml)
  [![WPT](https://img.shields.io/badge/WPT-90.6%25_pass-brightgreen)](docs/WPT.md)
  [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
</div>

---

> **Important Notice**
> JSRT is an experimental, research-oriented project created to explore AI-assisted development by building a JavaScript runtime from scratch. The codebase is under active development and may change significantly; please evaluate carefully before using it in production environments.

## Why JSRT?

**JSRT = Minimal footprint + Maximum standards compliance**

Perfect for scenarios where size and startup speed matter:

| Aspect | JSRT | Node.js | Deno |
|--------|------|---------|------|
| **Binary Size** | ~2MB | ~50MB | ~90MB |
| **Memory Baseline** | ~5MB | ~30MB | ~20MB |
| **WPT Compliance** | 90.6% | N/A | 95%+ |
| **Target Use Cases** | Embedded, Edge, IoT | General purpose | Modern web apps |

### 🎯 Ideal For

- ✅ **Edge Computing**: Cloudflare Workers-style serverless functions
- ✅ **Embedded Systems**: IoT devices, resource-constrained environments
- ✅ **CLI Tools**: Fast-starting command-line utilities
- ✅ **Learning**: Understanding JavaScript runtime internals

### ⚠️ Not Recommended For

- ❌ **Production-critical applications** - Still in experimental phase
- ❌ **Heavy npm ecosystem dependency** - Node compatibility is partial
- ❌ **Native module intensive projects** - FFI is proof-of-concept

## ✨ Features

- **🏃‍♂️ Fast & Lightweight**: Minimal footprint JavaScript runtime (~2MB binary)
- **🌐 Web Standards**: WinterCG Minimum Common API compliant (29/32 WPT tests pass)
- **⚡ Async Support**: Full async/await and Promise support with libuv event loop
- **📦 Smart Module Loader**: Node-style resolution, multi-format detection, caching, and `file://`/`http://`/`https://` protocol handlers
- **🧱 Node Compatibility**: 19 `node:` modules including fs, http, crypto, net, and more
- **🔧 Rich APIs**: Console, Fetch, WebCrypto, Streams, Timers, URL, AbortController, WebAssembly
- **🧪 Testing Ready**: Integrated WPT and unit suites with detailed compliance reporting
- **🔒 Security**: Complete WebCrypto API with RSA, AES, HMAC, digest, and random UUID support
- **🌍 Cross-platform**: Builds on Linux, macOS, and Windows
- **🛠️ Developer Workflow**: REPL, bytecode compilation, dev containers, and Docker automation
- **🧩 Extensible**: FFI bridge and custom module hooks for native integration

## 🚀 Quick Start

### Installation

```bash
# Clone the repository
git clone https://github.com/leizongmin/jsrt.git
cd jsrt

# Init submodule
git submodule update --init --recursive

# Build the runtime
make
```

### Hello World

```bash
# Run from file
echo 'console.log("Hello from JSRT! 🎉");' > hello.js
./bin/jsrt hello.js

# Run from stdin
echo 'console.log("Quick test");' | ./bin/jsrt -

# Interactive REPL
./bin/jsrt repl

# Run from URL
./bin/jsrt https://example.com/script.js

# Compile to standalone binary
./bin/jsrt build hello.js hello-binary
./hello-binary
```

## 📖 Usage Examples

### Basic JavaScript
```javascript
// Modern JavaScript features
const result = [1, 2, 3, 4, 5].reduce((sum, n) => sum + n, 0);
console.log('Sum:', result);

// Async/await
async function fetchData() {
  const response = await fetch('https://api.example.com/data');
  return await response.json();
}
```

### Real-World HTTP Server
```javascript
import http from 'node:http';

const server = http.createServer((req, res) => {
  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({
    message: 'Hello from JSRT!',
    uptime: process.uptime(),
    memory: process.memoryUsage()
  }));
});

server.listen(3000, () => {
  console.log('Server running at http://localhost:3000/');
});
```

### WebCrypto API
```javascript
// Generate random values
const randomBytes = new Uint8Array(16);
crypto.getRandomValues(randomBytes);
console.log('Random bytes:', randomBytes);

// Generate UUID
const uuid = crypto.randomUUID();
console.log('UUID:', uuid);

// Hash data
const data = new TextEncoder().encode('Hello, JSRT!');
const hashBuffer = await crypto.subtle.digest('SHA-256', data);
const hashArray = Array.from(new Uint8Array(hashBuffer));
const hashHex = hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
console.log('SHA-256:', hashHex);
```

### WebAssembly Support
```javascript
// Load and execute WebAssembly modules
const wasmBytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
  // ... WASM bytecode
]);

const wasmModule = new WebAssembly.Module(wasmBytes);
const wasmInstance = new WebAssembly.Instance(wasmModule);

// Call exported WASM functions
const result = wasmInstance.exports.add(5, 3);
console.log('WASM result:', result);
```

### Native Library Access (FFI)
```javascript
// Load and call native C library functions (experimental)
const ffi = require('jsrt:ffi');

// Load standard C library
const libc = ffi.Library('libc.so.6', {
  'strlen': ['int', ['string']],
  'strcmp': ['int', ['string', 'string']]
});

// Note: Function calls are proof-of-concept
console.log('Available functions:', Object.keys(libc));
```

## 🧩 Module System

JSRT includes a sophisticated module loader with Node.js-compatible resolution:

### Features

- **Unified Pipeline**: Cache lookup → Node-compatible resolution → Format detection → Protocol handler → Loader compilation
- **Multi-Format Support**: CommonJS, ES modules, JSON, built-in `jsrt:`/`node:` namespaces, and bytecode preloading
- **Protocol Handlers**: First-class `file://`, `http://`, and `https://` loaders with streaming fetch backed by libuv
- **Adaptive Cache**: FNV-1a hash cache with hit/miss statistics, collision tracking, and explicit invalidation APIs
- **Extensibility Hooks**: Register custom protocols, inject loaders, and introspect diagnostics from C or JavaScript embedders

### Module Loading Examples

```javascript
// CommonJS
const fs = require('fs');
const myModule = require('./my-module');
const lodash = require('lodash');

// ES Modules
import fs from 'fs';
import { readFile } from 'fs';
import myModule from './my-module.mjs';

// Remote modules (experimental)
import config from 'https://cdn.example.com/config.js';

// Built-in modules
import process from 'jsrt:process';
const http = require('node:http');
```

See `docs/module-system-architecture.md` and `docs/module-system-api.md` for implementation details and extension guides.

## 🧱 Node.js Compatibility

The `node:` compatibility layer mirrors core Node.js modules while keeping the runtime lightweight:

### Implemented Modules (19/46 Core Modules)

| Module | Support | Notes |
|--------|---------|-------|
| `assert` | ✅ Full | All assertion methods |
| `buffer` | ✅ Full | ArrayBuffer-backed buffers |
| `crypto` | 🟡 Partial | Common hash/cipher algorithms |
| `dns` | 🟡 Partial | Basic lookup functions |
| `dgram` | ✅ Full | UDP sockets |
| `events` | ✅ Full | EventEmitter implementation |
| `fs` | 🟡 Partial | Sync methods + core async methods |
| `http`/`https` | ✅ Full | Server & client (llhttp-based) |
| `net` | ✅ Full | TCP sockets |
| `os` | ✅ Full | System information |
| `path` | ✅ Full | Path manipulation |
| `process` | 🟡 Partial | Core properties & methods |
| `querystring` | ✅ Full | URL query parsing |
| `stream` | ✅ Full | All stream types |
| `timers` | ✅ Full | setTimeout/setInterval |
| `url` | ✅ Full | URL parsing & formatting |
| `util` | 🟡 Partial | Common utilities |
| `zlib` | 🟡 Partial | Compression/decompression |

### npm Package Compatibility

**✅ Known to Work**:
- Pure JavaScript packages (lodash, dayjs, chalk, etc.)
- HTTP clients (axios, node-fetch patterns)
- Simple web frameworks (basic Express routes)

**❌ Known Limitations**:
- Native addons (`.node` files) - FFI is experimental
- Packages requiring full Node.js `fs` async API
- Worker threads - not implemented
- Child processes - not implemented

**Test a package**: `./bin/jsrt -e "require('package-name')"`

Refer to `docs/nodejs-compatibility-layer.md` and module-specific plans in `docs/plan/` for progress reports and API coverage.

## 🛠️ API Reference

### Web Standard APIs (WinterCG Compatible)

<details>
<summary><strong>✅ Fully Implemented (100% WPT Pass Rate) - Click to expand</strong></summary>

| API | Global Object | Description |
|-----|---------------|-------------|
| **Console** | `console` | `log`, `error`, `warn`, `info`, `debug`, `assert`, `time`, `timeEnd` |
| **Performance** | `performance` | `performance.now()`, `performance.timeOrigin` |
| **HR-Time** | - | High-resolution timing APIs |
| **Timers** | `setTimeout`, `setInterval` | Timer functions with libuv backend |
| **URL** | `URL`, `URLSearchParams` | URL constructor and search params |
| **WebCrypto** | `crypto` | `getRandomValues`, `randomUUID`, `subtle.*` |
| **Encoding** | `TextEncoder`, `TextDecoder` | Text encoding/decoding |
| **Streams** | `ReadableStream`, `WritableStream` | Streams API implementation |
| **Base64** | `btoa`, `atob` | Base64 encoding/decoding |
| **AbortController** | `AbortController`, `AbortSignal` | Cancelable async operations |

</details>

<details>
<summary><strong>🔧 JSRT-Specific Extensions - Click to expand</strong></summary>

| Module | Usage | Description |
|--------|-------|-------------|
| `jsrt:process` | `import process from 'jsrt:process'` | Process info: `argv`, `pid`, `platform`, `arch`, `version`, `uptime` |
| `jsrt:assert` | `const assert = require('jsrt:assert')` | Testing assertions |
| `jsrt:ffi` | `const ffi = require('jsrt:ffi')` | Foreign Function Interface (experimental) |
| WebAssembly | `WebAssembly.Module`, `.Instance` | Full WebAssembly support via WAMR |
| Build System | `jsrt build` command | Bytecode compilation and standalone binary creation |

</details>

<details>
<summary><strong>⚠️ Browser-Dependent (Partially Implemented) - Click to expand</strong></summary>

| API | Status | Notes |
|-----|--------|-------|
| **Fetch** | 🟡 Working | `fetch`, `Request`, `Response`, `Headers` (3 browser-specific WPT tests skipped) |
| **FormData** | 🟡 Partial | Basic FormData API |
| **Blob** | 🟡 Partial | Basic Blob API |
| **DOM** | ⚠️ Limited | Minimal DOM utilities |

</details>

## 📊 Web Standards Compliance

JSRT achieves excellent [WinterCG Minimum Common API](https://wintercg.org/work/minimum-common-api/) compliance:

**Overall Progress: 90.6% WPT Pass Rate (29/32 tests)**

| Priority | Status | Test Results |
|----------|--------|--------------|
| **High** | ✅ Complete | Console, Performance, HR-Time, Timers, URL |
| **Medium** | ✅ Complete | WebCrypto, Encoding, Streams, Base64, AbortController |
| **Low** | ⚠️ Skipped | Fetch API (3 browser-dependent tests) |

All WPT tests now pass except for 3 browser-specific Fetch API tests that require DOM/browser environment.

See [docs/WPT.md](docs/WPT.md) for detailed WPT integration and compliance status.

## 🗺️ Roadmap

### Phase 1: Foundation (✅ Completed)
- [x] Core runtime with QuickJS + libuv integration
- [x] Module system (CommonJS, ESM, protocol handlers)
- [x] Web Standard APIs (90.6% WPT compliance)
- [x] WebAssembly support (synchronous APIs)
- [x] Basic Node.js compatibility layer (19 modules)

### Phase 2: Enhancement (🔄 In Progress - Q2 2025)
- [ ] **Complete Node.js compatibility** (target: 30+ core modules)
  - [ ] Full `fs` async API
  - [ ] Child process support
  - [ ] Worker threads
- [ ] **Improve Fetch API** for non-browser environments
- [ ] **WebAssembly async APIs** (`compile()`, `instantiate()`)
- [ ] **Enhanced FFI** - Move from proof-of-concept to production-ready
- [ ] **Package manager integration** - Improve npm compatibility

### Phase 3: Performance & Stability (📅 Q3 2025)
- [ ] **Performance optimization**
  - [ ] JIT compilation exploration
  - [ ] Memory usage optimization
  - [ ] Startup time improvements
- [ ] **Comprehensive test coverage** (target: 90%+ code coverage)
- [ ] **Benchmark suite** - Track performance metrics
- [ ] **Memory leak detection** - Automated leak checking in CI
- [ ] **Fuzzing integration** - Security and stability testing

### Phase 4: Ecosystem & Tools (📅 Q4 2025)
- [ ] **Package manager** - Native package installation (`jsrt install`)
- [ ] **Debugger integration** - Chrome DevTools protocol
- [ ] **TypeScript support** - Built-in `.ts` execution
- [ ] **Source maps** - Better error stack traces
- [ ] **Plugin system** - Native extension loading
- [ ] **Documentation site** - Comprehensive docs and examples

### Phase 5: Production Ready (🎯 2026)
- [ ] **Stability hardening** - Production-grade error handling
- [ ] **Security audit** - External security review
- [ ] **1.0 Release** - API stability guarantees
- [ ] **Enterprise features**
  - [ ] Monitoring & observability hooks
  - [ ] Resource limits & sandboxing
  - [ ] Multi-isolate support
- [ ] **Cloud deployment templates** - AWS Lambda, Cloudflare Workers, etc.

### Long-term Vision
- **Edge-first runtime** - Optimized for serverless and edge computing
- **WebAssembly Component Model** - Full component support
- **AI/ML integration** - Native tensor operations, model inference
- **Distribution platform** - Public registry for JSRT packages

**Want to contribute?** Check our [Contributing Guidelines](#-contributing) and [open issues](https://github.com/leizongmin/jsrt/issues).

## 🏗️ Architecture

JSRT is built on proven technologies:

- **[QuickJS](https://bellard.org/quickjs/)** - Fast, small JavaScript engine
- **[libuv](https://libuv.org/)** - Cross-platform asynchronous I/O library
- **[WAMR](https://github.com/bytecodealliance/wasm-micro-runtime)** - WebAssembly Micro Runtime
- **Custom Standard Library** - Web standards compliant APIs

```
┌─────────────────┐
│   JavaScript    │
│     Code        │
├─────────────────┤
│  JSRT Runtime   │
│   (std lib)     │
├─────────────────┤
│    QuickJS      │
│   (JS engine)   │
├─────────────────┤
│     libuv       │
│   (async I/O)   │
├─────────────────┤
│   OS Platform   │
└─────────────────┘
```

## 🏗️ Development

### Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| **GCC** | 7.0+ | C compiler |
| **Make** | 3.81+ | Build system |
| **CMake** | 3.16+ | Build configuration |
| **clang-format** | Latest | Code formatting |

### Building

```bash
# Clone and build
git clone https://github.com/leizongmin/jsrt.git
cd jsrt
git submodule update --init --recursive
make

# Build variants
make jsrt      # Release build (default, optimized)
make jsrt_g    # Debug build with symbols
make jsrt_m    # Debug build with AddressSanitizer
make jsrt_cov  # Coverage instrumentation build
```

### Testing

```bash
# Run all tests
make test

# Run specific test directory
make test N=module     # Test module system
make test N=node       # Test Node.js compatibility

# Run Web Platform Tests
make wpt               # Full WPT suite (32 tests, 90.6% pass rate)
make wpt N=console     # Specific WPT category
make wpt-quick         # Essential tests only

# Generate coverage report
make coverage          # Regular test coverage
make coverage-merged   # Combined regular + WPT coverage
# Reports available at: target/coverage/html/index.html
```

### Debugging

```bash
# Memory debugging with AddressSanitizer
./bin/jsrt_m script.js
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m script.js

# Single file testing with timeout
timeout 20 ./bin/jsrt test/specific-test.js

# Code formatting (mandatory before commits)
make clang-format
```

### Development Environment

#### 🐳 VS Code Dev Container
1. Install [VS Code](https://code.visualstudio.com/) and the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers).
2. Open the repository in VS Code and choose **Reopen in Container**.
3. Start hacking—build tools, submodules, and formatting utilities are pre-installed.

#### 🤖 Claude Code Docker Environment
```bash
make docker-build    # Build the development image (once)
make claude          # Launch Claude Code with /repo mapped to the project
make claude-shell    # Drop into an interactive container shell
```

## 🚀 Advanced Usage

### Cross-Compilation with Zig

Install Zig compiler for cross-platform builds:

```bash
# Install Zig (version 0.13.0+)
wget https://ziglang.org/download/0.13.0/zig-linux-x86_64-0.13.0.tar.xz
tar -xf zig-linux-x86_64-0.13.0.tar.xz
sudo mv zig-linux-x86_64-0.13.0 /opt/zig
sudo ln -s /opt/zig/zig /usr/local/bin/zig
```

Cross-compile for different targets:

```bash
# Linux musl (static linking)
CC="zig cc -target x86_64-linux-musl" make clean all

# Windows
CC="zig cc -target x86_64-windows-gnu" make clean all

# macOS
CC="zig cc -target x86_64-macos-none" make clean all

# ARM64 Linux
CC="zig cc -target aarch64-linux-gnu" make clean all
```

### Performance Tuning

```bash
# Release build with optimizations
CFLAGS="-O3 -DNDEBUG" make jsrt

# Profile-guided optimization
CFLAGS="-fprofile-generate" make jsrt
./bin/jsrt examples/benchmark.js  # Run representative workload
CFLAGS="-fprofile-use" make clean jsrt
```

## ❓ FAQ

**Q: Can I use npm packages?**
A: Pure JavaScript packages work well. Native modules (`.node` files) are experimental via FFI. Test with: `./bin/jsrt -e "require('package-name')"`

**Q: How does JSRT compare to QuickJS CLI?**
A: JSRT adds libuv event loop, Web Standard APIs, Node.js compatibility layer, module system with HTTP(S) loading, and WebAssembly support.

**Q: Is it production-ready?**
A: Not yet. JSRT is experimental and under active development. Use for learning, prototyping, and non-critical projects.

**Q: Why not use Bun/Deno instead?**
A: JSRT targets minimal binary size (~2MB vs 50-90MB) for embedded systems, edge computing, and resource-constrained environments.

**Q: What's the relationship with Node.js?**
A: JSRT implements a Node.js-compatible API surface but is a completely separate runtime built on QuickJS instead of V8.

**Q: How can I contribute?**
A: See our [Contributing Guidelines](#-contributing) below. We welcome bug reports, feature requests, and pull requests!

## 🆘 Troubleshooting

### Common Issues

**"Module not found" error**
```bash
# Check module resolution
./bin/jsrt -e "console.log(require.resolve('module-name'))"

# For built-in modules, use proper prefix
./bin/jsrt -e "const fs = require('node:fs')"  # Correct
./bin/jsrt -e "const fs = require('fs')"       # May fail
```

**Segmentation fault**
```bash
# Run with AddressSanitizer to locate issue
make jsrt_m
./bin/jsrt_m your-script.js
```

**Memory leak**
```bash
# Enable leak detection
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m your-script.js
```

**Test timeout**
```bash
# Use timeout for hanging scripts
timeout 20 ./bin/jsrt test/problematic-test.js
```

**Build errors**
```bash
# Ensure submodules are initialized
git submodule update --init --recursive

# Clean and rebuild
make clean
make
```

**More help**: [GitHub Issues](https://github.com/leizongmin/jsrt/issues) • [GitHub Discussions](https://github.com/leizongmin/jsrt/discussions)

## 🤝 Contributing

We welcome contributions! Here's how to get started:

1. **Fork** the repository
2. **Create** a feature branch: `git checkout -b feature/amazing-feature`
3. **Make** your changes and ensure tests pass: `make test`
4. **Run WPT tests**: `make wpt` to check web standards compliance
5. **Format** your code: `make clang-format && make prettier`
6. **Commit** with a clear message: `git commit -m "Add amazing feature"`
7. **Push** to your branch: `git push origin feature/amazing-feature`
8. **Open** a Pull Request

### Development Guidelines

- Follow existing code style and conventions
- Add tests for new features (prefer WPT-compatible tests)
- Update documentation as needed
- Ensure `make test`, `make wpt`, and `make clang-format` pass
- Check WPT compliance for web standard APIs
- See [AGENTS.md](AGENTS.md) for detailed development setup

### Good First Issues

Looking to contribute? Check out issues labeled [`good first issue`](https://github.com/leizongmin/jsrt/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) for beginner-friendly tasks.

### Development Priorities

**Current Focus Areas:**
1. Complete Node.js compatibility layer (target: 30+ modules)
2. Enhance Fetch API for non-browser environments
3. Improve FFI from proof-of-concept to production-ready
4. Expand test coverage and documentation
5. Performance optimization and benchmarking

## 📄 License

**MIT License** - see [LICENSE](LICENSE) file for details

Copyright © 2024-2025 [LEI Zongmin](https://github.com/leizongmin)

---

<div align="center">
  <p>Built with ❤️ using AI-assisted development</p>
  <p>
    <a href="https://github.com/leizongmin/jsrt/stargazers">⭐ Star this repo</a> •
    <a href="https://github.com/leizongmin/jsrt/issues">🐛 Report Bug</a> •
    <a href="https://github.com/leizongmin/jsrt/discussions">💬 Discussions</a>
  </p>
</div>
