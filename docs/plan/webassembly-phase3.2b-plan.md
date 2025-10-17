# WebAssembly Phase 3.2B: Full Type Support for Function Imports

**Created**: 2025-10-17
**Status**: READY FOR EXECUTION
**Phase**: 3.2B - Function Import Wrapping (Full Type Support)
**Dependencies**: Phase 3.2A (i32-only support) - ✅ COMPLETE

---

## Executive Summary

This document provides a detailed task breakdown for implementing full type support in WebAssembly function import wrapping. Phase 3.2A successfully implemented i32-only support using a fixed `(ii)i` signature. Phase 3.2B extends this to support all WebAssembly value types: f32, f64, i64, BigInt, as well as dynamic signature reading and multi-value returns.

**Current State:**
- Phase 3.2A: ✅ COMPLETE - i32 parameter and return value conversion working
- Fixed signature `(ii)i` used for all function imports
- Basic trampoline pattern established
- Memory safety validated with ASAN

**Phase 3.2B Goals:**
1. Support f32/f64 floating-point types
2. Support i64 with JavaScript BigInt conversion
3. Read actual function signatures from WASM module
4. Handle multi-value returns (if WAMR supports)
5. Support multiple module namespaces beyond "env"
6. Maintain backward compatibility with Phase 3.2A

---

## Current Implementation Analysis

### Phase 3.2A Architecture

**Files**: `src/std/webassembly.c:622-785`

**Key Components:**

1. **Import Trampoline** (`jsrt_wasm_import_func_trampoline`):
   - Single generic handler for all function imports
   - Fixed `(ii)i` signature assumption
   - i32-only argument/return conversion
   - Uses `wasm_runtime_get_function_attachment()` to access function data

2. **Import Registration** (`jsrt_wasm_register_function_imports`):
   - Creates `NativeSymbol` array for WAMR
   - Registers with `wasm_runtime_register_natives()`
   - Single module namespace ("env") assumption
   - Stores function_import as attachment

3. **Data Structures**:
```c
typedef struct {
  char* module_name;
  char* field_name;
  JSValue js_function;
  JSContext* ctx;
} jsrt_wasm_function_import_t;

typedef struct {
  JSContext* ctx;
  wasm_module_t module;
  uint32_t function_import_count;
  jsrt_wasm_function_import_t* function_imports;
  NativeSymbol* native_symbols;
  uint32_t native_symbol_count;
  char* module_name_for_natives;
  JSValue import_object_ref;
} jsrt_wasm_import_resolver_t;
```

### Limitations to Address

1. **Fixed Signature**: `"(ii)i"` hardcoded - doesn't match actual function signatures
2. **Type Support**: Only i32 supported - no f32, f64, i64
3. **Single Namespace**: Only "env" module supported
4. **No Signature Reading**: Doesn't query actual function types from WAMR
5. **Multi-value**: No support for functions returning multiple values

---

## Technical Approach

### 1. Reading Function Signatures from WAMR

**WAMR APIs Available:**
```c
// Runtime API (wasm_export.h)
uint32_t wasm_func_get_param_count(wasm_function_inst_t func, wasm_module_inst_t inst);
uint32_t wasm_func_get_result_count(wasm_function_inst_t func, wasm_module_inst_t inst);
void wasm_func_get_param_types(wasm_function_inst_t func, wasm_module_inst_t inst, wasm_valkind_t* types);
void wasm_func_get_result_types(wasm_function_inst_t func, wasm_module_inst_t inst, wasm_valkind_t* types);

// Import introspection (wasm_export.h)
void wasm_runtime_get_import_type(wasm_module_t module, uint32_t index, wasm_import_t* import);

// wasm_import_t structure (for function imports)
typedef struct {
  char* module_name;
  char* name;
  wasm_import_export_kind_t kind;  // WASM_IMPORT_EXPORT_KIND_FUNC
  union {
    struct {
      uint32_t param_count;
      uint32_t result_count;
      wasm_valkind_t* param_types;   // Array of types
      wasm_valkind_t* result_types;  // Array of types
    } func;
    // ... other kinds
  } u;
} wasm_import_t;

// Value kinds (wasm_export.h)
typedef enum {
  WASM_I32 = 0,
  WASM_I64 = 1,
  WASM_F32 = 2,
  WASM_F64 = 3,
  WASM_V128 = 4,      // SIMD (not supported in jsrt yet)
  WASM_FUNCREF = 5,   // Reference types
  WASM_EXTERNREF = 6
} wasm_valkind_t;
```

**Strategy**: Extract signature information during import parsing (Task 3.1) and store in `jsrt_wasm_function_import_t`.

### 2. Signature String Generation

**WAMR Native Function Signature Format:**
- Format: `"(params)results"`
- Types: `i` (i32), `I` (i64), `f` (f32), `F` (f64), `v` (void/no return)
- Examples:
  - `"(ii)i"` = `(i32, i32) → i32`
  - `"(fF)F"` = `(f32, f64) → f64`
  - `"(I)I"` = `(i64) → i64`
  - `"()v"` = `() → void`
  - `"(iI)v"` = `(i32, i64) → void`

**Implementation**: Generate signature string from stored types in function_import.

### 3. Type Conversion Strategy

**JavaScript ↔ WebAssembly Type Mapping:**

