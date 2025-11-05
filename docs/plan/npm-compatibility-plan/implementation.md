# npm Popular Packages Compatibility Implementation Details

## Implementation Strategy Overview

This document provides detailed technical implementation specifications for the npm compatibility support plan, focusing on concrete code changes and API implementations required to support the 50 failing packages.

## Category 1: Critical Node.js API Implementation

### 1.1 Node.js Domain Module Implementation

**Files to Create/Modify:**
- `src/node/domain.c` (new file)
- `src/node/domain.h` (new file)
- `src/node/node_modules.c` (modify to register domain module)

**Implementation Details:**

```c
// src/node/domain.c - Core domain API implementation
#include "domain.h"
#include "../std/events.h"
#include "../util/debug.h"

typedef struct JSRTDomain {
    JSContext* ctx;
    JSValue obj;
    JSValue members;  // Set of domain members
    JSValue error_handler;
    struct JSRTDomain* parent;
    int disposed;
} JSRTDomain;

// Core domain methods
static JSValue js_domain_create(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_domain_enter(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_domain_exit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_domain_add(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_domain_remove(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_domain_bind(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_domain_intercept(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
```

**Package Impact:** aws-sdk, babel_core, babel_eslint, babel_loader, babel_preset_es2015, ember_cli_babel

### 1.2 Timers Module Enhancement

**Files to Modify:**
- `src/node/node_timers.c` (extend existing)
- `src/node/timers_promises.c` (new file)

**Required API Surface:**

```c
// Enhanced timer exports
static JSValue js_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_set_immediate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_clear_immediate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// timers/promises API
static JSValue js_timers_setTimeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_timers_setImmediate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
```

**Package Impact:** mongodb, zone_js, async, request, node-fetch

### 1.3 fs/promises Module Implementation

**Files to Create:**
- `src/node/fs_promises.c` (new file)
- `src/node/fs_promises.h` (new file)

**Implementation Details:**

```c
// Promise-based file operations
static JSValue js_fs_promises_access(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_appendFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_chmod(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_chown(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_copyFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_lchmod(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_lchown(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_link(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_lstat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_mkdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_mkdtemp(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_open(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_readdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_readFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_readlink(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_realpath(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_rename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_rmdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_stat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_symlink(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_truncate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_unlink(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_utimes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_fs_promises_writeFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
```

**Package Impact:** eslint, webpack, webpack_dev_server, html_webpack_plugin

### 1.4 V8 Inspector API Stubs

**Files to Create:**
- `src/node/v8_inspector.c` (new file)
- `src/node/inspector_agent.c` (new file)

**Implementation Details:**

```c
// Minimal V8 inspector API surface
static JSValue js_inspector_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_inspector_disconnect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_inspector_open(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_inspector_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Performance hooks
static JSValue js_performance_mark(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_performance_measure(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_performance_clearMarks(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_performance_clearMeasures(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
```

**Package Impact:** vue, react_dom, babel_core, typescript

## Category 2: Module System Enhancements

### 2.1 Enhanced Module Resolution

**Files to Modify:**
- `src/module/resolver/path_resolver.c` (enhance existing)
- `src/module/resolver/relative_resolver.c` (new file)

**Implementation Details:**

```c
// Enhanced relative resolution for transpiled code
static char* resolve_relative_path(JSContext* ctx, const char* base, const char* specifier);
static int is_core_module(const char* name);
static char* resolve_node_module(const char* name, const char* base_path);
static int handle_legacy_extensions(const char* path, char** resolved_path);
```

### 2.2 Core-js Polyfill Integration

**Files to Create:**
- `src/std/polyfills.c` (new file)
- `src/std/core_js_shims.c` (new file)

**Implementation Details:**

```c
// Core-js symbol polyfills
static JSValue js_symbol_for(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_symbol_keyFor(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// ES2015+ built-ins
static JSValue js_array_from(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_array_of(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_object_assign(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_object_keys(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Promise polyfills
static JSValue js_promise_all(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_promise_race(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_promise_resolve(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_promise_reject(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
```

