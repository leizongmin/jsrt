# Web Platform Tests (WPT) Guide

## Overview

WPT is the official test suite for Web standards compliance, ensuring jsrt behaves consistently with other JavaScript runtimes.

## Quick Commands

```bash
# Run all WPT tests
make wpt

# Run specific category
make wpt N=url
make wpt N=console
make wpt N=encoding
make wpt N=streams
make wpt N=crypto

# Debug mode
SHOW_ALL_FAILURES=1 make wpt N=url
MAX_FAILURES=10 make wpt
```

## Available Test Categories

| Category | APIs Tested | Agent Support |
|----------|-------------|---------------|
| `console` | Console methods | jsrt-compliance |
| `url` | URL, URLSearchParams | jsrt-compliance |
| `encoding` | TextEncoder/TextDecoder | jsrt-compliance |
| `streams` | Streams API | jsrt-compliance |
| `crypto` | WebCrypto subset | jsrt-compliance |

## Debugging WPT Failures

### Step-by-Step Process

1. **Identify failing category**
```bash
make wpt 2>&1 | grep FAIL
```

2. **Get detailed failures**
```bash
mkdir -p target/tmp
SHOW_ALL_FAILURES=1 make wpt N=category > target/tmp/wpt-debug.log 2>&1
```

3. **Find specific test file**
```bash
grep "FAIL.*test-name" target/tmp/wpt-debug.log
find wpt -name "*test-name*"
```

4. **Run individual test**
```bash
./target/release/jsrt wpt/category/specific-test.js
```

5. **Debug with ASAN if needed**
```bash
make jsrt_m
./target/debug/jsrt_m wpt/category/specific-test.js
```

## Common Failure Patterns

| Failure Type | Likely Cause | Solution |
|--------------|--------------|----------|
| `TypeError` | Missing API method | Implement in `src/std/` |
| `RangeError` | Incorrect bounds | Check spec compliance |
| `Assertion failed` | Wrong return value | Fix implementation logic |
| `Timeout` | Async handling issue | Check event loop |
| `Segfault` | Memory issue | Use ASAN build |

## WPT Compliance Status

Track compliance using:
```bash
# Generate compliance report
make wpt 2>&1 | tee target/tmp/compliance.txt
grep -c PASS target/tmp/compliance.txt
grep -c FAIL target/tmp/compliance.txt
```

## Adding New WPT Tests

1. Update git submodule:
```bash
cd wpt
git pull origin master
cd ..
git add wpt
```

2. Enable new category in scripts/run-wpt.py

3. Run and fix failures:
```bash
make wpt N=new-category
```

## Related Documentation

- Testing guide: `.claude/docs/testing.md`
- Compliance agent: `.claude/agents/jsrt-compliance.md`
- WPT runner: `scripts/run-wpt.py`

## Best Practices

1. **Always run before commit**: `make wpt`
2. **Fix regressions immediately**: Don't let failures accumulate
3. **Document expected failures**: If spec deviation is intentional
4. **Use appropriate agent**: jsrt-compliance for WPT work
5. **Test cross-platform**: Ensure Windows/Linux/macOS pass