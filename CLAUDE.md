# jsrt Development Guidelines

## Project Overview

**jsrt** is a lightweight JavaScript runtime written in C, designed for minimal resource footprint while providing essential JavaScript execution capabilities.

### Core Technologies
- **QuickJS**: JavaScript engine for parsing and executing JS code
- **libuv**: Asynchronous I/O operations and event loop management
- **Custom Stdlib**: Minimal standard library implementations

## Quick Start

```bash
# Clone with submodules
git clone --recursive <repo_url>

# Build and test
make            # Build release version
make test       # Run unit tests (MANDATORY before commit)
make wpt        # Run Web Platform Tests (MANDATORY before commit)

# Development builds
make jsrt_g     # Debug build with symbols
make jsrt_m     # Debug with AddressSanitizer
make format     # Format all code (MANDATORY before commit)
```

## AI Assistant Core Rules

### Three Core Principles
1. **Understanding First**: Read code > Guess interfaces | Confirm > Assume
2. **Quality Assurance**: Test > Commit | Reuse > Innovate
3. **Honest Collaboration**: Admit ignorance > Pretend knowledge | Human decisions > AI autonomy

### Critical Development Rules
- **MANDATORY**: Run baseline tests before modifications
- **MANDATORY**: Make minimal, targeted changes only
- **MANDATORY**: Test immediately after each change
- **MANDATORY**: Report exact numbers (e.g., "653 failures" not "many")
- **NEVER**: Modify code without understanding its purpose
- **NEVER**: Claim improvement without concrete evidence

### Verification Checklist
Must complete after every modification:
```bash
□ make format    # Code formatting
□ make test      # Unit tests (100% pass rate)
□ make wpt       # WPT tests (failures ≤ baseline)
```

## Agent System

Project uses specialized agents for different tasks. See `.claude/AGENTS_SUMMARY.md` for complete guide.

| Agent | Purpose | Key Commands |
|-------|---------|--------------|
| **jsrt-developer** | Feature development | `make format && make test && make wpt` |
| **jsrt-tester** | Testing & debugging | `make test`, `make wpt`, ASAN analysis |
| **jsrt-compliance** | Standards compliance | WPT/WinterCG validation |
| **jsrt-code-reviewer** | Code quality | Security, memory, standards review |
| **jsrt-cross-platform** | Platform support | Windows/Linux/macOS compatibility |
| **jsrt-formatter** | Code formatting | `make format` enforcement |

## Project Structure

```
jsrt/
├── src/                    # Core runtime source
│   ├── main.c             # Entry point
│   ├── jsrt.c/h           # Core runtime & API
│   ├── runtime.c/h        # Runtime environment setup
│   ├── repl.c/h           # REPL implementation
│   ├── build.c/h          # Build utilities
│   ├── node/              # Node.js-compatible modules (fs, http, net, dns, etc.)
│   ├── std/               # Standard library modules (console, timers, etc.)
│   ├── util/              # Utility functions (debug.h, etc.)
│   ├── crypto/            # Cryptographic functions
│   ├── http/              # HTTP implementation
│   ├── url/               # URL parsing
│   └── wasm/              # WebAssembly support
├── test/                   # Test suite (test_*.js)
├── wpt/                    # Web Platform Tests
├── deps/                   # Dependencies (DO NOT MODIFY)
├── examples/              # Example JavaScript files
├── docs/                   # Project documentation
├── scripts/               # Build and utility scripts
├── bin/                    # Binary executables (symlinks to target/)
│   ├── jsrt -> ../target/release/jsrt      # Release build
│   ├── jsrt_g -> ../target/debug/jsrt      # Debug build
│   ├── jsrt_m -> ../target/asan/jsrt       # AddressSanitizer build
│   ├── jsrt_s -> ../target/release/jsrt_s  # Static release
│   └── jsrt_static -> ../target/static/jsrt # Static build
├── build/                  # CMake build directory
├── .claude/               # AI assistant configuration
│   ├── agents/            # Specialized agent definitions
│   ├── docs/              # Detailed documentation
│   └── AGENTS_SUMMARY.md  # Agent usage guide
└── target/                # Build outputs
    ├── release/           # Release builds
    ├── debug/             # Debug builds
    ├── asan/              # AddressSanitizer builds
    ├── static/            # Static builds
    └── tmp/               # Temporary test files (git ignored)
```