## Category 3: Tooling & Build System Support

### 3.1 Process Stack Trace API

**Files to Modify:**
- `src/node/process.c` (enhance existing)
- `src/std/error.c` (enhance existing)

**Implementation Details:**

```c
// Stack trace enhancement
static JSValue js_error_captureStackTrace(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_error_prepareStackTrace(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_error_stackTraceLimit_getter(JSContext* ctx, JSValueConst this_val);
static JSValue js_error_stackTraceLimit_setter(JSContext* ctx, JSValueConst this_val, JSValueConst value);
```

### 3.2 Stream API Enhancements

**Files to Modify:**
- `src/std/stream.c` (enhance existing)
- `src/std/transform.c` (enhance existing)

**Implementation Details:**

```c
// Transform stream fixes
static JSValue js_transform_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_transform_flush(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_transform_final(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Stream improvements
static JSValue js_stream_pipe_with_options(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_stream_async_iterator(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
```

## Testing Strategy

### Individual Package Testing

```bash
# Test specific packages with timeout
timeout 30s ./bin/jsrt examples/popular_npm_packages/test_aws_sdk.js
timeout 30s ./bin/jsrt examples/popular_npm_packages/test_babel_core.js
timeout 30s ./bin/jsrt examples/popular_npm_packages/test_express.js

# Batch testing by category
for pkg in aws_sdk babel_core babel_eslint; do
    echo "Testing $pkg..."
    timeout 30s ./bin/jsrt examples/popular_npm_packages/test_${pkg}.js
done
```

### Regression Testing

```bash
# Ensure existing functionality works
make test
make wpt N=module
make wpt N=node

# Memory safety validation
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 timeout 60s ./bin/jsrt_m examples/popular_npm_packages/test_aws_sdk.js
```

### Success Metrics

**Phase 1 Success Criteria:**
- aws-sdk loads without 'domain' errors
- babel_core loads without 'core-js' errors
- mongodb loads without 'timers' errors
- eslint loads without 'fs/promises' errors
- vue loads without V8 API errors

**Overall Success Criteria:**
- Reduce failure rate from 50% to ≤15%
- All Phase 1 packages (25) load successfully
- Module resolution works for transpiled code
- No regressions in currently working packages

## Development Workflow

1. **Implementation Phase**
   - Implement one API category at a time
   - Test each implementation with affected packages
   - Run `make test` after each major change
   - Run `make format` before commits

2. **Validation Phase**
   - Test all 100 packages after each phase completion
   - Document remaining issues and blockers
   - Update the compatibility matrix

3. **Integration Phase**
   - Final testing of all implemented changes
   - Performance impact assessment
   - Documentation updates

## Risk Assessment & Mitigation

### High-Risk Implementations
- **V8 APIs**: Provide functional stubs with clear warnings
- **Native modules**: Detect and provide informative error messages
- **Complex stream interactions**: Careful testing to avoid regressions

### Medium-Risk Implementations
- **Module resolution changes**: Extensive testing across package types
- **Async/await integration**: Ensure proper promise handling
- **Error handling improvements**: Maintain backward compatibility

### Low-Risk Implementations
- **Utility functions**: Straightforward polyfill implementations
- **Simple API additions**: Clear interfaces and existing patterns
- **Enhanced error messages**: User experience improvements

## Implementation Priority Matrix

| Category | Impact | Effort | Priority | Dependencies |
|----------|--------|--------|----------|--------------|
| Domain Module | High (6 packages) | High | Critical | None |
| fs/promises | High (4 packages) | Medium | Critical | None |
| Timers Module | High (5 packages) | Medium | Critical | None |
| Module Resolution | High (10 packages) | Medium | High | Critical APIs |
| Stream Enhancements | Medium (8 packages) | Medium | High | None |
| V8 Inspector APIs | Medium (4 packages) | High | Medium | Critical APIs |
| Framework Support | Low (5 packages) | High | Low | All previous phases |

This implementation plan provides a structured approach to achieving the target ≤15% failure rate while maintaining jsrt's lightweight design principles and development standards.