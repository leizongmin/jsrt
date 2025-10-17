# WebAssembly Implementation: Next Immediate Tasks - Detailed Breakdown

**Created**: 2025-10-17
**Status**: Ready for Execution
**Context**: Phase 1 ✅ Complete, Phase 2 🔴 52% (blocked), Phase 3 🔵 34% (in progress)

---

## Executive Summary

This document provides atomic, executable task breakdowns for the next immediate WebAssembly implementation priorities. Tasks are organized by execution order with clear dependencies, risk assessment, and parallel execution opportunities.

**Current State:**
- Phase 1: ✅ COMPLETED (25/25 tasks) - Infrastructure & Error Types
- Phase 2: 🔴 BLOCKED (22/42 tasks - 52%) - Tasks 2.4-2.6 blocked by WAMR API limitation
- Phase 3: 🔵 IN_PROGRESS (13/38 tasks - 34%) - Tasks 3.3 and 3.4 complete

**Critical Path:**
1. ✅ Task 2.3: Module.customSections() - Independent, can execute now
2. 🔴 Task 3.1: Instance Import Object Parsing - Critical for Phase 3
3. 🔴 Task 3.2: Function Import Wrapping - Depends on 3.1
4. 🟡 Task 3.5: Update Instance Constructor - Depends on 3.2

---

## Priority 1: Task 2.3 - Module.customSections() [READY TO EXECUTE]

### Overview
**ID**: 2.3
**Priority**: Medium (Low impact but completes Phase 2 partial work)
**Complexity**: SIMPLE
**Risk**: LOW
**Execution Mode**: [P] Can run in parallel with other tasks
**Dependencies**: Task 2.2 (Module.imports) - ✅ COMPLETED
**Estimated Effort**: 2-3 hours

### Technical Approach

#### Background
WebAssembly modules can contain custom sections for metadata, debugging info, or application-specific data. The `Module.customSections(module, sectionName)` static method returns all custom sections matching the given name as an array of ArrayBuffer.

#### WAMR API Investigation
**CRITICAL FIRST STEP**: Research WAMR support for custom sections
- Check `wasm_export.h` for custom section APIs
- WAMR may not expose custom sections (likely limitation)
- If not exposed, implement minimal parser or document limitation

#### Implementation Strategy

**Option A: WAMR Provides API (Ideal)**
```c
// Expected WAMR API (if exists):
uint32_t wasm_runtime_get_custom_section_count(wasm_module_t module, const char* name);
bool wasm_runtime_get_custom_section(wasm_module_t module, uint32_t index,
                                      const char* name, uint8_t** data, uint32_t* size);
```

**Option B: WAMR Lacks API (Likely)**
- Access `module_data->wasm_bytes` (raw WASM binary)
- Implement minimal WASM binary parser for custom sections
- Parse section structure: `[0x00 (custom) | length | name_len | name | payload]`
- Match section name, extract payload as ArrayBuffer

**Option C: Document Limitation (Fallback)**
- Return empty array with documentation note
- Add TODO for future enhancement when WAMR adds support

#### Implementation Steps

**Phase 1: Research (30 min)**
1. [ ] Read `/repo/deps/wamr/core/iwasm/include/wasm_export.h`
2. [ ] Search for keywords: "custom", "section", "metadata"
3. [ ] Check WAMR documentation in `/repo/deps/wamr/doc/`
4. [ ] Decide on Option A, B, or C

**Phase 2: Implementation (1-2 hours)**

**If Option A (WAMR API exists):**
```c
static JSValue js_webassembly_module_custom_sections(JSContext* ctx, JSValueConst this_val,
                                                      int argc, JSValueConst* argv) {
  // Validate arguments
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Module.customSections requires 2 arguments");
  }

  // Get module data
  jsrt_wasm_module_data_t* module_data = JS_GetOpaque(argv[0], js_webassembly_module_class_id);
  if (!module_data) {
    return JS_ThrowTypeError(ctx, "First argument must be a WebAssembly.Module");
  }

  // Get section name
  const char* section_name = JS_ToCString(ctx, argv[1]);
  if (!section_name) {
    return JS_EXCEPTION;
  }

  // Get matching custom sections
  uint32_t count = wasm_runtime_get_custom_section_count(module_data->module, section_name);

  // Create result array
  JSValue result = JS_NewArray(ctx);

  // Extract each matching section
  for (uint32_t i = 0; i < count; i++) {
    uint8_t* data;
    uint32_t size;
    if (wasm_runtime_get_custom_section(module_data->module, i, section_name, &data, &size)) {
      // Create ArrayBuffer with copied data
      JSValue buffer = JS_NewArrayBufferCopy(ctx, data, size);
      JS_SetPropertyUint32(ctx, result, i, buffer);
    }
  }

  JS_FreeCString(ctx, section_name);
  return result;
}
```

**If Option B (Manual parsing):**
```c
static JSValue js_webassembly_module_custom_sections(JSContext* ctx, JSValueConst this_val,
                                                      int argc, JSValueConst* argv) {
  // Validate arguments (same as Option A)
  // ...

  // Parse WASM binary format
  uint8_t* bytes = module_data->wasm_bytes;
  size_t size = module_data->wasm_size;

  // Skip magic + version (8 bytes)
  size_t pos = 8;

  JSValue result = JS_NewArray(ctx);
  uint32_t match_count = 0;

  // Parse sections
  while (pos < size) {
    uint8_t section_id = bytes[pos++];

    // Read section size (LEB128)
    uint32_t section_size = 0;
    uint32_t shift = 0;
    uint8_t byte;
    do {
      byte = bytes[pos++];
      section_size |= (byte & 0x7F) << shift;
      shift += 7;
    } while (byte & 0x80);

    // Check if custom section (id == 0)
    if (section_id == 0) {
      size_t section_start = pos;

      // Read name length (LEB128)
      uint32_t name_len = 0;
      shift = 0;
      do {
        byte = bytes[pos++];
        name_len |= (byte & 0x7F) << shift;
        shift += 7;
      } while (byte & 0x80);

      // Compare name
      if (strncmp((char*)&bytes[pos], section_name, name_len) == 0 &&
          strlen(section_name) == name_len) {
        pos += name_len;

        // Extract payload
        uint32_t payload_size = section_size - (pos - section_start);
        JSValue buffer = JS_NewArrayBufferCopy(ctx, &bytes[pos], payload_size);
        JS_SetPropertyUint32(ctx, result, match_count++, buffer);
      }
    }

    // Skip to next section
    pos += section_size;
  }

  JS_FreeCString(ctx, section_name);
  return result;
}
```