| WASM Type | JS Type | Conversion |
|-----------|---------|------------|
| i32 | Number | `JS_ToInt32()` / `JS_NewInt32()` |
| i64 | BigInt | `JS_ToBigInt64()` / `JS_NewBigInt64()` |
| f32 | Number | `JS_ToFloat64()` / `JS_NewFloat64()` (with f32 cast) |
| f64 | Number | `JS_ToFloat64()` / `JS_NewFloat64()` |
| funcref | Function | Not yet supported (Phase 4+) |
| externref | Any | Not yet supported (Phase 4+) |

**WAMR Argument Array Layout:**
- Each argument is a `uint32_t` slot
- **i32**: 1 slot
- **f32**: 1 slot (reinterpret bits)
- **i64**: 2 slots (low 32 bits, high 32 bits) on 32-bit platforms, 1 slot on 64-bit
- **f64**: 2 slots (low 32 bits, high 32 bits)
- Return values use same layout in args[0], args[1], ...

### 4. Multi-Value Returns

**WebAssembly Spec**: Functions can return multiple values (e.g., `(i32, i32)`).
**JavaScript Behavior**: Return as array `[value1, value2]` or throw if unsupported.

**WAMR Support**: Check if WAMR supports multi-value returns through result_count.

**Strategy**:
- If `result_count == 0`: Return `undefined`
- If `result_count == 1`: Return single value
- If `result_count > 1`: Return array (if WAMR supports, otherwise error)

### 5. Multiple Module Namespaces

**Current Limitation**: Only "env" module supported.

**Strategy**:
- Group function imports by module_name
- Register each module separately with `wasm_runtime_register_natives()`
- Store multiple `NativeSymbol*` arrays in resolver

---

## Task Breakdown

### Task 3.2B.1: Enhance Data Structures for Signature Storage

**Priority**: HIGH
**Complexity**: SIMPLE
**Estimated Time**: 1 hour

**Subtasks:**

1. [ ] Update `jsrt_wasm_function_import_t` to store signature information
2. [ ] Add dynamic signature string field
3. [ ] Add param_types and result_types arrays

**Implementation:**

```c
// Enhanced function import structure
typedef struct {
  char* module_name;
  char* field_name;
  JSValue js_function;
  JSContext* ctx;

  // NEW: Signature information
  char* signature;              // WAMR signature string (e.g., "(if)F")
  uint32_t param_count;
  uint32_t result_count;
  wasm_valkind_t param_types[16];   // Max 16 params
  wasm_valkind_t result_types[4];   // Max 4 results (multi-value)
} jsrt_wasm_function_import_t;
```

**Validation:**
- [ ] Build succeeds
- [ ] Existing tests still pass

---

### Task 3.2B.2: Extract Signature from wasm_import_t

**Priority**: HIGH
**Complexity**: MEDIUM
**Estimated Time**: 2 hours

**Background**: In Phase 3.2A, `parse_function_import()` doesn't extract signature details. We need to read param/result types from `wasm_import_t`.

**Subtasks:**

1. [ ] Update `parse_function_import()` to extract signature
2. [ ] Read `import_info->u.func.param_count` and `result_count`
3. [ ] Copy param/result type arrays from `import_info->u.func.param_types`
4. [ ] Validate type counts don't exceed limits

**Implementation:**

```c
static int parse_function_import(jsrt_wasm_import_resolver_t* resolver,
                                  wasm_import_t* import_info,
                                  JSValue js_func) {
  JSContext* ctx = resolver->ctx;

  // Validate it's a function
  if (!JS_IsFunction(ctx, js_func)) {
    JSRT_Debug("Import '%s.%s' is not a function",
               import_info->module_name, import_info->name);
    return -1;
  }

  // ... existing allocation code ...

  // NEW: Extract signature from import_info
  func_import->param_count = import_info->u.func.param_count;
  func_import->result_count = import_info->u.func.result_count;

  // Validate counts
  if (func_import->param_count > 16) {
    JSRT_Debug("Function '%s.%s' has too many parameters (%u > 16)",
               import_info->module_name, import_info->name,
               func_import->param_count);
    return -1;
  }

  if (func_import->result_count > 4) {
    JSRT_Debug("Function '%s.%s' has too many results (%u > 4)",
               import_info->module_name, import_info->name,
               func_import->result_count);
    return -1;
  }

  // Copy parameter types
  for (uint32_t i = 0; i < func_import->param_count; i++) {
    func_import->param_types[i] = import_info->u.func.param_types[i];
  }

  // Copy result types
  for (uint32_t i = 0; i < func_import->result_count; i++) {
    func_import->result_types[i] = import_info->u.func.result_types[i];
  }

  JSRT_Debug("Function '%s.%s' signature: params=%u, results=%u",
             func_import->module_name, func_import->field_name,
             func_import->param_count, func_import->result_count);

  return 0;
}
```

**Testing:**
- [ ] Create test with function requiring f32 parameter
- [ ] Verify param_types array contains WASM_F32
- [ ] Test with i64 return type

---

### Task 3.2B.3: Implement Signature String Generator

**Priority**: HIGH
**Complexity**: MEDIUM
**Estimated Time**: 2 hours

**Subtasks:**

1. [ ] Implement `generate_wamr_signature()` function
2. [ ] Map wasm_valkind_t to WAMR signature characters
3. [ ] Generate format: "(params)results"
4. [ ] Handle void return (use 'v')

**Implementation:**

