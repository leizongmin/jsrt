# GitHub Copilot Instructions for jsrt

## Project Overview

jsrt is a small JavaScript runtime written in C that provides a minimal environment for executing JavaScript code. It's built on top of QuickJS (JavaScript engine) and libuv (asynchronous I/O library).

## Project Structure

- `src/` - Main source code for the JavaScript runtime
  - `main.c` - Entry point of the application
  - `jsrt.c` - Core runtime functionality
  - `runtime.c` - Runtime environment setup
  - `std/` - Standard library implementations (console, timer, etc.)
  - `util/` - Utility functions
- `test/` - Test files (both C and JavaScript tests)
- `deps/` - Git submodules for dependencies
  - `quickjs/` - QuickJS JavaScript engine
  - `libuv/` - libuv for async operations
- `examples/` - Example JavaScript files
- `CMakeLists.txt` - CMake build configuration
- `Makefile` - Make wrapper for common build tasks

## Build System

The project uses CMake as the primary build system with Make as a convenient wrapper:

- `make` or `make jsrt` - Build release version
- `make jsrt_g` - Build debug version with debug symbols
- `make jsrt_m` - Build debug version with AddressSanitizer
- `make test` - Run tests
- `make clean` - Clean build artifacts
- `make clang-format` - Format code with clang-format

### Dependencies

- **gcc** - C compiler
- **make** - Build system
- **cmake** (>= 3.16) - Build configuration
- **clang-format** - Code formatting

## Code Style

- The project uses clang-format for consistent code formatting
- **ALWAYS run `make clang-format` before committing changes** - this is mandatory to ensure consistent formatting
- **ALWAYS ensure `make test` passes before committing changes** - this is mandatory to ensure code quality
- Follow existing C coding conventions in the codebase

## Testing

- JavaScript tests are in `test/test_*.js`
- C test runner is `test/test_js.c`
- Use `make test` to run all tests
- Tests can be run individually with `./target/release/jsrt_test_js test/test_file.js`

## Key Files to Understand

- `src/jsrt.h` - Main header with public API
- `src/jsrt.c` - Core runtime implementation
- `src/std/console.c` - Console object implementation
- `src/std/timer.c` - Timer functions (setTimeout, setInterval)

## Development Tips

- The runtime provides a minimal JavaScript environment
- Focus on keeping the runtime small and efficient
- New standard library functions should go in `src/std/`
- Utility functions should go in `src/util/`
- Always run tests after making changes
- **ALWAYS ensure `make test` passes before committing changes**
- Use debug build (`make jsrt_g`) for development and debugging
- **MANDATORY: Run `make clang-format` before committing any code changes**

## Architecture

jsrt integrates:
- QuickJS as the JavaScript engine for parsing and executing JS code
- libuv for asynchronous I/O operations and event loop
- Custom standard library implementations for console, timers, etc.

When suggesting code changes:
- Maintain compatibility with the existing API
- Keep memory management patterns consistent
- Follow the error handling patterns used throughout the codebase
- Consider cross-platform compatibility