**If Option C (Not supported):**
```c
static JSValue js_webassembly_module_custom_sections(JSContext* ctx, JSValueConst this_val,
                                                      int argc, JSValueConst* argv) {
  // Validate arguments
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Module.customSections requires 2 arguments");
  }

  jsrt_wasm_module_data_t* module_data = JS_GetOpaque(argv[0], js_webassembly_module_class_id);
  if (!module_data) {
    return JS_ThrowTypeError(ctx, "First argument must be a WebAssembly.Module");
  }

  // Return empty array - custom sections not yet supported
  // TODO: Implement custom section parsing when WAMR adds support
  return JS_NewArray(ctx);
}
```

**Phase 3: Integration (15 min)**
3. [ ] Register method on Module constructor in `JSRT_RuntimeSetupStdWebAssembly()`
```c
JS_SetPropertyStr(rt->ctx, module_ctor, "customSections",
                  JS_NewCFunction(rt->ctx, js_webassembly_module_custom_sections, "customSections", 2));
```

**Phase 4: Testing (30 min)**
4. [ ] Create test file: `test/web/webassembly/test_web_wasm_module_custom_sections.js`
5. [ ] Test Case 1: Module with custom section
6. [ ] Test Case 2: Module without custom sections (return empty array)
7. [ ] Test Case 3: Invalid module argument (should throw TypeError)
8. [ ] Test Case 4: Multiple custom sections with same name
9. [ ] Run: `make test N=web/webassembly`

### Test Strategy

**Test WASM Module Generation:**
```javascript
// Use wat2wasm or existing binary with custom sections
// Example: module with "name" custom section (common for debugging)
const wasmBytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // Magic
  0x01, 0x00, 0x00, 0x00,  // Version
  // Custom section: name="test", payload=[0x01, 0x02, 0x03]
  0x00,  // Section ID (custom)
  0x08,  // Section size (8 bytes)
  0x04,  // Name length (4)
  0x74, 0x65, 0x73, 0x74,  // "test"
  0x01, 0x02, 0x03  // Payload
]);

const module = new WebAssembly.Module(wasmBytes);
const sections = WebAssembly.Module.customSections(module, "test");

console.assert(sections.length === 1, "Should find 1 custom section");
console.assert(sections[0].byteLength === 3, "Payload should be 3 bytes");
```

**Test Cases:**
1. Module with matching custom section → return array with 1 ArrayBuffer
2. Module without matching section → return empty array
3. Module with multiple matching sections → return array with all
4. Invalid module argument → TypeError
5. Missing section name argument → TypeError

### Risk Assessment

**Risks:**
- **WAMR API Limitation** (HIGH probability): WAMR may not expose custom sections
  - **Mitigation**: Implement Option B (manual parsing) or Option C (document limitation)
- **Binary Parsing Complexity** (MEDIUM): LEB128 decoding, bounds checking
  - **Mitigation**: Use well-tested parsing code, validate with fuzzing
- **Memory Safety** (LOW): ArrayBuffer copying, bounds checks
  - **Mitigation**: Validate with AddressSanitizer

**Blockers:** None - this task is independent

### Acceptance Criteria

- [ ] `Module.customSections(module, name)` function implemented
- [ ] Returns array of ArrayBuffer for matching sections
- [ ] Returns empty array if no matches
- [ ] Throws TypeError for invalid arguments
- [ ] All test cases pass
- [ ] ASAN validation clean (no leaks)
- [ ] Code formatted (`make format`)
- [ ] Documentation updated in plan if limitations exist

### Deliverables

1. Implementation in `src/std/webassembly.c`
2. Test file `test/web/webassembly/test_web_wasm_module_custom_sections.js`
3. Documentation of WAMR limitations (if applicable)
4. Update `webassembly-plan.md` Task 2.3 status to DONE

---

## Priority 2: Task 3.1 - Instance Import Object Parsing [CRITICAL PATH]

### Overview
**ID**: 3.1
**Priority**: HIGH (Critical for Phase 3 completion)
**Complexity**: COMPLEX
**Risk**: MEDIUM
**Execution Mode**: [S] Sequential (blocks 3.2)
**Dependencies**: Task 1.3 (Module refactor) - ✅ COMPLETED
**Estimated Effort**: 6-8 hours

### Technical Approach

#### Background
When instantiating a WebAssembly module, the second argument is an import object that provides JavaScript implementations for the module's imports. The structure is:

```javascript
const importObject = {
  "env": {
    "memory": new WebAssembly.Memory({initial: 1}),
    "log": function(arg) { console.log(arg); },
    "table": new WebAssembly.Table({initial: 1, element: "anyfunc"})
  },
  "console": {
    "log": function(arg) { console.log(arg); }
  }
};

const instance = new WebAssembly.Instance(module, importObject);
```

The import object must match the module's import requirements:
- Module namespace (e.g., "env", "console")
- Field name (e.g., "log", "memory")
- Type (function, memory, table, global)

#### WAMR Import API Investigation

**WAMR Import Structure:**
```c
// From WAMR documentation
typedef struct {
  char* module_name;
  char* name;
  wasm_import_export_kind_t kind;
  // Additional type-specific fields
} wasm_import_t;

// Functions we'll use:
int32_t wasm_runtime_get_import_count(wasm_module_t module);
void wasm_runtime_get_import_type(wasm_module_t module, uint32_t index, wasm_import_t* import);
```

**WAMR Instantiation with Imports:**
```c
// Standard instantiation (no imports)
wasm_module_inst_t wasm_runtime_instantiate(wasm_module_t module,
                                             uint32_t stack_size,
                                             uint32_t heap_size,
                                             char* error_buf,
                                             uint32_t error_buf_size);

// With native functions (for function imports)
void wasm_runtime_register_natives(const char* module_name,
                                    NativeSymbol* native_symbols,
                                    uint32_t n_native_symbols);
```

#### Implementation Strategy

**Design Pattern:** Build WAMR-compatible import structures from JavaScript import object

**Key Challenges:**
1. Parse nested JavaScript object structure
2. Match import names with module requirements
3. Convert JavaScript values to WAMR import types
4. Handle type mismatches (throw LinkError)
5. Store JS references to prevent GC during module lifetime

**Data Structures:**

```c
// Import resolver context - stores parsed imports
typedef struct {
  JSContext* ctx;
  wasm_module_t module;

  // Parsed imports by type
  uint32_t function_import_count;
  jsrt_wasm_function_import_t* function_imports;  // Array

  uint32_t memory_import_count;
  wasm_memory_inst_t* memory_imports;  // Array

  uint32_t table_import_count;
  wasm_table_inst_t* table_imports;  // Array

  uint32_t global_import_count;
  jsrt_wasm_global_import_t* global_imports;  // Array

  // Keep JS values alive
  JSValue import_object_ref;
} jsrt_wasm_import_resolver_t;

// Function import wrapper
typedef struct {
  char* module_name;
  char* field_name;
  JSValue js_function;  // Keep reference
  uint32_t param_count;
  uint32_t result_count;
  uint8_t param_types[16];  // WASM value types
  uint8_t result_types[4];
} jsrt_wasm_function_import_t;
```

