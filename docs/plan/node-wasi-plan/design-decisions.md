# Key Design Decisions

## Architecture Decisions

1. **WAMR Integration**: Leverage existing WAMR infrastructure in src/wasm/
   - Rationale: Reuse proven WebAssembly runtime, avoid additional dependencies
   - Impact: Must work within WAMR's WASI implementation constraints

2. **Module System**: Follow existing pattern from src/node/ modules
   - Rationale: Consistency with other Node.js-compatible modules
   - Impact: Standard registration in node_modules.c, dual protocol support

3. **Memory Management**: Use QuickJS memory functions (js_malloc/js_free)
   - Rationale: Proper integration with QuickJS garbage collector
   - Impact: All allocations must use QuickJS allocator

4. **Error Handling**: Match Node.js error patterns and messages
   - Rationale: API compatibility and user expectations
   - Impact: Need to study and replicate Node.js error behavior

5. **Sandboxing**: Implement strict path validation for security
   - Rationale: WASI security model requires sandbox enforcement
   - Impact: Complex path validation logic, security testing critical

## Technology Stack

- **QuickJS**: JavaScript runtime
- **WAMR**: WebAssembly execution engine
- **libuv**: Async I/O (already in jsrt)
- **POSIX APIs**: File and process operations

## Performance Considerations

- **Module caching**: Avoid repeated initialization
- **Import object caching**: Cache per WASI instance
- **Efficient FD table**: Minimize lookup overhead
- **Minimal allocations**: Reduce GC pressure

## Security Considerations

- **Strict preopen validation**: Prevent directory traversal
- **Path traversal prevention**: Canonicalize and validate all paths
- **Memory bounds checking**: Validate WASM memory access
- **Input validation**: Sanitize all user inputs

## Compatibility Notes

- **Target**: Node.js WASI API v24.x (latest)
- **WASI spec**: Preview1 specification compliance
- **Module formats**: Both CommonJS and ES Module support
- **Platforms**: Linux, macOS, Windows
