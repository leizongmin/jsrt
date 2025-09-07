<div align="center">
  <h1>🚀 JSRT</h1>
  <p><strong>A lightweight, fast JavaScript runtime built on QuickJS and libuv</strong></p>
  <p><em>Version 0.1.0 • WinterCG Compatible • 21.9% WPT Pass Rate</em></p>

  [![CI](https://github.com/leizongmin/jsrt/actions/workflows/ci.yml/badge.svg)](https://github.com/leizongmin/jsrt/actions/workflows/ci.yml)
  [![Coverage](https://img.shields.io/badge/coverage-70.8%25-yellow)](https://github.com/leizongmin/jsrt/actions/workflows/coverage.yml)
  [![WPT](https://img.shields.io/badge/WPT-21.9%25_pass-orange)](docs/WPT.md)
  [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
</div>

---

## ✨ Features

- **🏃‍♂️ Fast & Lightweight**: Minimal footprint JavaScript runtime
- **🌐 Web Standards**: WinterCG Minimum Common API compliant
- **⚡ Async Support**: Full async/await and Promise support with libuv event loop
- **📦 Module System**: CommonJS and ES modules support
- **🔧 Rich APIs**: Console, Fetch, WebCrypto, Streams, Timers, URL, and more
- **🧪 Testing Ready**: Web Platform Tests (WPT) integration with 32 test categories
- **🔒 Security**: Complete WebCrypto API with RSA, AES, HMAC, and digest support
- **🌍 Cross-platform**: Builds on Linux, macOS, and Windows
- **🛠️ Developer Tools**: REPL, bytecode compilation, and Docker dev environment
- **🧩 Extensible**: FFI support for native library integration

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
# Create a simple JavaScript file
echo 'console.log("Hello from JSRT! 🎉");' > hello.js

# Run it
./bin/jsrt hello.js

# Or use REPL for interactive development
./bin/jsrt repl

# Run from URL
./bin/jsrt https://example.com/script.js

# Compile to standalone binary
./bin/jsrt build hello.js hello-binary
```

## 📖 Usage Examples

### Basic JavaScript
```javascript
// Basic computation
const result = [1, 2, 3, 4, 5].reduce((sum, n) => sum + n, 0);
console.log('Sum:', result);
```

### Async Operations
```javascript
// Timer example
console.log('Starting timer...');
setTimeout(() => {
  console.log('Timer completed!');
}, 1000);
```

### Fetch API
```javascript
// HTTP requests
fetch('https://httpbin.org/json')
  .then(response => response.json())
  .then(data => console.log('Response:', data))
  .catch(error => console.error('Error:', error));
```

### Process Information
```javascript
import process from 'jsrt:process';

console.log(`PID: ${process.pid}`);
console.log(`Platform: ${process.platform}`);
console.log(`Arguments:`, process.argv);
```

### WebCrypto
```javascript
// Generate random values
const randomBytes = new Uint8Array(16);
crypto.getRandomValues(randomBytes);
console.log('Random bytes:', randomBytes);

// Generate UUID
const uuid = crypto.randomUUID();
console.log('UUID:', uuid);
```

### WebAssembly Support
```javascript
// Load and execute WebAssembly modules
const wasmModule = new WebAssembly.Module(wasmBytes);
const wasmInstance = new WebAssembly.Instance(wasmModule);

// Call exported WASM functions
const result = wasmInstance.exports.add(5, 3);
console.log('WASM result:', result);
```

### Native Library Access (FFI)
```javascript
// Load and call native C library functions
const ffi = require('jsrt:ffi');

// Load standard C library
const libc = ffi.Library('libc.so.6', {
  'strlen': ['int', ['string']],
  'strcmp': ['int', ['string', 'string']]
});

// Note: Function calls are proof-of-concept
console.log('Available functions:', Object.keys(libc));
```

## 🛠️ API Reference

### Standard Modules

| Module | Description | Usage | WPT Status |
|--------|-------------|-------|------------|
| `console` | Enhanced console logging | Global `console` object | ✅ 67% pass |
| `jsrt:process` | Process information and control | `import process from 'jsrt:process'` | - |
| `jsrt:assert` | Testing assertions | `const assert = require('jsrt:assert')` | - |
| `jsrt:ffi` | Foreign Function Interface | `const ffi = require('jsrt:ffi')` | - |
| `crypto` | WebCrypto API | Global `crypto` object | ❌ 0% pass |
| `fetch` | HTTP client (Fetch API) | Global `fetch` function | ⚠️ Skipped |
| `encoding` | Text encoding/decoding | Global `TextEncoder`, `TextDecoder` | ❌ 0% pass |
| `timer` | Timer functions | Global `setTimeout`, `setInterval` | ⚠️ 25% pass |
| `url` | URL and URLSearchParams | Global `URL`, `URLSearchParams` | ⚠️ 20% pass |
| `streams` | Streams API | Global `ReadableStream`, etc. | ❌ 0% pass |
| `performance` | Performance timing | Global `performance` | ✅ 100% pass |
| `abort` | AbortController/AbortSignal | Global `AbortController` | ❌ 0% pass |
| `webassembly` | WebAssembly runtime | Global `WebAssembly` | - |
| `base64` | Base64 utilities | Global `btoa`, `atob` | ❌ 0% pass |
| `formdata` | FormData API | Global `FormData` | - |
| `blob` | Blob API | Global `Blob` | - |
| `dom` | Basic DOM utilities | Limited DOM support | - |

### API Implementation Status

#### ✅ Well-Implemented (High WPT Pass Rate)
- **Console**: `log`, `error`, `warn`, `info`, `debug`, `assert`, `time`, `timeEnd` (67% WPT pass)
- **Performance**: `performance.now()`, `performance.timeOrigin` (100% WPT pass)
- **HR-Time**: High-resolution timing APIs (100% WPT pass)

#### ⚠️ Partially Implemented (Basic Functionality Works)
- **Timers**: `setTimeout`, `setInterval`, `clearTimeout`, `clearInterval` (25% WPT pass)
- **URL**: `URL` constructor, `url.origin`, `url.href` (20% WPT pass, URLSearchParams issues)
- **Process**: `argv`, `pid`, `ppid`, `platform`, `arch`, `version`, `uptime`

#### ❌ Needs Improvement (0% WPT Pass)
- **WebCrypto**: `getRandomValues`, `randomUUID`, `subtle.*` (basic failures)
- **Encoding**: `TextEncoder`, `TextDecoder`, `btoa`, `atob` (missing test fixtures)
- **Streams**: `ReadableStream`, `WritableStream`, `TransformStream` 
- **Base64**: `btoa`, `atob` utilities
- **AbortController**: `AbortController`, `AbortSignal`

#### ⚠️ Browser-Dependent (Skipped in WPT)
- **Fetch**: `fetch`, `Request`, `Response`, `Headers` (needs jsrt-specific variants)

#### 🔧 jsrt-Specific Extensions
- **FFI**: `Library` function for loading native libraries and calling C functions
- **WebAssembly**: `WebAssembly.Module`, `WebAssembly.Instance`
- **Build System**: Bytecode compilation and standalone binary creation

## 🏗️ Development

### Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| **GCC** | 7.0+ | C compiler |
| **Make** | 3.81+ | Build system |
| **CMake** | 3.16+ | Build configuration |
| **clang-format** | Latest | Code formatting |


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


### 🔨 Building

```bash
# Clone and build
git clone https://github.com/leizongmin/jsrt.git
cd jsrt
make

# Or build specific variants
make jsrt      # Release build (default)
make jsrt_g    # Debug build with symbols
make jsrt_m    # Debug build with AddressSanitizer
make jsrt_cov  # Coverage instrumentation build
```

### 🧪 Testing

```bash
# Run all tests
make test

# Run Web Platform Tests for WinterCG compliance
make wpt                    # Full WPT suite (32 tests, 21.9% pass rate)
make wpt-quick             # Essential tests only
make wpt-list              # List available test categories

# Generate coverage report
make coverage              # Regular test coverage
make coverage-merged       # Combined regular + WPT coverage
# Reports available at: target/coverage/html/index.html
```

JSRT includes [Web Platform Tests (WPT)](docs/WPT.md) integration to validate compliance with web standards and the WinterCG Minimum Common API. See [docs/WPT.md](docs/WPT.md) for details.

### 🎯 Code Quality

```bash
# Format code (required before commits)
make clang-format

# Clean build artifacts
make clean
```

### 🐛 Debugging with VSCode

Create `.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "lldb",
      "request": "launch",
      "name": "Debug JavaScript File",
      "program": "${workspaceFolder}/bin/jsrt_g",
      "args": ["${file}"],
      "cwd": "${workspaceFolder}"
    }
  ]
}
```


## 🏗️ Architecture

JSRT is built on proven technologies:

- **[QuickJS](https://bellard.org/quickjs/)** - Fast, small JavaScript engine
- **[libuv](https://libuv.org/)** - Cross-platform asynchronous I/O library
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

## 📊 Web Standards Compliance

JSRT aims for full [WinterCG Minimum Common API](https://wintercg.org/work/minimum-common-api/) compliance. Current status:

**Overall Progress: 21.9% WPT Pass Rate (7/32 tests)**

| Priority | Status | Focus Areas |
|----------|--------|-----------|
| **High** | ✅ Complete | Console API, Performance timing, HR-Time |
| **High** | ⚠️ Partial | URL API (basic), Timer functions (basic) |
| **Medium** | ❌ Failing | WebCrypto, Encoding APIs, Streams |
| **Low** | ⚠️ Skipped | Fetch API (browser-dependent) |

See [docs/WPT.md](docs/WPT.md) for detailed WPT integration and compliance status.

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

### Development Priorities

**High Impact Improvements:**
1. Fix URLSearchParams implementation (currently 0/10 tests passing)
2. Improve WebCrypto basic functionality (digest, etc.)
3. Complete TextEncoder/TextDecoder APIs
4. Enhance Timer precision and edge cases

### Development Guidelines

- Follow existing code style and conventions
- Add tests for new features (prefer WPT-compatible tests)
- Update documentation as needed
- Ensure `make test`, `make wpt`, and `make clang-format` pass
- Check WPT compliance for web standard APIs
- See [CLAUDE.md](CLAUDE.md) for detailed development setup

## 📄 License

**MIT License** - see [LICENSE](LICENSE) file for details

Copyright © 2024-2025 [LEI Zongmin](https://github.com/leizongmin)
