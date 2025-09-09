# Windows Static Build Guide

This guide explains how to build jsrt with static linking on Windows, creating a standalone executable that includes all dependencies (including OpenSSL) statically linked.

## Prerequisites

### 1. Install MSYS2
1. Download and install MSYS2 from https://www.msys2.org/
2. Open MSYS2 UCRT64 terminal (not MinGW64 or MSYS2)

### 2. Install Required Packages
```bash
# Update package database
pacman -Syu

# Install build tools
pacman -S mingw-w64-ucrt-x86_64-gcc
pacman -S mingw-w64-ucrt-x86_64-cmake
pacman -S mingw-w64-ucrt-x86_64-openssl
pacman -S make
pacman -S git

# Optional: Install Node.js for llhttp building
pacman -S mingw-w64-ucrt-x86_64-nodejs-lts
```

## Building Static Version

### Method 1: Using Makefile (Recommended)
```bash
# Clone repository with submodules
git clone --recursive https://github.com/your-repo/jsrt.git
cd jsrt

# Build static version
make jsrt_static
```

### Method 2: Using Batch Script
For Windows users who prefer a simpler approach:
```cmd
# Run from MSYS2 terminal
./build-static.bat
```

### Method 3: Manual CMake
```bash
# Create build directory
mkdir -p target/static
cd target/static

# Configure
cmake -G "MSYS Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DJSRT_STATIC_OPENSSL=ON \
    -DCMAKE_MAKE_PROGRAM=/usr/bin/make \
    ../..

# Build
make -j$(nproc)
```

## Output

The static build will create:
- `target/static/jsrt.exe` - Standalone executable with all dependencies

## Features of Static Build

- **No external dependencies**: All libraries are statically linked
- **OpenSSL included**: Full WebCrypto API support without needing OpenSSL DLLs
- **Portable**: Can run on any Windows machine without additional installations
- **Self-contained**: Single executable file

## Verifying Static Linking

To verify that the executable is truly static:

```bash
# Check dependencies (should only show system DLLs)
ldd target/static/jsrt.exe

# Test crypto functionality
target/static/jsrt.exe test/test_webcrypto_digest.js
```

## Troubleshooting

### OpenSSL Not Found
```
Error: OpenSSL static libraries not found
```
**Solution**: Install OpenSSL development package:
```bash
pacman -S mingw-w64-ucrt-x86_64-openssl
```

### CMake Generator Issues
```
Error: Could not find CMAKE_MAKE_PROGRAM
```
**Solution**: Ensure you're using MSYS2 and specify make program:
```bash
cmake -G "MSYS Makefiles" -DCMAKE_MAKE_PROGRAM=/usr/bin/make ...
```

### llhttp Build Fails
```
Error: Failed to build llhttp dependencies
```
**Solution**: Install Node.js or use pre-built release:
```bash
pacman -S mingw-w64-ucrt-x86_64-nodejs-lts
```

## Binary Size Optimization

To reduce the binary size of static builds:

```bash
# Strip debug symbols
strip target/static/jsrt.exe

# Compress with UPX (optional)
pacman -S upx
upx --best target/static/jsrt.exe
```

## Comparison: Dynamic vs Static

| Feature | Dynamic Build | Static Build |
|---------|---------------|--------------|
| Size | ~2MB | ~15MB |
| Dependencies | Requires OpenSSL DLLs | None |
| Portability | Requires DLLs in PATH | Fully portable |
| WebCrypto | Runtime loading | Built-in |
| Startup | Slightly faster | Slightly slower |

## Distribution

The static executable can be distributed as a single file without any additional requirements. This makes it ideal for:
- Standalone applications
- CI/CD environments
- Systems without package managers
- Air-gapped environments