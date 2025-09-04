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
```

## Test Categories

The following WinterCG Minimum Common API categories are currently supported:

- **console**: Console API tests (namespace, logging, etc.)
- **encoding**: Text encoding/decoding APIs 
- **hr-time**: High Resolution Time API
- **performance**: Performance measurement APIs
- **url**: URL and URLSearchParams APIs
- **webcrypto**: Web Crypto API (limited subset)

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

The WPT integration is designed to test jsrt's implementation of the WinterCG Minimum Common API. As of the current implementation:

- **Console API**: 2/3 tests passing (67%)
- **High Resolution Time**: 1/1 tests passing (100%)
- **URL API**: 3/8 tests passing (38%)
- **Overall**: ~40% pass rate

Tests are selected based on compatibility with jsrt's non-browser environment. Many WPT tests are designed for browsers and require DOM or other browser-specific APIs that jsrt does not implement.

## Architecture

The WPT integration consists of:

1. **WPT Submodule** (`wpt/`): The official Web Platform Tests repository
2. **Test Runner** (`scripts/run-wpt.py`): Python script to run selected WPT tests
3. **Test Harness** (`scripts/wpt-testharness.js`): jsrt-compatible test harness that adapts WPT assertions to jsrt's environment
4. **Make Targets**: Convenient `make wpt` commands

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