```c
// Map WASM value kind to WAMR signature character
static char wasm_type_to_sig_char(wasm_valkind_t type) {
  switch (type) {
    case WASM_I32: return 'i';
    case WASM_I64: return 'I';
    case WASM_F32: return 'f';
    case WASM_F64: return 'F';
    // Reference types - not yet supported
    case WASM_FUNCREF:
    case WASM_EXTERNREF:
    case WASM_V128:
    default:
      return '\0';  // Invalid
  }
}

// Generate WAMR signature string from function import
static char* generate_wamr_signature(JSContext* ctx,
                                      jsrt_wasm_function_import_t* func_import) {
  // Calculate signature length: "(" + params + ")" + results + '\0'
  size_t len = 2 + func_import->param_count + func_import->result_count + 1;

  char* signature = js_malloc(ctx, len);
  if (!signature) {
    return NULL;
  }

  size_t pos = 0;
  signature[pos++] = '(';

  // Add parameter types
  for (uint32_t i = 0; i < func_import->param_count; i++) {
    char c = wasm_type_to_sig_char(func_import->param_types[i]);
    if (c == '\0') {
      JSRT_Debug("Unsupported parameter type %d in function '%s.%s'",
                 func_import->param_types[i],
                 func_import->module_name, func_import->field_name);
      js_free(ctx, signature);
      return NULL;
    }
    signature[pos++] = c;
  }

  signature[pos++] = ')';

  // Add result types
  if (func_import->result_count == 0) {
    // Void return
    signature[pos++] = 'v';
  } else {
    for (uint32_t i = 0; i < func_import->result_count; i++) {
      char c = wasm_type_to_sig_char(func_import->result_types[i]);
      if (c == '\0') {
        JSRT_Debug("Unsupported result type %d in function '%s.%s'",
                   func_import->result_types[i],
                   func_import->module_name, func_import->field_name);
        js_free(ctx, signature);
        return NULL;
      }
      signature[pos++] = c;
    }
  }

  signature[pos] = '\0';

  JSRT_Debug("Generated signature '%s' for function '%s.%s'",
             signature, func_import->module_name, func_import->field_name);

  return signature;
}
```

**Testing:**
- [ ] Test `(i32) → void` generates `"(i)v"`
- [ ] Test `(i32, f32) → f64` generates `"(if)F"`
- [ ] Test `(i64) → i64` generates `"(I)I"`
- [ ] Test `() → i32` generates `"()i"`

---

### Task 3.2B.4: Implement Full Type Conversion in Trampoline

**Priority**: HIGH
**Complexity**: COMPLEX
**Estimated Time**: 4 hours

**Subtasks:**

1. [ ] Update `jsrt_wasm_import_func_trampoline()` for all types
2. [ ] Implement i64 ↔ BigInt conversion
3. [ ] Implement f32 conversion (with proper casting)
4. [ ] Implement f64 conversion
5. [ ] Handle argument array slot calculations (i64/f64 use 2 slots)
6. [ ] Handle multi-value returns

**Implementation:**