#### Implementation Steps

**Phase 1: Import Resolver Infrastructure (2 hours)**

1. [ ] Define data structures in `webassembly.c`
2. [ ] Implement `jsrt_wasm_import_resolver_create(ctx, module, import_obj)`
3. [ ] Implement `jsrt_wasm_import_resolver_destroy(resolver)`

```c
static jsrt_wasm_import_resolver_t* jsrt_wasm_import_resolver_create(JSContext* ctx,
                                                                       wasm_module_t module,
                                                                       JSValue import_obj) {
  jsrt_wasm_import_resolver_t* resolver = js_malloc(ctx, sizeof(jsrt_wasm_import_resolver_t));
  if (!resolver) {
    return NULL;
  }

  memset(resolver, 0, sizeof(jsrt_wasm_import_resolver_t));
  resolver->ctx = ctx;
  resolver->module = module;
  resolver->import_object_ref = JS_DupValue(ctx, import_obj);

  return resolver;
}

static void jsrt_wasm_import_resolver_destroy(jsrt_wasm_import_resolver_t* resolver) {
  if (!resolver) return;

  JSContext* ctx = resolver->ctx;

  // Free function imports
  for (uint32_t i = 0; i < resolver->function_import_count; i++) {
    js_free(ctx, resolver->function_imports[i].module_name);
    js_free(ctx, resolver->function_imports[i].field_name);
    JS_FreeValue(ctx, resolver->function_imports[i].js_function);
  }
  if (resolver->function_imports) {
    js_free(ctx, resolver->function_imports);
  }

  // Free memory/table/global imports (Phase 4)
  // ...

  JS_FreeValue(ctx, resolver->import_object_ref);
  js_free(ctx, resolver);
}
```

**Phase 2: Import Object Parsing (2 hours)**

4. [ ] Implement `parse_import_object(resolver, import_obj)`
5. [ ] Extract module namespaces (e.g., "env", "console")
6. [ ] Extract field names within each namespace
7. [ ] Validate types match module requirements

```c
static int parse_import_object(jsrt_wasm_import_resolver_t* resolver, JSValue import_obj) {
  JSContext* ctx = resolver->ctx;
  wasm_module_t module = resolver->module;

  // Get all imports from module
  int32_t import_count = wasm_runtime_get_import_count(module);
  if (import_count < 0) {
    return -1;
  }

  JSRT_Debug("Module requires %d imports", import_count);

  // Iterate through required imports
  for (int32_t i = 0; i < import_count; i++) {
    wasm_import_t import_info;
    wasm_runtime_get_import_type(module, i, &import_info);

    JSRT_Debug("Import %d: module='%s', name='%s', kind=%d",
               i, import_info.module_name, import_info.name, import_info.kind);

    // Get module namespace from import object
    JSValue module_ns = JS_GetPropertyStr(ctx, import_obj, import_info.module_name);
    if (JS_IsUndefined(module_ns) || JS_IsNull(module_ns)) {
      JS_FreeValue(ctx, module_ns);
      // Missing required import - will throw LinkError later
      JSRT_Debug("Missing module namespace '%s'", import_info.module_name);
      return -1;
    }

    // Get field from namespace
    JSValue field_value = JS_GetPropertyStr(ctx, module_ns, import_info.name);
    JS_FreeValue(ctx, module_ns);

    if (JS_IsUndefined(field_value) || JS_IsNull(field_value)) {
      JS_FreeValue(ctx, field_value);
      JSRT_Debug("Missing field '%s' in module '%s'", import_info.name, import_info.module_name);
      return -1;
    }

    // Process by kind
    switch (import_info.kind) {
      case WASM_IMPORT_EXPORT_KIND_FUNC:
        if (parse_function_import(resolver, &import_info, field_value) < 0) {
          JS_FreeValue(ctx, field_value);
          return -1;
        }
        break;

      case WASM_IMPORT_EXPORT_KIND_MEMORY:
        // Phase 2 blocked - defer to Phase 3.1B
        JSRT_Debug("Memory imports not yet supported");
        JS_FreeValue(ctx, field_value);
        return -1;

      case WASM_IMPORT_EXPORT_KIND_TABLE:
        // Phase 4 - defer
        JSRT_Debug("Table imports not yet supported");
        JS_FreeValue(ctx, field_value);
        return -1;

      case WASM_IMPORT_EXPORT_KIND_GLOBAL:
        // Phase 4 - defer
        JSRT_Debug("Global imports not yet supported");
        JS_FreeValue(ctx, field_value);
        return -1;

      default:
        JS_FreeValue(ctx, field_value);
        JSRT_Debug("Unknown import kind %d", import_info.kind);
        return -1;
    }

    JS_FreeValue(ctx, field_value);
  }

  return 0;
}
```

**Phase 3: Function Import Parsing (2 hours)**

8. [ ] Implement `parse_function_import(resolver, import_info, js_func)`
9. [ ] Validate `js_func` is callable
10. [ ] Extract function signature from `import_info`
11. [ ] Store function reference

```c
static int parse_function_import(jsrt_wasm_import_resolver_t* resolver,
                                  wasm_import_t* import_info,
                                  JSValue js_func) {
  JSContext* ctx = resolver->ctx;

  // Validate it's a function
  if (!JS_IsFunction(ctx, js_func)) {
    JSRT_Debug("Import '%s.%s' is not a function", import_info->module_name, import_info->name);
    return -1;
  }

  // Allocate function import array if needed
  if (!resolver->function_imports) {
    resolver->function_imports = js_malloc(ctx, sizeof(jsrt_wasm_function_import_t) * 16);
    if (!resolver->function_imports) {
      return -1;
    }
  }

  uint32_t idx = resolver->function_import_count++;
  jsrt_wasm_function_import_t* func_import = &resolver->function_imports[idx];

  // Copy module name
  size_t module_len = strlen(import_info->module_name);
  func_import->module_name = js_malloc(ctx, module_len + 1);
  if (!func_import->module_name) {
    return -1;
  }
  memcpy(func_import->module_name, import_info->module_name, module_len + 1);

  // Copy field name
  size_t field_len = strlen(import_info->name);
  func_import->field_name = js_malloc(ctx, field_len + 1);
  if (!func_import->field_name) {
    js_free(ctx, func_import->module_name);
    return -1;
  }
  memcpy(func_import->field_name, import_info->name, field_len + 1);

  // Keep JS function reference alive
  func_import->js_function = JS_DupValue(ctx, js_func);

  // Extract signature from import_info (WAMR provides this)
  // For now, assume i32 parameters only (Phase 3.1A)
  func_import->param_count = import_info->u.func.param_count;
  func_import->result_count = import_info->u.func.result_count;

  // Copy parameter types
  for (uint32_t i = 0; i < func_import->param_count && i < 16; i++) {
    func_import->param_types[i] = import_info->u.func.param_types[i];
  }

  // Copy result types
  for (uint32_t i = 0; i < func_import->result_count && i < 4; i++) {
    func_import->result_types[i] = import_info->u.func.result_types[i];
  }

  JSRT_Debug("Registered function import '%s.%s' (params=%u, results=%u)",
             func_import->module_name, func_import->field_name,
             func_import->param_count, func_import->result_count);

  return 0;
}
```

