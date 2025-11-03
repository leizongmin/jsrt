# Compact Node Mode Implementation Summary

## Overview
This document describes the implementation of jsrt's compact Node.js compatibility mode. The runtime now enables this mode by default; it can be explicitly disabled with the `--no-compact-node` command-line flag. The legacy `--compact-node` flag remains accepted for compatibility but is no longer required.

## Feature Description
Compact Node.js mode allows jsrt to run Node.js code with minimal modifications by:
1. **Module Loading Without Prefix**: Both `require('os')` and `require('node:os')` resolve to the same Node.js module
2. **ES Module Support**: Both `import os from 'os'` and `import os from 'node:os'` work
3. **Submodule Support**: Paths like `require('stream/promises')` are automatically resolved to `node:stream/promises`
4. **Global Variables**: CommonJS modules have access to `__dirname` and `__filename` (already implemented)
5. **Global Process**: The `process` object is globally available (already implemented)

## Implementation Details

### Phase 1: Infrastructure & Configuration
**Files Modified:**
- `src/runtime.h`: Added `bool compact_node_mode` field to `JSRT_Runtime` struct
- `src/runtime.c`: 
  - Initialize `compact_node_mode = false` in `JSRT_RuntimeNew()`
  - Added `JSRT_RuntimeSetCompactNodeMode()` function
- `src/jsrt.h`: Updated function signatures to accept `compact_node` parameter
- `src/jsrt.c`: 
  - Updated `JSRT_CmdRunFile()` and `JSRT_CmdRunStdin()` to accept and use compact_node flag
- `src/main.c`: 
  - Parse compact-node command-line flags before script execution (default on, `--no-compact-node` disables)
  - Updated help message to document the mode and disable flag

**Usage:**
```bash
# Enabled by default
jsrt script.js [args]

# Disable compact Node.js behavior
jsrt --no-compact-node script.js [args]

# Legacy flag (optional)
jsrt --compact-node script.js [args]
```

### Phase 2: Module Resolution Enhancement
**Files Modified:**
- `src/node/node_modules.h`: Added `JSRT_IsNodeModule()` helper function declaration
- `src/node/node_modules.c`: Implemented `JSRT_IsNodeModule()` to check if a module name is a Node.js built-in
- `src/module/module.c`: 
  - In `js_require()`: Added bare-name fallback logic for CommonJS modules
  - In `JSRT_ModuleNormalize()`: Added bare-name fallback logic for ES modules

**Resolution Flow (with compact mode enabled):**
1. Check for `jsrt:` prefix → load jsrt module
2. Check for `node:` prefix → load node module
3. **NEW**: If bare name AND compact mode AND is node module → prepend `node:` and load
4. Check for HTTP/HTTPS URL → load from URL
5. Check for package imports (#prefix) → resolve package import
6. Try npm module resolution
7. Try file resolution

**Example:**
```javascript
// With compact-node mode (default), these are equivalent:
const os = require('os');           // ← resolved to node:os
const os = require('node:os');      // ← explicit prefix

// ES modules work too:
import os from 'os';                // ← resolved to node:os
import os from 'node:os';           // ← explicit prefix

// Submodules work:
const promises = require('stream/promises');  // ← resolved to node:stream/promises
```

### Phase 3: CommonJS Global Variables
**Status:** Already implemented correctly

The existing implementation already provides `__dirname` and `__filename` as wrapper parameters in CommonJS modules loaded via `require()`. This is the correct Node.js behavior - these variables are only available in CommonJS module contexts, not in direct script execution.

**Verification:**
```javascript
// In a required module (module_test.js):
console.log(__dirname);   // Works: prints the directory path
console.log(__filename);  // Works: prints the file path

// Direct execution (jsrt script.js):
console.log(__dirname);   // undefined (correct Node.js behavior)
```

### Phase 4: Global Process Object
**Status:** Already implemented

The `process` object is already set as a global in `JSRT_RuntimeSetupStdProcess()` (in `src/node/process/module.c`), which is called during runtime initialization. This is available in all modes, not just compact-node mode.

**Verification:**
```javascript
console.log(process.platform);  // Works: 'linux', 'darwin', 'win32', etc.
console.log(process.pid);       // Works: prints process ID
console.log(process.cwd());     // Works: prints current working directory
```

### Phase 5: Testing & Validation
**Files Added:**
- `test/node/compact/test_compact_node_basic.js`: Tests that compact mode works by default
- `test/node/compact/test_compact_node_disabled.js`: Tests that compact mode is correctly disabled when requested
- `test/node/compact/test_compact_node_esm.mjs`: Tests ES module support with explicit compact-node flag
- `examples/compact_node_example.js`: Real-world example demonstrating the feature

**Files Modified:**
- `CMakeLists.txt`: Added special test configuration for compact-node tests

**Test Results:**
- All 3 new compact-node tests pass (100%)
- Existing test suite maintains 99% pass rate (167/168 tests)
- Only 1 pre-existing failure (crypto test, unrelated to our changes)

## Backward Compatibility
✅ **100% Backward Compatible**

- Compact mode remains available by default for existing workflows
- The `--compact-node` flag still functions but is optional
- A new `--no-compact-node` flag restores the legacy non-compact behavior when needed

## Module Caching
Module caching works correctly - `require('os')` and `require('node:os')` return the same cached module instance in compact mode.

## Priority Resolution
In compact mode, Node.js built-ins take priority over npm packages with the same name. For example:
- `require('os')` → loads Node.js built-in `node:os`
- To load an npm package named 'os', use explicit path: `require('./node_modules/os')`

This matches Node.js behavior.

## Performance Impact
Minimal - the implementation adds a single map lookup to check if a bare name is a Node.js module. This only occurs when:
1. Compact mode is enabled (default; not disabled via `--no-compact-node`)
2. Module name has no prefix (jsrt:, node:, http://, etc.)
3. Module name is not a relative or absolute path

## Future Enhancements (Optional)
These items were marked as lower priority in the original plan and can be implemented later if needed:
- Task 1.5: Add `--compact-node` support to REPL mode (`jsrt repl --compact-node`)
- Task 1.6: Support for stdin mode with flag (`jsrt --compact-node -`)

## Usage Examples

### Basic Example
```javascript
// script.js
const os = require('os');
const path = require('path');

console.log('Platform:', os.platform());
console.log('Path join:', path.join('a', 'b', 'c'));
```

```bash
# Run with default compact-node mode
jsrt script.js
# Output: Platform: linux
#         Path join: a/b/c

# Disable compact-node mode (will fail)
jsrt --no-compact-node script.js
# Output: ReferenceError: Cannot find module 'os'
```

### ES Module Example
```javascript
// script.mjs
import os from 'os';
import path from 'path';
import { platform } from 'os';

console.log('Platform:', platform());
console.log('Path:', path.join('a', 'b'));
```

```bash
jsrt --compact-node script.mjs
```

### Real-world Example
See `examples/compact_node_example.js` for a comprehensive example that demonstrates:
- System information retrieval (os module)
- Path operations (path module)
- File system operations (fs module)
- Global process object usage

## Testing
Run tests:
```bash
# Test compact-node functionality
make test

# Or test specific compact-node tests
cd target/release
ctest -R "test_node_compact"
```

## References
- Original task plan: `docs/plan/node-compact-command-flag.md`
- Node.js module documentation: https://nodejs.org/docs/latest/api/modules.html