```c
static void jsrt_wasm_import_func_trampoline(wasm_exec_env_t exec_env,
                                              uint32_t* args,
                                              uint32_t argc) {
  // Get function import data
  jsrt_wasm_function_import_t* func_import =
      (jsrt_wasm_function_import_t*)wasm_runtime_get_function_attachment(exec_env);

  if (!func_import) {
    JSRT_Debug("ERROR: No function import attachment");
    wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                "Internal error: missing function attachment");
    return;
  }

  JSContext* ctx = func_import->ctx;
  JSValue js_func = func_import->js_function;

  JSRT_Debug("Calling JS import '%s.%s' with %u params",
             func_import->module_name, func_import->field_name,
             func_import->param_count);

  // Convert WASM arguments to JS values
  JSValue* js_args = NULL;
  if (func_import->param_count > 0) {
    js_args = js_malloc(ctx, sizeof(JSValue) * func_import->param_count);
    if (!js_args) {
      wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                  "Out of memory");
      return;
    }

    uint32_t arg_idx = 0;  // Index into args[] array

    for (uint32_t i = 0; i < func_import->param_count; i++) {
      switch (func_import->param_types[i]) {
        case WASM_I32: {
          int32_t val = (int32_t)args[arg_idx];
          js_args[i] = JS_NewInt32(ctx, val);
          JSRT_Debug("  arg[%u] = i32(%d)", i, val);
          arg_idx += 1;
          break;
        }

        case WASM_I64: {
          // i64 uses 2 slots (low, high) or 1 slot on 64-bit
          #if UINTPTR_MAX == 0xFFFFFFFF
            // 32-bit platform
            int64_t val = (int64_t)args[arg_idx] |
                         ((int64_t)args[arg_idx + 1] << 32);
            arg_idx += 2;
          #else
            // 64-bit platform
            int64_t val = (int64_t)args[arg_idx];
            arg_idx += 1;
          #endif
          js_args[i] = JS_NewBigInt64(ctx, val);
          JSRT_Debug("  arg[%u] = i64(%lld)", i, (long long)val);
          break;
        }

        case WASM_F32: {
          float val = *(float*)&args[arg_idx];
          js_args[i] = JS_NewFloat64(ctx, (double)val);
          JSRT_Debug("  arg[%u] = f32(%f)", i, val);
          arg_idx += 1;
          break;
        }

        case WASM_F64: {
          // f64 uses 2 slots
          uint64_t bits = (uint64_t)args[arg_idx] |
                         ((uint64_t)args[arg_idx + 1] << 32);
          double val = *(double*)&bits;
          js_args[i] = JS_NewFloat64(ctx, val);
          JSRT_Debug("  arg[%u] = f64(%f)", i, val);
          arg_idx += 2;
          break;
        }

        default:
          JSRT_Debug("Unsupported parameter type %d",
                     func_import->param_types[i]);
          for (uint32_t j = 0; j < i; j++) {
            JS_FreeValue(ctx, js_args[j]);
          }
          js_free(ctx, js_args);
          wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                      "Unsupported parameter type");
          return;
      }
    }
  }

  // Call JavaScript function
  JSValue result = JS_Call(ctx, js_func, JS_UNDEFINED,
                           func_import->param_count, js_args);

  // Free JS arguments
  if (js_args) {
    for (uint32_t i = 0; i < func_import->param_count; i++) {
      JS_FreeValue(ctx, js_args[i]);
    }
    js_free(ctx, js_args);
  }

  // Check for JS exception
  if (JS_IsException(result)) {
    JSValue exception = JS_GetException(ctx);
    const char* err_msg = JS_ToCString(ctx, exception);
    JSRT_Debug("JS exception in import: %s", err_msg ? err_msg : "(unknown)");

    char exception_buf[256];
    snprintf(exception_buf, sizeof(exception_buf),
             "JavaScript exception: %s", err_msg ? err_msg : "(unknown)");
    wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                exception_buf);

    if (err_msg) JS_FreeCString(ctx, err_msg);
    JS_FreeValue(ctx, exception);
    return;
  }

  // Convert return value(s) to WASM types
  if (func_import->result_count > 0) {
    JSValue result_value = result;
    bool is_array = false;

    // Multi-value return: expect array
    if (func_import->result_count > 1) {
      if (!JS_IsArray(ctx, result)) {
        JS_FreeValue(ctx, result);
        wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                    "Multi-value return must be an array");
        return;
      }
      is_array = true;
    }

    uint32_t arg_idx = 0;  // Index into args[] for return values

    for (uint32_t i = 0; i < func_import->result_count; i++) {
      // Get value (from array or direct)
      JSValue val;
      if (is_array) {
        val = JS_GetPropertyUint32(ctx, result, i);
        if (JS_IsException(val)) {
          JS_FreeValue(ctx, result);
          wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                      "Failed to get array element");
          return;
        }
      } else {
        val = JS_DupValue(ctx, result_value);
      }

      // Convert by type
      switch (func_import->result_types[i]) {
        case WASM_I32: {
          int32_t ret_val;
          if (JS_ToInt32(ctx, &ret_val, val)) {
            JS_FreeValue(ctx, val);
            JS_FreeValue(ctx, result);
            wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                        "Return value conversion to i32 failed");
            return;
          }
          args[arg_idx] = (uint32_t)ret_val;
          JSRT_Debug("  result[%u] = i32(%d)", i, ret_val);
          arg_idx += 1;
          break;
        }

        case WASM_I64: {
          int64_t ret_val;
          if (JS_ToBigInt64(ctx, &ret_val, val)) {
            JS_FreeValue(ctx, val);
            JS_FreeValue(ctx, result);
            wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                        "Return value conversion to i64 failed");
            return;
          }
          #if UINTPTR_MAX == 0xFFFFFFFF
            args[arg_idx] = (uint32_t)(ret_val & 0xFFFFFFFF);
            args[arg_idx + 1] = (uint32_t)(ret_val >> 32);
            arg_idx += 2;
          #else
            args[arg_idx] = (uint32_t)ret_val;
            arg_idx += 1;
          #endif
          JSRT_Debug("  result[%u] = i64(%lld)", i, (long long)ret_val);
          break;
        }

        case WASM_F32: {
          double ret_val;
          if (JS_ToFloat64(ctx, &ret_val, val)) {
            JS_FreeValue(ctx, val);
            JS_FreeValue(ctx, result);
            wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                        "Return value conversion to f32 failed");
            return;
          }
          float f32 = (float)ret_val;
          args[arg_idx] = *(uint32_t*)&f32;
          JSRT_Debug("  result[%u] = f32(%f)", i, f32);
          arg_idx += 1;
          break;
        }

        case WASM_F64: {
          double ret_val;
          if (JS_ToFloat64(ctx, &ret_val, val)) {
            JS_FreeValue(ctx, val);
            JS_FreeValue(ctx, result);
            wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                        "Return value conversion to f64 failed");
            return;
          }
          uint64_t bits = *(uint64_t*)&ret_val;
          args[arg_idx] = (uint32_t)(bits & 0xFFFFFFFF);
          args[arg_idx + 1] = (uint32_t)(bits >> 32);
          JSRT_Debug("  result[%u] = f64(%f)", i, ret_val);
          arg_idx += 2;
          break;
        }

        default:
          JS_FreeValue(ctx, val);
          JS_FreeValue(ctx, result);
          wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env),
                                      "Unsupported return type");
          return;
      }

      JS_FreeValue(ctx, val);
    }
  }

  JS_FreeValue(ctx, result);
  JSRT_Debug("JS import call completed successfully");
}
```

**Testing:**
- [ ] Test i64 parameter and return
- [ ] Test f32 parameter and return
- [ ] Test f64 parameter and return
- [ ] Test mixed types: `(i32, f64, i64) → f32`
- [ ] Test multi-value return: `() → (i32, i32)`
- [ ] Test void return

