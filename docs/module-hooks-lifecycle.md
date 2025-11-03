# Module Hooks Lifecycle and Ownership Semantics

This document describes the lifecycle, ownership semantics, and usage patterns for the jsrt module hooks infrastructure.

## Overview

Module hooks allow embedders to customize module resolution and loading behavior without forking the runtime. The system provides two types of hooks:

- **Resolve Hooks**: Customize module specifier resolution
- **Load Hooks**: Customize module source loading

## Hook Lifecycle

### 1. Hook Registration

```c
// Register a hook with resolve and/or load functions
JSValue resolve_fn = /* JavaScript function */;
JSValue load_fn = /* JavaScript function */;

int result = jsrt_hook_register(registry, resolve_fn, load_fn);
```

**Semantics**:
- Hooks are registered in LIFO order (Last In, First Out)
- Each hook can provide either resolve_fn, load_fn, or both
- Functions are reference-counted to keep them alive
- Registry takes ownership of the provided function references

### 2. Hook Execution

#### Resolve Hook Execution

```c
JSRTHookContext context = {
    .specifier = "./my-module.js",
    .base_path = "/path/to/current/dir",
    .resolved_url = NULL,  // Will be set by resolver
    .is_main_module = false,
    .user_data = NULL
};

char* resolved = jsrt_hook_execute_resolve(registry, &context);
```

**Execution Order**: Last registered hook executes first
**Short-circuiting**: First non-null result ends the chain
**Ownership**: Caller owns the returned string and must free it

**Hook Function Signature**:
```javascript
function resolve(specifier, context, next) {
    // specifier: string - original module specifier
    // context: object - { specifier, basePath, resolvedUrl, isMain }
    // next: function - call next() to continue chain

    // Return a string to short-circuit resolution
    // Return null/undefined to continue normal processing
    return next();
}
```

#### Load Hook Execution

```c
JSRTHookContext context = {
    .specifier = "./my-module.js",
    .base_path = "/path/to/current/dir",
    .resolved_url = "file:///path/to/my-module.js",
    .is_main_module = false,
    .user_data = NULL
};

char* source = jsrt_hook_execute_load(registry, &context, "file:///path/to/my-module.js");
```

**Execution Order**: Last registered hook executes first
**Short-circuiting**: First non-null result ends the chain
**Ownership**: Caller owns the returned string and must free it

**Hook Function Signature**:
```javascript
function load(url, context, next) {
    // url: string - resolved module URL
    // context: object - { specifier, basePath, resolvedUrl, isMain }
    // next: function - call next() to continue chain

    // Return source code string to short-circuit loading
    // Return null/undefined to continue normal processing
    return next();
}
```

### 3. Hook Cleanup

```c
// Registry cleanup automatically frees all hooks
jsrt_hook_registry_free(registry);
```

**Automatic Cleanup**:
- Registry automatically frees all JavaScript function references
- All hook memory is released
- No manual cleanup required for individual hooks

## Memory Management Rules

### For Hook Authors (JavaScript)

1. **Function References**: Registry maintains reference counts
2. **Return Values**: Strings are copied and managed by C code
3. **Error Handling**: Throw exceptions to indicate errors

### For Registry Users (C)

1. **Returned Strings**: Must free strings returned from hook execution
2. **Registry Ownership**: Registry owns hook function references
3. **Context Structure**: Context struct is read-only for hooks

### Example Usage

```c
// Initialize registry
JSRTHookRegistry* registry = jsrt_hook_registry_init(ctx);

// Register hooks
JSValue resolve_hook = /* get resolve function from JS */;
JSValue load_hook = /* get load function from JS */;
jsrt_hook_register(registry, resolve_hook, load_hook);

// Use hooks in module loading
char* resolved = jsrt_hook_execute_resolve(registry, &context);
if (resolved) {
    // Hook provided custom resolution
    char* source = jsrt_hook_execute_load(registry, &context, resolved);
    if (source) {
        // Hook provided custom source
        // Use the source...
        free(source);
    }
    free(resolved);
}

// Cleanup (automatic)
jsrt_hook_registry_free(registry);
```

## Error Handling

- **JavaScript Exceptions**: Converted to NULL return values in C
- **Memory Allocation**: NULL returned on allocation failures
- **Invalid Arguments**: Functions validate arguments and return error codes
- **Hook Execution**: Exceptions in hooks are caught and logged

## Thread Safety

- **Registry**: Not thread-safe, use external synchronization
- **Hook Functions**: Must be thread-safe if called from multiple threads
- **Context**: Context struct is read-only during hook execution

## Performance Considerations

- **Hook Chain**: Executed in LIFO order, minimal overhead
- **Memory**: Reference counting avoids unnecessary copies
- **Short-circuiting**: First non-null result stops chain execution
- **Caching**: Hook results are not cached (hooks must implement caching if needed)

## Compatibility with Node.js

The hook system maintains compatibility with Node.js hook semantics:

1. **Synchronous Execution**: Hooks are called synchronously
2. **LIFO Ordering**: Matches Node.js hook registration order
3. **Function Signatures**: Compatible with Node.js hook patterns
4. **Error Handling**: Exceptions propagate like in Node.js

## Best Practices

1. **Minimal Hooks**: Keep hooks simple and fast
2. **Error Handling**: Always handle exceptions in hook functions
3. **Memory Management**: Free returned strings promptly
4. **Validation**: Validate hook function arguments
5. **Testing**: Test hooks with various module types and edge cases