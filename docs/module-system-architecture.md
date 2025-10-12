# Module System Architecture

## Overview

This document describes the architecture of the jsrt module loading system - a unified, extensible infrastructure for loading JavaScript modules across different formats (CommonJS, ESM, JSON) and protocols (file://, http://, https://).

**Design Philosophy:**
- **Modularity**: Clean separation between path resolution, format detection, protocol handling, and loading
- **Extensibility**: Easy to add new protocols, formats, and loaders
- **Performance**: Efficient caching with FNV-1a hashing, minimal allocations
- **Compatibility**: Node.js-compatible resolution algorithm, supports both CommonJS and ESM
- **Safety**: Comprehensive error handling, memory safety, circular dependency detection

---

## System Components

The module loading system consists of six major subsystems:

```
┌─────────────────────────────────────────────────────────────────┐
│                      JSRT Runtime (runtime.c)                    │
│  - Lifecycle management (init/cleanup)                           │
│  - Protocol registry initialization                              │
│  - Module loader creation                                        │
└───────────────────────┬─────────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────────────┐
│            Module Loader Core (module/module_loader.c)           │
│  - Main dispatcher and coordinator                               │
│  - Module cache integration                                      │
│  - Format detection dispatch                                     │
│  - Protocol handler dispatch                                     │
│  - Loader dispatch (CommonJS/ESM)                                │
└──────┬────────────┬────────────┬─────────────┬──────────────────┘
       │            │            │             │
       ▼            ▼            ▼             ▼
┌──────────┐  ┌──────────┐  ┌────────┐  ┌─────────┐
│  Cache   │  │  Resolver│  │Detector│  │Protocols│
│  System  │  │  System  │  │ System │  │ System  │
└──────────┘  └──────────┘  └────────┘  └─────────┘
       │            │            │             │
       │            │            │             │
       └────────────┴────────────┴─────────────┘
                        │
                        ▼
              ┌─────────────────┐
              │  Module Loaders │
              │  (CJS/ESM/JSON) │
              └─────────────────┘
```

### 1. Core Infrastructure (`module/core/`)

**Components:**
- **module_context.c**: Runtime context and lifecycle management
- **module_cache.c**: FNV-1a hash-based module cache
- **module_errors.c**: Centralized error handling (60+ error codes)
- **module_debug.c**: Debug logging infrastructure

**Responsibilities:**
- Module loader initialization and cleanup
- Cache management and statistics
- Error creation and propagation
- Debug logging coordination

### 2. Path Resolution (`module/resolver/`)

**Components:**
- **path_util.c**: Cross-platform path operations
- **module_resolver.c**: Node.js-compatible resolution algorithm

**Responsibilities:**
- Resolve relative paths (./file, ../file)
- Resolve absolute paths (/absolute/path)
- Resolve bare specifiers (lodash → node_modules/lodash)
- Parse and validate package.json
- Handle cross-platform path differences (Windows/Unix)

### 3. Format Detection (`module/detector/`)

**Components:**
- **extension_detector.c**: File extension analysis
- **package_type_detector.c**: package.json type field parsing
- **content_analyzer.c**: Source code content analysis

**Responsibilities:**
- Three-stage detection: extension → package.json → content
- Detect CommonJS vs ESM vs JSON format
- Handle ambiguous .js files
- Exclude keywords in comments/strings

### 4. Protocol Handlers (`module/protocols/`)

**Components:**
- **protocol_registry.c**: Protocol handler registry
- **file_handler.c**: Local filesystem protocol (file://)
- **http_handler.c**: HTTP/HTTPS protocol handlers

**Responsibilities:**
- Manage protocol handler lifecycle
- Dispatch to appropriate handler based on URL scheme
- Load module source from various protocols
- Provide extensibility for custom protocols

### 5. Module Loaders (`module/loaders/`)

**Components:**
- **cjs_loader.c**: CommonJS module loader
- **esm_loader.c**: ES module loader
- **json_loader.c**: JSON module loader
- **builtin_loader.c**: Built-in module loader (jsrt:, node:)

**Responsibilities:**
- Format-specific module compilation
- Wrapper function generation (CommonJS)
- Module environment setup (__filename, __dirname, etc.)
- Integration with QuickJS module system

### 6. Module Cache (`module/core/module_cache.c`)

**Cache Implementation:**
- Hash-based cache using FNV-1a algorithm
- Configurable capacity (default: 128 entries)
- Collision handling with linear probing
- Statistics tracking (hits, misses, collisions)

---

## Data Flow

### Module Loading Pipeline

```
User Code
   │
   │ require('module') or import 'module'
   ▼
┌─────────────────────────────────────────────┐
│ 1. Main Dispatcher (module_loader.c)        │
│    - Entry point for all module loads       │
│    - Check cache first                      │
└─────────────┬───────────────────────────────┘
              │ Cache miss
              ▼
┌─────────────────────────────────────────────┐
│ 2. Path Resolution (module_resolver.c)      │
│    - Resolve module specifier to full path  │
│    - Handle relative/absolute/bare          │
│    - Search node_modules if needed          │
└─────────────┬───────────────────────────────┘
              │ Resolved path
              ▼
┌─────────────────────────────────────────────┐
│ 3. Format Detection (detector/)             │
│    a. Extension (.cjs/.mjs/.js/.json)       │
│    b. package.json type field               │
│    c. Content analysis (import/export)      │
└─────────────┬───────────────────────────────┘
              │ Format determined
              ▼
┌─────────────────────────────────────────────┐
│ 4. Protocol Handler (protocols/)            │
│    - Extract protocol (file://, http://)    │
│    - Dispatch to appropriate handler        │
│    - Load source code                       │
└─────────────┬───────────────────────────────┘
              │ Source loaded
              ▼
┌─────────────────────────────────────────────┐
│ 5. Module Loader (loaders/)                 │
│    - CommonJS: Wrap and execute             │
│    - ESM: Compile as JS_EVAL_TYPE_MODULE    │
│    - JSON: Parse and return                 │
└─────────────┬───────────────────────────────┘
              │ Module loaded
              ▼
┌─────────────────────────────────────────────┐
│ 6. Cache Update (module_cache.c)            │
│    - Store in cache for future loads        │
│    - Update statistics                      │
└─────────────┬───────────────────────────────┘
              │
              ▼
         Return module to user
```

### Cache Hit Flow

```
User Code
   │
   │ require('previously-loaded-module')
   ▼
┌─────────────────────────────────────────────┐
│ Main Dispatcher (module_loader.c)           │
│    - Check cache                            │
│    - Cache HIT! ✓                           │
└─────────────┬───────────────────────────────┘
              │
              ▼
         Return cached module (FAST PATH)
```

---

## Component Interactions

### Initialization Sequence

```
Runtime Startup
   │
   ├─► 1. jsrt_init_protocol_handlers()
   │      - Initialize protocol registry
   │      - Register file:// handler
   │      - Register http://, https:// handlers
   │
   └─► 2. jsrt_module_loader_create(ctx)
          │
          ├─► a. Allocate JSRT_ModuleLoader
          │      - Create context
          │      - Initialize state
          │
          ├─► b. jsrt_module_cache_create(128)
          │      - Allocate cache with 128 slots
          │      - Initialize statistics
          │
          └─► c. Return initialized loader
```

### Module Load Sequence

```
jsrt_load_module(loader, specifier, base_path)
   │
   ├─► 1. jsrt_module_cache_get(cache, path)
   │      │
   │      ├─► Cache hit → Return cached module
   │      └─► Cache miss → Continue
   │
   ├─► 2. jsrt_resolve_path(specifier, base_path)
   │      │
   │      ├─► Relative/Absolute → Normalize path
   │      └─► Bare specifier → Search node_modules
   │
   ├─► 3. jsrt_detect_module_format(path)
   │      │
   │      ├─► Check extension (.cjs → CJS, .mjs → ESM)
   │      ├─► Check package.json type field
   │      └─► Analyze content (import/export keywords)
   │
   ├─► 4. jsrt_protocol_handler_load(path)
   │      │
   │      ├─► Extract protocol (file://, http://)
   │      ├─► Dispatch to handler
   │      └─► Load source code
   │
   ├─► 5. jsrt_load_<format>_module(ctx, source, path)
   │      │
   │      ├─► CommonJS: Wrap + compile + execute
   │      ├─► ESM: Compile as JS_EVAL_TYPE_MODULE
   │      └─► JSON: Parse JSON
   │
   └─► 6. jsrt_module_cache_put(cache, path, module)
          │
          └─► Store for future loads
```

### Cleanup Sequence

```
Runtime Shutdown
   │
   ├─► 1. jsrt_module_loader_free(loader)
   │      │
   │      ├─► a. jsrt_module_cache_free(cache)
   │      │      - Free all cached entries
   │      │      - Free cache structure
   │      │
   │      └─► b. Free JSRT_ModuleLoader
   │             - Free context
   │             - Free state
   │
   └─► 2. jsrt_cleanup_protocol_handlers()
          - Unregister all handlers
          - Free protocol registry
```

---

## Design Decisions

### 1. Why FNV-1a Hash for Cache?

**Rationale:**
- Fast computation (simpler than cryptographic hashes)
- Good distribution for file paths
- Low collision rate for typical module counts
- Small code footprint (important for embedded)

**Trade-offs:**
- Not cryptographically secure (not needed for cache)
- Collision handling required (linear probing)
- Fixed capacity (configurable, default 128)

### 2. Why Three-Stage Format Detection?

**Rationale:**
- Extension check is fastest (O(1))
- package.json check handles most ambiguous cases
- Content analysis is fallback for legacy code

**Trade-offs:**
- Content analysis is slower (requires reading file)
- May misdetect in edge cases (keywords in strings)
- Three-stage check adds complexity

### 3. Why Protocol Handler Registry?

**Rationale:**
- Extensibility: Easy to add new protocols (zip://, data://)
- Separation of concerns: Protocol logic isolated
- Testability: Mock handlers for testing
- Future-proof: Can support custom protocols

**Trade-offs:**
- Additional indirection (small performance cost)
- More complex architecture
- Registry management overhead

### 4. Why Separate CommonJS and ESM Loaders?

**Rationale:**
- Different compilation requirements (wrapper vs module)
- Different runtime environments (__dirname vs import.meta)
- Different error handling (sync vs potential async)
- Cleaner code separation

**Trade-offs:**
- Code duplication in some areas
- More loader functions to maintain
- Increased binary size (minimal)

### 5. Why Hash-Based Cache Instead of Tree?

**Rationale:**
- O(1) average lookup vs O(log n) tree
- Simpler implementation
- Better cache locality
- Sufficient for typical module counts (<1000)

**Trade-offs:**
- Fixed capacity (can be increased)
- Collision handling needed
- Not ordered (doesn't matter for cache)

---

## Extension Points

The architecture is designed for extensibility:

### 1. Adding a New Protocol

```c
// 1. Implement handler function
static JSRT_ProtocolLoadResult my_protocol_load(
    JSContext* ctx,
    const char* url,
    void* userdata) {
  // Load from your protocol
  JSRT_ProtocolLoadResult result;
  result.source = load_from_my_protocol(url);
  result.source_len = strlen(result.source);
  return result;
}

// 2. Register during initialization
void jsrt_init_protocol_handlers(void) {
  jsrt_protocol_registry_init();
  jsrt_register_protocol_handler("myproto", my_protocol_load, NULL);
}
```

### 2. Adding a New Format Detector

```c
// Add to jsrt_detect_module_format() in module_loader.c
JSRT_ModuleFormat jsrt_detect_module_format(
    JSContext* ctx,
    const char* path) {

  // Stage 1: Extension
  JSRT_ModuleFormat ext_format = detect_by_extension(path);
  if (ext_format != FORMAT_UNKNOWN) return ext_format;

  // Stage 2: package.json
  JSRT_ModuleFormat pkg_format = detect_by_package_type(ctx, path);
  if (pkg_format != FORMAT_UNKNOWN) return pkg_format;

  // Stage 3: Content
  JSRT_ModuleFormat content_format = detect_by_content(ctx, path);
  if (content_format != FORMAT_UNKNOWN) return content_format;

  // NEW: Stage 4: Your custom detector
  JSRT_ModuleFormat custom_format = my_custom_detector(ctx, path);
  if (custom_format != FORMAT_UNKNOWN) return custom_format;

  return FORMAT_COMMONJS; // Default
}
```

### 3. Adding a New Module Loader

```c
// 1. Implement loader function
JSValue jsrt_load_my_format_module(
    JSContext* ctx,
    const char* source,
    size_t source_len,
    const char* path) {
  // Parse and compile your format
  return compiled_module;
}

// 2. Add to loader dispatch in module_loader.c
JSValue jsrt_load_module_with_format(
    JSRT_ModuleLoader* loader,
    const char* path,
    JSRT_ModuleFormat format) {

  switch (format) {
    case FORMAT_COMMONJS:
      return jsrt_load_cjs_module(loader->ctx, source, path);
    case FORMAT_ESM:
      return jsrt_load_esm_module(loader->ctx, source, path);
    case FORMAT_JSON:
      return jsrt_load_json_module(loader->ctx, source, path);
    case FORMAT_MY_FORMAT: // NEW
      return jsrt_load_my_format_module(loader->ctx, source, path);
    default:
      return JS_ThrowTypeError(ctx, "Unsupported format");
  }
}
```

---

## Performance Characteristics

### Time Complexity

| Operation | Average Case | Worst Case | Notes |
|-----------|--------------|------------|-------|
| Cache lookup | O(1) | O(n) | Linear probing on collision |
| Path resolution | O(n) | O(n*m) | n = path segments, m = node_modules depth |
| Format detection | O(1) | O(k) | k = file size (content analysis) |
| Module load (cached) | O(1) | O(1) | Direct cache hit |
| Module load (uncached) | O(n+k) | O(n*m+k) | Resolution + loading |

### Space Complexity

| Component | Memory Usage | Notes |
|-----------|--------------|-------|
| Cache | O(c) | c = cache capacity (default 128) |
| Module loader | O(1) | Fixed overhead per loader |
| Protocol registry | O(p) | p = number of protocols (typically 3-5) |
| Path resolution | O(d) | d = directory depth (stack) |
| Format detection | O(k) | k = file size (temporary buffer) |

### Cache Performance

Based on typical application patterns:

```
Cache Size: 128 entries
Typical Hit Rate: 85-95% (after warmup)
Collision Rate: <5% (with FNV-1a)
Memory Overhead: ~8KB empty, ~50-100KB populated
```

---

## Error Handling Architecture

### Error Code Categories

The system defines 60+ error codes across 7 categories:

```
┌─────────────────────────────────────┐
│  Error Categories                   │
├─────────────────────────────────────┤
│ 1. General (0-99)                   │
│    - Unknown errors, NULL checks    │
│                                     │
│ 2. Cache (100-199)                  │
│    - Cache full, allocation failed  │
│                                     │
│ 3. Resolution (200-299)             │
│    - Module not found, invalid path │
│                                     │
│ 4. Format Detection (300-399)       │
│    - Unknown format, invalid JSON   │
│                                     │
│ 5. Protocol (400-499)               │
│    - Unknown protocol, load failed  │
│                                     │
│ 6. Loading (500-599)                │
│    - Compilation failed, circular   │
│                                     │
│ 7. System (600-699)                 │
│    - Out of memory, I/O errors      │
└─────────────────────────────────────┘
```

### Error Propagation Pattern

```
Low-Level Function
   │
   │ Error occurs
   ▼
Create error with jsrt_module_error_create()
   │
   │ Include context (file, line, code, message)
   ▼
Return NULL or error indicator
   │
   │ Caller checks return value
   ▼
Propagate up or handle locally
   │
   │ Eventually reaches JavaScript boundary
   ▼
Convert to JS exception with JS_ThrowError()
   │
   ▼
JavaScript error thrown to user
```

---

## Memory Management Strategy

### Allocation Patterns

| Type | Allocator | Free Function | Ownership |
|------|-----------|---------------|-----------|
| JS values | QuickJS (js_malloc) | JS_FreeValue | Context |
| Module structures | malloc | free | Loader |
| Path strings | malloc | free | Caller |
| Source code | malloc | free | Temporary |
| Cache entries | malloc | free | Cache |

### Lifecycle Management

```
Module Loader Creation
   │
   ├─► Allocate JSRT_ModuleLoader (malloc)
   ├─► Allocate cache (malloc)
   └─► Store JSContext reference (borrowed)

Module Load
   │
   ├─► Allocate path string (malloc)
   ├─► Allocate source buffer (malloc)
   ├─► Create JS values (js_malloc via QuickJS)
   ├─► Free path string (free)
   └─► Free source buffer (free)
       [JS values owned by context, freed on cleanup]

Module Loader Destruction
   │
   ├─► Free cache entries (free)
   ├─► Free cache structure (free)
   └─► Free JSRT_ModuleLoader (free)
       [JSContext freed separately by runtime]
```

### Memory Safety Guarantees

1. **All allocations checked**: NULL checks after every malloc
2. **Cleanup on error paths**: All error returns free partial allocations
3. **No leaks**: ASAN validated (see Phase 7 validation)
4. **No use-after-free**: Clear ownership semantics
5. **No double-free**: Single owner for each allocation

---

## Threading and Concurrency

### Current Implementation

**Single-threaded**: The current implementation assumes single-threaded access.

**Rationale:**
- QuickJS is single-threaded per runtime
- JavaScript execution is single-threaded
- Simpler implementation, no locking overhead
- Sufficient for typical embedding scenarios

### Future Considerations

For multi-threaded environments:

```
Option 1: Per-thread loaders
  - Each thread has own JSRT_ModuleLoader
  - No shared state, no locking needed
  - Higher memory usage

Option 2: Shared cache with locking
  - Mutex-protected cache operations
  - Shared modules across threads
  - Lower memory, synchronization overhead

Option 3: Immutable cache with copy-on-write
  - Read-only cache, lock-free reads
  - New cache on modification
  - Complex implementation
```

---

## Testing Architecture

### Test Organization

```
test/module/
├── core/              # Core infrastructure tests
├── detector/          # Format detection tests
├── protocols/         # Protocol handler tests
├── resolver/          # Path resolution tests
└── integration/       # End-to-end tests
```

### Test Coverage

See `docs/plan/module-loader-phase7-validation.md` for comprehensive test coverage details.

**Summary:**
- 31 test files
- ~1,374 lines of test code
- ~22% test-to-code ratio
- 177/185 tests passing (96%)
- Zero regressions from refactoring

---

## References

- **API Documentation**: `docs/module-system-api.md`
- **Validation Report**: `docs/plan/module-loader-phase7-validation.md`
- **Implementation Files**: `src/module/`
- **Test Files**: `test/module/`
- **Project Guidelines**: `CLAUDE.md`

---

**Document Version**: 1.0
**Last Updated**: 2025-10-12
**Status**: Complete
