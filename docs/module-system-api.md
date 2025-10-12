# JSRT Module System API Documentation

**Version**: 1.0
**Date**: 2025-10-12
**Status**: Production Ready

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Core API](#core-api)
4. [Path Resolution API](#path-resolution-api)
5. [Format Detection API](#format-detection-api)
6. [Protocol Handler API](#protocol-handler-api)
7. [Module Loader API](#module-loader-api)
8. [Usage Examples](#usage-examples)
9. [Error Handling](#error-handling)
10. [Best Practices](#best-practices)

---

## Overview

The JSRT module system provides a unified, extensible architecture for loading JavaScript modules in multiple formats (CommonJS, ESM) from various sources (file system, HTTP, etc.).

### Key Features

- **Unified Module Loading**: Single API for CommonJS and ES modules
- **Protocol-Based Loading**: Extensible protocol handler system (file://, http://, https://)
- **Automatic Format Detection**: Smart detection based on extension, package.json, or content
- **Cross-Platform**: Works on Windows, Linux, and macOS
- **Node.js Compatible**: Implements Node.js module resolution algorithm
- **High Performance**: Efficient caching with FNV-1a hash-based cache
- **Memory Safe**: Validated with AddressSanitizer, zero leaks

### Directory Structure

```
src/module/
├── core/           # Core module loading infrastructure
├── resolver/       # Path resolution (npm, package.json, etc.)
├── detector/       # Format detection (CommonJS, ESM, JSON)
├── protocols/      # Protocol handlers (file://, http://, https://)
├── loaders/        # Format-specific loaders (CommonJS, ESM, builtin)
└── util/           # Utilities (errors, debug logging)
```

---

## Architecture

### Data Flow

```
User Code (require/import)
    ↓
Module Loader Core (jsrt_load_module)
    ↓
    ├─→ Cache Check
    ├─→ Path Resolution (jsrt_resolve_path)
    ├─→ Format Detection (jsrt_detect_module_format)
    ├─→ Protocol Dispatch (jsrt_load_content_by_protocol)
    └─→ Format Loader (CommonJS/ESM/Builtin)
    ↓
Cached Result
    ↓
Module Exports
```

### Component Interaction

```
┌─────────────────────────────────────────────────────────┐
│              Module Loader Core                          │
│         (module_loader.c)                                │
│  - Orchestrates all module loading operations            │
│  - Manages cache, resolution, detection, protocols       │
└───┬────────────────────────────────────────────────┬────┘
    │                                                 │
    ├─→ Module Cache ───────────────────────────────┤
    │   (module_cache.c)                             │
    │   - FNV-1a hash map                            │
    │   - Collision handling                         │
    │   - Statistics tracking                        │
    │                                                 │
    ├─→ Path Resolver ──────────────────────────────┤
    │   (path_resolver.c)                            │
    │   - Specifier parsing                          │
    │   - npm resolution                             │
    │   - package.json parsing                       │
    │   - Cross-platform paths                       │
    │                                                 │
    ├─→ Format Detector ────────────────────────────┤
    │   (format_detector.c)                          │
    │   - Extension detection                        │
    │   - package.json type                          │
    │   - Content analysis                           │
    │                                                 │
    ├─→ Protocol Registry ──────────────────────────┤
    │   (protocol_registry.c)                        │
    │   - Handler registration                       │
    │   - Protocol dispatch                          │
    │   - file://, http://, https://                 │
    │                                                 │
    └─→ Format Loaders ─────────────────────────────┘
        (commonjs_loader.c, esm_loader.c, builtin_loader.c)
        - CommonJS wrapper execution
        - ESM compilation
        - Builtin module loading
```

---

## Core API

### Module Context

#### `JSRT_ModuleLoader`

Main module loader context structure.

```c
typedef struct JSRT_ModuleLoader {
    JSContext* ctx;                  // JavaScript context
    JSRT_ModuleCache* cache;         // Module cache
    bool enable_cache;               // Enable caching
    bool allow_http_imports;         // Allow HTTP imports
    bool node_compat_mode;           // Node.js compatibility

    // Statistics
    uint64_t loads_total;            // Total load attempts
    uint64_t loads_success;          // Successful loads
    uint64_t loads_failed;           // Failed loads
    uint64_t cache_hits;             // Cache hits
    uint64_t cache_misses;           // Cache misses
} JSRT_ModuleLoader;
```

#### `jsrt_module_loader_create()`

Create a new module loader instance.

```c
JSRT_ModuleLoader* jsrt_module_loader_create(JSContext* ctx);
```

**Parameters:**
- `ctx`: JavaScript context

**Returns:**
- Pointer to module loader, or NULL on error

**Example:**
```c
JSRT_ModuleLoader* loader = jsrt_module_loader_create(ctx);
if (!loader) {
    // Handle error
}
```

#### `jsrt_module_loader_free()`

Free module loader and cleanup resources.

```c
void jsrt_module_loader_free(JSRT_ModuleLoader* loader);
```

**Parameters:**
- `loader`: Module loader to free

**Example:**
```c
jsrt_module_loader_free(loader);
```

#### `jsrt_module_loader_set()`

Set module loader on context.

```c
void jsrt_module_loader_set(JSContext* ctx, JSRT_ModuleLoader* loader);
```

#### `jsrt_module_loader_get()`

Get module loader from context.

```c
JSRT_ModuleLoader* jsrt_module_loader_get(JSContext* ctx);
```

---

## Path Resolution API

### Resolved Path Structure

```c
typedef struct {
    char* resolved_path;      // Absolute path or URL
    char* protocol;           // Protocol (file, http, https, etc.)
    bool is_builtin;          // Is jsrt: or node: builtin
} JSRT_ResolvedPath;
```

### Resolution Functions

#### `jsrt_resolve_path()`

Resolve module specifier to absolute path/URL.

```c
JSRT_ResolvedPath* jsrt_resolve_path(JSContext* ctx,
                                      const char* specifier,
                                      const char* base_path);
```

**Parameters:**
- `ctx`: JavaScript context
- `specifier`: Module specifier (e.g., "./foo", "lodash", "http://...")
- `base_path`: Base path for relative resolution (can be NULL)

**Returns:**
- Resolved path structure, or NULL on error

**Example:**
```c
// Resolve relative path
JSRT_ResolvedPath* resolved = jsrt_resolve_path(ctx, "./lib/utils.js", "/app/src/main.js");
if (resolved) {
    printf("Resolved to: %s\n", resolved->resolved_path);
    jsrt_resolved_path_free(resolved);
}

// Resolve npm package
resolved = jsrt_resolve_path(ctx, "lodash", "/app/src/main.js");
if (resolved) {
    printf("Found at: %s\n", resolved->resolved_path);
    jsrt_resolved_path_free(resolved);
}
```

#### `jsrt_resolved_path_free()`

Free resolved path structure.

```c
void jsrt_resolved_path_free(JSRT_ResolvedPath* resolved);
```

---

## Format Detection API

### Module Format Enum

```c
typedef enum {
    JSRT_MODULE_FORMAT_UNKNOWN,     // Unknown format
    JSRT_MODULE_FORMAT_COMMONJS,    // CommonJS (.cjs, require/exports)
    JSRT_MODULE_FORMAT_ESM,         // ES Module (.mjs, import/export)
    JSRT_MODULE_FORMAT_JSON         // JSON module
} JSRT_ModuleFormat;
```

### Detection Functions

#### `jsrt_detect_module_format()`

Detect module format using extension, package.json, and content analysis.

```c
JSRT_ModuleFormat jsrt_detect_module_format(JSContext* ctx,
                                             const char* path,
                                             const char* content,
                                             size_t content_length);
```

**Parameters:**
- `ctx`: JavaScript context
- `path`: Module file path
- `content`: Module content (optional, can be NULL)
- `content_length`: Content length (0 if content is NULL)

**Returns:**
- Detected module format

**Detection Priority:**
1. File extension (.cjs → CommonJS, .mjs → ESM, .json → JSON)
2. package.json "type" field
3. Content analysis (import/export vs require/module.exports)

**Example:**
```c
JSRT_ModuleFormat format = jsrt_detect_module_format(ctx, "/app/lib/utils.js", NULL, 0);
switch (format) {
    case JSRT_MODULE_FORMAT_COMMONJS:
        printf("CommonJS module\n");
        break;
    case JSRT_MODULE_FORMAT_ESM:
        printf("ES module\n");
        break;
    case JSRT_MODULE_FORMAT_JSON:
        printf("JSON module\n");
        break;
    default:
        printf("Unknown format\n");
}
```

---

## Protocol Handler API

### Protocol Handler Structure

```c
typedef struct {
    const char* protocol_name;      // "file", "http", "https", etc.

    // Load function
    JSRT_ReadFileResult (*load)(const char* url, void* user_data);

    // Cleanup function (optional)
    void (*cleanup)(void* user_data);

    void* user_data;                // Custom handler data
} JSRT_ProtocolHandler;
```

### Registry Functions

#### `jsrt_init_protocol_handlers()`

Initialize protocol handler registry. Call once at startup.

```c
void jsrt_init_protocol_handlers(void);
```

#### `jsrt_cleanup_protocol_handlers()`

Cleanup protocol handler registry. Call once at shutdown.

```c
void jsrt_cleanup_protocol_handlers(void);
```

#### `jsrt_register_protocol_handler()`

Register a custom protocol handler.

```c
bool jsrt_register_protocol_handler(const char* protocol,
                                     const JSRT_ProtocolHandler* handler);
```

**Parameters:**
- `protocol`: Protocol name (e.g., "zip", "data")
- `handler`: Handler structure

**Returns:**
- true on success, false if already registered

**Example:**
```c
// Custom protocol handler for zip:// URLs
JSRT_ReadFileResult zip_load(const char* url, void* user_data) {
    // Implementation...
}

JSRT_ProtocolHandler zip_handler = {
    .protocol_name = "zip",
    .load = zip_load,
    .cleanup = NULL,
    .user_data = NULL
};

if (jsrt_register_protocol_handler("zip", &zip_handler)) {
    printf("Zip protocol registered\n");
}
```

#### `jsrt_get_protocol_handler()`

Get registered protocol handler.

```c
const JSRT_ProtocolHandler* jsrt_get_protocol_handler(const char* protocol);
```

---

## Module Loader API

### Main Loading Functions

#### `jsrt_load_module()`

Load a module (automatically detects format).

```c
JSValue jsrt_load_module(JSRT_ModuleLoader* loader,
                         const char* specifier,
                         const char* base_path);
```

**Parameters:**
- `loader`: Module loader instance
- `specifier`: Module specifier
- `base_path`: Base path for relative resolution

**Returns:**
- Module exports as JSValue, or JS_EXCEPTION on error

**Example:**
```c
// Load CommonJS module
JSValue exports = jsrt_load_module(loader, "./utils.js", "/app/src/main.js");
if (JS_IsException(exports)) {
    // Handle error
}

// Load npm package
exports = jsrt_load_module(loader, "lodash", "/app/src/main.js");
if (!JS_IsException(exports)) {
    // Use exports
    JS_FreeValue(ctx, exports);
}
```

### Format-Specific Loaders

#### `jsrt_load_commonjs_module()`

Load CommonJS module explicitly.

```c
JSValue jsrt_load_commonjs_module(JSContext* ctx,
                                   JSRT_ModuleLoader* loader,
                                   const char* resolved_path,
                                   const char* specifier);
```

**CommonJS Environment:**
- `exports`: Module exports object
- `require`: Require function
- `module`: Module object with exports property
- `__filename`: Absolute path to module
- `__dirname`: Directory containing module

#### `jsrt_load_esm_module()`

Load ES module explicitly.

```c
JSModuleDef* jsrt_load_esm_module(JSContext* ctx,
                                   JSRT_ModuleLoader* loader,
                                   const char* resolved_path,
                                   const char* specifier);
```

**ES Module Features:**
- `import.meta.url`: Module URL
- `import.meta.resolve()`: Resolve relative specifiers

#### `jsrt_load_builtin_module()`

Load builtin module (jsrt: or node:).

```c
JSValue jsrt_load_builtin_module(JSContext* ctx,
                                  JSRT_ModuleLoader* loader,
                                  const char* specifier);
```

**Supported Builtins:**
- `jsrt:assert`: Assertion utilities
- `jsrt:process`: Process information
- `jsrt:ffi`: Foreign function interface
- `node:*`: Node.js compatible modules (when enabled)

---

## Usage Examples

### Example 1: Basic Module Loading

```c
#include "module/core/module_loader.h"

// Initialize runtime
JSRT_Runtime* rt = JSRT_RuntimeNew();

// Module loader is automatically created in runtime
JSRT_ModuleLoader* loader = rt->module_loader;

// Load a CommonJS module
JSValue utils = jsrt_load_module(loader, "./lib/utils.js", "/app/src/main.js");
if (JS_IsException(utils)) {
    JSValue exception = JS_GetException(rt->ctx);
    // Handle error
    JS_FreeValue(rt->ctx, exception);
} else {
    // Use the module
    JSValue result = JS_Call(rt->ctx, utils, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(rt->ctx, result);
    JS_FreeValue(rt->ctx, utils);
}

// Cleanup
JSRT_RuntimeFree(rt);
```

### Example 2: Custom Protocol Handler

```c
#include "module/protocols/protocol_registry.h"

// Custom data:// protocol handler
JSRT_ReadFileResult data_protocol_load(const char* url, void* user_data) {
    JSRT_ReadFileResult result = JSRT_ReadFileResultDefault();

    // Parse data URL: data:text/javascript,console.log('hello')
    const char* comma = strchr(url, ',');
    if (!comma) {
        result.error = JSRT_READ_FILE_ERROR_READ_ERROR;
        return result;
    }

    const char* data = comma + 1;
    result.data = strdup(data);
    result.size = strlen(data);
    result.error = JSRT_READ_FILE_OK;

    return result;
}

// Register handler
JSRT_ProtocolHandler data_handler = {
    .protocol_name = "data",
    .load = data_protocol_load,
    .cleanup = NULL,
    .user_data = NULL
};

jsrt_register_protocol_handler("data", &data_handler);

// Now can load data URLs
JSValue module = jsrt_load_module(loader, "data:text/javascript,export const x = 42", NULL);
```

### Example 3: Module Format Detection

```c
#include "module/detector/format_detector.h"

// Detect format of a file
JSRT_ModuleFormat format = jsrt_detect_module_format(ctx, "/app/lib/utils.js", NULL, 0);

switch (format) {
    case JSRT_MODULE_FORMAT_COMMONJS:
        printf("Load as CommonJS\n");
        break;
    case JSRT_MODULE_FORMAT_ESM:
        printf("Load as ES module\n");
        break;
    case JSRT_MODULE_FORMAT_JSON:
        printf("Parse as JSON\n");
        break;
    default:
        printf("Unknown format, use default\n");
}
```

### Example 4: Path Resolution

```c
#include "module/resolver/path_resolver.h"

// Resolve various specifier types
const char* base = "/app/src/main.js";

// Relative path
JSRT_ResolvedPath* resolved = jsrt_resolve_path(ctx, "./utils.js", base);
printf("Resolved to: %s\n", resolved->resolved_path);
jsrt_resolved_path_free(resolved);

// npm package
resolved = jsrt_resolve_path(ctx, "lodash", base);
printf("Found at: %s\n", resolved->resolved_path);
jsrt_resolved_path_free(resolved);

// Absolute path
resolved = jsrt_resolve_path(ctx, "/etc/config.json", base);
printf("Absolute: %s\n", resolved->resolved_path);
jsrt_resolved_path_free(resolved);

// URL
resolved = jsrt_resolve_path(ctx, "https://cdn.example.com/module.js", NULL);
printf("URL: %s\n", resolved->resolved_path);
jsrt_resolved_path_free(resolved);
```

---

## Error Handling

### Error Categories

The module system defines 60+ error codes across 7 categories:

```c
// Resolution errors
JSRT_MODULE_NOT_FOUND              // Module not found
JSRT_MODULE_INVALID_SPECIFIER      // Invalid specifier format
JSRT_MODULE_RESOLUTION_FAILED      // General resolution failure

// Loading errors
JSRT_MODULE_LOAD_FAILED            // Failed to load module content
JSRT_MODULE_PARSE_ERROR            // Syntax error in module
JSRT_MODULE_COMPILE_ERROR          // Compilation failed

// Type errors
JSRT_MODULE_TYPE_MISMATCH          // Wrong module type
JSRT_MODULE_FORMAT_UNKNOWN         // Unknown module format

// Protocol errors
JSRT_MODULE_PROTOCOL_NOT_SUPPORTED // Unsupported protocol
JSRT_MODULE_PROTOCOL_ERROR         // Protocol handler error

// Cache errors
JSRT_MODULE_CACHE_ERROR            // Cache operation failed

// Security errors
JSRT_MODULE_SECURITY_ERROR         // Security validation failed
JSRT_MODULE_PERMISSION_DENIED      // Access denied

// System errors
JSRT_MODULE_INTERNAL_ERROR         // Internal system error
JSRT_MODULE_OUT_OF_MEMORY          // Memory allocation failed
```

### Error Handling Pattern

```c
JSValue module = jsrt_load_module(loader, specifier, base_path);

if (JS_IsException(module)) {
    // Get exception details
    JSValue exception = JS_GetException(ctx);

    // Get error message
    const char* error_msg = JS_ToCString(ctx, exception);
    fprintf(stderr, "Module loading error: %s\n", error_msg);
    JS_FreeCString(ctx, error_msg);

    // Cleanup
    JS_FreeValue(ctx, exception);

    return -1;
}

// Success - use module
// ...

JS_FreeValue(ctx, module);
```

---

## Best Practices

### Memory Management

1. **Always free resolved paths:**
   ```c
   JSRT_ResolvedPath* resolved = jsrt_resolve_path(ctx, spec, base);
   // Use resolved...
   jsrt_resolved_path_free(resolved);  // Don't forget!
   ```

2. **Free JSValues:**
   ```c
   JSValue module = jsrt_load_module(loader, spec, base);
   // Use module...
   JS_FreeValue(ctx, module);  // Required!
   ```

3. **Check allocations:**
   ```c
   JSRT_ModuleLoader* loader = jsrt_module_loader_create(ctx);
   if (!loader) {
       // Handle allocation failure
   }
   ```

### Performance

1. **Enable caching:**
   ```c
   loader->enable_cache = true;  // Default, but be explicit
   ```

2. **Reuse loader instances:**
   ```c
   // Create once, use many times
   JSRT_ModuleLoader* loader = jsrt_module_loader_create(ctx);
   for (int i = 0; i < N; i++) {
       jsrt_load_module(loader, modules[i], base);
   }
   jsrt_module_loader_free(loader);
   ```

3. **Monitor cache statistics:**
   ```c
   printf("Cache hits: %lu\n", loader->cache_hits);
   printf("Cache misses: %lu\n", loader->cache_misses);
   printf("Hit rate: %.2f%%\n",
          100.0 * loader->cache_hits / (loader->cache_hits + loader->cache_misses));
   ```

### Security

1. **Validate HTTP imports:**
   ```c
   loader->allow_http_imports = false;  // Disable for security
   ```

2. **Check protocol safety:**
   ```c
   JSRT_ResolvedPath* resolved = jsrt_resolve_path(ctx, spec, base);
   if (resolved && strcmp(resolved->protocol, "http") == 0) {
       // Validate URL is from trusted domain
   }
   ```

### Debugging

1. **Enable debug logging:**
   ```bash
   # Build with DEBUG flag
   make jsrt_g

   # Run with debug output
   ./target/debug/jsrt your_script.js
   ```

2. **Check loader statistics:**
   ```c
   printf("Total loads: %lu\n", loader->loads_total);
   printf("Successful: %lu\n", loader->loads_success);
   printf("Failed: %lu\n", loader->loads_failed);
   ```

3. **Use ASAN for memory debugging:**
   ```bash
   make jsrt_m
   ./target/asan/jsrt your_script.js
   ```

---

## API Reference Summary

### Core Functions
| Function | Description |
|----------|-------------|
| `jsrt_module_loader_create()` | Create module loader |
| `jsrt_module_loader_free()` | Free module loader |
| `jsrt_load_module()` | Load module (auto-detect format) |

### Resolution Functions
| Function | Description |
|----------|-------------|
| `jsrt_resolve_path()` | Resolve specifier to path/URL |
| `jsrt_resolved_path_free()` | Free resolved path |

### Detection Functions
| Function | Description |
|----------|-------------|
| `jsrt_detect_module_format()` | Detect module format |

### Protocol Functions
| Function | Description |
|----------|-------------|
| `jsrt_init_protocol_handlers()` | Initialize registry |
| `jsrt_register_protocol_handler()` | Register handler |
| `jsrt_get_protocol_handler()` | Get handler |
| `jsrt_cleanup_protocol_handlers()` | Cleanup registry |

### Loader Functions
| Function | Description |
|----------|-------------|
| `jsrt_load_commonjs_module()` | Load CommonJS explicitly |
| `jsrt_load_esm_module()` | Load ESM explicitly |
| `jsrt_load_builtin_module()` | Load builtin module |

---

**For more information:**
- Architecture: See `docs/module-system-architecture.md`
- Testing: See `docs/plan/module-loader-phase7-validation.md`
- Migration: See `docs/module-system-migration.md`
- Source Code: See `src/module/` directory
