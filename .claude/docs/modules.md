# Module Development Guide

## Creating a New Module

### Step 1: Define Module Structure
```c
// src/std/mymodule.c
#include "../jsrt.h"
#include <string.h>

// Private module state (if needed)
typedef struct {
    int counter;
    JSValue callback;
} mymodule_state_t;

// Module methods
static JSValue js_mymodule_method(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv);
```

### Step 2: Implement Module Methods
```c
static JSValue js_mymodule_method(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    // Argument validation
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Expected at least 1 argument");
    }
    
    // Type checking
    if (!JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "First argument must be a string");
    }
    
    // Get string value
    const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_EXCEPTION;
    
    // Process...
    JSValue result = JS_NewString(ctx, str);
    
    // Clean up
    JS_FreeCString(ctx, str);
    
    return result;
}
```

### Step 3: Module Initialization
```c
JSValue jsrt_init_module_mymodule(JSContext *ctx) {
    JSValue module = JS_NewObject(ctx);
    
    // Add functions
    JS_SetPropertyStr(ctx, module, "method",
                      JS_NewCFunction(ctx, js_mymodule_method, "method", 1));
    
    // Add constants
    JS_SetPropertyStr(ctx, module, "VERSION",
                      JS_NewString(ctx, "1.0.0"));
    
    // Add nested objects if needed
    JSValue submodule = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, submodule, "helper",
                      JS_NewCFunction(ctx, js_helper, "helper", 0));
    JS_SetPropertyStr(ctx, module, "utils", submodule);
    
    return module;
}
```

### Step 4: Register Module
```c
// In src/runtime.c
extern JSValue jsrt_init_module_mymodule(JSContext *ctx);

// In jsrt_init_runtime() function
js_std_add_module(ctx, "mymodule", jsrt_init_module_mymodule);
```

### Step 5: Update Build System
```cmake
# In CMakeLists.txt
set(JSRT_SOURCES
    ${JSRT_SOURCES}
    src/std/mymodule.c
)
```

## Module Patterns

### Singleton Pattern
```c
static JSValue module_instance = JS_UNDEFINED;

JSValue jsrt_init_module_singleton(JSContext *ctx) {
    if (!JS_IsUndefined(module_instance)) {
        return JS_DupValue(ctx, module_instance);
    }
    
    module_instance = JS_NewObject(ctx);
    // Initialize module...
    
    return JS_DupValue(ctx, module_instance);
}
```

### Factory Pattern
```c
static JSValue js_create_instance(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    JSValue obj = JS_NewObject(ctx);
    
    // Set up instance properties
    JS_SetPropertyStr(ctx, obj, "id", JS_NewInt32(ctx, next_id++));
    
    return obj;
}
```

### Event Emitter Pattern
```c
typedef struct {
    JSContext *ctx;
    JSValue listeners;  // Object with event arrays
} event_emitter_t;

static JSValue js_on(JSContext *ctx, JSValueConst this_val,
                     int argc, JSValueConst *argv) {
    // Add listener to event array
    return JS_UNDEFINED;
}

static JSValue js_emit(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
    // Call all listeners for event
    return JS_UNDEFINED;
}
```

## Best Practices

### 1. Argument Validation
```c
// Check argument count
if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Expected 2 arguments, got %d", argc);
}

// Check types
if (!JS_IsNumber(argv[0])) {
    return JS_ThrowTypeError(ctx, "First argument must be a number");
}

// Get and validate values
int32_t value;
if (JS_ToInt32(ctx, &value, argv[0])) {
    return JS_EXCEPTION;
}
if (value < 0 || value > 100) {
    return JS_ThrowRangeError(ctx, "Value must be between 0 and 100");
}
```

### 2. Memory Management
```c
// Always free C strings
const char *str = JS_ToCString(ctx, argv[0]);
if (!str) return JS_EXCEPTION;
// Use str...
JS_FreeCString(ctx, str);

// Duplicate values when storing
stored_value = JS_DupValue(ctx, argv[0]);
// Later: JS_FreeValue(ctx, stored_value);

// Handle allocation failures
void *buffer = js_malloc(ctx, size);
if (!buffer) {
    return JS_ThrowOutOfMemory(ctx);
}
```

### 3. Error Handling
```c
// Propagate exceptions
JSValue result = some_operation(ctx);
if (JS_IsException(result)) {
    return result;  // Don't wrap exceptions
}

// Throw appropriate error types
return JS_ThrowTypeError(ctx, "Invalid type");
return JS_ThrowRangeError(ctx, "Out of range");
return JS_ThrowReferenceError(ctx, "Undefined variable");
return JS_ThrowSyntaxError(ctx, "Invalid syntax");
```

### 4. Async Operations
```c
// Store callback for async operation
typedef struct {
    JSContext *ctx;
    JSValue callback;
    uv_work_t work;
} async_work_t;

static void async_work_cb(uv_work_t *req) {
    async_work_t *work = req->data;
    // Do work in thread pool...
}

static void async_after_cb(uv_work_t *req, int status) {
    async_work_t *work = req->data;
    
    // Call JS callback
    JSValue ret = JS_Call(work->ctx, work->callback, JS_UNDEFINED, 0, NULL);
    if (JS_IsException(ret)) {
        // Handle exception
    }
    
    // Cleanup
    JS_FreeValue(work->ctx, work->callback);
    JS_FreeValue(work->ctx, ret);
    js_free(work->ctx, work);
}
```

## Testing Modules

### Unit Test Template
```javascript
// test/test_mymodule.js
const assert = require("jsrt:assert");
const mymodule = require("jsrt:mymodule");

// Test basic functionality
assert.strictEqual(mymodule.VERSION, "1.0.0", "Version should be 1.0.0");

// Test method
const result = mymodule.method("test");
assert.strictEqual(result, "test", "Method should return input");

// Test error cases
assert.throws(() => {
    mymodule.method();  // No arguments
}, TypeError, "Should throw on missing arguments");

assert.throws(() => {
    mymodule.method(123);  // Wrong type
}, TypeError, "Should throw on wrong type");

console.log("=== mymodule tests passed ===");
```

## Common Module Types

### 1. I/O Modules
- File system operations
- Network requests
- Database connections

### 2. Utility Modules
- String manipulation
- Data formatting
- Validation helpers

### 3. API Modules
- Web standards (URL, Crypto)
- Node.js compatibility
- Custom protocols

### 4. System Modules
- Process management
- Environment variables
- System information