#+TITLE: Task Plan: Implement node:module API
#+AUTHOR: Claude AI Assistant
#+DATE: 2025-10-31T00:00:00Z
#+STARTUP: overview indent
#+TODO: TODO IN-PROGRESS BLOCKED | DONE CANCELLED
#+PRIORITIES: A C B

* Task Metadata
:PROPERTIES:
:CREATED: 2025-10-31T00:00:00Z
:UPDATED: 2025-10-31T00:00:00Z
:STATUS: 🟡 PLANNING
:PROGRESS: 0/153
:COMPLETION: 0%
:END:

* Status Update Guidelines
:PROPERTIES:
:CREATED: 2025-10-31T00:00:00Z
:END:

This plan uses three-level status tracking for precise progress monitoring:

** Level 1: Phase Status (L1 Headers)
Update phase status in the main header:
- Format: ~** TODO [#A] Phase N: Phase Name [S][R:LEVEL][C:LEVEL]~
- When to update: After completing ALL tasks in the phase
- Progress tracking: Update ~:PROGRESS:~ property in phase properties block

** Level 2: Task Status (L2 Headers)
Update individual task status:
- Format: ~*** TODO [#A] Task N.M: Task Description [EXEC_MODE][DEPS]~
- When to update: After completing ALL subtasks within the task
- Change ~TODO~ → ~IN-PROGRESS~ → ~DONE~

** Level 3: Subtask Status (Checkboxes)
Update granular steps using checkboxes:
- Format: ~- [ ] Subtask description~
- When to update: Immediately after completing each subtask
- Change ~[ ]~ to ~[X]~ when done

** Consistency Rules
1. **Bottom-up updates**: Always update from subtask → task → phase
2. **Complete propagation**: Don't mark task DONE until all subtasks are [X]
3. **Phase completion**: Don't mark phase DONE until all tasks are DONE
4. **Progress tracking**: Update :PROGRESS: property at each level
5. **History logging**: Add entry to History section for significant milestones

** Example Update Flow
#+BEGIN_EXAMPLE
1. Complete subtask: - [X] Create header file
2. After all subtasks done in task: *** DONE Task 1.1: Core API
3. After all tasks in phase: ** DONE Phase 1: Foundation
4. Update phase :PROGRESS: from 0/25 to 25/25
5. Add history entry with timestamp
#+END_EXAMPLE

* 📋 Overview

** Purpose
Implement the Node.js ~node:module~ API in jsrt to provide programmatic access to the module system, enabling:
- Module introspection and manipulation
- Custom module resolution and loading hooks
- Source map support
- CommonJS/ESM interoperability utilities
- Compilation cache management

** Scope
This implementation covers the stable and active development APIs from Node.js module system:

*** Core APIs (Stable)
- ~Module~ class with static/instance methods
- ~module.builtinModules~ - List of built-in modules
- ~module.createRequire(filename)~ - Create require function for ESM
- ~module.isBuiltin(moduleName)~ - Check if module is built-in
- ~module.syncBuiltinESMExports()~ - Sync CommonJS/ESM exports
- ~module.findSourceMap(path)~ - Source map lookup
- ~SourceMap~ class for source map manipulation

*** Active Development APIs
- ~module.enableCompileCache([options])~ - Enable compilation cache
- ~module.getCompileCacheDir()~ - Get cache directory
- ~module.flushCompileCache()~ - Flush cache to disk
- ~module.constants.compileCacheStatus~ - Cache status constants
- ~module.findPackageJSON(specifier[, base])~ - Package.json lookup
- ~module.getSourceMapsSupport()~ - Get source map configuration
- ~module.setSourceMapsSupport(enabled[, options])~ - Configure source maps
- ~module.stripTypeScriptTypes(code[, options])~ - TypeScript stripping

*** Experimental APIs (Phase 2)
- ~module.register(specifier[, parentURL][, options])~ - Async hooks (experimental)
- ~module.registerHooks(options)~ - Sync hooks (experimental)
- Hook functions: ~initialize()~, ~resolve()~, ~load()~

** Integration Points

*** Existing jsrt Systems
1. **Module Loader** (~src/module/core/module_loader.c~)
   - Already handles CommonJS, ESM, JSON, builtin modules
   - Has caching system (FNV-1a hash-based)
   - Protocol registry (file://, http://, https://)
   - Path resolution (Node.js-compatible)

2. **Node Modules Registry** (~src/node/node_modules.c~)
   - Tracks built-in Node.js modules
   - Dependency management
   - Initialization tracking

3. **CommonJS Loader** (~src/module/loaders/commonjs_loader.c~)
   - ~require()~ implementation
   - Module wrapping and execution
   - Circular dependency detection

4. **ESM Loader** (~src/module/loaders/esm_loader.c~)
   - ESM normalization and loading
   - ~import~ statement handling
   - Builtin module resolution

*** New Components Required
1. **Module API Module** (~src/node/module/module_api.c~)
   - Expose ~node:module~ namespace
   - Implement Module class
   - API bindings to existing systems

2. **Source Map Support** (~src/node/module/sourcemap.c~)
   - SourceMap class implementation
   - Source map parsing and lookup
   - Integration with V8/QuickJS debugging

3. **Compile Cache** (~src/node/module/compile_cache.c~)
   - Bytecode caching infrastructure
   - Disk persistence
   - Cache invalidation

4. **Module Hooks** (~src/node/module/hooks.c~)
   - Hook registration and chaining
   - ~resolve()~ and ~load()~ hook execution
   - Thread isolation (async hooks)

** Success Criteria

*** Functional Requirements
- [ ] All stable APIs implemented and tested
- [ ] Active development APIs implemented (cache, source maps)
- [ ] Integration with existing module loader
- [ ] CommonJS and ESM interoperability working
- [ ] Source map support functional
- [ ] Unit tests passing (100% for implemented features)
- [ ] Memory safety validated with AddressSanitizer

*** Quality Requirements
- [ ] No memory leaks (ASAN clean)
- [ ] Proper error handling (comprehensive error codes)
- [ ] Cross-platform compatibility (Linux, macOS, Windows)
- [ ] Performance: Module API calls < 1ms overhead
- [ ] Documentation complete (API reference, examples)

*** Compatibility Requirements
- [ ] Node.js API compatibility for stable features
- [ ] Existing jsrt code continues to work
- [ ] No breaking changes to current module loading
- [ ] WPT tests pass (baseline maintained or improved)

** Out of Scope
- Full TypeScript support (~stripTypeScriptTypes~ - requires external parser)
- WebAssembly module hooks (~load~ with format 'wasm')
- Addon/native module support (~.node~ files)
- Full ESM loader customization (will implement basic version)
- Network module loading hooks (http://, https:// already supported)

* 📝 Task Breakdown

** DONE [#A] Phase 1: Foundation & Core API [S][R:LOW][C:MEDIUM] :foundation:
CLOSED: [2025-11-03 02:37]
:PROPERTIES:
:ID: phase-1
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T02:37:00Z
:DEPS: None
:PROGRESS: 10/10
:COMPLETION: 100%
:END:

Set up basic infrastructure and implement core Module class APIs.

*** DONE [#A] Task 1.1: Project Structure Setup [P][R:LOW][C:SIMPLE]
CLOSED: [2025-10-31 07:51]
:PROPERTIES:
:ID: 1.1
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-10-31T07:51:00Z
:DEPS: None
:END:

Create directory structure and build system integration.

**** Subtasks
- [X] Create ~src/node/module/~ directory
- [X] Create ~module_api.c~ and ~module_api.h~ files
- [X] Add to CMake build system (~CMakeLists.txt~)
- [X] Add to Makefile dependencies
- [X] Create ~test/node/module/~ test directory
- [X] Create initial test files structure

**** Files to Create
- ~src/node/module/module_api.c~
- ~src/node/module/module_api.h~
- ~test/node/module/test_module_builtins.js~
- ~test/node/module/test_module_create_require.js~
- ~test/node/module/test_module_class.js~

**** Acceptance Criteria
- Directory structure exists
- Build system recognizes new files
- Empty test files can be executed with ~make test N=node/module~

*** DONE [#A] Task 1.2: Module Class Foundation [S][R:LOW][C:MEDIUM][D:1.1]
CLOSED: [2025-10-31 08:14]
:PROPERTIES:
:ID: 1.2
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-10-31T08:14:00Z
:DEPS: 1.1
:END:

Implement the Module class constructor and basic structure.

**** Subtasks
- [X] Define ~Module~ class structure in C
- [X] Implement Module constructor
- [X] Add ~module.exports~ property
- [X] Add ~module.require~ property (bound require function)
- [X] Add ~module.id~ property (module identifier)
- [X] Add ~module.filename~ property (absolute path)
- [X] Add ~module.loaded~ property (boolean flag)
- [X] Add ~module.parent~ property (parent module reference)
- [X] Add ~module.children~ property (array of child modules)
- [X] Add ~module.paths~ property (array of search paths)
- [X] Add ~module.path~ property (directory name)
- [X] Implement property getters/setters

**** Implementation Notes
#+BEGIN_SRC c
// Module class structure
typedef struct {
  JSValue exports;      // module.exports object
  JSValue require;      // Bound require function
  char* id;             // Module identifier
  char* filename;       // Absolute file path
  bool loaded;          // Load completion flag
  JSValue parent;       // Parent module
  JSValue children;     // Array of child modules
  JSValue paths;        // Search paths array
  char* path;           // Directory name
} JSRTModuleData;
#+END_SRC

**** Acceptance Criteria
- Module class can be instantiated from JavaScript
- All properties are accessible
- Properties have correct types

*** DONE [#A] Task 1.3: module.builtinModules Implementation [P][R:LOW][C:SIMPLE][D:1.2]
CLOSED: [2025-10-31 08:42]
:PROPERTIES:
:ID: 1.3
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-10-31T08:42:00Z
:DEPS: 1.2
:END:

Expose list of built-in modules.

**** Subtasks
- [X] Query ~node_modules[]~ registry in ~node_modules.c~
- [X] Generate array of module name strings
- [X] Include both prefixed (~node:fs~) and unprefixed (~fs~) forms
- [X] Create ~module.builtinModules~ static array property
- [X] Handle dynamic module list (account for conditional compilation)
- [X] Add getter function for the array

**** Implementation Approach
- Iterate through ~node_modules[]~ array in ~src/node/node_modules.c~
- Extract module names dynamically
- Return frozen array to prevent modification

**** Acceptance Criteria
- ~module.builtinModules~ returns array of strings
- Array includes all registered node modules
- Array is read-only (frozen)
- Includes core modules: ~fs~, ~http~, ~net~, ~path~, ~buffer~, etc.

*** DONE [#A] Task 1.4: module.isBuiltin() Implementation [P][R:LOW][C:SIMPLE][D:1.2]
CLOSED: [2025-10-31 08:42]
:PROPERTIES:
:ID: 1.4
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-10-31T08:42:00Z
:DEPS: 1.2
:END:

Check if a module name is built-in.

**** Subtasks
- [X] Implement ~jsrt_module_is_builtin()~ C function
- [X] Handle prefixed names (~node:fs~ → ~fs~)
- [X] Query ~find_module()~ in ~node_modules.c~
- [X] Handle edge cases (empty string, null, invalid)
- [X] Expose as ~module.isBuiltin(moduleName)~
- [X] Add comprehensive tests

**** Implementation Notes
- Reuse existing ~jsrt_is_builtin_specifier()~ logic
- Strip ~node:~ prefix before checking
- Return boolean result

**** Test Cases
- ~module.isBuiltin('fs')~ → ~true~
- ~module.isBuiltin('node:fs')~ → ~true~
- ~module.isBuiltin('lodash')~ → ~false~
- ~module.isBuiltin('./mymodule')~ → ~false~
- ~module.isBuiltin('')~ → ~false~

*** DONE [#A] Task 1.5: module.createRequire() Implementation [S][R:MED][C:MEDIUM][D:1.2]
CLOSED: [2025-11-03 02:35]
:PROPERTIES:
:ID: 1.5
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T02:35:00Z
:DEPS: 1.2
:END:

Create require function for use in ESM contexts.

**** Subtasks
- [X] Implement ~jsrt_module_create_require()~ C function
- [X] Accept filename/URL parameter (string or URL object)
- [X] Validate and normalize path
- [X] Create new require function bound to specific path
- [X] Set ~require.resolve~ property
- [X] Set ~require.cache~ property reference
- [X] Set ~require.extensions~ property (deprecated, empty object)
- [X] Set ~require.main~ property reference
- [X] Handle file:// URLs
- [X] Add error handling for invalid paths

**** Integration
- Leverage ~jsrt_create_require_function()~ from ~commonjs_loader.c~
- Ensure context is correct for relative module resolution

**** Test Cases
#+BEGIN_SRC javascript
// ESM context
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const fs = require('fs'); // Should work

// Create require for specific directory
const require2 = createRequire('/path/to/dir/index.js');
const mod = require2('./relative'); // Resolves from /path/to/dir/
#+END_SRC

*** DONE [#A] Task 1.6: Module._cache Implementation [S][R:MED][C:MEDIUM][D:1.2]
CLOSED: [2025-11-03 02:30]
:PROPERTIES:
:ID: 1.6
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T02:30:00Z
:DEPS: 1.2
:END:

Expose module cache for introspection.

**** Subtasks
- [X] Create wrapper for ~jsrt_module_cache~ (FNV-1a cache)
- [X] Implement cache iterator to build JS object
- [X] Map cache keys (resolved paths) to Module instances
- [X] Expose as ~Module._cache~ static property
- [X] Make cache object modifiable (allow ~delete Module._cache[key]~)
- [X] Implement cache invalidation when entries are deleted
- [X] Handle cache statistics (~cache_hits~, ~cache_misses~)

**** Implementation Strategy
- Create JS object mapping paths → Module objects
- Sync with underlying C cache on access
- Implement ~delete~ trap to invalidate cache entries

**** Acceptance Criteria
- ~Module._cache~ returns object with cached modules
- Keys are absolute file paths
- Values are Module instances
- Deleting entries invalidates cache
- Cache persists across require calls

*** DONE [#A] Task 1.7: Module._load Implementation [S][R:MED][C:COMPLEX][D:1.2,1.6]
CLOSED: [2025-11-03 02:37]
:PROPERTIES:
:ID: 1.7
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T02:37:00Z
:DEPS: 1.2,1.6
:END:

Implement low-level module loading function.

**** Subtasks
- [X] Implement ~Module._load(request, parent, isMain)~
- [X] Check ~Module._cache~ for existing module
- [X] Return cached module if found
- [X] Create new Module instance if not cached
- [X] Call ~jsrt_load_module()~ for actual loading
- [X] Populate Module properties (~id~, ~filename~, ~loaded~, etc.)
- [X] Add to ~Module._cache~
- [X] Return ~module.exports~
- [X] Handle circular dependencies (return partial exports)
- [X] Set ~require.main~ for main module

**** Integration Points
- Bridge to ~jsrt_load_module()~ in ~module_loader.c~
- Coordinate with ~commonjs_loader.c~ execution
- Handle both CommonJS and ESM modules

**** Acceptance Criteria
- ~Module._load()~ can load modules programmatically
- Cache is populated correctly
- Circular dependencies handled
- Main module detection works

*** DONE [#A] Task 1.8: Module._resolveFilename Implementation [S][R:MED][C:COMPLEX][D:1.2]
CLOSED: [2025-11-03 02:33]
:PROPERTIES:
:ID: 1.8
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T02:33:00Z
:DEPS: 1.2
:END:

Implement module path resolution function.

**** Subtasks
- [X] Implement ~Module._resolveFilename(request, parent, isMain, options)~
- [X] Bridge to ~jsrt_resolve_path()~ in ~path_resolver.c~
- [X] Handle ~options.paths~ custom search paths
- [X] Implement ~Module._findPath()~ helper
- [X] Handle file extensions (~.js~, ~.json~, ~.node~, ~.mjs~, ~.cjs~)
- [X] Resolve ~node_modules~ traversal
- [X] Handle package.json ~main~ field
- [X] Handle package.json ~exports~ field
- [X] Throw MODULE_NOT_FOUND error on failure
- [X] Return absolute path string

**** Resolution Algorithm
1. Check for builtin module
2. Check for absolute path
3. Check for relative path
4. Search in ~node_modules~
5. Check each path in ~module.paths~
6. Apply file extensions if needed

**** Test Cases
- Resolve builtin: ~Module._resolveFilename('fs')~ → ~'fs'~
- Resolve relative: ~Module._resolveFilename('./foo', parent)~
- Resolve node_modules: ~Module._resolveFilename('lodash', parent)~
- Resolve with extensions: ~Module._resolveFilename('./foo')~ → ~'./foo.js'~

*** DONE [#B] Task 1.9: Module._extensions Implementation [P][R:LOW][C:MEDIUM][D:1.2]
CLOSED: [2025-11-03 02:25]
:PROPERTIES:
:ID: 1.9
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T02:25:00Z
:DEPS: 1.2
:END:

Implement extension handlers (deprecated but needed for compatibility).

**** Subtasks
- [X] Create ~Module._extensions~ object
- [X] Implement ~Module._extensions['.js']~ handler
- [X] Implement ~Module._extensions['.json']~ handler
- [X] Implement ~Module._extensions['.node']~ stub (not supported)
- [X] Implement ~Module._extensions['.mjs']~ handler
- [X] Implement ~Module._extensions['.cjs']~ handler
- [X] Allow users to add custom handlers
- [X] Mark as deprecated in documentation

**** Handler Signature
#+BEGIN_SRC javascript
Module._extensions['.js'] = function(module, filename) {
  const content = fs.readFileSync(filename, 'utf8');
  module._compile(content, filename);
};
#+END_SRC

**** Acceptance Criteria
- Extension handlers exist and are callable
- Standard extensions work
- Custom extensions can be added
- Marked as deprecated

*** DONE [#B] Task 1.10: Module Compilation Methods [S][R:MED][C:COMPLEX][D:1.2,1.9]
CLOSED: [2025-11-03 02:36]
:PROPERTIES:
:ID: 1.10
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T02:36:00Z
:DEPS: 1.2,1.9
:END:

Implement module compilation and wrapping.

**** Subtasks
- [X] Implement ~module._compile(content, filename)~
- [X] Create CommonJS wrapper: ~(function(exports, require, module, __filename, __dirname) { ... })~
- [X] Compile wrapped code with QuickJS
- [X] Execute compiled function
- [X] Set ~module.loaded = true~ after execution
- [X] Implement ~Module.wrap(script)~ static method
- [X] Implement ~Module.wrapper~ static property (wrapper parts)
- [X] Handle compilation errors
- [X] Preserve stack traces

**** Wrapper Format
#+BEGIN_SRC javascript
Module.wrapper = [
  '(function (exports, require, module, __filename, __dirname) { ',
  '\n});'
];
#+END_SRC

**** Acceptance Criteria
- Modules compile and execute correctly
- ~module.exports~ is populated
- ~require~, ~__filename~, ~__dirname~ are available in modules
- Errors have correct stack traces

** TODO [#A] Phase 2: Source Map Support [S][R:MED][C:COMPLEX][D:phase-1] :sourcemap:
:PROPERTIES:
:ID: phase-2
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-1
:PROGRESS: 27/71
:COMPLETION: 38.0%
:END:

Implement source map parsing, lookup, and the SourceMap class.

*** DONE [#A] Task 2.1: Source Map Infrastructure [S][R:MED][C:MEDIUM]
CLOSED: [2025-11-03 03:13]
:PROPERTIES:
:ID: 2.1
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T03:13:00Z
:DEPS: phase-1
:END:

Set up source map storage and management.

**** Subtasks
- [X] Create ~src/node/module/sourcemap.c~ and ~.h~
- [X] Define ~SourceMap~ data structure
- [X] Add to build system
- [X] Create source map registry (path → source map mapping)
- [X] Implement source map storage lifecycle
- [X] Add cleanup on module unload

**** Data Structure
#+BEGIN_SRC c
typedef struct {
  char* version;               // Source map version
  char* file;                  // Generated file name
  char* sourceRoot;            // Source root path
  char** sources;              // Source file names
  int sources_count;
  char** sourcesContent;       // Inlined source content
  int sourcesContent_count;
  char** names;                // Symbol names
  int names_count;
  char* mappings;              // VLQ-encoded mappings
  JSValue payload;             // Original JSON payload
} JSRTSourceMap;
#+END_SRC

*** DONE [#A] Task 2.2: Source Map Parsing [S][R:MED][C:COMPLEX][D:2.1]
CLOSED: [2025-11-03 03:19]
:PROPERTIES:
:ID: 2.2
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T03:19:00Z
:DEPS: 2.1
:END:

Parse Source Map v3 format.

**** Subtasks
- [X] Implement JSON parsing for source map payload
- [X] Validate source map version (must be 3)
- [X] Extract source map fields (~version~, ~sources~, ~mappings~, etc.)
- [X] Implement VLQ (Variable-Length Quantity) decoder
- [X] Decode mappings string into position mappings
- [X] Build internal mapping table (generated line/col → original line/col)
- [X] Handle optional fields (~sourcesContent~, ~names~, ~sourceRoot~)
- [X] Handle invalid/malformed source maps gracefully

**** VLQ Decoding
Reference: Source Map v3 spec for VLQ encoding format
- Base64 encoding of variable-length integers
- Values: generated column, source index, original line, original column, name index

**** Acceptance Criteria
- Valid source maps parse successfully
- Invalid source maps throw appropriate errors
- Mappings are decoded correctly
- All source map fields are accessible

*** DONE [#A] Task 2.3: SourceMap Class Implementation [S][R:MED][C:MEDIUM][D:2.2]
CLOSED: [2025-11-03 03:23]
:PROPERTIES:
:ID: 2.3
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T03:23:00Z
:DEPS: 2.2
:END:

Implement the SourceMap JavaScript class.

**** Subtasks
- [X] Implement ~new SourceMap(payload[, options])~ constructor
- [X] Add ~payload~ parameter validation (must be object)
- [X] Add ~options.lineLengths~ parameter support
- [X] Store source map data in opaque handle
- [X] Expose ~sourceMap.payload~ getter (return original payload)
- [X] Add prototype methods preparation
- [X] Handle constructor errors

**** Constructor Signature
#+BEGIN_SRC javascript
const map = new SourceMap({
  version: 3,
  sources: ['input.js'],
  names: ['foo'],
  mappings: 'AAAA,CAAC,CAAE'
}, {
  lineLengths: [10, 20, 15] // Optional
});
#+END_SRC

*** DONE [#A] Task 2.4: SourceMap.findEntry() Method [S][R:MED][C:COMPLEX][D:2.3]
CLOSED: [2025-11-03 03:31]
:PROPERTIES:
:ID: 2.4
:CREATED: 2025-10-31T00:00:00Z
:COMPLETED: 2025-11-03T03:31:00Z
:DEPS: 2.3
:END:

Implement zero-indexed position lookup.

**** Subtasks
- [X] Implement ~sourceMap.findEntry(lineOffset, columnOffset)~
- [X] Binary search in mapping table by line
- [X] Linear search within line for column
- [X] Return object with mapping information
- [X] Handle missing mappings (return empty object ~{}~)
- [X] Validate parameters (must be non-negative integers)

**** Return Format
#+BEGIN_SRC javascript
{
  generatedLine: 0,
  generatedColumn: 10,
  originalSource: 'input.js',
  originalLine: 0,
  originalColumn: 5,
  name: 'foo'
}
// Or {} if no mapping found
#+END_SRC

*** TODO [#A] Task 2.5: SourceMap.findOrigin() Method [S][R:MED][C:MEDIUM][D:2.3]
:PROPERTIES:
:ID: 2.5
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 2.3
:END:

Implement one-indexed position lookup (for Error stacks).

**** Subtasks
- [ ] Implement ~sourceMap.findOrigin(lineNumber, columnNumber)~
- [ ] Convert one-indexed to zero-indexed
- [ ] Call ~findEntry()~ internally
- [ ] Convert result back to one-indexed
- [ ] Return ~{ fileName, lineNumber, columnNumber, name }~
- [ ] Handle missing mappings

**** Return Format
#+BEGIN_SRC javascript
{
  fileName: 'input.js',
  lineNumber: 1,      // 1-indexed
  columnNumber: 6,    // 1-indexed
  name: 'foo'
}
#+END_SRC

*** TODO [#A] Task 2.6: module.findSourceMap() Implementation [S][R:MED][C:MEDIUM][D:2.3]
:PROPERTIES:
:ID: 2.6
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 2.3
:END:

Implement source map lookup by file path.

**** Subtasks
- [ ] Implement ~module.findSourceMap(path)~ function
- [ ] Search for inline source map in file (~//# sourceMappingURL=data:...~)
- [ ] Search for external source map file (~//# sourceMappingURL=file.js.map~)
- [ ] Load and parse source map
- [ ] Return SourceMap instance or ~undefined~
- [ ] Cache parsed source maps
- [ ] Handle relative source map URLs

**** Source Map Discovery
1. Check for inline data URI: ~//# sourceMappingURL=data:application/json;base64,...~
2. Check for external reference: ~//# sourceMappingURL=file.js.map~
3. Look for ~<file>.js.map~ adjacent to ~<file>.js~

*** TODO [#A] Task 2.7: Source Map Support Configuration [P][R:LOW][C:SIMPLE][D:2.3]
:PROPERTIES:
:ID: 2.7
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 2.3
:END:

Implement source map configuration APIs.

**** Subtasks
- [ ] Implement ~module.getSourceMapsSupport()~
- [ ] Return ~{ enabled, nodeModules, generatedCode }~ object
- [ ] Implement ~module.setSourceMapsSupport(enabled[, options])~
- [ ] Add ~options.nodeModules~ flag (include node_modules)
- [ ] Add ~options.generatedCode~ flag (support eval/Function)
- [ ] Store configuration in runtime context
- [ ] Apply configuration to error stack traces

**** Configuration Structure
#+BEGIN_SRC javascript
module.setSourceMapsSupport(true, {
  nodeModules: true,     // Include node_modules in mappings
  generatedCode: true    // Support eval() and new Function()
});

const config = module.getSourceMapsSupport();
// { enabled: true, nodeModules: true, generatedCode: true }
#+END_SRC

*** TODO [#B] Task 2.8: Source Map Integration with Errors [S][R:MED][C:COMPLEX][D:2.6,2.7]
:PROPERTIES:
:ID: 2.8
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 2.6,2.7
:END:

Integrate source maps with error stack traces.

**** Subtasks
- [ ] Hook into QuickJS error stack generation
- [ ] For each stack frame, check if source map exists
- [ ] If source map found, map generated position to original
- [ ] Replace file/line/column in stack trace with original values
- [ ] Respect ~sourceMapSupport~ configuration
- [ ] Handle ~nodeModules~ filtering
- [ ] Handle ~generatedCode~ (eval/Function) if enabled
- [ ] Preserve unmapped frames as-is

**** Integration Point
- Hook: QuickJS error preparation callback
- Alternative: Post-process stack traces in ~Error.prepareStackTrace~

** TODO [#A] Phase 3: Compilation Cache [S][R:MED][C:COMPLEX][D:phase-2] :cache:
:PROPERTIES:
:ID: phase-3
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-2
:PROGRESS: 0/26
:COMPLETION: 0%
:END:

Implement bytecode compilation cache for faster module loading.

*** TODO [#A] Task 3.1: Cache Infrastructure [S][R:MED][C:MEDIUM]
:PROPERTIES:
:ID: 3.1
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-2
:END:

Set up compilation cache storage and management.

**** Subtasks
- [ ] Create ~src/node/module/compile_cache.c~ and ~.h~
- [ ] Define cache directory structure
- [ ] Implement cache directory creation
- [ ] Implement cache entry format (bytecode + metadata)
- [ ] Add cache versioning (invalidate on jsrt version change)
- [ ] Add to build system

**** Cache Structure
#+BEGIN_EXAMPLE
~/.jsrt/compile-cache/
  ├── version.txt          # jsrt version
  ├── <hash1>.jsc          # Compiled bytecode
  ├── <hash2>.jsc
  └── metadata.json        # Cache metadata
#+END_EXAMPLE

**** Cache Entry Format
- Hash: SHA-256 of source file path + mtime + content
- Metadata: Source path, mtime, jsrt version, QuickJS version
- Bytecode: QuickJS bytecode from ~JS_WriteObject()~

*** TODO [#A] Task 3.2: Cache Key Generation [S][R:LOW][C:MEDIUM][D:3.1]
:PROPERTIES:
:ID: 3.2
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 3.1
:END:

Generate stable cache keys for modules.

**** Subtasks
- [ ] Implement hash function (SHA-256 or FNV-1a)
- [ ] Hash source file absolute path
- [ ] Hash file modification time (mtime)
- [ ] Hash file content (optional, for portability)
- [ ] Combine hashes into cache key
- [ ] Implement ~portable~ mode (content-based hashing)

**** Portable Mode
- ~portable: false~: Hash path + mtime (fast, not relocatable)
- ~portable: true~: Hash content only (slow, survives directory moves)

*** TODO [#A] Task 3.3: Cache Read Operations [S][R:MED][C:MEDIUM][D:3.2]
:PROPERTIES:
:ID: 3.3
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 3.2
:END:

Load compiled bytecode from cache.

**** Subtasks
- [ ] Implement ~jsrt_cache_lookup(path)~
- [ ] Generate cache key from path
- [ ] Check if cache entry exists
- [ ] Validate cache entry metadata (version, mtime)
- [ ] Load bytecode from ~.jsc~ file
- [ ] Deserialize bytecode with ~JS_ReadObject()~
- [ ] Return compiled module or NULL if invalid/missing
- [ ] Update cache statistics (hits/misses)

**** Validation Checks
- jsrt version matches
- QuickJS version matches
- Source file mtime matches (non-portable mode)
- Bytecode format is valid

*** TODO [#A] Task 3.4: Cache Write Operations [S][R:MED][C:MEDIUM][D:3.2]
:PROPERTIES:
:ID: 3.4
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 3.2
:END:

Save compiled bytecode to cache.

**** Subtasks
- [ ] Implement ~jsrt_cache_store(path, bytecode)~
- [ ] Generate cache key from path
- [ ] Serialize bytecode with ~JS_WriteObject()~
- [ ] Write bytecode to ~.jsc~ file
- [ ] Write metadata (path, mtime, versions)
- [ ] Handle disk write errors gracefully
- [ ] Implement atomic write (temp file + rename)
- [ ] Update cache statistics

**** Error Handling
- Disk full → log warning, continue without caching
- Permission denied → disable cache for session
- Corrupted cache → delete and recreate

*** TODO [#A] Task 3.5: module.enableCompileCache() Implementation [S][R:MED][C:MEDIUM][D:3.3,3.4]
:PROPERTIES:
:ID: 3.5
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 3.3,3.4
:END:

Enable compilation cache with configuration.

**** Subtasks
- [ ] Implement ~module.enableCompileCache([options])~
- [ ] Accept string parameter (cache directory)
- [ ] Accept object parameter (~{ directory, portable }~)
- [ ] Set cache directory (default: ~$HOME/.jsrt/compile-cache/~)
- [ ] Set portable mode flag
- [ ] Initialize cache directory
- [ ] Return status object ~{ status, message, directory }~
- [ ] Handle already-enabled case

**** Return Status Values
- ~module.constants.compileCacheStatus.ENABLED~ (0)
- ~module.constants.compileCacheStatus.ALREADY_ENABLED~ (1)
- ~module.constants.compileCacheStatus.FAILED~ (-1)
- ~module.constants.compileCacheStatus.DISABLED~ (-2)

**** Example Usage
#+BEGIN_SRC javascript
const result = module.enableCompileCache({
  directory: '/tmp/jsrt-cache',
  portable: true
});
console.log(result);
// { status: 0, message: 'success', directory: '/tmp/jsrt-cache' }
#+END_SRC

*** TODO [#A] Task 3.6: Cache Helper Functions [P][R:LOW][C:SIMPLE][D:3.5]
:PROPERTIES:
:ID: 3.6
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 3.5
:END:

Implement cache management utilities.

**** Subtasks
- [ ] Implement ~module.getCompileCacheDir()~
- [ ] Return cache directory or ~undefined~ if disabled
- [ ] Implement ~module.flushCompileCache()~
- [ ] Write all pending cache entries to disk
- [ ] Implement ~module.constants.compileCacheStatus~ constants
- [ ] Add status codes: ~ENABLED~, ~ALREADY_ENABLED~, ~FAILED~, ~DISABLED~

*** TODO [#B] Task 3.7: Cache Integration with Module Loader [S][R:MED][C:MEDIUM][D:3.5]
:PROPERTIES:
:ID: 3.7
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 3.5
:END:

Integrate cache with module loading pipeline.

**** Subtasks
- [ ] Hook into ~jsrt_load_module()~ before compilation
- [ ] Check cache before compiling module
- [ ] If cached bytecode found and valid, use it
- [ ] If cache miss, compile and store in cache
- [ ] Add cache bypass flag for development
- [ ] Add ~--no-compile-cache~ CLI flag
- [ ] Update module loader statistics

**** Integration Flow
#+BEGIN_EXAMPLE
Module Load Request
  ↓
Check Cache (if enabled)
  ↓
Hit? → Load Bytecode → Execute
  ↓
Miss? → Compile Source → Store in Cache → Execute
#+END_EXAMPLE

*** TODO [#B] Task 3.8: Cache Maintenance [P][R:LOW][C:SIMPLE][D:3.5]
:PROPERTIES:
:ID: 3.8
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 3.5
:END:

Implement cache cleanup and maintenance.

**** Subtasks
- [ ] Implement cache size limit (default: 100MB)
- [ ] Implement LRU eviction when size exceeded
- [ ] Implement cache cleanup on startup (remove stale entries)
- [ ] Add ~module.clearCompileCache()~ function
- [ ] Add cache statistics reporting

** TODO [#B] Phase 4: Module Hooks (Basic) [S][R:HIGH][C:COMPLEX][D:phase-3] :hooks:
:PROPERTIES:
:ID: phase-4
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-3
:PROGRESS: 0/32
:COMPLETION: 0%
:END:

Implement basic module resolution and loading hooks (synchronous only).

*** TODO [#A] Task 4.1: Hook Infrastructure [S][R:HIGH][C:MEDIUM]
:PROPERTIES:
:ID: 4.1
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-3
:END:

Set up hook registration and execution framework.

**** Subtasks
- [ ] Create ~src/node/module/hooks.c~ and ~.h~
- [ ] Define hook chain data structure
- [ ] Implement hook registration list
- [ ] Implement hook chaining (LIFO order)
- [ ] Add to build system

**** Hook Chain Structure
#+BEGIN_SRC c
typedef struct JSRTModuleHook {
  JSValue resolve_fn;    // resolve(specifier, context, next)
  JSValue load_fn;       // load(url, context, next)
  struct JSRTModuleHook* next;  // Next in chain (LIFO)
} JSRTModuleHook;
#+END_SRC

*** TODO [#A] Task 4.2: module.registerHooks() Implementation [S][R:MED][C:MEDIUM][D:4.1]
:PROPERTIES:
:ID: 4.2
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 4.1
:END:

Implement synchronous hook registration (main thread only).

**** Subtasks
- [ ] Implement ~module.registerHooks(options)~
- [ ] Accept ~options.resolve~ function
- [ ] Accept ~options.load~ function
- [ ] Validate hook functions (must be functions)
- [ ] Add hooks to chain (LIFO order)
- [ ] Store hooks in runtime context
- [ ] Return hook registration handle

**** Function Signature
#+BEGIN_SRC javascript
module.registerHooks({
  resolve(specifier, context, nextResolve) {
    // Custom resolution logic
    return nextResolve(specifier, context);
  },
  load(url, context, nextLoad) {
    // Custom loading logic
    return nextLoad(url, context);
  }
});
#+END_SRC

*** TODO [#A] Task 4.3: Resolve Hook Execution [S][R:MED][C:COMPLEX][D:4.2]
:PROPERTIES:
:ID: 4.3
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 4.2
:END:

Execute resolve hooks during module resolution.

**** Subtasks
- [ ] Hook into ~jsrt_resolve_path()~ before resolution
- [ ] Build ~context~ object (~{ conditions, parentURL }~)
- [ ] Call first resolve hook in chain
- [ ] Implement ~nextResolve()~ function for chaining
- [ ] Validate hook return value (~{ url, format?, shortCircuit? }~)
- [ ] Handle ~shortCircuit~ flag (skip remaining hooks)
- [ ] Fall back to default resolution if no hooks
- [ ] Handle hook errors

**** Context Object
#+BEGIN_SRC javascript
{
  conditions: ['node', 'import'],  // or ['node', 'require']
  importAttributes: {},            // ESM import attributes
  parentURL: 'file:///path/to/parent.js'
}
#+END_SRC

**** Return Object
#+BEGIN_SRC javascript
{
  url: 'file:///path/to/module.js',
  format: 'module',      // or 'commonjs', 'json', etc.
  shortCircuit: false    // Skip remaining hooks?
}
#+END_SRC

*** TODO [#A] Task 4.4: Load Hook Execution [S][R:MED][C:COMPLEX][D:4.2]
:PROPERTIES:
:ID: 4.4
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 4.2
:END:

Execute load hooks during module loading.

**** Subtasks
- [ ] Hook into protocol dispatcher before loading
- [ ] Build ~context~ object (~{ format, conditions }~)
- [ ] Call first load hook in chain
- [ ] Implement ~nextLoad()~ function for chaining
- [ ] Validate hook return value (~{ format, source, shortCircuit? }~)
- [ ] Handle ~shortCircuit~ flag
- [ ] Support multiple source types (string, ArrayBuffer, Uint8Array)
- [ ] Fall back to default loading if no hooks
- [ ] Handle hook errors

**** Context Object
#+BEGIN_SRC javascript
{
  format: 'module',
  conditions: ['node', 'import'],
  importAttributes: {}
}
#+END_SRC

**** Return Object
#+BEGIN_SRC javascript
{
  format: 'module',
  source: 'export default 42;',  // or ArrayBuffer, Uint8Array
  shortCircuit: false
}
#+END_SRC

*** TODO [#B] Task 4.5: Hook Error Handling [P][R:MED][C:MEDIUM][D:4.3,4.4]
:PROPERTIES:
:ID: 4.5
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 4.3,4.4
:END:

Implement robust error handling for hooks.

**** Subtasks
- [ ] Catch exceptions in hook functions
- [ ] Wrap hook errors with context (which hook, module being loaded)
- [ ] Propagate errors to user code
- [ ] Allow hooks to throw and abort loading
- [ ] Log hook errors to stderr
- [ ] Add ~--trace-module-hooks~ debug flag

*** TODO [#C] Task 4.6: Hook Examples and Documentation [P][R:LOW][C:SIMPLE][D:4.2]
:PROPERTIES:
:ID: 4.6
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 4.2
:END:

Create examples demonstrating hook usage.

**** Subtasks
- [ ] Create ~examples/module/hooks-resolve.js~
- [ ] Create ~examples/module/hooks-load.js~
- [ ] Create ~examples/module/hooks-transform.js~
- [ ] Document hook API in ~docs/module-hooks.md~
- [ ] Add TypeScript type definitions

** TODO [#B] Phase 5: Package.json Utilities [PS][R:LOW][C:MEDIUM][D:phase-3] :utils:
:PROPERTIES:
:ID: phase-5
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-3
:PROGRESS: 0/12
:COMPLETION: 0%
:END:

Implement package.json lookup and parsing utilities.

*** TODO [#A] Task 5.1: module.findPackageJSON() Implementation [S][R:LOW][C:MEDIUM]
:PROPERTIES:
:ID: 5.1
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-3
:END:

Find nearest package.json file.

**** Subtasks
- [ ] Implement ~module.findPackageJSON(specifier[, base])~
- [ ] Resolve ~specifier~ to absolute path
- [ ] Search upward for ~package.json~ (parent directories)
- [ ] Return absolute path to ~package.json~ or ~undefined~
- [ ] Cache package.json paths for performance
- [ ] Handle file vs directory specifiers

**** Search Algorithm
#+BEGIN_EXAMPLE
Given: /path/to/project/src/utils/helper.js
Search:
1. /path/to/project/src/utils/package.json
2. /path/to/project/src/package.json
3. /path/to/project/package.json
4. /path/to/package.json
5. /path/package.json
Stop at first found, or return undefined
#+END_EXAMPLE

*** TODO [#B] Task 5.2: Package.json Parsing Utilities [P][R:LOW][C:SIMPLE][D:5.1]
:PROPERTIES:
:ID: 5.2
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 5.1
:END:

Helper functions for package.json manipulation.

**** Subtasks
- [ ] Implement ~parsePackageJSON(path)~ helper
- [ ] Validate package.json format
- [ ] Extract common fields (~name~, ~version~, ~main~, ~exports~, ~type~)
- [ ] Cache parsed package.json data
- [ ] Handle malformed package.json gracefully

** TODO [#B] Phase 6: Advanced Features [PS][R:MED][C:MEDIUM][D:phase-4] :advanced:
:PROPERTIES:
:ID: phase-6
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-4
:PROGRESS: 0/15
:COMPLETION: 0%
:END:

Implement advanced module features and optimizations.

*** TODO [#A] Task 6.1: module.syncBuiltinESMExports() Implementation [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 6.1
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-4
:END:

Synchronize CommonJS and ESM exports for built-in modules.

**** Subtasks
- [ ] Implement ~module.syncBuiltinESMExports()~
- [ ] Iterate through built-in modules
- [ ] For each module with both CJS and ESM exports
- [ ] Copy new properties from CJS to ESM namespace
- [ ] Update existing properties (live bindings)
- [ ] Skip deleted properties (don't remove from ESM)

**** Use Case
When Node.js updates built-in module exports at runtime:
#+BEGIN_SRC javascript
const fs = require('fs');
fs.newFunction = () => {};  // Add new export

module.syncBuiltinESMExports();

import fs from 'fs';
fs.newFunction();  // Now available
#+END_SRC

*** TODO [#C] Task 6.2: Module Loading Statistics [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 6.2
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-4
:END:

Expose module loading performance statistics.

**** Subtasks
- [ ] Track module loading times
- [ ] Track cache hit/miss ratios
- [ ] Track compilation times
- [ ] Expose ~Module.getStatistics()~ function
- [ ] Add ~--trace-module-loading~ debug flag

*** TODO [#C] Task 6.3: Hot Module Replacement Support [P][R:MED][C:COMPLEX]
:PROPERTIES:
:ID: 6.3
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-4
:END:

Enable module reloading for development.

**** Subtasks
- [ ] Implement ~module.reloadModule(path)~
- [ ] Invalidate module cache
- [ ] Re-execute module code
- [ ] Update existing references (if possible)
- [ ] Trigger reload callbacks
- [ ] Handle module dependency updates

** TODO [#C] Phase 7: Testing & Quality Assurance [S][R:LOW][C:MEDIUM][D:phase-1,phase-2,phase-3,phase-4,phase-5,phase-6] :testing:
:PROPERTIES:
:ID: phase-7
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:PROGRESS: 0/5
:COMPLETION: 0%
:END:

Comprehensive testing and quality validation.

*** TODO [#A] Task 7.1: Unit Test Suite [S][R:LOW][C:MEDIUM]
:PROPERTIES:
:ID: 7.1
:CREATED: 2025-10-31T00:00:00Z
:DEPS: phase-6
:END:

Create comprehensive unit tests for all APIs.

**** Test Coverage Areas
- [ ] Module class APIs (~builtinModules~, ~isBuiltin()~, ~createRequire()~)
- [ ] Module loading (~_load~, ~_resolveFilename~, ~_cache~)
- [ ] Source maps (~SourceMap~ class, ~findSourceMap()~, integration)
- [ ] Compilation cache (enable, get, flush, invalidation)
- [ ] Module hooks (register, resolve, load, chaining)
- [ ] Package.json utilities (~findPackageJSON()~)
- [ ] Edge cases (circular deps, missing modules, invalid input)
- [ ] Memory safety (AddressSanitizer clean)

**** Test Files
- ~test/node/module/test_module_class.js~
- ~test/node/module/test_module_loading.js~
- ~test/node/module/test_source_maps.js~
- ~test/node/module/test_compile_cache.js~
- ~test/node/module/test_module_hooks.js~
- ~test/node/module/test_package_json.js~

*** TODO [#A] Task 7.2: Integration Testing [S][R:MED][C:MEDIUM][D:7.1]
:PROPERTIES:
:ID: 7.2
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 7.1
:END:

Test integration with existing jsrt systems.

**** Integration Points
- [ ] Test with existing module loader (~src/module/~)
- [ ] Test with CommonJS loader
- [ ] Test with ESM loader
- [ ] Test with builtin modules
- [ ] Test with HTTP/HTTPS modules
- [ ] Test ~require()~ from ~node:module~
- [ ] Test ~import~ with hooks
- [ ] Verify no regressions in existing tests

*** TODO [#A] Task 7.3: Memory Safety Validation [P][R:MED][C:MEDIUM][D:7.1]
:PROPERTIES:
:ID: 7.3
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 7.1
:END:

Ensure no memory leaks or safety issues.

**** Validation Steps
- [ ] Run all tests with ~make jsrt_m~ (AddressSanitizer)
- [ ] Check for memory leaks (~ASAN_OPTIONS=detect_leaks=1~)
- [ ] Check for use-after-free
- [ ] Check for buffer overflows
- [ ] Verify proper ~JS_FreeValue()~ calls
- [ ] Check reference counting for Module objects

*** TODO [#A] Task 7.4: Performance Benchmarking [P][R:LOW][C:SIMPLE][D:7.1]
:PROPERTIES:
:ID: 7.4
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 7.1
:END:

Measure performance impact of module API.

**** Benchmarks
- [ ] Module loading time (with/without cache)
- [ ] Cache hit/miss performance
- [ ] Source map lookup performance
- [ ] Hook execution overhead
- [ ] Memory usage per module

**** Performance Targets
- Cache hit: < 0.1ms overhead
- Cache miss: < 5ms overhead (compilation + caching)
- Hook execution: < 0.5ms per hook
- Source map lookup: < 1ms

*** TODO [#B] Task 7.5: Documentation & Examples [P][R:LOW][C:SIMPLE][D:7.1]
:PROPERTIES:
:ID: 7.5
:CREATED: 2025-10-31T00:00:00Z
:DEPS: 7.1
:END:

Complete documentation and examples.

**** Documentation
- [ ] API reference document (~docs/node-module-api.md~)
- [ ] Integration guide (~docs/node-module-integration.md~)
- [ ] Hook development guide (~docs/module-hooks-guide.md~)
- [ ] Source map setup guide
- [ ] Compilation cache configuration guide

**** Examples
- [ ] ~examples/module/basic-usage.js~
- [ ] ~examples/module/create-require-esm.mjs~
- [ ] ~examples/module/custom-hooks.js~
- [ ] ~examples/module/source-maps.js~
- [ ] ~examples/module/compile-cache.js~

* 🚀 Execution Dashboard
:PROPERTIES:
:CURRENT_PHASE: Phase 2 - IN PROGRESS 🔄
:PROGRESS: 37/171
:COMPLETION: 21.6%
:ACTIVE_TASK: Task 2.5 - SourceMap.findOrigin() Method
:UPDATED: 2025-11-03T03:31:00Z
:END:

** Current Status
- Phase: Phase 2 IN PROGRESS 🔄 (Source Map Support)
- Progress: 37/171 tasks (21.6%)
- Active: Task 2.4 complete ✅ (SourceMap.findEntry()), ready for Task 2.5 (findOrigin)

** Completed (Phase 1)
1. [X] Task 1.1: Project Structure Setup
2. [X] Task 1.2: Module Class Foundation
3. [X] Task 1.3: module.builtinModules Implementation
4. [X] Task 1.4: module.isBuiltin() Implementation
5. [X] Task 1.5: module.createRequire() Implementation
6. [X] Task 1.6: Module._cache Implementation
7. [X] Task 1.7: Module._load Implementation
8. [X] Task 1.8: Module._resolveFilename Implementation
9. [X] Task 1.9: Module._extensions Implementation
10. [X] Task 1.10: Module Compilation Methods

** Completed (Phase 2)
1. [X] Task 2.1: Source Map Infrastructure ✅
2. [X] Task 2.2: Source Map Parsing & VLQ Decoder ✅
3. [X] Task 2.3: SourceMap Class Implementation ✅
4. [X] Task 2.4: SourceMap.findEntry() Method ✅

** Next Steps (Phase 2)
1. [ ] Task 2.5: SourceMap.findOrigin() Method ← READY TO START
2. [ ] Task 2.6: module.findSourceMap() Implementation
3. [ ] Task 2.7: Source Map Support Configuration

** Phase Overview
| Phase | Title | Tasks | Status | Completion |
|-------|-------|-------|--------|------------|
| 1 | Foundation & Core API | 10 | ✅ DONE | 100% |
| 2 | Source Map Support | 71 | 🔄 IN PROGRESS | 38.0% |
| 3 | Compilation Cache | 26 | TODO | 0% |
| 4 | Module Hooks (Basic) | 32 | TODO | 0% |
| 5 | Package.json Utilities | 12 | TODO | 0% |
| 6 | Advanced Features | 15 | TODO | 0% |
| 7 | Testing & QA | 5 | TODO | 0% |
| **Total** | | **171** | | **21.6%** |

* 📜 History & Updates
:LOGBOOK:
- Note taken on [2025-11-03T03:31:00Z] \\
  Task 2.4 COMPLETED: SourceMap.findEntry() method (binary search, VLQ decoding to mappings, 13/13 tests passing)
- Note taken on [2025-11-03T03:23:00Z] \\
  Task 2.3 COMPLETED: SourceMap JavaScript class (constructor, payload getter, findEntry/findOrigin stubs, finalizer)
- Note taken on [2025-11-03T03:19:00Z] \\
  Task 2.2 COMPLETED: VLQ decoder & source map parsing (Base64, VLQ, JSON parsing, field extraction)
- Note taken on [2025-11-03T03:13:00Z] \\
  Task 2.1 COMPLETED: Source map infrastructure implemented (cache, lifecycle, runtime integration)
- Note taken on [2025-11-03T02:37:00Z] \\
  Phase 1 COMPLETED: All 10 core module API tasks implemented and tested
- Note taken on [2025-10-31T00:00:00Z] \\
  Initial plan document created with 7 phases and 153 tasks
:END:

** Recent Changes
| Timestamp | Action | Task ID | Details |
|-----------|--------|---------|---------|
| 2025-11-03T03:31:00Z | Completed | 2.4 | SourceMap.findEntry() with binary search, VLQ→mapping conversion, 13/13 tests passing ✅ |
| 2025-11-03T03:23:00Z | Completed | 2.3 | SourceMap class with payload, findEntry, findOrigin, 13/13 tests passing ✅ |
| 2025-11-03T03:19:00Z | Completed | 2.2 | VLQ decoder & JSON parsing, 12/12 module tests passing ✅ |
| 2025-11-03T03:13:00Z | Completed | 2.1 | Source map cache & infrastructure, tests passing ✅ |
| 2025-11-03T02:37:00Z | Completed | Phase 1 | All 10 tasks done, 100% tests passing |
| 2025-11-03T02:36:00Z | Completed | 1.10 | Module._compile() and compilation methods |
| 2025-11-03T02:35:00Z | Completed | 1.5 | module.createRequire() for ESM contexts |
| 2025-11-03T02:33:00Z | Completed | 1.8 | Module._resolveFilename() path resolution |
| 2025-11-03T02:30:00Z | Completed | 1.6 | Module._cache implementation |
| 2025-11-03T02:25:00Z | Completed | 1.9 | Module._extensions object |
| 2025-10-31T08:42:00Z | Completed | 1.3,1.4 | builtinModules and isBuiltin() |
| 2025-10-31T08:14:00Z | Completed | 1.2 | Module class foundation |
| 2025-10-31T07:51:00Z | Completed | 1.1 | Project structure setup |
| 2025-10-31T00:00:00Z | Created | - | Initial plan document |

** Risk Assessment

*** Technical Risks
1. **Source Map Parsing Complexity** [MED]
   - VLQ decoding is complex
   - Mitigation: Use existing libraries or reference implementations

2. **QuickJS Bytecode Serialization** [MED]
   - ~JS_WriteObject()~/~JS_ReadObject()~ may have limitations
   - Mitigation: Test thoroughly, implement validation

3. **Hook Thread Isolation** [HIGH]
   - Async hooks require worker threads (not implemented in Phase 4)
   - Mitigation: Start with sync hooks only, defer async to future phase

4. **Memory Management** [MED]
   - Module lifecycle and cache cleanup complex
   - Mitigation: Use AddressSanitizer, comprehensive tests

*** Integration Risks
1. **Existing Module Loader** [LOW]
   - Need to integrate without breaking existing code
   - Mitigation: Extensive integration tests, gradual rollout

2. **Node.js Compatibility** [MED]
   - API surface is large, behavior must match Node.js
   - Mitigation: Test against Node.js test suite where possible

*** Mitigation Strategies
- Incremental implementation (phase by phase)
- Comprehensive testing at each phase
- Memory safety validation throughout
- Regular integration testing with existing code
- Clear rollback points (each phase can be disabled)

* Implementation Notes

** Code Style & Patterns
- Follow jsrt conventions (~JSRT_~ prefix for types, ~jsrt_~ for functions)
- Use QuickJS idioms (~JS_NewCFunction2~, ~JS_SetPropertyStr~, etc.)
- Memory: Always pair ~JS_DupValue~/~JS_FreeValue~
- Error handling: Use ~JS_ThrowTypeError~, ~JS_ThrowReferenceError~, etc.

** Build Integration
- Add new files to ~src/node/module/~ directory
- Update ~CMakeLists.txt~ with new source files
- Add to ~Makefile~ dependencies
- Ensure ~make format~ works on all new files

** Testing Strategy
- Unit tests: ~test/node/module/*.js~
- Run with: ~make test N=node/module~
- Memory safety: ~make jsrt_m && ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/node/module/*.js~
- Integration: Run full test suite after each phase

** Dependency Management
Tasks are marked with dependencies:
- ~[D:TaskID]~ means task depends on completing TaskID first
- ~[D:phase-N]~ means task depends on entire phase N
- Sequential tasks ~[S]~ must complete in order
- Parallel tasks ~[P]~ can be worked on simultaneously
- Parallel-Sequential ~[PS]~ can run in parallel within phase

** Progress Tracking
Use the three-level status tracking system:
1. Update subtask checkboxes as work completes
2. Update task TODO/IN-PROGRESS/DONE status
3. Update phase status and progress counters
4. Log significant milestones in History section