---

### Task 3.2B.5: Update Registration to Use Dynamic Signatures

**Priority**: HIGH
**Complexity**: MEDIUM
**Estimated Time**: 2 hours

**Subtasks:**

1. [ ] Update `jsrt_wasm_register_function_imports()` to generate signatures
2. [ ] Replace hardcoded `"(ii)i"` with generated signatures
3. [ ] Store signature in function_import for cleanup

**Implementation:**

```c
static int jsrt_wasm_register_function_imports(jsrt_wasm_import_resolver_t* resolver) {
  if (!resolver || resolver->function_import_count == 0) {
    return 0;
  }

  JSContext* ctx = resolver->ctx;
  uint32_t count = resolver->function_import_count;

  JSRT_Debug("Registering %u function imports with WAMR", count);

  // For Phase 3.2B: Assume single module for now (typically "env")
  // TODO: Support multiple modules in future
  const char* module_name = resolver->function_imports[0].module_name;

  // Create NativeSymbol array
  NativeSymbol* native_symbols = js_malloc(ctx, sizeof(NativeSymbol) * count);
  if (!native_symbols) {
    JSRT_Debug("Failed to allocate NativeSymbol array");
    return -1;
  }

  memset(native_symbols, 0, sizeof(NativeSymbol) * count);

  // Populate NativeSymbol array
  for (uint32_t i = 0; i < count; i++) {
    jsrt_wasm_function_import_t* func_import = &resolver->function_imports[i];

    // Verify module name matches (single module assumption)
    if (strcmp(func_import->module_name, module_name) != 0) {
      JSRT_Debug("ERROR: Multiple module namespaces not yet supported");
      js_free(ctx, native_symbols);
      return -1;
    }

    // Generate signature (NEW - replaces hardcoded "(ii)i")
    func_import->signature = generate_wamr_signature(ctx, func_import);
    if (!func_import->signature) {
      JSRT_Debug("Failed to generate signature for '%s.%s'",
                 func_import->module_name, func_import->field_name);
      // Cleanup previously generated signatures
      for (uint32_t j = 0; j < i; j++) {
        if (resolver->function_imports[j].signature) {
          js_free(ctx, resolver->function_imports[j].signature);
        }
      }
      js_free(ctx, native_symbols);
      return -1;
    }

    native_symbols[i].symbol = func_import->field_name;
    native_symbols[i].func_ptr = (void*)jsrt_wasm_import_func_trampoline;
    native_symbols[i].signature = func_import->signature;  // Use generated signature
    native_symbols[i].attachment = func_import;

    JSRT_Debug("  [%u] symbol='%s' signature='%s'", i,
               native_symbols[i].symbol, native_symbols[i].signature);
  }

  // Register with WAMR
  if (!wasm_runtime_register_natives(module_name, native_symbols, count)) {
    JSRT_Debug("wasm_runtime_register_natives failed");
    js_free(ctx, native_symbols);
    return -1;
  }

  // Store native_symbols in resolver (must not be freed while module is alive)
  resolver->native_symbols = native_symbols;
  resolver->native_symbol_count = count;

  // Copy module name for unregistration later
  size_t name_len = strlen(module_name);
  resolver->module_name_for_natives = js_malloc(ctx, name_len + 1);
  if (!resolver->module_name_for_natives) {
    JSRT_Debug("Failed to allocate module name copy");
  } else {
    memcpy(resolver->module_name_for_natives, module_name, name_len + 1);
  }

  JSRT_Debug("Successfully registered %u function imports", count);
  return 0;
}
```

**Update Cleanup:**

```c
// Update jsrt_wasm_import_resolver_destroy to free signatures
static void jsrt_wasm_import_resolver_destroy(jsrt_wasm_import_resolver_t* resolver) {
  if (!resolver) {
    return;
  }

  JSContext* ctx = resolver->ctx;

  // ... existing cleanup ...

  // Free function imports (including signatures)
  for (uint32_t i = 0; i < resolver->function_import_count; i++) {
    if (resolver->function_imports[i].module_name) {
      js_free(ctx, resolver->function_imports[i].module_name);
    }
    if (resolver->function_imports[i].field_name) {
      js_free(ctx, resolver->function_imports[i].field_name);
    }
    if (resolver->function_imports[i].signature) {  // NEW
      js_free(ctx, resolver->function_imports[i].signature);
    }
    if (!JS_IsUndefined(resolver->function_imports[i].js_function)) {
      JS_FreeValue(ctx, resolver->function_imports[i].js_function);
    }
  }

  // ... rest of cleanup ...
}
```

**Testing:**
- [ ] Test signature generation for various types
- [ ] Verify WAMR accepts generated signatures
- [ ] Verify cleanup frees signatures properly

---

### Task 3.2B.6: Support Multiple Module Namespaces

**Priority**: MEDIUM
**Complexity**: MEDIUM
**Estimated Time**: 2 hours

**Background**: Currently only "env" module namespace is supported. Need to support multiple namespaces like "wasi_snapshot_preview1", "console", etc.

**Subtasks:**

1. [ ] Group imports by module_name
2. [ ] Register each module separately
3. [ ] Store multiple NativeSymbol arrays in resolver

**Implementation:**

