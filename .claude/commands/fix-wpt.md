@agent-jsrt-developer Fix the failing cases of make wpt N=$1

## 1. Pre-fix Baseline
Execute `make wpt N=$1` to establish the initial baseline:
- Record exact failure count
- Document failure categories and patterns
- Save output to `target/tmp/wpt-$1-baseline.log` for reference

## 2. Analysis Phase
Analyze failures systematically:
- Use `SHOW_ALL_FAILURES=1 make wpt N=$1` if needed for detailed output
- Group failures by root cause (e.g., encoding, validation, parsing)
- Prioritize fixes by impact (fix categories with most failures first)
- Understand WHATWG URL specification requirements for each failure type

## 3. Implementation Phase
Implement fixes following these principles:
- **Minimal changes**: Only modify what's necessary to fix the issue
- **Test after each change**: Run `make wpt N=$1` after each fix
- **No regressions**: Ensure failure count never increases
- **Follow existing patterns**: Match codebase conventions and style

Additional requirements: $2

## 4. Verification Phase
Before finalizing changes:
1. Run `make format` to ensure code formatting
2. Run `make test` to verify all unit tests pass
3. Run `make wpt N=$1` for final WPT results
4. Compare with baseline to ensure net improvement

## 5. Post-fix Summary
Provide a comprehensive report:

### Quantitative Results
- **Baseline**: X failures in Y test files
- **Final**: X failures in Y test files
- **Net improvement**: X fewer failures (Y% reduction)

### Fixed Test Cases
List specific test cases fixed with examples:
```
✅ "http://?foo" - Now correctly throws "Invalid URL"
✅ "http://[::1%2561]" - IPv6 percent-encoding properly rejected
```

### Technical Changes
- **Files modified**: List each file with brief description
- **Key fixes**: Describe the core logic changes
- **Compliance improvements**: Note WHATWG spec alignment

### Regression Check
- ✅ All unit tests passing (X/X)
- ✅ No new failures introduced
- ✅ Code formatted with `make format`

## 6. Code Review
@jsrt-code-reviewer Review the fix for:
- Memory safety and leak prevention
- Standards compliance
- Performance impact
- Edge case handling

## 7. Commit
Create a detailed commit message documenting the improvements and specific issues fixed.
