# npm Popular Packages Compatibility Testing Strategy

## Testing Overview

This document outlines the comprehensive testing strategy for validating npm package compatibility improvements in jsrt, including success criteria, testing methodologies, and automated validation processes.

## Testing Infrastructure

### Current Test Setup

The project uses smoke tests located in `examples/popular_npm_packages/`:
- 100 individual test files (`test_<package>.js`)
- Each test demonstrates basic package functionality
- Tests are designed to load packages and exercise representative APIs
- Some tests make external HTTP requests (marked as safe to ignore failures)

### Test Execution Commands

```bash
# Individual package testing (from project root)
timeout 30s ./bin/jsrt examples/popular_npm_packages/test_<package>.js

# Category-based testing
for pkg in aws_sdk babel_core express react mongodb vue; do
    echo "=== Testing $pkg ==="
    timeout 30s ./bin/jsrt examples/popular_npm_packages/test_${pkg}.js 2>&1 | head -10
done

# Batch testing with timeout protection
cd examples/popular_npm_packages
find . -name "test_*.js" -exec timeout 30s /repo/bin/jsrt {} \;
```

## Testing Categories

### Category 1: API Implementation Testing

**Testing Goal**: Verify that newly implemented APIs work correctly with target packages.

**Test Cases**:
1. **Domain Module Testing**
   ```bash
   # Test packages that require domain module
   ./bin/jsrt examples/popular_npm_packages/test_aws_sdk.js
   ./bin/jsrt examples/popular_npm_packages/test_babel_core.js
   ./bin/jsrt examples/popular_npm_packages/test_eslint.js
   ```

2. **fs/promises Module Testing**
   ```bash
   # Test packages that require fs/promises
   ./bin/jsrt examples/popular_npm_packages/test_eslint.js
   ./bin/jsrt examples/popular_npm_packages/test_webpack.js
   ./bin/jsrt examples/popular_npm_packages/test_webpack_dev_server.js
   ```

3. **Timers Module Testing**
   ```bash
   # Test packages that require timers module
   ./bin/jsrt examples/popular_npm_packages/test_mongodb.js
   ./bin/jsrt examples/popular_npm_packages/test_zone_js.js
   ./bin/jsrt examples/popular_npm_packages/test_async.js
   ```

### Category 2: Module Resolution Testing

**Testing Goal**: Verify that module resolution works for complex dependency chains.

**Test Cases**:
```bash
# Test packages with complex dependency resolution
./bin/jsrt examples/popular_npm_packages/test_core_js.js
./bin/jsrt examples/popular_npm_packages/test_babel_runtime.js
./bin/jsrt examples/popular_npm_packages/test_tslib.js
./bin/jsrt examples/popular_npm_packages/test_eslint_plugin_import.js
```

### Category 3: Stream and Process Integration Testing

**Testing Goal**: Verify stream APIs and process integration work correctly.

**Test Cases**:
```bash
# Test packages using streams heavily
./bin/jsrt examples/popular_npm_packages/test_express.js
./bin/jsrt examples/popular_npm_packages/test_gulp.js
./bin/jsrt examples/popular_npm_packages/test_through2.js
./bin/jsrt examples/popular_npm_packages/test_ora.js
```

## Success Criteria Definition

### Phase-wise Success Criteria

#### Phase 1: Critical API Implementation
**Target**: Reduce failures from 50 to 35 packages (30% improvement)

**Specific Success Metrics**:
- ✅ aws-sdk loads without domain errors
- ✅ babel_core loads without core-js symbol errors
- ✅ mongodb loads without timers errors
- ✅ eslint loads without fs/promises errors
- ✅ vue loads without V8 API errors
- ✅ express loads without stack trace errors
- ✅ webpack loads without module resolution errors

