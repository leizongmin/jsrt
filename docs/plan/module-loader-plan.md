#+TITLE: Task Plan: Module Loading Mechanism Refactoring
#+AUTHOR: Claude AI Assistant
#+DATE: 2025-10-12T00:00:00Z
#+STARTUP: overview indent
#+TODO: TODO IN-PROGRESS BLOCKED | DONE CANCELLED
#+PRIORITIES: A C B

* Task Metadata
:PROPERTIES:
:CREATED: 2025-10-12T00:00:00Z
:UPDATED: 2025-10-12T00:00:00Z
:STATUS: 🟡 PLANNING
:PROGRESS: 0/150
:COMPLETION: 0%
:END:

* 📋 Executive Summary

** Goal
Refactor jsrt's module loading system into a unified, extensible architecture that supports:
- CommonJS (require) and ESM (import) with consistent path resolution
- Multiple path formats (relative, absolute, URL-based)
- Multiple protocols (file://, http://, https://, future: zip://)
- Automatic format detection (.cjs, .mjs, .js with content analysis)
- Clean code organization in src/module/ directory

** Current State Issues
- Module loading logic scattered across multiple files (src/std/module.c, src/http/module_loader.c)
- Inconsistent path resolution between CommonJS and ESM
- Duplicate code for path handling across different loaders
- Limited extensibility for new protocols
- Format detection logic intertwined with loading logic

** Success Criteria
1. All module loading code consolidated in src/module/ directory
2. Single unified path resolver for both CommonJS and ESM
3. Protocol-based content loader architecture (file://, http://, https://, extensible for zip://)
4. Automatic format detection working correctly for all scenarios
5. All existing tests pass (make test && make wpt)
6. No memory leaks (verified with make jsrt_m)
7. Code coverage >90% for new module system

* 📊 Current State Analysis

** Existing Module Loading Code

*** File: src/std/module.c (1152 lines)
- JSRT_ModuleNormalize(): ES module path resolution
- JSRT_ModuleLoader(): ES module loading
- js_require(): CommonJS require() implementation
- Path resolution helpers (resolve_module_path, resolve_npm_module, etc.)
- Module cache management (RequireModuleCache)
- Package.json parsing for "type", "main", "module" fields
- node_modules resolution algorithm

*** File: src/http/module_loader.c (418 lines)
- jsrt_load_http_module(): ES module loading from HTTP/HTTPS
- jsrt_require_http_module(): CommonJS loading from HTTP/HTTPS
- HTTP module caching (g_http_cache)
- Content cleaning for JavaScript parsing
- Security validation integration

*** File: src/node/node_modules.c (455 lines)
- Node.js built-in module registry
- Module dependency tracking
- CommonJS and ES module initializers
- Module export registration

*** File: src/jsrt.c (388 lines)
- is_url(): URL detection helper
- download_url(): URL content fetching
- File path handling in main command execution

** Current Capabilities
✅ CommonJS require() with npm package resolution
✅ ES module import with relative/absolute paths
✅ HTTP/HTTPS module loading (both CommonJS and ESM)
✅ node_modules resolution algorithm
✅ package.json parsing (type, main, module, imports fields)
✅ Module caching for performance
✅ Security validation for HTTP modules
✅ Cross-platform path handling (Windows/Unix)

** Current Limitations
❌ Inconsistent path resolution logic between CommonJS/ESM
❌ Duplicate code for path normalization
❌ Format detection logic scattered and inconsistent
❌ No unified protocol handler architecture
❌ Hard to add new protocols (e.g., zip://)
❌ Module loading state machine unclear
❌ Error messages inconsistent across loaders
❌ Testing coverage gaps for edge cases

** Technical Debt
1. Path resolution duplicated ~3 times across files
2. Module cache has two separate implementations (CommonJS vs HTTP)
3. Package.json parsing repeated in multiple places
4. Format detection heuristics inconsistent
5. No central registry for protocol handlers
6. Memory management patterns inconsistent

* 🏗 Architecture Design

** Component Overview

*** Source Code Structure
```
src/module/
├── core/
│   ├── module_loader.c/h      # Main module loader entry point
│   ├── module_cache.c/h       # Unified module cache
│   └── module_context.c/h     # Module loading context/state
├── resolver/
│   ├── path_resolver.c/h      # Unified path resolution
│   ├── npm_resolver.c/h       # npm package resolution
│   ├── package_json.c/h       # package.json parsing
│   └── specifier.c/h          # Module specifier handling
├── detector/
│   ├── format_detector.c/h    # Format detection (.cjs/.mjs/.js)
│   └── content_analyzer.c/h   # Content-based format analysis
├── protocols/
│   ├── protocol_handler.c/h   # Protocol handler registry
│   ├── file_handler.c/h       # file:// protocol
│   ├── http_handler.c/h       # http:// and https://
│   └── protocol_registry.c/h  # Handler registration system
├── loaders/
│   ├── commonjs_loader.c/h    # CommonJS loading logic
│   ├── esm_loader.c/h         # ES module loading logic
│   └── builtin_loader.c/h     # jsrt:/node: built-ins
└── util/
    ├── module_errors.c/h      # Error handling/reporting
    └── module_debug.c/h       # Debug logging helpers
```

*** Test Code Structure
```
test/module/
├── commonjs/                  # CommonJS require() tests
│   ├── test_basic_require.js
│   ├── test_npm_packages.js
│   ├── test_caching.js
│   └── test_circular_deps.js
├── esm/                       # ES module import tests
│   ├── test_basic_import.js
│   ├── test_dynamic_import.js
│   ├── test_top_level_await.js
│   └── test_import_meta.js
├── resolver/                  # Path resolution tests
│   ├── test_relative_paths.js
│   ├── test_absolute_paths.js
│   ├── test_npm_resolution.js
│   └── test_package_json.js
├── detector/                  # Format detection tests
│   ├── test_extension_detect.js
│   ├── test_content_analysis.js
│   └── test_package_type.js
├── protocols/                 # Protocol handler tests
│   ├── test_file_protocol.js
│   ├── test_http_protocol.js
│   └── test_https_protocol.js
├── interop/                   # CommonJS/ESM interop tests
│   ├── test_require_esm.js
│   ├── test_import_cjs.js
│   └── test_esmodule_flag.js
└── edge-cases/                # Edge cases and error scenarios
    ├── test_missing_modules.js
    ├── test_invalid_json.js
    └── test_unicode_paths.js
```

** Data Flow Architecture

```
User Code
    │
    ├─→ require(specifier)  ──→  CommonJS Loader
    │                                  │
    └─→ import specifier    ──→  ESM Loader
                                       │
                            ┌──────────┴──────────┐
                            │  Module Loader Core  │
                            │  (module_loader.c)   │
                            └──────────┬───────────┘
                                       │
                    ┌──────────────────┼──────────────────┐
                    ▼                  ▼                  ▼
            Path Resolver      Format Detector    Module Cache
         (path_resolver.c)  (format_detector.c)  (module_cache.c)
                    │                  │                  │
                    │                  │         (check cache first)
                    │                  │                  │
                    └──────┬───────────┴──────────────────┘
                           ▼
                 Protocol Handler Registry
                  (protocol_registry.c)
                           │
          ┌────────────────┼────────────────┐
          ▼                ▼                ▼
    File Handler    HTTP Handler    (Future: Zip Handler)
   (file_handler)   (http_handler)
          │                │
          └────────┬───────┘
                   ▼
          Content Loaded & Cached
                   │
          ┌────────┴────────┐
          ▼                 ▼
   CommonJS Exec      ESM Compile
  (commonjs_loader)  (esm_loader)
          │                 │
          └────────┬────────┘
                   ▼
            Module Exports
```

** API Design

*** Core Module Loader API
```c
// module_loader.h
typedef struct JSRT_ModuleLoader JSRT_ModuleLoader;

// Initialize module loading system
JSRT_ModuleLoader* jsrt_module_loader_create(JSContext* ctx);
void jsrt_module_loader_free(JSRT_ModuleLoader* loader);

// Main entry points
JSValue jsrt_load_commonjs_module(JSRT_ModuleLoader* loader, const char* specifier);
JSModuleDef* jsrt_load_esm_module(JSRT_ModuleLoader* loader, const char* specifier);

// Module resolution
char* jsrt_resolve_module_specifier(JSRT_ModuleLoader* loader,
                                    const char* specifier,
                                    const char* base_path);
```

*** Path Resolver API
```c
// path_resolver.h
typedef struct {
    char* resolved_path;      // Final resolved path/URL
    bool is_url;              // Is this a URL?
    bool is_relative;         // Is this relative?
    bool is_builtin;          // Is this jsrt:/node: builtin?
    char* protocol;           // Protocol (file, http, https, etc.)
} JSRT_ResolvedPath;

JSRT_ResolvedPath* jsrt_resolve_path(const char* specifier, const char* base_path);
void jsrt_resolved_path_free(JSRT_ResolvedPath* path);
```

*** Format Detector API
```c
// format_detector.h
typedef enum {
    JSRT_MODULE_FORMAT_UNKNOWN,
    JSRT_MODULE_FORMAT_COMMONJS,
    JSRT_MODULE_FORMAT_ESM,
    JSRT_MODULE_FORMAT_JSON
} JSRT_ModuleFormat;

JSRT_ModuleFormat jsrt_detect_module_format(const char* path,
                                            const char* content,
                                            size_t content_size);
```

*** Protocol Handler API
```c
// protocol_handler.h
typedef struct {
    const char* protocol_name;  // "file", "http", "https", "zip"

    // Load content from protocol
    JSRT_ReadFileResult (*load)(const char* url, void* user_data);

    // Cleanup handler resources
    void (*cleanup)(void* user_data);

    void* user_data;
} JSRT_ProtocolHandler;

// Register protocol handler
void jsrt_register_protocol_handler(const char* protocol, JSRT_ProtocolHandler* handler);

// Get handler for protocol
JSRT_ProtocolHandler* jsrt_get_protocol_handler(const char* protocol);
```

*** Module Cache API
```c
// module_cache.h
typedef struct JSRT_ModuleCache JSRT_ModuleCache;

JSRT_ModuleCache* jsrt_module_cache_create(size_t capacity);
void jsrt_module_cache_free(JSRT_ModuleCache* cache, JSContext* ctx);

// Cache operations
JSValue jsrt_module_cache_get(JSRT_ModuleCache* cache, const char* key, JSContext* ctx);
void jsrt_module_cache_put(JSRT_ModuleCache* cache, const char* key, JSValue exports, JSContext* ctx);
bool jsrt_module_cache_has(JSRT_ModuleCache* cache, const char* key);
void jsrt_module_cache_clear(JSRT_ModuleCache* cache, JSContext* ctx);
```

* 📝 Task Execution

** TODO [#A] Phase 0: Preparation & Planning [S][R:LOW][C:SIMPLE] :planning:
:PROPERTIES:
:ID: phase-0
:CREATED: 2025-10-12T00:00:00Z
:DEPS: None
:PROGRESS: 0/8
:COMPLETION: 0%
:END:

*** TODO [#A] Task 0.1: Create directory structure [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:ID: 0.1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: None
:ESTIMATE: 5 min
:END:
Create src/module/ directory hierarchy with all subdirectories

*** TODO [#A] Task 0.2: Document current module loading flows [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 0.2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: None
:ESTIMATE: 30 min
:END:
Trace and document existing CommonJS and ESM loading paths

*** TODO [#B] Task 0.3: Run baseline tests [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:ID: 0.3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: None
:ESTIMATE: 5 min
:END:
Run `make test && make wpt` and record baseline results

*** TODO [#A] Task 0.4: Setup test harness for module system [P][R:MED][C:MEDIUM]
:PROPERTIES:
:ID: 0.4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 0.1
:ESTIMATE: 1 hour
:END:
Create test/module/ directory with comprehensive test cases

**** Note
All module system tests should be organized in test/module/ directory

*** TODO [#B] Task 0.5: Extract common patterns [P][R:LOW][C:MEDIUM]
:PROPERTIES:
:ID: 0.5
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 0.2
:ESTIMATE: 45 min
:END:
Identify reusable code patterns from existing implementation

*** TODO [#B] Task 0.6: Create migration checklist [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 0.6
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 0.2, 0.5
:ESTIMATE: 20 min
:END:
List all functions/features that need migration

*** TODO [#A] Task 0.7: Setup debug logging framework [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 0.7
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 0.1
:ESTIMATE: 30 min
:END:
Create module_debug.h with standardized debug macros

*** TODO [#A] Task 0.8: Design error code system [P][R:MED][C:MEDIUM]
:PROPERTIES:
:ID: 0.8
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 0.2
:ESTIMATE: 45 min
:END:
Define error codes and messages for module loading failures

** TODO [#A] Phase 1: Core Infrastructure [S][R:MED][C:COMPLEX][D:phase-0] :core:
:PROPERTIES:
:ID: phase-1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: phase-0
:PROGRESS: 0/25
:COMPLETION: 0%
:END:

*** TODO [#A] Task 1.1: Implement module context structure [S][R:MED][C:MEDIUM][D:0.1]
:PROPERTIES:
:ID: 1.1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 0.1
:ESTIMATE: 2 hours
:END:
Create JSRT_ModuleLoader struct with context management

**** Subtasks
- [ ] Define JSRT_ModuleLoader struct in module_context.h
- [ ] Implement jsrt_module_loader_create()
- [ ] Implement jsrt_module_loader_free()
- [ ] Add context opaque pointer to JSContext
- [ ] Add unit tests for context lifecycle

*** TODO [#A] Task 1.2: Implement unified module cache [S][R:MED][C:MEDIUM][D:1.1]
:PROPERTIES:
:ID: 1.2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 1.1
:ESTIMATE: 3 hours
:END:
Consolidate CommonJS and HTTP module caches into one

**** Subtasks
- [ ] Create module_cache.c/h
- [ ] Define JSRT_ModuleCache struct (LRU cache)
- [ ] Implement jsrt_module_cache_create()
- [ ] Implement jsrt_module_cache_get()
- [ ] Implement jsrt_module_cache_put()
- [ ] Implement jsrt_module_cache_clear()
- [ ] Implement jsrt_module_cache_free()
- [ ] Add cache statistics tracking
- [ ] Add unit tests for cache operations
- [ ] Add tests for cache eviction policy

*** TODO [#A] Task 1.3: Implement error handling system [P][R:LOW][C:MEDIUM][D:0.8]
:PROPERTIES:
:ID: 1.3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 0.8
:ESTIMATE: 2 hours
:END:
Create module_errors.c/h with standardized error handling

**** Subtasks
- [ ] Define JSRT_ModuleError enum
- [ ] Implement jsrt_module_throw_error()
- [ ] Implement jsrt_module_error_to_string()
- [ ] Add error context information
- [ ] Add unit tests for error handling

*** TODO [#B] Task 1.4: Setup debug logging [P][R:LOW][C:SIMPLE][D:0.7]
:PROPERTIES:
:ID: 1.4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 0.7
:ESTIMATE: 1 hour
:END:
Implement module_debug.h with logging macros

**** Subtasks
- [ ] Create MODULE_DEBUG macro
- [ ] Add log level support (ERROR, WARN, INFO, DEBUG)
- [ ] Add conditional compilation (#ifdef DEBUG)
- [ ] Test debug output in jsrt_g build

*** TODO [#A] Task 1.5: Implement module loader core [S][R:HIGH][C:COMPLEX][D:1.1,1.2,1.3]
:PROPERTIES:
:ID: 1.5
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 1.1,1.2,1.3
:ESTIMATE: 4 hours
:END:
Create main module loader orchestration logic

**** Subtasks
- [ ] Create module_loader.c/h
- [ ] Implement jsrt_load_module() dispatcher
- [ ] Implement cache check logic
- [ ] Implement format detection dispatch
- [ ] Implement protocol handler dispatch
- [ ] Implement loader dispatch (CommonJS/ESM)
- [ ] Add comprehensive error handling
- [ ] Add unit tests for core orchestration
- [ ] Add integration tests

** TODO [#A] Phase 2: Path Resolution System [S][R:MED][C:COMPLEX][D:phase-1] :resolver:
:PROPERTIES:
:ID: phase-2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: phase-1
:PROGRESS: 0/30
:COMPLETION: 0%
:END:

*** TODO [#A] Task 2.1: Extract path utilities [S][R:LOW][C:MEDIUM][D:1.1]
:PROPERTIES:
:ID: 2.1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 1.1
:ESTIMATE: 2 hours
:END:
Migrate path helper functions from src/std/module.c

**** Subtasks
- [ ] Create path_util.c/h in src/module/resolver/
- [ ] Migrate is_path_separator()
- [ ] Migrate find_last_separator()
- [ ] Migrate normalize_path()
- [ ] Migrate get_parent_directory()
- [ ] Migrate path_join()
- [ ] Migrate is_absolute_path()
- [ ] Migrate is_relative_path()
- [ ] Add cross-platform tests (Windows/Unix)
- [ ] Add edge case tests

*** TODO [#A] Task 2.2: Implement module specifier parser [S][R:MED][C:MEDIUM][D:2.1]
:PROPERTIES:
:ID: 2.2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 2.1
:ESTIMATE: 3 hours
:END:
Create specifier.c/h for parsing module specifiers

**** Subtasks
- [ ] Define JSRT_ModuleSpecifier struct
- [ ] Implement jsrt_parse_specifier()
- [ ] Handle bare specifiers (lodash)
- [ ] Handle relative specifiers (./module)
- [ ] Handle absolute specifiers (/path/to/module)
- [ ] Handle URL specifiers (http://...)
- [ ] Handle builtin specifiers (jsrt:, node:)
- [ ] Handle package imports (#imports)
- [ ] Add unit tests for all specifier types

*** TODO [#A] Task 2.3: Implement package.json parser [S][R:MED][C:MEDIUM][D:2.1]
:PROPERTIES:
:ID: 2.3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 2.1
:ESTIMATE: 3 hours
:END:
Extract and enhance package.json parsing logic

**** Subtasks
- [ ] Create package_json.c/h
- [ ] Define JSRT_PackageJson struct
- [ ] Implement jsrt_parse_package_json()
- [ ] Parse "type" field
- [ ] Parse "main" field
- [ ] Parse "module" field
- [ ] Parse "exports" field (conditional exports)
- [ ] Parse "imports" field (package imports)
- [ ] Add caching for parsed package.json
- [ ] Add unit tests with sample package.json

*** TODO [#A] Task 2.4: Implement npm resolver [S][R:HIGH][C:COMPLEX][D:2.1,2.3]
:PROPERTIES:
:ID: 2.4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 2.1,2.3
:ESTIMATE: 4 hours
:END:
Create npm_resolver.c/h for node_modules resolution

**** Subtasks
- [ ] Migrate find_node_modules_path()
- [ ] Migrate resolve_package_main()
- [ ] Migrate resolve_npm_module()
- [ ] Implement package.json exports resolution
- [ ] Implement package.json imports resolution
- [ ] Handle subpath exports
- [ ] Handle conditional exports (import/require)
- [ ] Add unit tests with mock node_modules
- [ ] Add tests for nested node_modules

*** TODO [#A] Task 2.5: Implement unified path resolver [S][R:HIGH][C:COMPLEX][D:2.1,2.2,2.3,2.4]
:PROPERTIES:
:ID: 2.5
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 2.1,2.2,2.3,2.4
:ESTIMATE: 5 hours
:END:
Create path_resolver.c/h as main resolution orchestrator

**** Subtasks
- [ ] Implement jsrt_resolve_path()
- [ ] Route builtin specifiers (jsrt:, node:)
- [ ] Route URL specifiers
- [ ] Route relative specifiers
- [ ] Route absolute specifiers
- [ ] Route bare specifiers (npm)
- [ ] Handle file extension resolution (.js, .mjs, .cjs)
- [ ] Handle directory index resolution (index.js)
- [ ] Return JSRT_ResolvedPath struct
- [ ] Add comprehensive integration tests
- [ ] Add performance benchmarks

*** TODO [#B] Task 2.6: Implement extension resolution [S][R:LOW][C:MEDIUM][D:2.5]
:PROPERTIES:
:ID: 2.6
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 2.5
:ESTIMATE: 2 hours
:END:
Handle automatic .js/.mjs/.cjs extension resolution

**** Subtasks
- [ ] Implement try_extensions() (from module.c)
- [ ] Support extensionless imports
- [ ] Follow Node.js extension priority
- [ ] Add tests for extension resolution

** TODO [#A] Phase 3: Format Detection [S][R:MED][C:MEDIUM][D:phase-2] :detector:
:PROPERTIES:
:ID: phase-3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: phase-2
:PROGRESS: 0/18
:COMPLETION: 0%
:END:

*** TODO [#A] Task 3.1: Implement file extension detection [S][R:LOW][C:SIMPLE][D:2.1]
:PROPERTIES:
:ID: 3.1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 2.1
:ESTIMATE: 1 hour
:END:
Detect format based on file extension

**** Subtasks
- [ ] Create format_detector.c/h
- [ ] Implement jsrt_detect_format_by_extension()
- [ ] Handle .cjs → CommonJS
- [ ] Handle .mjs → ESM
- [ ] Handle .js → depends on package.json
- [ ] Handle .json → JSON module
- [ ] Add unit tests

*** TODO [#A] Task 3.2: Implement package.json type detection [S][R:MED][C:MEDIUM][D:2.3,3.1]
:PROPERTIES:
:ID: 3.2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 2.3,3.1
:ESTIMATE: 2 hours
:END:
Check package.json "type" field for .js files

**** Subtasks
- [ ] Implement jsrt_detect_format_by_package()
- [ ] Walk up directory tree to find package.json
- [ ] Parse "type" field (module/commonjs)
- [ ] Cache package.json lookups
- [ ] Add unit tests with nested packages

*** TODO [#A] Task 3.3: Implement content analysis [S][R:MED][C:COMPLEX][D:3.1]
:PROPERTIES:
:ID: 3.3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 3.1
:ESTIMATE: 4 hours
:END:
Analyze file content for format hints

**** Subtasks
- [ ] Create content_analyzer.c/h
- [ ] Implement jsrt_analyze_content_format()
- [ ] Detect "import" / "export" keywords (ESM)
- [ ] Detect "require" / "module.exports" (CommonJS)
- [ ] Handle mixed content (prefer ESM if both)
- [ ] Handle edge cases (comments, strings)
- [ ] Add comprehensive test cases

*** TODO [#A] Task 3.4: Implement unified format detector [S][R:MED][C:MEDIUM][D:3.1,3.2,3.3]
:PROPERTIES:
:ID: 3.4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 3.1,3.2,3.3
:ESTIMATE: 2 hours
:END:
Orchestrate format detection with fallback logic

**** Subtasks
- [ ] Implement jsrt_detect_module_format()
- [ ] Priority: extension → package.json → content
- [ ] Handle ambiguous cases
- [ ] Return JSRT_ModuleFormat enum
- [ ] Add integration tests

** TODO [#A] Phase 4: Protocol Handlers [S][R:MED][C:COMPLEX][D:phase-2] :protocols:
:PROPERTIES:
:ID: phase-4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: phase-2
:PROGRESS: 0/28
:COMPLETION: 0%
:END:

*** TODO [#A] Task 4.1: Create protocol handler registry [S][R:MED][C:MEDIUM][D:1.1]
:PROPERTIES:
:ID: 4.1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 1.1
:ESTIMATE: 3 hours
:END:
Implement extensible protocol handler system

**** Subtasks
- [ ] Create protocol_registry.c/h
- [ ] Define JSRT_ProtocolHandler struct
- [ ] Implement jsrt_register_protocol_handler()
- [ ] Implement jsrt_get_protocol_handler()
- [ ] Implement jsrt_unregister_protocol_handler()
- [ ] Add handler lifecycle management
- [ ] Add unit tests for registry

*** TODO [#A] Task 4.2: Implement file:// protocol handler [S][R:LOW][C:MEDIUM][D:4.1]
:PROPERTIES:
:ID: 4.2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 4.1
:ESTIMATE: 2 hours
:END:
Create file_handler.c/h for local filesystem

**** Subtasks
- [ ] Create file_handler.c/h
- [ ] Implement jsrt_file_handler_load()
- [ ] Use JSRT_ReadFile() for actual I/O
- [ ] Handle file:// URL format
- [ ] Handle file:///absolute/path format
- [ ] Add error handling for missing files
- [ ] Register with protocol registry
- [ ] Add unit tests

*** TODO [#A] Task 4.3: Refactor HTTP module loader [S][R:MED][C:COMPLEX][D:4.1]
:PROPERTIES:
:ID: 4.3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 4.1
:ESTIMATE: 4 hours
:END:
Migrate src/http/module_loader.c to protocol handler

**** Subtasks
- [ ] Create http_handler.c/h
- [ ] Migrate jsrt_load_http_module() logic
- [ ] Migrate HTTP caching to unified cache
- [ ] Migrate security validation
- [ ] Migrate content cleaning
- [ ] Support both http:// and https://
- [ ] Implement jsrt_http_handler_load()
- [ ] Register with protocol registry
- [ ] Add unit tests
- [ ] Add integration tests with mock HTTP server

*** TODO [#B] Task 4.4: Design zip:// protocol handler [S][R:LOW][C:MEDIUM][D:4.1]
:PROPERTIES:
:ID: 4.4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 4.1
:ESTIMATE: 2 hours
:END:
Design (but don't implement) zip:// protocol for future

**** Subtasks
- [ ] Document zip:// URL format spec
- [ ] Document handler interface requirements
- [ ] Create placeholder zip_handler.h
- [ ] Add TODO markers in code
- [ ] Document in architecture.md

*** TODO [#A] Task 4.5: Implement protocol dispatcher [S][R:MED][C:MEDIUM][D:4.1,4.2,4.3]
:PROPERTIES:
:ID: 4.5
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 4.1,4.2,4.3
:ESTIMATE: 2 hours
:END:
Create dispatcher to route to appropriate handler

**** Subtasks
- [ ] Implement jsrt_load_content_by_protocol()
- [ ] Parse protocol from resolved path
- [ ] Dispatch to registered handler
- [ ] Handle unknown protocols
- [ ] Add error handling
- [ ] Add unit tests

** TODO [#A] Phase 5: Module Loaders [S][R:HIGH][C:COMPLEX][D:phase-3,phase-4] :loaders:
:PROPERTIES:
:ID: phase-5
:CREATED: 2025-10-12T00:00:00Z
:DEPS: phase-3,phase-4
:PROGRESS: 0/26
:COMPLETION: 0%
:END:

*** TODO [#A] Task 5.1: Implement CommonJS loader [S][R:HIGH][C:COMPLEX][D:3.4,4.5]
:PROPERTIES:
:ID: 5.1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 3.4,4.5
:ESTIMATE: 5 hours
:END:
Migrate and enhance CommonJS loading logic

**** Subtasks
- [ ] Create commonjs_loader.c/h
- [ ] Implement jsrt_load_commonjs_module()
- [ ] Create module/exports objects
- [ ] Create wrapper function (exports, require, module, __filename, __dirname)
- [ ] Execute module code
- [ ] Extract module.exports
- [ ] Cache result
- [ ] Add unit tests
- [ ] Add integration tests with real modules

*** TODO [#A] Task 5.2: Implement ESM loader [S][R:HIGH][C:COMPLEX][D:3.4,4.5]
:PROPERTIES:
:ID: 5.2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 3.4,4.5
:ESTIMATE: 5 hours
:END:
Migrate and enhance ES module loading logic

**** Subtasks
- [ ] Create esm_loader.c/h
- [ ] Implement jsrt_load_esm_module()
- [ ] Use JS_Eval with JS_EVAL_TYPE_MODULE
- [ ] Set up import.meta.url
- [ ] Handle top-level await
- [ ] Extract JSModuleDef
- [ ] Add unit tests
- [ ] Add integration tests

*** TODO [#A] Task 5.3: Implement builtin module loader [S][R:MED][C:MEDIUM][D:5.1,5.2]
:PROPERTIES:
:ID: 5.3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 5.1,5.2
:ESTIMATE: 3 hours
:END:
Handle jsrt: and node: builtin modules

**** Subtasks
- [ ] Create builtin_loader.c/h
- [ ] Implement jsrt_load_builtin_module()
- [ ] Handle jsrt: prefix
- [ ] Handle node: prefix
- [ ] Integrate with node_modules.c registry
- [ ] Add dependency resolution
- [ ] Add unit tests

*** TODO [#A] Task 5.4: Implement CommonJS/ESM interop [S][R:HIGH][C:COMPLEX][D:5.1,5.2]
:PROPERTIES:
:ID: 5.4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 5.1,5.2
:ESTIMATE: 4 hours
:END:
Handle require() of ESM and import of CommonJS

**** Subtasks
- [ ] Wrap CommonJS as synthetic ESM (default export)
- [ ] Handle ESM top-level await in require()
- [ ] Add __esModule flag handling
- [ ] Test interop scenarios
- [ ] Document interop behavior

*** TODO [#A] Task 5.5: Update QuickJS loader callbacks [S][R:MED][C:MEDIUM][D:5.2,2.5]
:PROPERTIES:
:ID: 5.5
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 5.2,2.5
:ESTIMATE: 2 hours
:END:
Update JSRT_ModuleNormalize and JSRT_ModuleLoader

**** Subtasks
- [ ] Update JSRT_ModuleNormalize to use new resolver
- [ ] Update JSRT_ModuleLoader to use new loaders
- [ ] Maintain backward compatibility
- [ ] Add integration tests

*** TODO [#A] Task 5.6: Implement require() global function [S][R:MED][C:MEDIUM][D:5.1]
:PROPERTIES:
:ID: 5.6
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 5.1
:ESTIMATE: 2 hours
:END:
Update global require() to use new loader

**** Subtasks
- [ ] Update js_require() implementation
- [ ] Use new module loader infrastructure
- [ ] Maintain context for relative resolution
- [ ] Add tests for global require()

** TODO [#A] Phase 6: Integration & Migration [S][R:HIGH][C:COMPLEX][D:phase-5] :integration:
:PROPERTIES:
:ID: phase-6
:CREATED: 2025-10-12T00:00:00Z
:DEPS: phase-5
:PROGRESS: 0/22
:COMPLETION: 0%
:END:

*** TODO [#A] Task 6.1: Update runtime initialization [S][R:HIGH][C:MEDIUM][D:5.5,5.6]
:PROPERTIES:
:ID: 6.1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 5.5,5.6
:ESTIMATE: 3 hours
:END:
Integrate new module system into JSRT_Runtime

**** Subtasks
- [ ] Add JSRT_ModuleLoader* to JSRT_Runtime struct
- [ ] Initialize module loader in JSRT_RuntimeNew()
- [ ] Cleanup module loader in JSRT_RuntimeFree()
- [ ] Update JSRT_StdModuleInit()
- [ ] Update JSRT_StdCommonJSInit()
- [ ] Add tests for runtime lifecycle

*** TODO [#A] Task 6.2: Migrate src/std/module.c [S][R:HIGH][C:COMPLEX][D:6.1]
:PROPERTIES:
:ID: 6.2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.1
:ESTIMATE: 4 hours
:END:
Replace old module.c with new system

**** Subtasks
- [ ] Remove duplicated path resolution code
- [ ] Remove old module cache code
- [ ] Keep only compatibility shims if needed
- [ ] Update function calls to use new API
- [ ] Verify all tests still pass

*** TODO [#A] Task 6.3: Migrate src/http/module_loader.c [S][R:MED][C:MEDIUM][D:4.3]
:PROPERTIES:
:ID: 6.3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 4.3
:ESTIMATE: 2 hours
:END:
Remove old HTTP module loader

**** Subtasks
- [ ] Remove jsrt_load_http_module() (now in protocol handler)
- [ ] Remove jsrt_require_http_module() (now in protocol handler)
- [ ] Keep only security validation exports
- [ ] Update header references
- [ ] Verify HTTP module tests pass

*** TODO [#A] Task 6.4: Update src/jsrt.c [S][R:LOW][C:SIMPLE][D:4.2,4.3]
:PROPERTIES:
:ID: 6.4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 4.2,4.3
:ESTIMATE: 1 hour
:END:
Use new protocol handlers in main command

**** Subtasks
- [ ] Remove is_url() (now in resolver)
- [ ] Remove download_url() (now in protocol handlers)
- [ ] Update JSRT_CmdRunFile() to use new loader
- [ ] Verify command-line execution works

*** TODO [#A] Task 6.5: Update build system [P][R:LOW][C:SIMPLE][D:6.1,6.2,6.3]
:PROPERTIES:
:ID: 6.5
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.1,6.2,6.3
:ESTIMATE: 1 hour
:END:
Add new source files to Makefile/CMakeLists.txt

**** Subtasks
- [ ] Add src/module/ files to build
- [ ] Update include paths
- [ ] Verify build succeeds
- [ ] Test all build variants (release, debug, asan)

*** TODO [#B] Task 6.6: Run code formatter [P][R:LOW][C:TRIVIAL][D:6.5]
:PROPERTIES:
:ID: 6.6
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.5
:ESTIMATE: 5 min
:END:
Run `make format` on all new code

*** TODO [#A] Task 6.7: Fix compiler warnings [P][R:LOW][C:SIMPLE][D:6.5]
:PROPERTIES:
:ID: 6.7
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.5
:ESTIMATE: 1 hour
:END:
Address all compiler warnings in new code

** TODO [#A] Phase 7: Testing & Validation [PS][R:HIGH][C:COMPLEX][D:phase-6] :testing:
:PROPERTIES:
:ID: phase-7
:CREATED: 2025-10-12T00:00:00Z
:DEPS: phase-6
:PROGRESS: 0/28
:COMPLETION: 0%
:TEST_DIRECTORY: test/module/
:END:

**** Testing Organization
All test files for the module system refactoring should be placed in the =test/module/= directory.
This keeps module-related tests organized separately from other test categories.

*** TODO [#A] Task 7.1: Run unit tests [P][R:HIGH][C:TRIVIAL][D:6.5]
:PROPERTIES:
:ID: 7.1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.5
:ESTIMATE: 5 min
:END:
Run `make test` and verify all pass

*** TODO [#A] Task 7.2: Run WPT tests [P][R:HIGH][C:TRIVIAL][D:6.5]
:PROPERTIES:
:ID: 7.2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.5
:ESTIMATE: 10 min
:END:
Run `make wpt` and verify no regressions

*** TODO [#A] Task 7.3: Test CommonJS scenarios [P][R:HIGH][C:MEDIUM][D:5.1]
:PROPERTIES:
:ID: 7.3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 5.1
:ESTIMATE: 2 hours
:END:
Comprehensive CommonJS test coverage in test/module/commonjs/

**** Test Scenarios
- [ ] Basic require('./local')
- [ ] npm package require('lodash')
- [ ] Nested node_modules
- [ ] package.json main field
- [ ] Circular dependencies
- [ ] require() caching
- [ ] __filename and __dirname
- [ ] JSON modules
- [ ] .cjs extension

*** TODO [#A] Task 7.4: Test ESM scenarios [P][R:HIGH][C:MEDIUM][D:5.2]
:PROPERTIES:
:ID: 7.4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 5.2
:ESTIMATE: 2 hours
:END:
Comprehensive ESM test coverage in test/module/esm/

**** Test Scenarios
- [ ] Basic import './local.mjs'
- [ ] Named imports
- [ ] Default imports
- [ ] Dynamic import()
- [ ] Top-level await
- [ ] import.meta.url
- [ ] Circular imports
- [ ] .mjs extension

*** TODO [#A] Task 7.5: Test path resolution [P][R:HIGH][C:MEDIUM][D:2.5]
:PROPERTIES:
:ID: 7.5
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 2.5
:ESTIMATE: 2 hours
:END:
Test all path resolution scenarios in test/module/resolver/

**** Test Scenarios
- [ ] Relative paths (./file, ../file)
- [ ] Absolute paths (/absolute/path)
- [ ] Bare specifiers (lodash)
- [ ] Subpath imports (#internal/utils)
- [ ] package.json exports
- [ ] Extension resolution
- [ ] Directory index resolution
- [ ] Cross-platform paths (Windows/Unix)

*** TODO [#A] Task 7.6: Test protocol handlers [P][R:HIGH][C:MEDIUM][D:4.5]
:PROPERTIES:
:ID: 7.6
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 4.5
:ESTIMATE: 2 hours
:END:
Test all protocol handlers in test/module/protocols/

**** Test Scenarios
- [ ] file:// URLs
- [ ] http:// URLs
- [ ] https:// URLs
- [ ] Invalid protocols
- [ ] Protocol security validation
- [ ] Caching behavior

*** TODO [#A] Task 7.7: Test format detection [P][R:HIGH][C:MEDIUM][D:3.4]
:PROPERTIES:
:ID: 7.7
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 3.4
:ESTIMATE: 2 hours
:END:
Test format detection accuracy in test/module/detector/

**** Test Scenarios
- [ ] .cjs files
- [ ] .mjs files
- [ ] .js with type:module
- [ ] .js with type:commonjs
- [ ] .js with no package.json
- [ ] Mixed syntax detection
- [ ] Ambiguous cases

*** TODO [#A] Task 7.8: Test interop [P][R:HIGH][C:COMPLEX][D:5.4]
:PROPERTIES:
:ID: 7.8
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 5.4
:ESTIMATE: 3 hours
:END:
Test CommonJS/ESM interoperability in test/module/interop/

**** Test Scenarios
- [ ] require() ESM module
- [ ] import CommonJS module
- [ ] Default exports
- [ ] Named exports
- [ ] __esModule flag
- [ ] Top-level await in require()

*** TODO [#A] Task 7.9: Run memory leak tests [P][R:HIGH][C:MEDIUM][D:6.5]
:PROPERTIES:
:ID: 7.9
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.5
:ESTIMATE: 2 hours
:END:
Test with AddressSanitizer (make jsrt_m)

**** Subtasks
- [ ] Build with `make jsrt_m`
- [ ] Run all module tests with ASAN
- [ ] Fix any memory leaks detected
- [ ] Fix any use-after-free errors
- [ ] Verify clean ASAN output

*** TODO [#B] Task 7.10: Performance benchmarks [P][R:MED][C:MEDIUM][D:7.1,7.2]
:PROPERTIES:
:ID: 7.10
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 7.1,7.2
:ESTIMATE: 2 hours
:END:
Measure performance vs old implementation

**** Metrics
- [ ] Module resolution time
- [ ] Module loading time
- [ ] Cache hit rate
- [ ] Memory usage
- [ ] Compare before/after
- [ ] Document results

*** TODO [#B] Task 7.11: Edge case testing [P][R:MED][C:MEDIUM][D:7.1,7.2]
:PROPERTIES:
:ID: 7.11
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 7.1,7.2
:ESTIMATE: 3 hours
:END:
Test unusual and error scenarios in test/module/edge-cases/

**** Test Scenarios
- [ ] Missing modules
- [ ] Circular dependencies
- [ ] Invalid package.json
- [ ] Syntax errors in modules
- [ ] Network timeouts (HTTP)
- [ ] Permission errors
- [ ] Very deep node_modules nesting
- [ ] Large modules (>10MB)
- [ ] Unicode in paths
- [ ] Special characters in filenames

** TODO [#B] Phase 8: Documentation [PS][R:LOW][C:MEDIUM][D:phase-7] :docs:
:PROPERTIES:
:ID: phase-8
:CREATED: 2025-10-12T00:00:00Z
:DEPS: phase-7
:PROGRESS: 0/13
:COMPLETION: 0%
:END:

*** TODO [#B] Task 8.1: Document API [P][R:LOW][C:MEDIUM][D:6.1]
:PROPERTIES:
:ID: 8.1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.1
:ESTIMATE: 3 hours
:END:
Create comprehensive API documentation

**** Subtasks
- [ ] Document all public functions
- [ ] Document all public structs
- [ ] Document all enums
- [ ] Add usage examples
- [ ] Create docs/module-system.md

*** TODO [#B] Task 8.2: Document architecture [P][R:LOW][C:MEDIUM][D:6.1]
:PROPERTIES:
:ID: 8.2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.1
:ESTIMATE: 2 hours
:END:
Document system architecture

**** Subtasks
- [ ] Update architecture diagrams
- [ ] Document data flow
- [ ] Document component interaction
- [ ] Update .claude/docs/architecture.md

*** TODO [#B] Task 8.3: Document migration guide [P][R:LOW][C:SIMPLE][D:6.2,6.3]
:PROPERTIES:
:ID: 8.3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.2,6.3
:ESTIMATE: 1 hour
:END:
Create migration guide for API changes

**** Subtasks
- [ ] Document breaking changes
- [ ] Document deprecated functions
- [ ] Provide migration examples
- [ ] Create docs/migration/module-system.md

*** TODO [#B] Task 8.4: Update CLAUDE.md [P][R:LOW][C:SIMPLE][D:8.1,8.2]
:PROPERTIES:
:ID: 8.4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 8.1,8.2
:ESTIMATE: 30 min
:END:
Update project instructions with module system info

*** TODO [#B] Task 8.5: Add inline code comments [P][R:LOW][C:MEDIUM][D:6.1,6.2]
:PROPERTIES:
:ID: 8.5
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 6.1,6.2
:ESTIMATE: 2 hours
:END:
Add comprehensive inline documentation

**** Subtasks
- [ ] Add function header comments
- [ ] Add complex logic comments
- [ ] Add error handling explanations
- [ ] Document non-obvious design decisions

** TODO [#C] Phase 9: Optimization [PS][R:LOW][C:MEDIUM][D:phase-7] :optimization:
:PROPERTIES:
:ID: phase-9
:CREATED: 2025-10-12T00:00:00Z
:DEPS: phase-7
:PROGRESS: 0/10
:COMPLETION: 0%
:END:

*** TODO [#C] Task 9.1: Optimize path resolution [P][R:LOW][C:MEDIUM][D:7.10]
:PROPERTIES:
:ID: 9.1
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 7.10
:ESTIMATE: 2 hours
:END:
Optimize hot paths in resolver

**** Subtasks
- [ ] Profile path resolution
- [ ] Add fast paths for common cases
- [ ] Cache realpath() results
- [ ] Optimize string operations
- [ ] Measure improvement

*** TODO [#C] Task 9.2: Optimize cache performance [P][R:LOW][C:MEDIUM][D:7.10]
:PROPERTIES:
:ID: 9.2
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 7.10
:ESTIMATE: 2 hours
:END:
Improve cache hit rate and speed

**** Subtasks
- [ ] Profile cache operations
- [ ] Optimize hash function
- [ ] Implement better eviction policy
- [ ] Add cache prewarming
- [ ] Measure improvement

*** TODO [#C] Task 9.3: Reduce memory allocations [P][R:LOW][C:MEDIUM][D:7.9]
:PROPERTIES:
:ID: 9.3
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 7.9
:ESTIMATE: 2 hours
:END:
Minimize malloc/free in hot paths

**** Subtasks
- [ ] Use stack allocation where possible
- [ ] Reuse buffers
- [ ] Pool common allocations
- [ ] Measure memory improvement

*** TODO [#C] Task 9.4: Optimize protocol handlers [P][R:LOW][C:SIMPLE][D:7.10]
:PROPERTIES:
:ID: 9.4
:CREATED: 2025-10-12T00:00:00Z
:DEPS: 7.10
:ESTIMATE: 1 hour
:END:
Improve protocol handler performance

**** Subtasks
- [ ] Fast path for file:// (most common)
- [ ] Async I/O for network protocols
- [ ] Measure improvement

* 🚀 Execution Dashboard
:PROPERTIES:
:CURRENT_PHASE: Phase 0: Preparation & Planning
:PROGRESS: 0/150
:COMPLETION: 0%
:ACTIVE_TASK: None
:UPDATED: 2025-10-12T00:00:00Z
:END:

** Current Status
- Phase: Phase 0 - Preparation & Planning
- Progress: 0/150 tasks (0%)
- Active: Plan created, awaiting execution approval
- Blockers: None

** Execution Order (Critical Path)
1. Phase 0: Preparation (0.1 → 0.2 → 0.3 → 0.4,0.5,0.6,0.7,0.8 parallel)
2. Phase 1: Core Infrastructure (1.1 → 1.2,1.3,1.4 → 1.5)
3. Phase 2: Path Resolution (2.1 → 2.2,2.3 parallel → 2.4 → 2.5 → 2.6)
4. Phase 3: Format Detection (3.1 → 3.2,3.3 parallel → 3.4)
5. Phase 4: Protocol Handlers (4.1 → 4.2,4.3,4.4 parallel → 4.5)
6. Phase 5: Module Loaders (5.1,5.2,5.3 parallel → 5.4 → 5.5,5.6)
7. Phase 6: Integration (6.1 → 6.2,6.3,6.4 → 6.5 → 6.6,6.7 parallel)
8. Phase 7: Testing (all parallel after dependencies)
9. Phase 8: Documentation (all parallel)
10. Phase 9: Optimization (all parallel)

** Next Up (after approval)
- [ ] Task 0.1: Create directory structure
- [ ] Task 0.2: Document current flows (parallel)
- [ ] Task 0.3: Run baseline tests (parallel)

** Parallel Execution Opportunities
- Phase 0: Tasks 0.4-0.8 can run in parallel after 0.1-0.3
- Phase 1: Tasks 1.2-1.4 can run in parallel after 1.1
- Phase 2: Tasks 2.2-2.3 can run in parallel after 2.1
- Phase 4: Tasks 4.2-4.4 can run in parallel after 4.1
- Phase 5: Tasks 5.1-5.3 can run in parallel
- Phase 6: Tasks 6.2-6.4 can run in parallel after 6.1
- Phase 7: Most tasks can run in parallel
- Phase 8: All tasks can run in parallel
- Phase 9: All tasks can run in parallel

* 📜 History & Updates
:LOGBOOK:
- Note taken on [2025-10-12T00:00:00Z] \\
  Initial task plan created with 150 tasks across 9 phases
- Note taken on [2025-10-12T00:00:00Z] \\
  Analyzed existing codebase (src/std/module.c, src/http/module_loader.c, src/node/node_modules.c)
- Note taken on [2025-10-12T00:00:00Z] \\
  Designed architecture with src/module/ directory structure
- Note taken on [2025-10-12T00:00:00Z] \\
  Identified 150 tasks with dependencies and execution order
:END:

** Change Log
| Timestamp | Phase | Task | Action | Details |
|-----------|-------|------|--------|---------|
| 2025-10-12T00:00:00Z | Planning | N/A | Created | Initial plan document |

** Risks & Mitigations

*** Risk: Breaking existing tests
- **Probability**: High
- **Impact**: High
- **Mitigation**:
  - Run baseline tests before starting (Task 0.3)
  - Run tests after each phase
  - Maintain backward compatibility during migration
  - Keep old code until new code fully validated

*** Risk: Performance regression
- **Probability**: Medium
- **Impact**: Medium
- **Mitigation**:
  - Benchmark before/after (Task 7.10)
  - Profile hot paths
  - Optimize in Phase 9
  - Accept small regression if architecture gains worth it

*** Risk: Memory leaks in new code
- **Probability**: Medium
- **Impact**: High
- **Mitigation**:
  - Test with ASAN frequently (Task 7.9)
  - Follow jsrt memory patterns (js_malloc/js_free)
  - Code review for every allocation
  - Use valgrind for additional validation

*** Risk: Incomplete migration
- **Probability**: Low
- **Impact**: High
- **Mitigation**:
  - Detailed migration checklist (Task 0.6)
  - Verify all tests pass
  - Check for unused old code
  - Document migration status

*** Risk: Scope creep
- **Probability**: Medium
- **Impact**: Medium
- **Mitigation**:
  - Stick to defined tasks
  - Mark future work as TODO (e.g., zip://)
  - Focus on parity first, enhancements later
  - Get user approval for scope changes

* ⚙️ Implementation Notes

** Code Quality Standards
- Follow existing jsrt code style
- Run `make format` before committing
- Zero compiler warnings
- All memory properly freed
- Use JSRT_Debug for logging
- Comprehensive error handling

** Memory Management Rules
- Use js_malloc/js_free for JS-related allocations
- Use malloc/free for internal structures
- Every malloc needs corresponding free
- JS_FreeValue for all JSValue temporaries
- Check all allocation returns
- Test with ASAN

** Testing Requirements
- Unit tests for each component
- Integration tests for workflows
- All existing tests must pass
- WPT tests must pass
- ASAN clean
- No performance regressions >10%

** File Organization
- Max 500 lines per .c file
- Clear separation of concerns
- Public API in .h files
- Internal helpers in .c files
- Consistent naming conventions
- All source code in src/module/ directory
- All test code in test/module/ directory with subdirectories matching component structure

** Naming Conventions
- Public API: `jsrt_module_*`
- Internal functions: `module_*` (static)
- Structs: `JSRT_ModuleXxx`
- Enums: `JSRT_MODULE_XXX`
- Constants: `MODULE_XXX`

** Dependencies
- QuickJS (existing)
- libuv (existing)
- No new external dependencies
- Reuse existing utilities (util/, crypto/, http/)

* 📊 Success Metrics

** Functional Requirements
✅ Load CommonJS modules (require)
✅ Load ES modules (import)
✅ Resolve relative paths (./file)
✅ Resolve absolute paths (/file)
✅ Resolve npm packages (lodash)
✅ Resolve builtin modules (jsrt:, node:)
✅ Handle file:// URLs
✅ Handle http:// URLs
✅ Handle https:// URLs
✅ Detect .cjs as CommonJS
✅ Detect .mjs as ESM
✅ Detect .js by package.json
✅ Detect .js by content analysis
✅ Support package.json "type" field
✅ Support package.json "main" field
✅ Support package.json "module" field
✅ Support package.json "exports" field
✅ Support package.json "imports" field
✅ Cache loaded modules
✅ Handle circular dependencies
✅ CommonJS/ESM interop

** Quality Requirements
✅ All tests pass (make test)
✅ All WPT tests pass (make wpt)
✅ Zero memory leaks (ASAN)
✅ Zero compiler warnings
✅ Code coverage >90%
✅ Performance ±10% of baseline
✅ Clean code formatting (make format)

** Architecture Requirements
✅ Code consolidated in src/module/
✅ Clear component boundaries
✅ Extensible protocol handler system
✅ Unified module cache
✅ Single path resolver
✅ Documented APIs
✅ Migration guide

** Maintenance Requirements
✅ Comprehensive inline comments
✅ Architecture documentation
✅ API documentation
✅ Test coverage documentation
✅ Clear error messages
✅ Debug logging support

* 📚 References

** Existing Code
- src/std/module.c - Current module implementation
- src/http/module_loader.c - HTTP module loading
- src/node/node_modules.c - Node.js module registry
- src/jsrt.c - Main entry point with URL handling
- src/util/file.h - File I/O utilities
- src/util/http_client.h - HTTP client

** Specifications
- Node.js ESM Resolution: https://nodejs.org/api/esm.html
- Node.js CommonJS: https://nodejs.org/api/modules.html
- Package.json Exports: https://nodejs.org/api/packages.html#exports
- Package.json Imports: https://nodejs.org/api/packages.html#imports

** Tools
- make test - Unit tests
- make wpt - Web Platform Tests
- make format - Code formatting
- make jsrt_m - ASAN build
- JSRT_Debug macro - Debug logging

** Documentation
- CLAUDE.md - Project guidelines
- .claude/AGENTS_SUMMARY.md - Agent usage
- .claude/docs/architecture.md - Architecture docs
- .claude/docs/testing.md - Testing guide

---

**Total Tasks**: 150
**Estimated Time**: 120-160 hours (3-4 weeks full-time)
**Complexity**: High
**Risk**: Medium
**Priority**: High

**Approval Required**: Yes - Please review and approve before execution