**Phase 4: Testing (2 hours)**

11. [ ] Create test file: `test/web/webassembly/test_web_wasm_import_parsing.js`
12. [ ] Test Case 1: Valid function import
13. [ ] Test Case 2: Missing module namespace (should fail)
14. [ ] Test Case 3: Missing field (should fail)
15. [ ] Test Case 4: Non-function value for function import (should fail)
16. [ ] Test Case 5: Multiple imports from same namespace
17. [ ] Run with ASAN: `make jsrt_m && ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/web/webassembly/test_web_wasm_import_parsing.js`

### Test Strategy

**Test WASM Module:**
```wat
(module
  (import "env" "log" (func $log (param i32)))
  (import "env" "add" (func $add (param i32 i32) (result i32)))
  (func $test (export "test")
    i32.const 42
    call $log
  )
)
```

**Test Cases:**
```javascript
// Test 1: Valid import object
const importObject = {
  env: {
    log: function(x) { console.log("LOG:", x); },
    add: function(a, b) { return a + b; }
  }
};

const module = new WebAssembly.Module(wasmBytes);
const instance = new WebAssembly.Instance(module, importObject);
// Should succeed

// Test 2: Missing import
const badImport1 = {
  env: {
    log: function(x) { console.log(x); }
    // Missing 'add'
  }
};
// Should throw LinkError

// Test 3: Wrong type
const badImport2 = {
  env: {
    log: 42,  // Not a function
    add: function(a, b) { return a + b; }
  }
};
// Should throw LinkError
```

### Risk Assessment

**Risks:**
- **WAMR API Complexity** (MEDIUM): Import type structures may be complex
  - **Mitigation**: Study WAMR examples, test with simple imports first
- **Type Validation** (MEDIUM): Ensuring type safety between JS and WASM
  - **Mitigation**: Strict validation, comprehensive error messages
- **Memory Management** (MEDIUM): Keeping JS references alive, cleanup on failure
  - **Mitigation**: Careful reference counting, ASAN validation
- **Signature Extraction** (HIGH): Getting function signatures from WAMR
  - **Mitigation**: Research WAMR wasm_import_t structure, may need fallback

**Dependencies:**
- **Blocks**: Task 3.2 (Function Import Wrapping)
- **Blocked By**: None (Task 1.3 complete)

### Acceptance Criteria

- [ ] `parse_import_object()` function implemented
- [ ] Extracts module namespaces and fields correctly
- [ ] Validates import types match module requirements
- [ ] Throws LinkError for missing/mismatched imports
- [ ] Stores function import references safely
- [ ] All test cases pass
- [ ] ASAN validation clean
- [ ] Code formatted
- [ ] Update `webassembly-plan.md` Task 3.1 status to DONE

### Deliverables

1. Import resolver implementation in `src/std/webassembly.c`
2. Test file `test/web/webassembly/test_web_wasm_import_parsing.js`
3. Update `webassembly-plan.md` with progress

---

## Priority 3: Task 3.2 - Function Import Wrapping [CRITICAL PATH]

### Overview
**ID**: 3.2
**Priority**: HIGH (Critical for Phase 3 completion)
**Complexity**: COMPLEX
**Risk**: HIGH
**Execution Mode**: [S] Sequential (blocks 3.5)
**Dependencies**: Task 3.1 (Import parsing) - 🔴 PENDING
**Estimated Effort**: 8-10 hours

### Technical Approach

#### Background
When a WASM module imports a JavaScript function, we need to create a "bridge" that:
1. WASM calls a native function registered with WAMR
2. Native function calls the JavaScript function
3. Arguments are converted: WASM types → JS values
4. Return value is converted: JS value → WASM type
5. Exceptions are propagated as WASM traps

This is the **inverse** of exported function wrapping (Task 3.4, already complete).

#### WAMR Native Function Registration

**WAMR API:**
```c
typedef struct NativeSymbol {
  const char* symbol;        // Function name (e.g., "log")
  void* func_ptr;            // Pointer to C function
  const char* signature;     // Type signature (e.g., "(i)v" = int param, void return)
  void* attachment;          // Custom data
} NativeSymbol;

void wasm_runtime_register_natives(const char* module_name,
                                    NativeSymbol* native_symbols,
                                    uint32_t n_native_symbols);
```

**Signature Format:**
- `(ii)i` = (param i32 i32) (result i32)
- `(i)v` = (param i32) (no result)
- `()i` = (no params) (result i32)
- Types: `i` (i32), `I` (i64), `f` (f32), `F` (f64), `*` (pointer), `v` (void)

#### Implementation Strategy

**Design Pattern:** Dynamic native function generation with closure over JS function reference

**Key Challenges:**
1. Generate unique C function for each JS import
2. Store JS function reference accessible from C function
3. Convert WASM arguments to JS values
4. Call JS function with correct `this` binding
5. Convert JS return value to WASM type
6. Propagate JS exceptions as WASM traps
7. Memory management during calls

**Data Structure Enhancements:**

```c
// Enhanced function import (from Task 3.1)
typedef struct {
  char* module_name;
  char* field_name;
  JSValue js_function;
  uint32_t param_count;
  uint32_t result_count;
  uint8_t param_types[16];
  uint8_t result_types[4];

  // New for Task 3.2:
  void* native_func_ptr;     // Generated C function pointer
  char* signature;           // WAMR signature string
  JSContext* ctx;            // Context for calls
} jsrt_wasm_function_import_t;

// Global registry to map native functions → JS functions
// (WAMR native functions can't have custom data directly)
typedef struct {
  jsrt_wasm_function_import_t* func_import;
  uint32_t index;
} jsrt_wasm_native_func_registry_entry_t;

static jsrt_wasm_native_func_registry_entry_t native_func_registry[256];
static uint32_t native_func_registry_count = 0;
```

#### Implementation Steps

**Phase 1: Signature Generation (1 hour)**

1. [ ] Implement `generate_wamr_signature(func_import)` to create WAMR signature string