**Validation Script**:
```bash
#!/bin/bash
# phase1_success_check.sh
critical_packages=(aws_sdk babel_core mongodb eslint vue express webpack)
failed_count=0

for pkg in "${critical_packages[@]}"; do
    echo -n "Testing $pkg... "
    if timeout 30s ./bin/jsrt "examples/popular_npm_packages/test_${pkg}.js" >/dev/null 2>&1; then
        echo "PASS"
    else
        echo "FAIL"
        ((failed_count++))
    fi
done

echo "Phase 1 Results: $((7 - failed_count))/7 packages working"
if [ $failed_count -le 2 ]; then
    echo "Phase 1 SUCCESS: ≤2 failures acceptable"
    exit 0
else
    echo "Phase 1 FAILED: $failed_count failures"
    exit 1
fi
```

#### Phase 2: Module System Enhancements
**Target**: Reduce failures from 35 to 25 packages (10 more packages working)

**Additional Success Metrics**:
- ✅ core-js polyfills load correctly
- ✅ babel-runtime modules resolve
- ✅ eslint plugins work
- ✅ relative module paths resolve
- ✅ ESM/CJS interop works

#### Phase 3: Tooling & Build System Support
**Target**: Reduce failures from 25 to 18 packages (7 more packages working)

**Additional Success Metrics**:
- ✅ gulp streams work correctly
- ✅ webpack build process starts
- ✅ ora spinners display
- ✅ through2 transforms work
- ✅ postcss-loader loads

#### Phase 4: Database & Network Services
**Target**: Reduce failures from 18 to 15 packages (3 more packages working)

**Additional Success Metrics**:
- ✅ mongodb client initializes
- ✅ redis client connects (fails gracefully)
- ✅ websocket server starts
- ✅ http clients work

#### Phase 5: Advanced Framework Support
**Target**: Reduce failures from 15 to ≤15 packages (maintain success rate)

**Additional Success Metrics**:
- ✅ Vue.js reactive components work
- ✅ React server-side rendering works
- ✅ TypeScript compilation works
- ✅ Native module detection works

### Overall Success Criteria

**Final Target**: ≤15 packages failing (≤15% failure rate, up from current 50%)

**Quality Gates**:
1. **No regressions**: All currently working packages must continue working
2. **Memory safety**: No new memory leaks or crashes (ASAN testing)
3. **Performance**: No significant performance degradation (>20% slower)
4. **Standards compliance**: WPT test pass rate maintained

## Automated Testing Pipeline

### Continuous Integration Testing

```bash
#!/bin/bash
# ci_npm_compatibility.sh

echo "=== npm Package Compatibility Test ==="

# Build jsrt
make clean && make jsrt_g
if [ $? -ne 0 ]; then
    echo "BUILD FAILED"
    exit 1
fi

# Run baseline tests
make test
if [ $? -ne 0 ]; then
    echo "BASELINE TESTS FAILED"
    exit 1
fi

# Test currently working packages
working_packages=(lodash async chalk commander debug moment prop_types react)
for pkg in "${working_packages[@]}"; do
    if ! timeout 15s ./bin/jsrt "examples/popular_npm_packages/test_${pkg}.js" >/dev/null 2>&1; then
        echo "REGRESSION: $pkg stopped working"
        exit 1
    fi
done

# Test target failing packages
target_packages=(aws_sdk babel_core express react mongodb vue eslint webpack)
failed_count=0
for pkg in "${target_packages[@]}"; do
    if timeout 15s ./bin/jsrt "examples/popular_npm_packages/test_${pkg}.js" >/dev/null 2>&1; then
        echo "SUCCESS: $pkg now working"
    else
        echo "STILL FAILING: $pkg"
        ((failed_count++))
    fi
done

echo "Results: $((10 - failed_count))/10 target packages working"

if [ $failed_count -le 8 ]; then  # Allow 2 failures for now
    echo "COMPATIBILITY TEST PASSED"
    exit 0
else
    echo "COMPATIBILITY TEST FAILED"
    exit 1
fi
```

### Memory Safety Testing

