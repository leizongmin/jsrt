# Module System Migration Guide

## Overview

This guide helps developers migrate from jsrt's legacy module loading system to the new unified module loading architecture introduced in October 2025.

**Migration Status**: ✅ Complete - Both systems currently coexist for backward compatibility

**Target Audience**: jsrt contributors, embedders, and advanced users

---

## What Changed?

### High-Level Changes

| Aspect | Old System | New System |
|--------|-----------|------------|
| **Architecture** | Monolithic functions | Modular components (6 subsystems) |
| **Protocol Support** | File only | Extensible (file://, http://, https://) |
| **Format Detection** | Extension-based | Three-stage (ext → pkg.json → content) |
| **Caching** | Ad-hoc | FNV-1a hash-based cache |
| **Path Resolution** | Basic | Full Node.js algorithm |
| **Error Handling** | Generic errors | 60+ specific error codes |
| **Extensibility** | Hard-coded | Protocol/format/loader plugins |

### File Structure Changes

```
Old:
src/module/module.c    (Legacy code, previously in src/std/)
src/module/module.h

New:
src/module/
├── core/              (Infrastructure)
├── resolver/          (Path resolution)
├── detector/          (Format detection)
├── protocols/         (Protocol handlers)
├── loaders/           (Module loaders)
└── module_loader.c    (Main dispatcher)
```

---

## API Changes

### Runtime Integration (Embedders)

#### Before (Legacy System)

```c
// In your application initialization:
JSRT_Runtime* rt = JSRT_RuntimeNew();

// Module loading was handled internally by JSRT_StdModuleInit
// (called automatically during runtime initialization)
```

#### After (New System)

```c
// In your application initialization:
JSRT_Runtime* rt = JSRT_RuntimeNew();

// Module loader is now automatically created during runtime init
// Access via rt->module_loader if needed for advanced use cases

// The new system is already initialized and ready to use
```

**Migration Impact**: ✅ **NONE** - Automatic initialization, no code changes needed

---

### Module Loading (C API)

#### Before (Legacy System)

```c
// Old module loader function (now renamed for compatibility)
JSModuleDef* JSRT_ModuleLoader(JSContext* ctx, const char* module_name, void* opaque);

// Old module normalize function (now renamed)
char* JSRT_ModuleNormalize(JSContext* ctx, const char* base, const char* name, void* opaque);
```

#### After (New System)

```c
// New module loader struct and API
typedef struct JSRT_ModuleLoader JSRT_ModuleLoader;

// Create module loader (done automatically by runtime)
JSRT_ModuleLoader* jsrt_module_loader_create(JSContext* ctx);

// Load a module
JSValue jsrt_load_module(JSRT_ModuleLoader* loader, const char* specifier, const char* base_path);

// Free module loader (done automatically by runtime)
void jsrt_module_loader_free(JSRT_ModuleLoader* loader);
```

**Migration Impact**: ⚠️ **MEDIUM** - Only affects embedders using low-level module APIs directly

#### Legacy Compatibility

The old functions have been renamed but remain available:

```c
// Legacy functions (still available, internally use new system)
JSModuleDef* JSRT_StdModuleLoader(JSContext* ctx, const char* module_name, void* opaque);
char* JSRT_StdModuleNormalize(JSContext* ctx, const char* base, const char* name, void* opaque);
```

These legacy functions internally delegate to the new module system, ensuring backward compatibility.

---

### Path Resolution

#### Before (Legacy System)

```c
// Basic path resolution, limited functionality
// No public API, internal implementation only
```

#### After (New System)

```c
// Full Node.js-compatible resolution
typedef struct {
  char* resolved_path;           // Absolute path to module
  JSRT_ModuleFormat format;      // Detected format
  bool is_builtin;               // Is builtin module?
} JSRT_ResolvedPath;

// Resolve module path
JSRT_ResolvedPath* jsrt_resolve_path(
    JSContext* ctx,
    const char* specifier,
    const char* base_path
);

// Free resolved path
void jsrt_free_resolved_path(JSRT_ResolvedPath* resolved);
```

**Migration Impact**: ✅ **LOW** - New API, no breaking changes

---

### Format Detection

#### Before (Legacy System)

```c
// Format detection was implicit, based only on file extension
// .mjs → ESM
// .cjs → CommonJS
// .js → CommonJS (default)
```

#### After (New System)

```c
// Explicit format detection with three-stage algorithm
typedef enum {
  FORMAT_UNKNOWN = 0,
  FORMAT_COMMONJS = 1,
  FORMAT_ESM = 2,
  FORMAT_JSON = 3,
  FORMAT_BUILTIN = 4
} JSRT_ModuleFormat;

// Detect module format
JSRT_ModuleFormat jsrt_detect_module_format(
    JSContext* ctx,
    const char* path
);
```

**Migration Impact**: ✅ **LOW** - More accurate, backward compatible

---

### Protocol Handlers

#### Before (Legacy System)

```c
// Only file:// protocol supported
// No public API for protocols
```

#### After (New System)

```c
// Extensible protocol system
typedef struct {
  char* source;
  size_t source_len;
} JSRT_ProtocolLoadResult;

typedef JSRT_ProtocolLoadResult (*JSRT_ProtocolHandler)(
    JSContext* ctx,
    const char* url,
    void* userdata
);

// Register custom protocol handler
void jsrt_register_protocol_handler(
    const char* protocol,
    JSRT_ProtocolHandler handler,
    void* userdata
);

// Unregister protocol handler
void jsrt_unregister_protocol_handler(const char* protocol);
```

**Migration Impact**: ✅ **NONE** - New feature, no breaking changes

---

### Error Handling

#### Before (Legacy System)

```c
// Generic JS errors
JS_ThrowTypeError(ctx, "Module not found");
JS_ThrowError(ctx, "Failed to load module");
```

#### After (New System)

```c
// Specific error codes (60+ codes across 7 categories)
typedef enum {
  // General errors (0-99)
  JSRT_MODULE_ERROR_UNKNOWN = 0,
  JSRT_MODULE_ERROR_NULL_POINTER = 1,

  // Cache errors (100-199)
  JSRT_MODULE_ERROR_CACHE_FULL = 100,

  // Resolution errors (200-299)
  JSRT_MODULE_ERROR_MODULE_NOT_FOUND = 200,
  JSRT_MODULE_ERROR_INVALID_SPECIFIER = 201,

  // ... and 57 more error codes
} JSRT_ModuleErrorCode;

// Create detailed error with context
JSRT_ModuleError* jsrt_module_error_create(
    JSRT_ModuleErrorCode code,
    const char* message,
    const char* file,
    int line
);

// Throw JavaScript error
void jsrt_module_error_throw(JSContext* ctx, JSRT_ModuleError* error);
```

**Migration Impact**: ✅ **LOW** - Better error messages, no breaking changes

---

## JavaScript API Changes

### Module Loading (User Code)

#### Before (Legacy System)

```javascript
// CommonJS
const fs = require('fs');
const myModule = require('./my-module');
const lodash = require('lodash');

// ESM
import fs from 'fs';
import { readFile } from 'fs';
import myModule from './my-module.mjs';
```

#### After (New System)

```javascript
// CommonJS (unchanged)
const fs = require('fs');
const myModule = require('./my-module');
const lodash = require('lodash');

// ESM (unchanged)
import fs from 'fs';
import { readFile } from 'fs';
import myModule from './my-module.mjs';

// NEW: Protocol support
const remote = require('http://example.com/module.js');
import config from 'https://cdn.example.com/config.mjs';
```

**Migration Impact**: ✅ **NONE** - Fully backward compatible, new features available

---

### package.json Handling

#### Before (Legacy System)

```json
{
  "main": "index.js"
}
```

Only `main` field was supported.

#### After (New System)

```json
{
  "main": "index.js",
  "type": "module",
  "module": "index.mjs",
  "exports": {
    ".": "./index.js",
    "./utils": "./lib/utils.js"
  }
}
```

Full Node.js package.json support:
- ✅ `type` field ("module" or "commonjs")
- ✅ `module` field (ESM entry point)
- ✅ `main` field (CommonJS entry point)
- ⚠️ `exports` field (partial support)

**Migration Impact**: ✅ **LOW** - Backward compatible, more options available

---

## Migration Steps

### For Application Developers (JavaScript)

**Step 1**: No action required
- The new system is fully backward compatible
- All existing `require()` and `import` statements work unchanged

**Step 2**: Optionally adopt new features
- Use protocol-based imports: `import 'http://...'`
- Use `type: "module"` in package.json for ESM
- Rely on improved format detection for `.js` files

**Step 3**: Test your application
```bash
make test      # Run unit tests
make wpt       # Run Web Platform Tests
```

### For Embedders (C API)

**Step 1**: Update header includes (if using module APIs directly)

```c
// Before (old location)
#include "std/module.h"

// After (new location and APIs)
#include "module/module.h"          // For legacy compatibility functions
#include "module/module_loader.h"   // For new module loader APIs
```

**Step 2**: Update module loader usage (if using directly)

```c
// Before
JSModuleDef* mod = JSRT_ModuleLoader(ctx, "my-module", opaque);

// After (legacy functions still available)
JSModuleDef* mod = JSRT_StdModuleLoader(ctx, "my-module", opaque);

// Or use new API
JSRT_Runtime* rt = get_runtime();
JSValue module = jsrt_load_module(rt->module_loader, "my-module", base_path);
```

**Step 3**: Test integration
```bash
make clean && make     # Build
make test              # Test
```

### For Contributors (Adding Features)

**Step 1**: Familiarize with new architecture
- Read `docs/module-system-architecture.md`
- Read `docs/module-system-api.md`
- Study existing implementations in `src/module/`

**Step 2**: Use extension points
- Add protocol handlers via `jsrt_register_protocol_handler()`
- Add format detectors in `module_loader.c`
- Add module loaders in `loaders/`

**Step 3**: Follow coding standards
```bash
make format    # Format code
make test      # Run tests
make wpt       # Check compliance
```

---

## Breaking Changes

### None for Normal Usage

✅ **JavaScript code**: Fully backward compatible
✅ **Runtime initialization**: No changes needed
✅ **Module loading**: Transparent to users

### Potentially Affected: Direct Module API Users

⚠️ **Only affects**: Embedders using low-level module functions directly

**Changed functions**:
- `JSRT_ModuleLoader()` → `JSRT_StdModuleLoader()` (legacy compat)
- `JSRT_ModuleNormalize()` → `JSRT_StdModuleNormalize()` (legacy compat)

**Recommended action**: Use legacy functions for now, migrate to new API later

---

## Deprecation Timeline

### Current Status (October 2025)

- ✅ New system: Active and recommended
- ✅ Old system: Deprecated but available via `JSRT_StdModule*()` functions
- ✅ Both systems: Coexist peacefully

### Future Timeline

**Phase 1 (Current)**: Dual support
- Both old and new systems available
- Old system functions renamed with `Std` prefix
- No breaking changes

**Phase 2 (Future)**: Deprecation warnings
- Add deprecation warnings to legacy functions
- Encourage migration to new APIs
- Timeline: TBD

**Phase 3 (Future)**: Legacy removal
- Remove legacy `JSRT_StdModule*()` functions
- Complete migration to new system
- Timeline: TBD (at least 6 months after Phase 2)

---

## Troubleshooting

### Issue: Module not found after migration

**Symptom**: `Error: Cannot find module 'my-module'`

**Solution**: Check path resolution
```javascript
// Debug path resolution
console.log(__dirname);  // CommonJS
console.log(import.meta.url);  // ESM
```

The new resolver is stricter about relative paths. Ensure:
- Relative imports start with `./` or `../`
- Bare specifiers are in `node_modules/`

### Issue: Wrong module format detected

**Symptom**: `SyntaxError: Unexpected token 'export'` in `.js` file

**Solution**: Use `type` field in package.json
```json
{
  "type": "module"
}
```

Or rename files:
- `.mjs` for ESM
- `.cjs` for CommonJS

### Issue: HTTP/HTTPS modules not loading

**Symptom**: `Error: Unknown protocol: http://`

**Solution**: Ensure protocol handlers are initialized
```c
// In your runtime init:
jsrt_init_protocol_handlers();  // Should be automatic
```

Check that HTTP protocol handler is registered:
```bash
# Debug build
make jsrt_g
JSRT_DEBUG=1 ./bin/jsrt_g your_script.js
```

### Issue: Memory leaks after migration

**Symptom**: AddressSanitizer reports leaks

**Solution**: Ensure proper cleanup
```c
// Runtime cleanup should call:
jsrt_module_loader_free(rt->module_loader);
jsrt_cleanup_protocol_handlers();
```

If still leaking, run with ASAN:
```bash
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m script.js
```

### Issue: Performance regression

**Symptom**: Module loading slower than before

**Possible causes**:
1. Cache not enabled (should be automatic)
2. Content analysis for every `.js` file

**Solution**: Check cache statistics
```c
// Enable debug logging
JSRT_Debug("Cache stats - hits: %zu, misses: %zu",
           loader->cache->stats.hits,
           loader->cache->stats.misses);
```

Expected hit rate after warmup: 85-95%

---

## FAQ

### Q: Do I need to change my JavaScript code?

**A**: No, the new system is fully backward compatible. All existing `require()` and `import` statements work unchanged.

### Q: What about my existing modules in node_modules?

**A**: They work exactly as before. The new resolver is Node.js-compatible and handles all npm packages.

### Q: Can I mix CommonJS and ESM?

**A**: Yes, the new system supports full interop between CommonJS and ESM modules (same as before, but with better format detection).

### Q: Should I migrate to the new C API?

**A**: Only if you're an embedder using low-level module functions directly. For most users, no action needed.

### Q: When will legacy functions be removed?

**A**: No timeline set yet. Legacy functions will remain available with deprecation warnings for at least 6 months before removal.

### Q: Can I use custom protocols?

**A**: Yes! Register your protocol handler:
```c
jsrt_register_protocol_handler("myproto", my_handler, userdata);
```

Then use in JavaScript:
```javascript
import data from 'myproto://resource';
```

### Q: How do I report migration issues?

**A**: File an issue at the jsrt repository with:
- Migration steps you followed
- Error messages
- Expected vs actual behavior
- Minimal reproduction case

---

## Resources

### Documentation
- **Architecture**: `docs/module-system-architecture.md`
- **API Reference**: `docs/module-system-api.md`
- **Validation Report**: `docs/plan/module-loader-phase7-validation.md`
- **Project Guidelines**: `CLAUDE.md`

### Source Code
- **New System**: `src/module/`
- **Legacy Compatibility**: `src/module/module.c` (moved from src/std/)
- **Runtime Integration**: `src/runtime.c`

### Tests
- **Module Tests**: `test/module/`
- **Integration Tests**: `test/test_module_*.js`

### Getting Help
- File issues at GitHub repository
- Check `CLAUDE.md` for development guidelines
- Review test cases for usage examples

---

**Document Version**: 1.0
**Last Updated**: 2025-10-12
**Status**: Complete