```c
static char* generate_wamr_signature(JSContext* ctx, jsrt_wasm_function_import_t* func_import) {
  // Calculate signature length: "(" + params + ")" + results + null
  size_t sig_len = 2 + func_import->param_count + func_import->result_count + 1;
  char* signature = js_malloc(ctx, sig_len);
  if (!signature) {
    return NULL;
  }

  size_t pos = 0;
  signature[pos++] = '(';

  // Add parameter types
  for (uint32_t i = 0; i < func_import->param_count; i++) {
    switch (func_import->param_types[i]) {
      case WASM_TYPE_I32: signature[pos++] = 'i'; break;
      case WASM_TYPE_I64: signature[pos++] = 'I'; break;
      case WASM_TYPE_F32: signature[pos++] = 'f'; break;
      case WASM_TYPE_F64: signature[pos++] = 'F'; break;
      default:
        js_free(ctx, signature);
        return NULL;
    }
  }

  signature[pos++] = ')';

  // Add result types
  for (uint32_t i = 0; i < func_import->result_count; i++) {
    switch (func_import->result_types[i]) {
      case WASM_TYPE_I32: signature[pos++] = 'i'; break;
      case WASM_TYPE_I64: signature[pos++] = 'I'; break;
      case WASM_TYPE_F32: signature[pos++] = 'f'; break;
      case WASM_TYPE_F64: signature[pos++] = 'F'; break;
      default:
        js_free(ctx, signature);
        return NULL;
    }
  }

  signature[pos] = '\0';

  JSRT_Debug("Generated signature '%s' for function '%s.%s'",
             signature, func_import->module_name, func_import->field_name);

  return signature;
}
```

**Phase 2: Native Function Template (3 hours)**

2. [ ] Implement generic native function template with JS call logic

**Problem:** WAMR native functions must have fixed signatures at compile time. We can't generate C functions dynamically.

**Solution:** Use a dispatch approach with a generic handler:

```c
// Generic native function handler
// All imported functions call this, passing their registry index via exec_env attachment
static void jsrt_wasm_native_import_handler(wasm_exec_env_t exec_env, uint32_t* args) {
  // Get module instance
  wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);

  // Get attachment (contains registry index)
  void* attachment = wasm_runtime_get_function_attachment(exec_env);
  uint32_t registry_idx = (uint32_t)(uintptr_t)attachment;

  if (registry_idx >= native_func_registry_count) {
    wasm_runtime_set_exception(module_inst, "Invalid function registry index");
    return;
  }

  jsrt_wasm_function_import_t* func_import = native_func_registry[registry_idx].func_import;
  JSContext* ctx = func_import->ctx;

  JSRT_Debug("Calling JS import '%s.%s' with %u args",
             func_import->module_name, func_import->field_name, func_import->param_count);

  // Convert WASM arguments to JS values
  JSValue* js_args = js_malloc(ctx, sizeof(JSValue) * func_import->param_count);
  if (!js_args) {
    wasm_runtime_set_exception(module_inst, "Out of memory");
    return;
  }

  for (uint32_t i = 0; i < func_import->param_count; i++) {
    switch (func_import->param_types[i]) {
      case WASM_TYPE_I32:
        js_args[i] = JS_NewInt32(ctx, (int32_t)args[i]);
        break;
      case WASM_TYPE_I64:
        // i64 takes 2 slots on 32-bit, 1 slot on 64-bit
        #if UINTPTR_MAX == 0xFFFFFFFF
          int64_t val64 = (int64_t)args[i] | ((int64_t)args[i+1] << 32);
          js_args[i] = JS_NewBigInt64(ctx, val64);
          i++;  // Skip next slot
        #else
          js_args[i] = JS_NewBigInt64(ctx, (int64_t)args[i]);
        #endif
        break;
      case WASM_TYPE_F32:
        js_args[i] = JS_NewFloat64(ctx, *(float*)&args[i]);
        break;
      case WASM_TYPE_F64: {
        // f64 takes 2 slots
        uint64_t bits = (uint64_t)args[i] | ((uint64_t)args[i+1] << 32);
        js_args[i] = JS_NewFloat64(ctx, *(double*)&bits);
        i++;  // Skip next slot
        break;
      }
      default:
        js_free(ctx, js_args);
        wasm_runtime_set_exception(module_inst, "Unsupported parameter type");
        return;
    }
  }

  // Call JavaScript function
  JSValue result = JS_Call(ctx, func_import->js_function, JS_UNDEFINED,
                           func_import->param_count, js_args);

  // Free JS arguments
  for (uint32_t i = 0; i < func_import->param_count; i++) {
    JS_FreeValue(ctx, js_args[i]);
  }
  js_free(ctx, js_args);

  // Check for JS exception
  if (JS_IsException(result)) {
    JSValue exception = JS_GetException(ctx);
    const char* err_msg = JS_ToCString(ctx, exception);
    wasm_runtime_set_exception(module_inst, err_msg ? err_msg : "JavaScript exception");
    JS_FreeCString(ctx, err_msg);
    JS_FreeValue(ctx, exception);
    return;
  }

  // Convert return value to WASM type
  if (func_import->result_count > 0) {
    switch (func_import->result_types[0]) {
      case WASM_TYPE_I32: {
        int32_t ret_val;
        if (JS_ToInt32(ctx, &ret_val, result)) {
          JS_FreeValue(ctx, result);
          wasm_runtime_set_exception(module_inst, "Return value conversion failed");
          return;
        }
        args[0] = (uint32_t)ret_val;
        break;
      }
      case WASM_TYPE_I64: {
        int64_t ret_val;
        if (JS_ToBigInt64(ctx, &ret_val, result)) {
          JS_FreeValue(ctx, result);
          wasm_runtime_set_exception(module_inst, "Return value conversion failed");
          return;
        }
        #if UINTPTR_MAX == 0xFFFFFFFF
          args[0] = (uint32_t)(ret_val & 0xFFFFFFFF);
          args[1] = (uint32_t)(ret_val >> 32);
        #else
          args[0] = (uint32_t)ret_val;
        #endif
        break;
      }
      case WASM_TYPE_F32: {
        double ret_val;
        if (JS_ToFloat64(ctx, &ret_val, result)) {
          JS_FreeValue(ctx, result);
          wasm_runtime_set_exception(module_inst, "Return value conversion failed");
          return;
        }
        float f32 = (float)ret_val;
        args[0] = *(uint32_t*)&f32;
        break;
      }
      case WASM_TYPE_F64: {
        double ret_val;
        if (JS_ToFloat64(ctx, &ret_val, result)) {
          JS_FreeValue(ctx, result);
          wasm_runtime_set_exception(module_inst, "Return value conversion failed");
          return;
        }
        uint64_t bits = *(uint64_t*)&ret_val;
        args[0] = (uint32_t)(bits & 0xFFFFFFFF);
        args[1] = (uint32_t)(bits >> 32);
        break;
      }
    }
  }

  JS_FreeValue(ctx, result);
  JSRT_Debug("JS import call completed successfully");
}
```