```c
// Enhanced resolver to support multiple modules
typedef struct {
  char* module_name;
  NativeSymbol* symbols;
  uint32_t symbol_count;
} jsrt_wasm_native_module_t;

typedef struct {
  // ... existing fields ...

  // NEW: Multiple module support
  jsrt_wasm_native_module_t* native_modules;  // Array of registered modules
  uint32_t native_module_count;
} jsrt_wasm_import_resolver_t;

static int jsrt_wasm_register_function_imports(jsrt_wasm_import_resolver_t* resolver) {
  if (!resolver || resolver->function_import_count == 0) {
    return 0;
  }

  JSContext* ctx = resolver->ctx;

  // Group imports by module name
  // Simple approach: iterate and count unique modules first
  const char* module_names[16];  // Max 16 unique modules
  uint32_t module_counts[16];
  uint32_t num_modules = 0;

  for (uint32_t i = 0; i < resolver->function_import_count; i++) {
    const char* name = resolver->function_imports[i].module_name;

    // Find or add module
    uint32_t idx;
    for (idx = 0; idx < num_modules; idx++) {
      if (strcmp(module_names[idx], name) == 0) {
        module_counts[idx]++;
        break;
      }
    }

    if (idx == num_modules) {
      // New module
      if (num_modules >= 16) {
        JSRT_Debug("Too many unique module namespaces (max 16)");
        return -1;
      }
      module_names[num_modules] = name;
      module_counts[num_modules] = 1;
      num_modules++;
    }
  }

  JSRT_Debug("Registering imports from %u module namespace(s)", num_modules);

  // Allocate native modules array
  resolver->native_modules = js_malloc(ctx, sizeof(jsrt_wasm_native_module_t) * num_modules);
  if (!resolver->native_modules) {
    return -1;
  }
  resolver->native_module_count = num_modules;

  // Register each module
  for (uint32_t m = 0; m < num_modules; m++) {
    const char* module_name = module_names[m];
    uint32_t count = module_counts[m];

    JSRT_Debug("Registering module '%s' with %u functions", module_name, count);

    // Allocate NativeSymbol array
    NativeSymbol* symbols = js_malloc(ctx, sizeof(NativeSymbol) * count);
    if (!symbols) {
      // TODO: Cleanup previously allocated modules
      return -1;
    }

    // Populate symbols for this module
    uint32_t sym_idx = 0;
    for (uint32_t i = 0; i < resolver->function_import_count; i++) {
      jsrt_wasm_function_import_t* func_import = &resolver->function_imports[i];

      if (strcmp(func_import->module_name, module_name) != 0) {
        continue;  // Skip, different module
      }

      // Generate signature
      if (!func_import->signature) {
        func_import->signature = generate_wamr_signature(ctx, func_import);
        if (!func_import->signature) {
          js_free(ctx, symbols);
          return -1;
        }
      }

      symbols[sym_idx].symbol = func_import->field_name;
      symbols[sym_idx].func_ptr = (void*)jsrt_wasm_import_func_trampoline;
      symbols[sym_idx].signature = func_import->signature;
      symbols[sym_idx].attachment = func_import;
      sym_idx++;
    }

    // Register with WAMR
    if (!wasm_runtime_register_natives(module_name, symbols, count)) {
      JSRT_Debug("wasm_runtime_register_natives failed for module '%s'",
                 module_name);
      js_free(ctx, symbols);
      return -1;
    }

    // Store in resolver
    resolver->native_modules[m].module_name = js_strdup(ctx, module_name);
    resolver->native_modules[m].symbols = symbols;
    resolver->native_modules[m].symbol_count = count;
  }

  JSRT_Debug("Successfully registered all function imports");
  return 0;
}

// Update cleanup
static void jsrt_wasm_import_resolver_destroy(jsrt_wasm_import_resolver_t* resolver) {
  if (!resolver) {
    return;
  }

  JSContext* ctx = resolver->ctx;

  // Unregister and free native modules
  if (resolver->native_modules) {
    for (uint32_t i = 0; i < resolver->native_module_count; i++) {
      if (resolver->native_modules[i].module_name &&
          resolver->native_modules[i].symbols) {
        wasm_runtime_unregister_natives(resolver->native_modules[i].module_name,
                                         resolver->native_modules[i].symbols);
        js_free(ctx, resolver->native_modules[i].module_name);
        js_free(ctx, resolver->native_modules[i].symbols);
      }
    }
    js_free(ctx, resolver->native_modules);
  }

  // ... rest of cleanup ...
}
```

**Testing:**
- [ ] Test with imports from "env" only
- [ ] Test with imports from multiple modules ("env", "console")
- [ ] Verify cleanup unregisters all modules

---

### Task 3.2B.7: Comprehensive Testing

**Priority**: HIGH
**Complexity**: MEDIUM
**Estimated Time**: 3 hours

**Subtasks:**

1. [ ] Create WASM test modules with various type signatures
2. [ ] Test all type combinations
3. [ ] Test multi-value returns
4. [ ] Test multiple module namespaces
5. [ ] Test error cases
6. [ ] Validate with ASAN

**Test Cases:**

**Test 1: f32 Parameter and Return**
```wat
(module
  (import "env" "sqrt" (func $sqrt (param f32) (result f32)))
  (func (export "test") (result f32)
    f32.const 16.0
    call $sqrt
  )
)
```

