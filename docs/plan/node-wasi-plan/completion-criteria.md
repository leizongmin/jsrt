# Completion Criteria

## Functional Requirements

- [ ] **All WASI API methods implemented**
  - WASI constructor with options
  - getImportObject() method
  - start() method
  - initialize() method

- [ ] **WASI preview1 imports working**
  - All required WASI functions (fd_*, path_*, etc.)
  - Proper error code mapping
  - Memory access safety

- [ ] **Both module aliases work**
  - require('node:wasi')
  - require('jsrt:wasi')
  - import { WASI } from 'node:wasi'

- [ ] **File I/O with preopens works**
  - Directory mapping
  - Read/write operations
  - Sandbox enforcement

- [ ] **Args and env passing works**
  - Command-line arguments
  - Environment variables
  - Unicode support

- [ ] **Lifecycle methods work**
  - start() calls _start export
  - initialize() calls _initialize export
  - State management correct
  - Mutual exclusion enforced

## Quality Requirements

### Testing

- [ ] **All tests pass**
  - make test → 100% pass rate
  - make test N=wasi → All WASI tests pass
  - All test files in test/wasi/ pass

- [ ] **WPT baseline maintained**
  - make wpt → Failures <= baseline
  - No new regressions introduced
  - Document any known limitations

- [ ] **No memory leaks**
  - ASAN validation passes
  - make jsrt_m && run tests
  - ASAN_OPTIONS=detect_leaks=1 clean

- [ ] **Code formatted**
  - make format succeeds
  - All files follow project style
  - No formatting warnings

- [ ] **Cross-platform support verified**
  - Linux: Full test suite passes
  - macOS: Test suite passes (if available)
  - Windows: Test suite passes (if available)

### Code Quality

- [ ] **Memory management**
  - All allocations checked
  - All allocations freed
  - No dangling pointers
  - Proper GC integration

- [ ] **Error handling**
  - All error cases handled
  - Clear error messages
  - Matches Node.js behavior
  - No silent failures

- [ ] **State management**
  - Lifecycle enforced
  - No invalid transitions
  - Thread-safe if needed
  - Resource cleanup correct

## Documentation Requirements

- [ ] **API reference complete**
  - docs/wasi-api.md created
  - All methods documented
  - Parameter types specified
  - Return values documented
  - Examples included

- [ ] **Usage examples provided**
  - examples/wasi-hello.js
  - examples/wasi-file-io.js
  - examples/wasi-args-env.js
  - All examples tested and working

- [ ] **Migration guide created**
  - docs/wasi-migration.md
  - Differences from Node.js documented
  - Migration steps outlined
  - Compatibility notes included

- [ ] **Security considerations documented**
  - Sandbox behavior explained
  - Limitations documented
  - Best practices provided
  - Warnings for unsafe usage

## Integration Requirements

- [ ] **Module registered**
  - Added to src/node/node_modules.c
  - Builtin loader integration
  - Module cache working

- [ ] **Both module formats work**
  - CommonJS: require() works
  - ES Module: import works
  - Named exports available
  - Default export available

- [ ] **Module caching works**
  - Multiple requires return same object
  - No unnecessary reinitializations
  - Memory efficient

- [ ] **Error messages match Node.js style**
  - Error types correct
  - Error messages similar
  - Stack traces useful
  - Error codes match

## Performance Requirements

- [ ] **Acceptable performance**
  - No obvious slowdowns
  - File I/O reasonably fast
  - Memory usage reasonable
  - No memory leaks under load

- [ ] **Scalability**
  - Multiple WASI instances work
  - Many file operations supported
  - Large files handled correctly
  - No resource exhaustion

## Validation Checklist

Before marking implementation as complete:

1. ✅ **Build succeeds**
   ```bash
   make clean && make
   make jsrt_g
   make jsrt_m
   ```

2. ✅ **Tests pass**
   ```bash
   make test            # 100% pass
   make test N=wasi     # All WASI tests pass
   make wpt             # Failures <= baseline
   ```

3. ✅ **Code formatted**
   ```bash
   make format          # No changes needed
   ```

4. ✅ **Memory safe**
   ```bash
   make jsrt_m
   ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/wasi/*.js
   # No errors or leaks
   ```

5. ✅ **Examples work**
   ```bash
   ./bin/jsrt examples/wasi-*.js
   # All examples run successfully
   ```

6. ✅ **Documentation complete**
   - All required docs created
   - Examples verified
   - No TODOs remaining

7. ✅ **Integration verified**
   ```bash
   # Test both protocols
   ./bin/jsrt -e "const {WASI} = require('node:wasi'); console.log(typeof WASI)"
   ./bin/jsrt -e "const {WASI} = require('jsrt:wasi'); console.log(typeof WASI)"
   ```

## Sign-off

Implementation is complete when:
- ✅ All functional requirements met
- ✅ All quality requirements met
- ✅ All documentation requirements met
- ✅ All integration requirements met
- ✅ All validation checks pass
- ✅ Code reviewed by maintainer
- ✅ Ready for production use
