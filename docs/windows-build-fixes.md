# Windows Build Fixes

This document explains the Windows-specific build fixes implemented to resolve libuv compilation issues in MSYS2/MinGW environments.

## Problem

When building jsrt on Windows with MSYS2/MinGW, libuv sometimes incorrectly detects the build environment as Unix/Cygwin instead of Windows, leading to compilation errors like:

```
fatal error: sys/socket.h: No such file or directory
```

This happens because libuv includes Unix-specific source files (`src/unix/cygwin.c`, `src/unix/bsd-ifaddrs.c`) that contain Unix headers not available in Windows.

## Root Cause

The issue occurs because:
1. MSYS2 sets environment variables like `MSYSTEM` that confuse libuv's platform detection
2. libuv's CMakeLists.txt checks for these environment variables and incorrectly selects Unix build paths
3. MinGW environment is sometimes misidentified as Cygwin by libuv

## Solutions Implemented

### 1. CMake Platform Forcing (`CMakeLists.txt`)

```cmake
# Force libuv to use Windows build on MinGW to avoid Cygwin path conflicts  
if(WIN32)
    set(CMAKE_SYSTEM_NAME "Windows" CACHE STRING "Force Windows system name for libuv" FORCE)
    set(WIN32 TRUE CACHE BOOL "Force WIN32 for libuv" FORCE)
    set(MSYS FALSE CACHE BOOL "Force MSYS to FALSE for libuv" FORCE)
    set(CYGWIN FALSE CACHE BOOL "Force CYGWIN to FALSE for libuv" FORCE)
    
    # Force compiler predefined macros for Windows
    add_definitions(-DWIN32=1 -D_WIN32=1 -D__WIN32__=1)
endif()
```

### 2. Pre-build Script (`scripts/pre-build-windows.sh`)

- Temporarily clears `MSYSTEM` and `CYGWIN` environment variables during build
- Patches libuv's CMakeLists.txt to force Windows detection
- Creates backup of original libuv configuration
- Restores environment after build completion

### 3. Build System Integration

- **Makefile**: Automatically runs pre-build script on Windows
- **build-static.bat**: Includes Windows fixes for batch build
- **CMake fixes**: Applied automatically during configuration

## Usage

### Automatic (Recommended)
The fixes are automatically applied when using standard build commands:

```bash
# All fixes applied automatically
make jsrt_static
```

```cmd
# Windows batch script with fixes
./build-static.bat  
```

### Manual Application
If you need to apply fixes manually:

```bash
# Apply fixes
chmod +x scripts/pre-build-windows.sh
./scripts/pre-build-windows.sh

# Build normally
cmake -G "MSYS Makefiles" ...
make

# Restore environment (if script created it)
./scripts/post-build-windows.sh
```

## Verification

To verify the fixes are working:

1. **Check CMake output**: Should show "Windows detected: Forcing libuv to use Windows build path"
2. **Build succeeds**: No more `sys/socket.h` errors
3. **Test executable**: `target/static/jsrt.exe --version` works

## Files Modified

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Force Windows platform detection |
| `scripts/pre-build-windows.sh` | Patch libuv and clear environment |
| `scripts/fix-libuv-windows.cmake` | CMake platform forcing logic |
| `Makefile` | Integrate pre-build script |
| `build-static.bat` | Windows batch integration |

## Troubleshooting

### Script Permissions
```bash
# If script fails to run
chmod +x scripts/pre-build-windows.sh
```

### Environment Issues
```bash
# If MSYSTEM still causes problems
unset MSYSTEM
unset CYGWIN
make jsrt_static
```

### libuv Still Failing
```bash
# Clean build directory and retry
rm -rf target/static
make jsrt_static
```

### Restore libuv Original
```bash
# If libuv was patched and you need original
cd deps/libuv
git checkout CMakeLists.txt
cd ../..
```

## Technical Details

The fixes work by:

1. **Environment Isolation**: Clearing MSYS2-specific variables that confuse libuv
2. **Platform Forcing**: Explicitly setting CMake variables that libuv checks
3. **Compiler Definitions**: Adding Windows-specific macros at compile time
4. **Source Filtering**: Preventing Unix source files from being included

These fixes ensure libuv correctly identifies Windows as the target platform and includes only Windows-specific source files during compilation.