```javascript
const importObject = {
  env: {
    sqrt: function(x) { return Math.sqrt(x); }
  }
};

const instance = new WebAssembly.Instance(module, importObject);
const result = instance.exports.test();
console.assert(Math.abs(result - 4.0) < 0.001, "sqrt(16) should be 4");
```

**Test 2: f64 Parameter and Return**
```wat
(module
  (import "env" "pow" (func $pow (param f64 f64) (result f64)))
  (func (export "test") (result f64)
    f64.const 2.0
    f64.const 3.0
    call $pow
  )
)
```

```javascript
const importObject = {
  env: {
    pow: function(x, y) { return Math.pow(x, y); }
  }
};

const instance = new WebAssembly.Instance(module, importObject);
const result = instance.exports.test();
console.assert(Math.abs(result - 8.0) < 0.001, "pow(2,3) should be 8");
```

**Test 3: i64 with BigInt**
```wat
(module
  (import "env" "addBig" (func $addBig (param i64 i64) (result i64)))
  (func (export "test") (result i64)
    i64.const 9223372036854775807  ; Max i64
    i64.const 1
    call $addBig
  )
)
```

```javascript
const importObject = {
  env: {
    addBig: function(a, b) {
      return BigInt(a) + BigInt(b);
    }
  }
};

const instance = new WebAssembly.Instance(module, importObject);
const result = instance.exports.test();
// Should overflow and wrap around
console.log("Result:", result);
```

**Test 4: Mixed Types**
```wat
(module
  (import "env" "mixed" (func $mixed (param i32 f32 i64) (result f64)))
  (func (export "test") (result f64)
    i32.const 10
    f32.const 3.14
    i64.const 100
    call $mixed
  )
)
```

```javascript
const importObject = {
  env: {
    mixed: function(i, f, big) {
      return Number(i) + f + Number(big);
    }
  }
};

const instance = new WebAssembly.Instance(module, importObject);
const result = instance.exports.test();
console.assert(Math.abs(result - 113.14) < 0.01, "Mixed types should work");
```

**Test 5: Void Return**
```wat
(module
  (import "env" "log" (func $log (param i32)))
  (func (export "test")
    i32.const 42
    call $log
  )
)
```

```javascript
let logValue = null;
const importObject = {
  env: {
    log: function(x) { logValue = x; }
  }
};

const instance = new WebAssembly.Instance(module, importObject);
instance.exports.test();
console.assert(logValue === 42, "log should receive 42");
```

**Test 6: Multiple Module Namespaces**
```wat
(module
  (import "env" "add" (func $add (param i32 i32) (result i32)))
  (import "console" "log" (func $log (param i32)))
  (func (export "test") (result i32)
    i32.const 10
    i32.const 20
    call $add
    local.tee 0
    call $log
    local.get 0
  )
)
```

```javascript
let logValue = null;
const importObject = {
  env: {
    add: (a, b) => a + b
  },
  console: {
    log: (x) => { logValue = x; }
  }
};

const instance = new WebAssembly.Instance(module, importObject);
const result = instance.exports.test();
console.assert(result === 30, "add(10,20) should be 30");
console.assert(logValue === 30, "log should receive 30");
```

**Test 7: Error - Type Mismatch**
```javascript
const importObject = {
  env: {
    sqrt: function(x) { return "not a number"; }  // Wrong return type
  }
};

try {
  const instance = new WebAssembly.Instance(module, importObject);
  instance.exports.test();
  console.assert(false, "Should have thrown RuntimeError");
} catch (e) {
  console.assert(e instanceof WebAssembly.RuntimeError, "Should be RuntimeError");
}
```

**Test 8: ASAN Validation**
```bash
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/web/webassembly/test_web_wasm_function_imports_full_types.js
```

**Test File Structure:**
```javascript
// test/web/webassembly/test_web_wasm_function_imports_full_types.js

// Test 1: f32
// ...

// Test 2: f64
// ...

// Test 3: i64
// ...

// Test 4: Mixed types
// ...

// Test 5: Void return
// ...

// Test 6: Multiple modules
// ...

// Test 7: Error cases
// ...

console.log("All Phase 3.2B tests passed!");
```

---

## Risk Assessment

### High Risks

1. **WAMR Signature Compatibility** (HIGH)
   - **Risk**: Generated signatures may not match WAMR expectations
   - **Mitigation**: Test with WAMR examples, validate signature format
   - **Fallback**: Use WAMR signature examples as reference

2. **i64/f64 Slot Layout** (HIGH)
   - **Risk**: Incorrect slot calculations cause memory corruption
   - **Mitigation**: Thoroughly test on both 32-bit and 64-bit platforms
   - **Detection**: ASAN will catch memory errors

3. **BigInt Conversion** (MEDIUM)
   - **Risk**: BigInt ↔ i64 conversion edge cases (overflow, precision)
   - **Mitigation**: Test boundary values, follow WebAssembly spec
   - **Reference**: Check MDN WebAssembly BigInt conversion rules

4. **Multi-Value Returns** (MEDIUM)
   - **Risk**: WAMR may not fully support multi-value returns
   - **Mitigation**: Check WAMR capabilities first, document limitations
   - **Fallback**: Return error for multi-value if unsupported

### Medium Risks

5. **Type Coercion** (MEDIUM)
   - **Risk**: JS type coercion rules vs WebAssembly spec
   - **Mitigation**: Follow WebAssembly JS API spec precisely
   - **Testing**: Test edge cases (NaN, Infinity, undefined)