```bash
#!/bin/bash
# memory_safety_test.sh

# Build with AddressSanitizer
make jsrt_m

# Test critical packages with leak detection
critical_packages=(aws_sdk babel_core express react mongodb vue)
export ASAN_OPTIONS=detect_leaks=1

for pkg in "${critical_packages[@]}"; do
    echo "Testing $pkg for memory issues..."
    timeout 30s ./bin/jsrt_m "examples/popular_npm_packages/test_${pkg}.js" 2>&1 | \
        grep -E "(ERROR|LEAK|Sanitizer)" && echo "MEMORY ISSUE DETECTED in $pkg"
done
```

### Performance Impact Testing

```bash
#!/bin/bash
# performance_test.sh

# Baseline performance measurement
echo "Measuring baseline performance..."
time timeout 10s ./bin/jsrt examples/popular_npm_packages/test_lodash.js

# Test with implemented changes
echo "Measuring with compatibility changes..."
time timeout 10s ./bin/jsrt examples/popular_npm_packages/test_aws_sdk.js

# Compare and alert if >20% slower
# (Implementation depends on specific performance requirements)
```

## Testing Documentation

### Test Result Tracking

Create a compatibility matrix to track progress:

| Package | Current Status | Target Status | Implementation Required | Test Date |
|---------|----------------|---------------|------------------------|-----------|
| aws-sdk | ❌ Domain Error | ✅ Working | Domain Module | TBD |
| babel_core | ❌ core-js Error | ✅ Working | Module Resolution | TBD |
| express | ❌ Stack Trace Error | ✅ Working | Process API | TBD |
| react | ✅ Working | ✅ Working | None | 2025-01-05 |
| lodash | ✅ Working | ✅ Working | None | 2025-01-05 |

### Regression Testing Protocol

1. **Before Each Implementation**:
   - Run full test suite
   - Document current failure count
   - Run memory safety checks

2. **After Each Implementation**:
   - Test target packages specifically
   - Run full test suite
   - Check for regressions
   - Update compatibility matrix

3. **Before Each Commit**:
   - `make format` (code formatting)
   - `make test` (unit tests)
   - `make wpt` (standards compliance)
   - Memory safety validation

## Error Classification and Debugging

### Common Error Patterns and Solutions

1. **Module Not Found Errors**
   ```
   Error: Cannot find module 'domain'
   ```
   - **Cause**: Missing Node.js builtin module
   - **Solution**: Implement the missing module in `src/node/`
   - **Test**: Verify with `require('domain')`

2. **Module Resolution Failures**
   ```
   Failed to load module '../core-js/symbol': JSRT_READ_FILE_ERROR_FILE_NOT_FOUND
   ```
   - **Cause**: Incorrect relative path resolution in transpiled code
   - **Solution**: Enhance module resolver for transpiled paths
   - **Test**: Check with various transpiled packages

3. **API Compatibility Issues**
   ```
   not a function
   at getStack (/repo/.../depd/index.js:392:26)
   ```
   - **Cause**: Missing or incorrect API implementation
   - **Solution**: Implement the correct API signature
   - **Test**: Verify with specific API calls

4. **Native Module Issues**
   ```
   Error loading native module...
   ```
   - **Cause**: Attempting to load binary addons
   - **Solution**: Provide fallbacks or clear error messages
   - **Test**: Verify graceful failure handling

## Continuous Improvement

### Feedback Loop

1. **Test Results Analysis**: Weekly review of test results and progress
2. **Priority Adjustment**: Based on package popularity and implementation complexity
3. **Risk Assessment**: Identify potential regressions before implementation
4. **Documentation Updates**: Keep compatibility matrix current

### Long-term Maintenance

1. **Automated Testing**: Weekly full compatibility test runs
2. **Package Version Updates**: Test new package versions as they're released
3. **API Evolution**: Track Node.js API changes and update implementations
4. **Performance Monitoring**: Track jsrt performance impact of compatibility layers

This comprehensive testing strategy ensures that npm compatibility improvements can be implemented systematically while maintaining jsrt's quality standards and performance characteristics.