## Essential Commands

### Build Commands
```bash
make            # Release build (optimized)
make jsrt_g     # Debug build with symbols
make jsrt_m     # AddressSanitizer build
make clean      # Clean build artifacts
make format     # Format C/JS code (MANDATORY)
```

### Test Commands
```bash
make test                          # Unit tests (MUST PASS)
make test N=dir                    # Test specific directory (test/dir)
                                   # Useful for focused testing on a specific module
                                   # Avoids interference from other issues and improves efficiency
make wpt                           # All WPT tests (MUST PASS)
make wpt N=url                     # Specific WPT category
SHOW_ALL_FAILURES=1 make wpt N=url # Debug mode
MAX_FAILURES=10 make wpt           # Limit failures shown

# Single file testing with timeout
timeout 20 ./bin/jsrt test/file.js # Test individual file with 20s timeout
                                   # Prevents hanging when code has infinite loops or deadlocks
```

### Debug Commands
```bash
# Memory debugging
./bin/jsrt_m script.js                      # Run with AddressSanitizer
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m script.js  # Enable leak detection

# Debug logging with JSRT_Debug macro
make jsrt_g                 # Build debug version with DEBUG flag enabled
./bin/jsrt_g script.js      # Run with debug logging output
# JSRT_Debug prints to stderr with green color, showing file:line info

# WPT debugging
mkdir -p target/tmp
SHOW_ALL_FAILURES=1 make wpt N=console > target/tmp/debug.log 2>&1
```

## File Organization Rules

| File Type | Location | Purpose |
|-----------|----------|---------|
| **Permanent tests** | `test/` | Committed test cases |
| **Temporary tests** | `target/tmp/` | Debug files (git ignored) |
| **Examples** | `examples/` | Demo scripts |
| **Modules** | `src/std/` | Standard library |

### Code Organization Guidelines
- **Test files**: Keep test output minimal - only print on failures or warnings
- **Source files**: If a single file exceeds 500 lines, refactor into multiple files within a subdirectory

## Development Workflow

```bash
# 1. Before starting
git submodule update --init --recursive
make jsrt_g  # Build debug version

# 2. Development cycle
# Edit code...
make format     # Format code
make test       # Run tests
make wpt        # Check compliance

# 3. Before commit (ALL MANDATORY)
make format
make test
make wpt
make clean && make  # Verify release build
```

## Memory Management Rules
- Every `malloc` needs corresponding `free`
- Use QuickJS functions: `js_malloc`, `js_free`
- Handle JS values: `JS_FreeValue` for temporaries
- Check all allocation returns

## Error Handling Pattern
```c
JSValue result = some_operation(ctx);
if (JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}
// Use result...
JS_FreeValue(ctx, result);
```

## Debug Logging

Use the `JSRT_Debug` macro for adding debug logging to the codebase:

```c
#include "util/debug.h"

// Add debug logging in your code
JSRT_Debug("Variable value: %d", some_value);
JSRT_Debug("Processing request from %s:%d", host, port);
```

**How it works:**
- `JSRT_Debug` is only active when building with `make jsrt_g` (DEBUG flag enabled)
- Outputs to stderr with green color formatting
- Automatically includes file name and line number
- In release builds, `JSRT_Debug` calls are compiled out (no overhead)

**Usage:**
```bash
make jsrt_g                 # Build with debug logging enabled
./bin/jsrt_g script.js      # Run and see debug output
```

## Documentation References

Detailed documentation in `.claude/docs/`:
- `platform.md` - Cross-platform development
- `testing.md` - Complete testing guide
- `wpt.md` - Web Platform Tests guide
- `architecture.md` - System architecture
- `modules.md` - Module development
- `debugging.md` - Debugging techniques
- `contributing.md` - Contribution guidelines

## Quick Reference Links

- **Agent Guide**: `.claude/AGENTS_SUMMARY.md`
- **Agent Definitions**: `.claude/agents/`
- **Detailed Docs**: `.claude/docs/`
- **Local Settings**: `.claude/settings.local.json`

---
**Remember**: Always use appropriate agents for specific tasks. When in doubt, consult `.claude/AGENTS_SUMMARY.md` for agent selection guidance.