**Problem with above approach:** WAMR native functions need unique C function pointers, and function attachment may not work as expected.

**Better Solution:** Generate wrapper functions with function pointers stored in func_import

**Alternative Approach:**
Use a small set of pre-defined wrapper functions (trampoline pattern) for different arities:

```c
// Pre-defined trampolines for common signatures
static void native_import_void_void(wasm_exec_env_t exec_env) {
  // Implementation with 0 params, 0 results
}

static void native_import_i32_void(wasm_exec_env_t exec_env, uint32_t arg0) {
  // Implementation with 1 i32 param, 0 results
}

static uint32_t native_import_void_i32(wasm_exec_env_t exec_env) {
  // Implementation with 0 params, 1 i32 result
}

static uint32_t native_import_i32_i32(wasm_exec_env_t exec_env, uint32_t arg0) {
  // Implementation with 1 i32 param, 1 i32 result
}

static uint32_t native_import_i32i32_i32(wasm_exec_env_t exec_env, uint32_t arg0, uint32_t arg1) {
  // Implementation with 2 i32 params, 1 i32 result
}

// ... more trampolines for common signatures
```

Each trampoline calls a shared helper with the function import reference.

**Phase 3: Registration with WAMR (2 hours)**

3. [ ] Implement `register_function_imports(resolver, instance)` to register all imports

```c
static int register_function_imports(jsrt_wasm_import_resolver_t* resolver,
                                      wasm_module_inst_t instance) {
  JSContext* ctx = resolver->ctx;

  // Build NativeSymbol array by module namespace
  // WAMR requires registering all functions for a module at once

  // Group imports by module namespace
  // For now, assume "env" namespace (most common)
  const char* current_module = NULL;
  NativeSymbol* symbols = NULL;
  uint32_t symbol_count = 0;

  for (uint32_t i = 0; i < resolver->function_import_count; i++) {
    jsrt_wasm_function_import_t* func_import = &resolver->function_imports[i];

    // Check if module changed
    if (!current_module || strcmp(current_module, func_import->module_name) != 0) {
      // Register previous module if exists
      if (symbols && symbol_count > 0) {
        wasm_runtime_register_natives(current_module, symbols, symbol_count);
        js_free(ctx, symbols);
      }

      // Start new module
      current_module = func_import->module_name;
      symbols = NULL;
      symbol_count = 0;
    }

    // Allocate/expand symbols array
    symbols = js_realloc(ctx, symbols, sizeof(NativeSymbol) * (symbol_count + 1));
    if (!symbols) {
      return -1;
    }

    // Generate signature
    func_import->signature = generate_wamr_signature(ctx, func_import);
    if (!func_import->signature) {
      return -1;
    }

    // Select trampoline function based on signature
    void* func_ptr = select_trampoline(func_import->signature);
    if (!func_ptr) {
      JSRT_Debug("No trampoline for signature '%s'", func_import->signature);
      return -1;
    }

    // Add to registry for lookup during calls
    uint32_t registry_idx = native_func_registry_count++;
    native_func_registry[registry_idx].func_import = func_import;
    native_func_registry[registry_idx].index = registry_idx;
    func_import->ctx = ctx;

    // Fill NativeSymbol
    symbols[symbol_count].symbol = func_import->field_name;
    symbols[symbol_count].func_ptr = func_ptr;
    symbols[symbol_count].signature = func_import->signature;
    symbols[symbol_count].attachment = (void*)(uintptr_t)registry_idx;
    symbol_count++;
  }

  // Register last module
  if (symbols && symbol_count > 0) {
    wasm_runtime_register_natives(current_module, symbols, symbol_count);
    js_free(ctx, symbols);
  }

  return 0;
}
```

**Phase 4: Integration (1 hour)**

4. [ ] Update `js_webassembly_instance_constructor()` to register imports before instantiation
5. [ ] Handle LinkError if import registration fails

**Phase 5: Testing (3 hours)**

6. [ ] Create test file: `test/web/webassembly/test_web_wasm_function_imports.js`
7. [ ] Test Case 1: Simple function import (i32 → void)
8. [ ] Test Case 2: Function import with return value (i32, i32 → i32)
9. [ ] Test Case 3: JS exception propagation
10. [ ] Test Case 4: Type conversion edge cases
11. [ ] Run with ASAN validation

### Test Strategy

**Test WASM Module:**
```wat
(module
  (import "env" "log" (func $log (param i32)))
  (import "env" "add" (func $add (param i32 i32) (result i32)))
  (func $test (export "test") (result i32)
    i32.const 10
    call $log

    i32.const 20
    i32.const 30
    call $add
  )
)
```

**Test Cases:**
```javascript
let logCalled = false;
let logValue = 0;

const importObject = {
  env: {
    log: function(x) {
      logCalled = true;
      logValue = x;
      console.log("JS log called with:", x);
    },
    add: function(a, b) {
      console.log("JS add called:", a, b);
      return a + b;
    }
  }
};

const module = new WebAssembly.Module(wasmBytes);
const instance = new WebAssembly.Instance(module, importObject);

const result = instance.exports.test();
console.assert(logCalled === true, "log should be called");
console.assert(logValue === 10, "log should receive 10");
console.assert(result === 50, "test should return 50 (20 + 30)");
```

### Risk Assessment

**Risks:**
- **WAMR Native API Complexity** (HIGH): Function registration, signatures, trampolines
  - **Mitigation**: Start with Phase 3.2A (i32 only), expand to full types in 3.2B
- **Type Conversion Correctness** (HIGH): i32/i64/f32/f64 ABI differences
  - **Mitigation**: Extensive testing with all type combinations
- **Memory Management** (HIGH): JS references, argument arrays, cleanup
  - **Mitigation**: ASAN validation, careful cleanup paths
- **Exception Propagation** (MEDIUM): JS exceptions → WASM traps
  - **Mitigation**: Test exception cases thoroughly
- **Function Pointer Generation** (HIGH): May not be possible without code generation
  - **Mitigation**: Use trampoline pattern with limited set of signatures

**Dependencies:**
- **Blocks**: Task 3.5 (Update Instance Constructor)
- **Blocked By**: Task 3.1 (Import Parsing)

### Phased Implementation

Given high complexity, split into two sub-phases:

**Phase 3.2A: i32-only Support (6 hours)**
- Only support i32 parameters and results
- Simpler type conversion
- Fewer trampolines needed

**Phase 3.2B: Full Type Support (8 hours)**
- Add i64, f32, f64 support
- BigInt conversion
- More trampolines
- **DEFER to separate task**

### Acceptance Criteria

