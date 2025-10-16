# Testing Strategy

## Unit Tests

Test individual components in isolation:

- **Constructor and options parsing**
  - Valid and invalid options
  - Default value application
  - Type validation

- **Individual WASI functions**
  - fd_read, fd_write
  - path_open, environ_get, args_get
  - Error cases and edge conditions

- **Error handling**
  - Invalid arguments
  - State violations
  - Resource exhaustion

- **State management**
  - Lifecycle transitions
  - started/initialized flags
  - Mutual exclusion enforcement

## Integration Tests

Test complete workflows:

- **End-to-end WASM execution**
  - Load → Instantiate → Start
  - Output capture and verification
  - Exit code handling

- **File I/O operations**
  - Read, write, seek operations
  - Directory operations
  - Preopen enforcement

- **Multi-module scenarios**
  - Multiple WASI instances
  - Module caching behavior
  - Resource isolation

- **Cross-platform compatibility**
  - Linux, macOS, Windows
  - Path handling differences
  - Platform-specific APIs

## Security Tests

Verify sandbox enforcement:

- **Path traversal prevention**
  - ../ attack attempts
  - Symlink following
  - Absolute path escapes

- **Sandbox enforcement**
  - Access outside preopens fails
  - Proper error codes returned
  - No information leakage

- **Invalid input handling**
  - Malformed WASM modules
  - Invalid file descriptors
  - Buffer overflow attempts

## Performance Tests

Measure and optimize:

- **Large file operations**
  - Multi-MB file reads/writes
  - Memory usage tracking
  - Throughput measurement

- **Many WASI calls**
  - Call overhead
  - FD table performance
  - Cache effectiveness

- **Memory usage**
  - Peak memory consumption
  - Leak detection
  - GC behavior under load

## Validation Tests

Mandatory quality checks:

### ASAN (AddressSanitizer)

```bash
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/wasi/*.js
```

- Memory leaks
- Buffer overflows
- Use-after-free
- Double-free

### Test Suite

```bash
make test         # Must pass 100%
make test N=wasi  # WASI-specific tests
```

### Web Platform Tests

```bash
make wpt          # Failures must be <= baseline
```

## Test Organization

```
test/wasi/
├── test_wasi_basic.js          # Constructor, options, getImportObject
├── test_wasi_hello.js          # Hello world WASM execution
├── test_wasi_args.js           # Argument passing
├── test_wasi_env.js            # Environment variables
├── test_wasi_file_io.js        # File operations
├── test_wasi_security.js       # Sandbox and security
├── test_wasi_lifecycle.js      # start/initialize methods
├── test_wasi_errors.js         # Error handling
└── wasm/                       # Test WASM modules
    ├── hello.wasm
    ├── args_reader.wasm
    ├── env_reader.wasm
    └── file_io.wasm
```

## Test Requirements

- **Coverage**: All public API methods tested
- **Pass rate**: 100% for unit and integration tests
- **ASAN**: Zero memory errors or leaks
- **WPT**: No regression from baseline
- **Platforms**: Verified on Linux minimum, macOS/Windows if available
