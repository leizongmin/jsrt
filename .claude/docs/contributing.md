# Contributing Guidelines

## Before You Start

### Prerequisites
- C Compiler (gcc or clang)
- CMake >= 3.16
- clang-format (for code formatting)
- Git with submodule support
- Optional: OpenSSL/libcrypto for WebCrypto

### Setup Development Environment
```bash
# Clone repository with submodules
git clone --recursive <repo_url>
cd jsrt

# Initialize submodules if needed
git submodule update --init --recursive

# Build debug version
make jsrt_g

# Verify setup
make test
```

## Contribution Workflow

### 1. Create Feature Branch
```bash
git checkout -b feature/your-feature-name
```

### 2. Development Cycle
```bash
# Make changes
edit src/...

# Format code (MANDATORY)
make format

# Build and test
make jsrt_g
make test
make wpt

# Test with ASAN
make jsrt_m
./target/debug/jsrt_m examples/test.js
```

### 3. Commit Changes
```bash
# Stage changes
git add -A

# Commit with descriptive message
git commit -m "feat: add new feature description

- Detail 1
- Detail 2

Fixes #123"
```

### 4. Pre-PR Checklist

#### Required Checks
- [ ] Code formatted with `make format`
- [ ] All tests pass: `make test`
- [ ] WPT tests pass: `make wpt`
- [ ] No memory leaks: `make jsrt_m && ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m test.js`
- [ ] Cross-platform compatibility considered
- [ ] Documentation updated if API changed

#### Code Quality
- [ ] Functions under 50 lines
- [ ] Files under 500 lines
- [ ] Clear variable names
- [ ] Error handling complete
- [ ] Memory management correct

### 5. Submit Pull Request
```bash
# Push branch
git push origin feature/your-feature-name

# Create PR via GitHub
# Include:
# - Clear description
# - Test results
# - Breaking changes (if any)
```

## Code Style Guidelines

### C Code Style
```c
// Function naming: snake_case
static JSValue js_module_method(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    // 4-space indentation
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Expected 1 argument");
    }
    
    // Braces on same line for statements
    if (condition) {
        do_something();
    } else {
        do_other();
    }
    
    // Early returns for error cases
    if (error_condition) {
        return JS_EXCEPTION;
    }
    
    // Clear resource management
    char *buffer = js_malloc(ctx, size);
    if (!buffer) {
        return JS_ThrowOutOfMemory(ctx);
    }
    
    // Always free resources
    js_free(ctx, buffer);
    return result;
}
```

### JavaScript Test Style
```javascript
// Use jsrt:assert module
const assert = require("jsrt:assert");

// Descriptive test messages
assert.strictEqual(actual, expected, "Should return expected value");

// Group related tests
// URL parsing tests
assert.ok(URL.parse("http://example.com"));
assert.ok(URL.parse("https://example.com"));

// Error handling tests
assert.throws(() => {
    URL.parse("invalid");
}, TypeError, "Should throw on invalid URL");

// Clear completion message
console.log("=== Tests completed ===");
```

## Commit Message Format

### Format
```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Code style (formatting, semicolons, etc)
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `test`: Test additions or corrections
- `build`: Build system changes
- `ci`: CI configuration changes
- `chore`: Other changes (maintenance)

### Examples
```
feat(url): add URL.parse() static method

Implements the URL.parse() method according to the
WHATWG URL specification. Returns null for invalid URLs
instead of throwing an exception.

Fixes #456
```

```
fix(timer): prevent memory leak in clearTimeout

The timer callback was not being freed when clearTimeout
was called, causing a memory leak. This adds proper
cleanup of the JS callback value.

Fixes #789
```

## Testing Requirements

### Unit Tests
- Place in `test/test_*.js`
- Use `jsrt:assert` module
- Test both success and failure cases
- Handle platform differences

### WPT Compliance
- Run relevant WPT tests
- Document any expected failures
- Update compliance status

### Memory Testing
- Test with AddressSanitizer
- Check for leaks
- Verify cleanup paths

## Documentation

### When to Update Docs
- New public APIs
- Behavior changes
- New dependencies
- Platform considerations

### Documentation Locations
- API changes: Update relevant `.claude/docs/*.md`
- Examples: Add to `examples/`
- Agent updates: Modify `.claude/agents/*.md`
- General: Update `CLAUDE.md` if needed

## Review Process

### What Reviewers Look For
1. **Correctness**: Does it work as intended?
2. **Testing**: Are there adequate tests?
3. **Memory**: Proper resource management?
4. **Style**: Follows coding standards?
5. **Performance**: No regressions?
6. **Security**: No vulnerabilities introduced?
7. **Compatibility**: Works on all platforms?

### Addressing Feedback
- Respond to all comments
- Push fixes as new commits
- Request re-review when ready
- Be patient and respectful

## Getting Help

### Resources
- Check existing issues and PRs
- Review `.claude/AGENTS_SUMMARY.md`
- Consult architecture docs in `.claude/docs/`
- Ask questions in PR comments

### Common Issues
- Build failures: Check CMakeLists.txt
- Test failures: Run individually to debug
- Memory issues: Use ASAN build
- Platform issues: Check ifdef conditions

## License and Rights

By contributing, you agree that your contributions will be licensed under the project's license. Ensure you have the right to submit your changes.