6. **Memory Management** (MEDIUM)
   - **Risk**: Signature strings, larger argument arrays
   - **Mitigation**: ASAN validation, careful cleanup
   - **Testing**: Stress test with many imports

### Low Risks

7. **Performance** (LOW)
   - **Risk**: Dynamic type checking overhead
   - **Mitigation**: Acceptable for initial implementation
   - **Future**: Optimize hot paths if needed

---

## Implementation Order

### Week 1: Core Type Support (12 hours)

**Day 1 (4 hours):**
- Task 3.2B.1: Data structures (1h)
- Task 3.2B.2: Extract signatures (2h)
- Task 3.2B.3: Signature generator (2h)

**Day 2 (6 hours):**
- Task 3.2B.4: Full type conversion in trampoline (4h)
- Task 3.2B.5: Update registration (2h)

**Day 3 (3 hours):**
- Task 3.2B.6: Multiple module namespaces (2h)
- Build and basic testing (1h)

### Week 2: Testing & Polish (6 hours)

**Day 4 (3 hours):**
- Task 3.2B.7: Create test WASM modules
- Task 3.2B.7: Run all test cases

**Day 5 (3 hours):**
- ASAN validation
- Fix any issues found
- Documentation update

---

## Acceptance Criteria

### Functional Requirements

- [ ] f32 parameters and returns work correctly
- [ ] f64 parameters and returns work correctly
- [ ] i64 parameters and returns work with BigInt
- [ ] Mixed type signatures work (e.g., `(i32, f64, i64) → f32`)
- [ ] Void returns work
- [ ] Multi-value returns work (or properly documented as unsupported)
- [ ] Multiple module namespaces supported (not just "env")
- [ ] Dynamic signature generation from WAMR import types
- [ ] Backward compatible with Phase 3.2A (i32 only)

### Quality Requirements

- [ ] All test cases pass
- [ ] ASAN validation clean (no leaks, no memory errors)
- [ ] Code formatted (`make format`)
- [ ] No regressions in existing tests
- [ ] Memory usage reasonable (no excessive allocations)

### Documentation Requirements

- [ ] Update `webassembly-plan.md` Task 3.2B status
- [ ] Document any WAMR limitations discovered
- [ ] Add examples to test files
- [ ] Update CLAUDE.md if needed

---

## Success Metrics

**Phase 3.2B Completion:**
- All WebAssembly value types supported for function imports
- Dynamic signature reading and generation working
- Comprehensive test coverage for all type combinations
- Zero memory leaks or errors (ASAN validation)

**Phase 3 Overall:**
- Task 3.2B DONE → Phase 3 ~65% complete
- Full function import/export support (all types)
- Ready for Phase 4 (Table & Global)

---

## Deliverables

1. **Enhanced Implementation** (`src/std/webassembly.c`)
   - Updated data structures
   - Full type conversion in trampoline
   - Dynamic signature generation
   - Multi-module namespace support

2. **Comprehensive Tests** (`test/web/webassembly/test_web_wasm_function_imports_full_types.js`)
   - All type combinations
   - Multi-value returns
   - Multiple module namespaces
   - Error cases

3. **WASM Test Binaries**
   - Various type signature test modules
   - Generated from WAT files

4. **Documentation Updates**
   - `webassembly-plan.md` Task 3.2B marked DONE
   - Any WAMR limitations documented
   - Implementation notes for future reference

---

## Appendix: Type Conversion Reference

### JavaScript → WebAssembly

| JS Type | WASM Type | QuickJS API | Notes |
|---------|-----------|-------------|-------|
| Number (int) | i32 | `JS_ToInt32()` | Truncates to 32-bit signed |
| BigInt | i64 | `JS_ToBigInt64()` | Full 64-bit support |
| Number (float) | f32 | `JS_ToFloat64()` + cast | Precision loss acceptable |
| Number (double) | f64 | `JS_ToFloat64()` | Full precision |

### WebAssembly → JavaScript

| WASM Type | JS Type | QuickJS API | Notes |
|-----------|---------|-------------|-------|
| i32 | Number | `JS_NewInt32()` | -2^31 to 2^31-1 |
| i64 | BigInt | `JS_NewBigInt64()` | Full range |
| f32 | Number | `JS_NewFloat64()` | Converted to f64 |
| f64 | Number | `JS_NewFloat64()` | Native precision |

### WAMR Argument Array Layout

| Type | Slots | Layout | Example |
|------|-------|--------|---------|
| i32 | 1 | `[value]` | `[42]` |
| f32 | 1 | `[bits]` | `[0x40490FDB]` (3.14159) |
| i64 | 2 (32-bit) | `[low, high]` | `[0x12345678, 0x9ABCDEF0]` |
| i64 | 1 (64-bit) | `[value]` | `[0x9ABCDEF012345678]` |
| f64 | 2 | `[low, high]` | `[0x1F85EB51, 0x400921FB]` (π) |

---

## Next Steps After Phase 3.2B

1. **Task 3.5**: Update Instance Constructor (if not already done)
2. **Phase 4**: Table & Global implementation
3. **Phase 7**: WPT integration and testing
4. **Optimization**: Profile and optimize hot paths if needed

---

**Document Maintenance**
- **Last Updated**: 2025-10-17
- **Status**: Ready for execution
- **Owner**: WebAssembly Implementation Team