**Phase 3.2A (i32 only):**
- [ ] Function imports with i32 params/results work
- [ ] JS functions called from WASM
- [ ] Arguments converted correctly
- [ ] Return values converted correctly
- [ ] JS exceptions propagate as WASM traps
- [ ] All test cases pass (i32 only)
- [ ] ASAN validation clean
- [ ] Code formatted

### Deliverables

1. Function import wrapping implementation in `src/std/webassembly.c`
2. Test file `test/web/webassembly/test_web_wasm_function_imports.js`
3. Update `webassembly-plan.md` Task 3.2 status to DONE (Phase 3.2A)
4. Create Task 3.2B for full type support (deferred)

---

## Priority 4: Task 3.5 - Update Instance Constructor [INTEGRATION]

### Overview
**ID**: 3.5
**Priority**: HIGH (Completes Phase 3 import support)
**Complexity**: MEDIUM
**Risk**: LOW
**Execution Mode**: [S] Sequential (completes Phase 3)
**Dependencies**: Task 3.2 (Function wrapping) - 🔴 PENDING
**Estimated Effort**: 2-3 hours

### Technical Approach

#### Background
The `Instance` constructor currently ignores the `importObject` parameter (marked with TODO). After Tasks 3.1 and 3.2, we can now use the import resolver to handle imports properly.

#### Current Implementation
```c
static JSValue js_webassembly_instance_constructor(JSContext* ctx, JSValueConst new_target, int argc,
                                                   JSValueConst* argv) {
  // Get module data
  jsrt_wasm_module_data_t* module_data = JS_GetOpaque(argv[0], js_webassembly_module_class_id);

  // TODO: Process import object in Phase 3
  // For now, ignore importObject (argc >= 2 ? argv[1] : undefined)

  // Instantiate module
  wasm_module_inst_t instance =
      wasm_runtime_instantiate(module_data->module, stack_size, heap_size, error_buf, sizeof(error_buf));

  // ... rest of constructor
}
```

#### Implementation Strategy

**Simple Integration:** Replace TODO with import resolver calls

#### Implementation Steps

**Phase 1: Import Object Processing (1 hour)**

1. [ ] Parse `importObject` parameter (argv[1])
2. [ ] Create import resolver
3. [ ] Parse imports
4. [ ] Register native functions

```c
static JSValue js_webassembly_instance_constructor(JSContext* ctx, JSValueConst new_target, int argc,
                                                   JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "WebAssembly.Instance constructor requires 1 argument");
  }

  // Get module data using proper class ID
  jsrt_wasm_module_data_t* module_data = JS_GetOpaque(argv[0], js_webassembly_module_class_id);
  if (!module_data) {
    return JS_ThrowTypeError(ctx, "First argument must be a WebAssembly.Module");
  }

  // Process import object if provided
  jsrt_wasm_import_resolver_t* import_resolver = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
    // Create import resolver
    import_resolver = jsrt_wasm_import_resolver_create(ctx, module_data->module, argv[1]);
    if (!import_resolver) {
      return JS_ThrowOutOfMemory(ctx);
    }

    // Parse import object
    if (parse_import_object(import_resolver, argv[1]) < 0) {
      jsrt_wasm_import_resolver_destroy(import_resolver);
      return throw_webassembly_link_error(ctx, "Failed to parse import object");
    }

    // Register function imports with WAMR (must happen before instantiation)
    if (register_function_imports(import_resolver, NULL) < 0) {
      jsrt_wasm_import_resolver_destroy(import_resolver);
      return throw_webassembly_link_error(ctx, "Failed to register function imports");
    }
  }

  // Instantiate module
  char error_buf[256];
  uint32_t stack_size = 16384;  // 16KB stack
  uint32_t heap_size = 65536;   // 64KB heap

  wasm_module_inst_t instance =
      wasm_runtime_instantiate(module_data->module, stack_size, heap_size, error_buf, sizeof(error_buf));

  if (!instance) {
    if (import_resolver) {
      jsrt_wasm_import_resolver_destroy(import_resolver);
    }
    return throw_webassembly_link_error(ctx, error_buf);
  }

  // Create JS Instance object with proper class
  JSValue instance_obj = JS_NewObjectClass(ctx, js_webassembly_instance_class_id);
  if (JS_IsException(instance_obj)) {
    wasm_runtime_deinstantiate(instance);
    if (import_resolver) {
      jsrt_wasm_import_resolver_destroy(import_resolver);
    }
    return instance_obj;
  }

  // Store instance data using js_malloc
  jsrt_wasm_instance_data_t* instance_data = js_malloc(ctx, sizeof(jsrt_wasm_instance_data_t));
  if (!instance_data) {
    wasm_runtime_deinstantiate(instance);
    JS_FreeValue(ctx, instance_obj);
    if (import_resolver) {
      jsrt_wasm_import_resolver_destroy(import_resolver);
    }
    return JS_ThrowOutOfMemory(ctx);
  }

  instance_data->instance = instance;
  instance_data->module_data = module_data;
  instance_data->exports_object = JS_UNDEFINED;
  instance_data->import_resolver = import_resolver;  // Keep alive for instance lifetime

  JS_SetOpaque(instance_obj, instance_data);

  // Create exports object and set it as a property
  JSValue exports = js_webassembly_instance_exports_getter(ctx, instance_obj, 0, NULL);
  if (JS_IsException(exports)) {
    JS_FreeValue(ctx, instance_obj);
    return exports;
  }

  // Set exports as a readonly property on the instance
  JS_DefinePropertyValueStr(ctx, instance_obj, "exports", exports, JS_PROP_ENUMERABLE);

  JSRT_Debug("WebAssembly.Instance created successfully with imports");
  return instance_obj;
}
```

**Phase 2: Instance Data Structure Update (30 min)**

2. [ ] Update `jsrt_wasm_instance_data_t` to store import resolver

```c
typedef struct {
  wasm_module_inst_t instance;
  jsrt_wasm_module_data_t* module_data;
  JSValue exports_object;  // Cached exports object
  jsrt_wasm_import_resolver_t* import_resolver;  // Keep imports alive (NEW)
} jsrt_wasm_instance_data_t;
```

**Phase 3: Finalizer Update (15 min)**

3. [ ] Update instance finalizer to free import resolver

```c
static void js_webassembly_instance_finalizer(JSRuntime* rt, JSValue val) {
  jsrt_wasm_instance_data_t* data = JS_GetOpaque(val, js_webassembly_instance_class_id);
  if (data) {
    // Free cached exports object if it exists
    if (!JS_IsUndefined(data->exports_object)) {
      JS_FreeValueRT(rt, data->exports_object);
    }

    // Free import resolver (NEW)
    if (data->import_resolver) {
      // Need RT-safe version of jsrt_wasm_import_resolver_destroy
      jsrt_wasm_import_resolver_destroy_rt(rt, data->import_resolver);
    }

    // Cleanup instance
    if (data->instance) {
      wasm_runtime_deinstantiate(data->instance);
    }
    js_free_rt(rt, data);
  }
}
```

