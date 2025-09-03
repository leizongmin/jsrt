<div align="center">
  <h1>🚀 JSRT</h1>
  <p><strong>A lightweight, fast JavaScript runtime built on QuickJS and libuv</strong></p>

  [![CI](https://github.com/leizongmin/jsrt/actions/workflows/ci.yml/badge.svg)](https://github.com/leizongmin/jsrt/actions/workflows/ci.yml)
  [![Coverage](https://img.shields.io/badge/coverage-70.8%25-yellow)](https://github.com/leizongmin/jsrt/actions/workflows/coverage.yml)
  [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
</div>

---

## ✨ Features

- **🏃‍♂️ Fast & Lightweight**: Minimal footprint JavaScript runtime
- **🌐 Web Standards**: Support for Fetch API, WebCrypto, Streams, and more
- **⚡ Async Support**: Full async/await and Promise support with libuv event loop
- **📦 Module System**: CommonJS and ES modules support
- **🔧 Built-in APIs**: Console, timers, process information, and encoding utilities
- **🧪 Testing Ready**: Built-in assertion module for testing
- **🔒 Security**: WebCrypto API with RSA, AES, HMAC, and digest support
- **🌍 Cross-platform**: Builds on Linux, macOS, and Windows

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
import process from 'std:process';

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

### Native Library Access (FFI)
```javascript
// Load and call native C library functions
const ffi = require('std:ffi');

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

| Module | Description | Usage |
|--------|-------------|-------|
| `console` | Enhanced console logging | Global `console` object |
| `std:process` | Process information and control | `import process from 'std:process'` |
| `std:assert` | Testing assertions | `const assert = require('std:assert')` |
| `std:ffi` | Foreign Function Interface for native libraries | `const ffi = require('std:ffi')` |
| `crypto` | Cryptographic functions | Global `crypto` object |
| `fetch` | HTTP client (Fetch API) | Global `fetch` function |
| `encoding` | Text encoding/decoding | Global `TextEncoder`, `TextDecoder`, `btoa`, `atob` |
| `timer` | Timer functions | Global `setTimeout`, `setInterval` |

### Available APIs

- **Console**: `log`, `error`, `warn`, `info`, `debug`, `assert`, `time`, `timeEnd`
- **WebCrypto**: `getRandomValues`, `randomUUID`, `subtle.*`
- **Fetch**: Full Fetch API with `Request`, `Response`, `Headers`
- **Process**: `argv`, `pid`, `ppid`, `platform`, `arch`, `version`, `uptime`
- **FFI**: `Library` function for loading native libraries and calling C functions
- **Streams**: `ReadableStream`, `WritableStream`, `TransformStream`
- **Encoding**: `TextEncoder`, `TextDecoder`, `btoa`, `atob`

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

# Generate coverage report
make coverage
# Report available at: target/coverage/html/index.html
```

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

## 🤝 Contributing

We welcome contributions! Here's how to get started:

1. **Fork** the repository
2. **Create** a feature branch: `git checkout -b feature/amazing-feature`
3. **Make** your changes and ensure tests pass: `make test`
4. **Format** your code: `make clang-format`
5. **Commit** with a clear message: `git commit -m "Add amazing feature"`
6. **Push** to your branch: `git push origin feature/amazing-feature`
7. **Open** a Pull Request

### Development Guidelines

- Follow existing code style and conventions
- Add tests for new features
- Update documentation as needed
- Ensure `make test` and `make clang-format` pass

## 📄 License

**MIT License** - see [LICENSE](LICENSE) file for details

Copyright © 2024-2025 [LEI Zongmin](https://github.com/leizongmin)
