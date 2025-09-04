# Web Platform Tests (WPT) Integration

jsrt now includes Web Platform Tests (WPT) integration to validate compliance with the WinterCG Minimum Common API specification.

## Quick Start

```bash
# Run all WPT tests for WinterCG Minimum Common API
make wpt

# Run with debug build and verbose output
make wpt_g

# List available test categories
make wpt-list

# Download/update WPT tests manually
make wpt-download   # Download if not present
make wpt-update     # Re-download latest version
```

## Test Categories

The following WinterCG Minimum Common API categories are comprehensively tested:

- **console**: Console API tests (namespace, logging, etc.)
- **encoding**: Text encoding/decoding APIs (TextEncoder/TextDecoder) 
- **hr-time**: High Resolution Time API
- **performance**: Performance measurement APIs
- **url**: URL and URLSearchParams APIs
- **fetch-api**: Fetch API (Headers, Request, Response)
- **webcrypto**: Web Crypto API (getRandomValues, digest)
- **base64**: Base64 utilities (btoa/atob)
- **timers**: Timer functions (setTimeout, setInterval, etc.)
- **streams**: Streams API (ReadableStream, WritableStream)
- **abort**: AbortController and AbortSignal APIs

## Manual Usage

You can also run the WPT test runner directly:

```bash
# Run specific categories
python3 scripts/run-wpt.py --jsrt ./target/release/jsrt --category console --category url

# Run with verbose output
python3 scripts/run-wpt.py --jsrt ./target/release/jsrt --verbose

# Output results to JSON
python3 scripts/run-wpt.py --jsrt ./target/release/jsrt --output results.json

# List available categories
python3 scripts/run-wpt.py --list-categories
```

## Current Status

The WPT integration provides comprehensive testing of jsrt's implementation of the WinterCG Minimum Common API. As of the current implementation:

**Test Coverage Summary:**
- **Total Tests**: 32 tests across 11 API categories
- **Passed**: 7 tests (21.9% pass rate)
- **Failed**: 21 tests 
- **Skipped**: 4 tests (due to browser-specific requirements)

**API Category Results:**

| Category | Tests | Passed | Failed | Skipped | Pass Rate |
|----------|-------|--------|--------|---------|-----------|
| **Console API** | 3 | 2 | 1 | 0 | 67% |
| **HR-Time API** | 1 | 1 | 0 | 0 | 100% |
| **Performance API** | 1 | 1 | 0 | 0 | 100% |  
| **Timer Functions** | 4 | 1 | 3 | 0 | 25% |
| **URL API** | 10 | 2 | 7 | 1 | 20% |
| **Encoding API** | 5 | 0 | 5 | 0 | 0% |
| **Fetch API** | 3 | 0 | 0 | 3 | - |
| **WebCrypto API** | 1 | 0 | 1 | 0 | 0% |
| **Base64 Utilities** | 1 | 0 | 1 | 0 | 0% |
| **Streams API** | 2 | 0 | 2 | 0 | 0% |
| **Abort API** | 1 | 0 | 1 | 0 | 0% |

This comprehensive testing reveals the current completion level of jsrt's WinterCG API implementation and identifies specific areas requiring further development.

**Implementation Completeness Analysis:**

1. **✅ Well-Implemented APIs**: Console (partial), HR-Time, Performance, basic Timer functionality
2. **🟡 Partially Implemented**: URL (basic functionality works, URLSearchParams has issues), Timers (basic setTimeout/setInterval work)
3. **❌ Needs Improvement**: Encoding (missing test fixtures), WebCrypto (basic failures), Streams, Abort controllers
4. **⚠️ Browser-Dependent**: Fetch API tests require browser globals and may need jsrt-specific variants

The test results provide actionable insights into which WinterCG APIs need attention for full compliance.

## Architecture

The WPT integration consists of:

1. **WPT Directory** (`wpt/`): Dynamically downloaded Web Platform Tests repository (latest master branch)
2. **Test Runner** (`scripts/run-wpt.py`): Python script to run selected WPT tests
3. **Test Harness** (`scripts/wpt-testharness.js`): jsrt-compatible test harness that adapts WPT assertions to jsrt's environment
4. **Make Targets**: Convenient `make wpt` commands with automatic WPT downloading

**WPT Download System:**
- WPT tests are downloaded automatically when running `make wpt` or `make wpt-download`
- Uses the latest master branch from https://github.com/web-platform-tests/wpt
- `make wpt-update` refreshes to the latest version
- No git submodule dependency for faster repository cloning

## Test Selection Criteria

Tests are selected based on:

1. **WinterCG Compatibility**: Only tests relevant to WinterCG Minimum Common API
2. **jsrt Compatibility**: Tests that don't require browser-specific APIs
3. **Environment Support**: Tests that can run in jsrt's JavaScript-only environment

Tests are automatically skipped if they use unsupported features like:
- DOM APIs (`document`, browser-specific `window` usage)
- Web Workers (`Worker`, `SharedWorker`, `importScripts`)
- Browser navigation (`navigator`, `location`)
- Event handling for DOM events

## Contributing

To add new WPT test categories:

1. Update `WINTERCG_TESTS` in `scripts/run-wpt.py`
2. Ensure tests are compatible with jsrt's environment
3. Test locally with `make wpt`
4. Update documentation

## References

- [Web Platform Tests](https://web-platform-tests.org/)
- [WinterCG Minimum Common API](https://wintercg.org/work/minimum-common-api/)
- [WPT Test Writing Guide](https://web-platform-tests.org/writing-tests/)