**Phase 4: Testing (1 hour)**

4. [ ] Update existing tests to use import objects
5. [ ] Create comprehensive integration test
6. [ ] Test scenarios:
   - No imports (backward compatibility)
   - Valid imports
   - Missing imports (LinkError)
   - Type mismatches (LinkError)

### Test Strategy

**Integration Test:**
```javascript
// Test 1: No imports (backward compat)
const module1 = new WebAssembly.Module(simpleWasmBytes);
const instance1 = new WebAssembly.Instance(module1);
// Should succeed

// Test 2: Valid imports
const module2 = new WebAssembly.Module(wasmWithImportsBytes);
const importObject = {
  env: {
    log: function(x) { console.log(x); }
  }
};
const instance2 = new WebAssembly.Instance(module2, importObject);
instance2.exports.test();
// Should call log

// Test 3: Missing import
try {
  const instance3 = new WebAssembly.Instance(module2, {}); // Empty import object
  console.assert(false, "Should have thrown LinkError");
} catch (e) {
  console.assert(e instanceof WebAssembly.LinkError, "Should be LinkError");
}

// Test 4: Undefined importObject (same as empty)
const instance4 = new WebAssembly.Instance(module1);  // No imports required
// Should succeed

// Test 5: Null importObject
const instance5 = new WebAssembly.Instance(module1, null);
// Should succeed
```

### Risk Assessment

**Risks:**
- **Integration Issues** (LOW): Should be straightforward if Tasks 3.1 and 3.2 complete
  - **Mitigation**: Thorough testing with various import scenarios
- **Memory Leaks** (MEDIUM): Import resolver lifetime management
  - **Mitigation**: ASAN validation, careful finalizer implementation
- **Backward Compatibility** (LOW): Must not break existing tests
  - **Mitigation**: Test with and without imports

**Dependencies:**
- **Blocks**: None (completes Phase 3 import support)
- **Blocked By**: Task 3.2 (Function Import Wrapping)

### Acceptance Criteria

- [ ] Instance constructor accepts import object
- [ ] Imports are processed and registered
- [ ] LinkError thrown for missing/invalid imports
- [ ] Backward compatible (no imports still works)
- [ ] All existing tests still pass
- [ ] New integration tests pass
- [ ] ASAN validation clean
- [ ] Code formatted
- [ ] Update `webassembly-plan.md` Task 3.5 status to DONE

### Deliverables

1. Updated Instance constructor in `src/std/webassembly.c`
2. Updated instance data structure and finalizer
3. Integration test file `test/web/webassembly/test_web_wasm_instance_integration.js`
4. Update `webassembly-plan.md` with progress

---

## Execution Roadmap

### Recommended Order

**Week 1:**
1. **Day 1-2**: Task 2.3 (Module.customSections) - 2-3 hours
2. **Day 3-4**: Task 3.1 (Import Parsing) - 6-8 hours
3. **Day 5-7**: Task 3.2A (Function Import Wrapping - i32 only) - 6 hours

**Week 2:**
4. **Day 1**: Task 3.5 (Update Instance Constructor) - 2-3 hours
5. **Day 2**: Integration testing and ASAN validation - 4 hours
6. **Day 3**: Task 3.2B (Full type support) - 8 hours (OPTIONAL, can defer)

### Parallel Opportunities

- Task 2.3 can run in parallel with documentation/testing work
- After Task 3.1, documentation for import object format can be written in parallel with 3.2

### Blockers & Dependencies

```
2.3 (Module.customSections)  [READY]
  ↓ (no dependency)

3.1 (Import Parsing)  [READY after 1.3 ✅]
  ↓

3.2A (Function Wrapping - i32)  [BLOCKED by 3.1]
  ↓

3.5 (Update Constructor)  [BLOCKED by 3.2A]
  ↓

3.2B (Full Types)  [OPTIONAL]
```

### Success Metrics

**Phase 2 Completion:**
- [ ] Task 2.3 DONE
- [ ] Module.customSections() implemented
- [ ] Phase 2: 52% → 55% complete

**Phase 3 Completion:**
- [ ] Tasks 3.1, 3.2A, 3.5 DONE
- [ ] Instance import support working (i32 functions)
- [ ] Phase 3: 34% → 60% complete
- [ ] Overall: 27% → 35% complete

**Quality Gates:**
- [ ] All tests pass: `make test N=web/webassembly`
- [ ] ASAN clean: No leaks, no memory errors
- [ ] Code formatted: `make format`
- [ ] Documentation updated in plan

---

## Next Steps After These Tasks

After completing these 4 tasks, the next priorities would be:

1. **Task 3.2B**: Full type support (i64, f32, f64, BigInt) for function imports
2. **Task 4.x**: Table and Global implementation (Phase 4)
3. **Task 7.x**: WPT integration and testing (Phase 7)
4. **Memory API**: Revisit blocked Tasks 2.4-2.6 after understanding WAMR better

---

## Appendix: WAMR API Reference

### Key WAMR Functions

**Module Loading:**
- `wasm_runtime_load(bytes, size, error_buf, buf_size)` → `wasm_module_t`
- `wasm_runtime_unload(module)` → void

**Module Introspection:**
- `wasm_runtime_get_export_count(module)` → int32_t
- `wasm_runtime_get_export_type(module, index, export_info*)` → void
- `wasm_runtime_get_import_count(module)` → int32_t
- `wasm_runtime_get_import_type(module, index, import_info*)` → void

**Module Instantiation:**
- `wasm_runtime_instantiate(module, stack, heap, error_buf, buf_size)` → `wasm_module_inst_t`
- `wasm_runtime_deinstantiate(instance)` → void

**Function Calls:**
- `wasm_runtime_lookup_function(instance, name)` → `wasm_function_inst_t`
- `wasm_runtime_create_exec_env(instance, stack_size)` → `wasm_exec_env_t`
- `wasm_runtime_call_wasm(exec_env, func, argc, argv)` → bool
- `wasm_runtime_destroy_exec_env(exec_env)` → void
- `wasm_runtime_get_exception(instance)` → const char*

**Native Functions:**
- `wasm_runtime_register_natives(module_name, symbols, count)` → bool

**Function Signatures:**
- `wasm_func_get_param_count(func, instance)` → uint32_t
- `wasm_func_get_result_count(func, instance)` → uint32_t

---

## Document Maintenance

**Last Updated**: 2025-10-17
**Next Review**: After Task 2.3 completion
**Owner**: WebAssembly Implementation Team

**Change Log:**
- 2025-10-17: Initial detailed breakdown created
- Status: Ready for execution
