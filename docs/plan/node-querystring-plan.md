---
Created: 2025-10-07T00:00:00Z
Last Updated: 2025-10-07T00:00:00Z
Status: 📋 PLANNING - Ready for Implementation
Overall Progress: 0/38 tasks (0%)
API Coverage: 0/6 methods (0%)
---

# Node.js querystring Module Compatibility Plan

## 📋 Executive Summary

### Objective
Implement Node.js-compatible `node:querystring` module in jsrt by maximally reusing the existing WPT-compliant URL encoding/decoding infrastructure from `src/url/`.

### Current Status
- ❌ **node:querystring** module does not exist
- ✅ **src/url/** contains production-ready encoding/decoding functions (100% WPT compliant)
- ✅ **URLSearchParams** implementation available for reference
- 🎯 **Target**: 6 API methods with full Node.js compatibility

### Key Success Factors
1. **Maximum Code Reuse**: Leverage existing `src/url/encoding/` functions (97% WPT pass rate)
2. **Encoding Differences**: querystring uses `+` for space, URLSearchParams uses `%20`
3. **Simple Implementation**: querystring is simpler than URLSearchParams (no object state)
4. **Legacy Status**: Node.js marks querystring as "legacy" but still widely used

### Implementation Strategy
- **Phase 1**: Core encoding/decoding with URL code reuse (8 hours)
- **Phase 2**: parse() and stringify() with custom separator support (6 hours)
- **Phase 3**: Edge case handling and testing (4 hours)
- **Phase 4**: Documentation and optimization (2 hours)
- **Total Estimated Time**: 20 hours

---

## 🔍 Current Implementation Analysis

### Existing URL Module Assets (src/url/)

#### Available Encoding Functions
**File**: `src/url/encoding/url_basic_encoding.c`

```c
// Core encoding functions
✅ url_encode_with_len(str, len)           // Encodes with space → +
✅ url_encode(str)                         // Wrapper for url_encode_with_len
✅ hex_to_int(char)                        // Hex digit conversion
✅ url_decode_query_with_length_and_output_len() // Decodes with + → space

// Specialized encoding (less relevant)
url_component_encode()                     // Space → %20 (URLSearchParams style)
url_query_encode()                         // More restrictive
url_userinfo_encode()                      // User credentials encoding
```

**Key Insight**: The existing `url_encode_with_len()` function **already implements querystring encoding semantics** (space → `+`), unlike URLSearchParams which uses `%20`.

**Lines of Interest**:
- `url_basic_encoding.c:28-72` - url_encode_with_len (perfect for querystring.escape)
- `url_basic_encoding.c:238-320` - url_decode_query_with_length_and_output_len (perfect for querystring.unescape)

#### Available Parsing Infrastructure
**File**: `src/url/search_params/url_search_params_parser.c`

```c
✅ JSRT_ParseSearchParams(search_string, string_len)  // Parses query strings
   - Handles & separator
   - Handles = assignment
   - Properly decodes values
   - Returns linked list structure

✅ create_url_param(name, name_len, value, value_len)  // Parameter creation
✅ JSRT_AddSearchParam(search_params, name, value)     // Add parameter
```

**Key Insight**: The parsing logic from URLSearchParams can be adapted for querystring.parse() with minimal changes (custom separators, return plain object instead of URLSearchParams instance).

**Lines of Interest**:
- `url_search_params_parser.c:3-75` - Complete parse logic with & and = handling
- `url_search_params_core.c:4-37` - Parameter structure creation

### What Needs to be Built

**New File**: `src/node/node_querystring.c` (estimated ~400 lines)

```c
// Core API functions (to implement)
❌ js_querystring_parse()        // querystring.parse(str, sep, eq, options)
❌ js_querystring_stringify()    // querystring.stringify(obj, sep, eq, options)
❌ js_querystring_escape()       // querystring.escape(str) → wrapper for url_encode
❌ js_querystring_unescape()     // querystring.unescape(str) → wrapper for url_decode_query
❌ js_querystring_decode()       // Alias for parse()
❌ js_querystring_encode()       // Alias for stringify()
❌ JSRT_InitNodeQuerystring()    // Module initialization
```

### Code Reuse Mapping

| querystring Function | Reuses from src/url/ | Complexity |
|---------------------|---------------------|------------|
| `escape(str)` | `url_encode(str)` directly | **TRIVIAL** - wrapper only |
| `unescape(str)` | `url_decode_query_with_length_and_output_len()` | **TRIVIAL** - wrapper only |
| `parse(str, sep, eq)` | Logic from `JSRT_ParseSearchParams()` | **MEDIUM** - adapt for custom sep/eq |
| `stringify(obj, sep, eq)` | Encoding logic from URLSearchParams | **MEDIUM** - iterate object keys |
| `decode()` | Alias to parse() | **TRIVIAL** |
| `encode()` | Alias to stringify() | **TRIVIAL** |

**Code Reuse Percentage**: ~70% of core functionality already exists

---

## 📊 API Specification & Node.js Compatibility

### Complete API Reference

#### 1. querystring.parse(str[, sep[, eq[, options]]])
**Node.js Signature**: `parse(str: string, sep = '&', eq = '=', options = { maxKeys: 1000, decodeURIComponent })`

**Behavior**:
```javascript
querystring.parse('foo=bar&abc=xyz&abc=123')
// Returns: { foo: 'bar', abc: ['xyz', '123'] }

querystring.parse('foo:bar;baz:qux', ';', ':')
// Returns: { foo: 'bar', baz: 'qux' }

querystring.parse('a=b&a=c&a=d', null, null, { maxKeys: 2 })
// Returns: { a: ['b', 'c'] }  // Stops at maxKeys
```

**Edge Cases**:
- Empty string → `{}`
- No `=` sign → `{ key: '' }`
- Multiple values → array
- Trailing `&` → ignored
- `+` decodes to space
- maxKeys limit (default 1000)

**Implementation Notes**:
- Reuse parsing loop from `JSRT_ParseSearchParams()`
- Replace `&` and `=` with custom sep/eq
- Return plain JSObject instead of URLSearchParams
- Handle maxKeys limit
- Handle array values (multiple keys)

#### 2. querystring.stringify(obj[, sep[, eq[, options]]])
**Node.js Signature**: `stringify(obj: object, sep = '&', eq = '=', options = { encodeURIComponent })`

**Behavior**:
```javascript
querystring.stringify({ foo: 'bar', baz: ['qux', 'quux'], corge: '' })
// Returns: 'foo=bar&baz=qux&baz=quux&corge='

querystring.stringify({ foo: 'bar', baz: 'qux' }, ';', ':')
// Returns: 'foo:bar;baz:qux'

querystring.stringify({ foo: null, bar: undefined })
// Returns: 'foo=&bar='  // null/undefined become empty string
```

**Type Handling**:
- `string` → encode and use
- `number` → convert to string
- `bigint` → convert to string
- `boolean` → 'true' or 'false'
- `null` → empty string
- `undefined` → empty string
- `array` → multiple key=value pairs
- `object` → skip (or [object Object])

**Implementation Notes**:
- Iterate object own properties with `JS_GetOwnPropertyNames()`
- Use `url_encode()` for encoding values
- Support custom separators
- Handle arrays by repeating key
- Type coercion per Node.js spec

#### 3. querystring.escape(str)
**Node.js Signature**: `escape(str: string): string`

**Behavior**:
```javascript
querystring.escape('hello world')
// Returns: 'hello+world'

querystring.escape('foo=bar&baz')
// Returns: 'foo%3Dbar%26baz'

querystring.escape('café')
// Returns: 'caf%C3%A9'
```

**Encoding Rules**:
- Space → `+`
- Alphanumeric + `-_.~*` → unchanged
- Everything else → `%XX` (percent-encoded)

**Implementation**: Direct wrapper for `url_encode()`

#### 4. querystring.unescape(str)
**Node.js Signature**: `unescape(str: string): string`

**Behavior**:
```javascript
querystring.unescape('hello+world')
// Returns: 'hello world'

querystring.unescape('foo%3Dbar%26baz')
// Returns: 'foo=bar&baz'

querystring.unescape('caf%C3%A9')
// Returns: 'café'
```

**Decoding Rules**:
- `+` → space
- `%XX` → decoded byte
- Invalid sequences → kept as-is
- UTF-8 handling with replacement character

**Implementation**: Direct wrapper for `url_decode_query_with_length_and_output_len()`

#### 5. querystring.decode(str[, sep[, eq[, options]]])
**Alias for querystring.parse()** - same signature and behavior

#### 6. querystring.encode(obj[, sep[, eq[, options]]])
**Alias for querystring.stringify()** - same signature and behavior

---

## 🎯 Key Differences: querystring vs URLSearchParams

### Encoding Differences

| Character | querystring | URLSearchParams | Note |
|-----------|-------------|----------------|------|
| Space (` `) | `+` | `%20` | **Critical difference** |
| `*` | `*` | `%2A` | querystring preserves `*` |
| `~` | `~` | `~` | Both preserve |
| `-_.` | `-_.` | `-_.` | Both preserve |

**Example**:
```javascript
// querystring
querystring.stringify({ q: 'hello world' })
// → 'q=hello+world'

// URLSearchParams
new URLSearchParams({ q: 'hello world' }).toString()
// → 'q=hello%20world'
```

### Functional Differences

| Feature | querystring | URLSearchParams |
|---------|-------------|-----------------|
| **State** | Stateless (pure functions) | Stateful (object instance) |
| **Arrays** | Returns arrays for duplicate keys | Iterator-based access |
| **Custom separators** | ✅ Yes (`;` and `:` supported) | ❌ No (always `&` and `=`) |
| **Type coercion** | ✅ Converts number, boolean, null | ✅ Converts to string |
| **maxKeys limit** | ✅ Yes (default 1000) | ❌ No limit |
| **Encoding control** | ✅ Custom encodeURIComponent | ❌ Fixed encoding |

### When to Use Each

**Use querystring when**:
- Need custom separators (`;` `:` etc)
- Legacy code compatibility
- Performance critical (simpler, faster)
- Need `+` for spaces (URL query string convention)

**Use URLSearchParams when**:
- WHATWG URL standard compliance
- Browser compatibility needed
- Iterating over parameters
- Need `%20` for spaces (more standards-compliant)

---

## 🏗️ Implementation Architecture

### File Structure

```
src/node/
├── node_querystring.c          [NEW] Core implementation (~400 lines)
└── node_modules.c              [MODIFY] Add querystring registration

src/url/encoding/               [REUSE] Existing encoding functions
├── url_basic_encoding.c        ✅ url_encode(), url_decode_query()
└── url_component_encoding.c    ✅ Helper functions

src/url/url.h                   [REUSE] Function declarations
```

### Module Registration

**File**: `src/node/node_modules.c`

```c
// Add to initialization function
static const struct {
  const char* name;
  JSModuleDef* (*init)(JSContext* ctx, const char* module_name);
} node_modules[] = {
  // ... existing modules ...
  {"querystring", js_init_module_node_querystring},
  {NULL, NULL}
};
```

### Memory Management Pattern

```c
// Follow jsrt memory patterns
char* result = url_encode(input);
if (!result) {
  return JS_ThrowOutOfMemory(ctx);
}

JSValue js_result = JS_NewString(ctx, result);
free(result);  // Free C string after conversion

return js_result;
```

### Error Handling Strategy

1. **Invalid Input**: Return empty string or empty object (Node.js behavior)
2. **Memory Allocation Failure**: Throw OutOfMemory exception
3. **Invalid UTF-8**: Replace with U+FFFD (already handled in url_decode_query)
4. **maxKeys Exceeded**: Stop parsing, return partial result

---

## 📝 Detailed Task Breakdown

### Phase 1: Foundation & Basic Encoding (8 hours)

#### Task 1.1: [S][R:LOW][C:TRIVIAL] Implement escape() and unescape()
**Duration**: 2 hours
**Dependencies**: None (uses existing functions)

**Subtasks**:
- **1.1.1** [30min] Create `src/node/node_querystring.c` file skeleton
- **1.1.2** [30min] Implement `js_querystring_escape()` wrapping `url_encode()`
- **1.1.3** [30min] Implement `js_querystring_unescape()` wrapping `url_decode_query()`
- **1.1.4** [30min] Add error handling and memory management

**Acceptance Criteria**:
```javascript
assert.strictEqual(querystring.escape('hello world'), 'hello+world');
assert.strictEqual(querystring.unescape('hello+world'), 'hello world');
assert.strictEqual(querystring.escape('foo&bar=baz'), 'foo%26bar%3Dbaz');
assert.strictEqual(querystring.unescape('foo%26bar%3Dbaz'), 'foo&bar=baz');
```

**Implementation Template**:
```c
static JSValue js_querystring_escape(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewString(ctx, "");
  }

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_EXCEPTION;
  }

  char* encoded = url_encode(str);  // Reuse existing function
  JS_FreeCString(ctx, str);

  if (!encoded) {
    return JS_ThrowOutOfMemory(ctx);
  }

  JSValue result = JS_NewString(ctx, encoded);
  free(encoded);

  return result;
}
```

#### Task 1.2: [S][R:LOW][C:SIMPLE] Add encode() and decode() aliases
**Duration**: 1 hour
**Dependencies**: [D:1.1] escape/unescape implemented

**Subtasks**:
- **1.2.1** [30min] Create alias functions pointing to parse/stringify
- **1.2.2** [30min] Add to module exports

**Acceptance Criteria**:
```javascript
assert.strictEqual(querystring.encode, querystring.stringify);
assert.strictEqual(querystring.decode, querystring.parse);
```

#### Task 1.3: [S][R:LOW][C:MEDIUM] Module registration and exports
**Duration**: 3 hours
**Dependencies**: [D:1.1, 1.2] Basic functions implemented

**Subtasks**:
- **1.3.1** [1h] Add module initialization function `JSRT_InitNodeQuerystring()`
- **1.3.2** [1h] Register in `src/node/node_modules.c`
- **1.3.3** [30min] Add CommonJS support
- **1.3.4** [30min] Add ESM support

**Acceptance Criteria**:
```javascript
// CommonJS
const querystring = require('node:querystring');
assert.ok(querystring.parse);

// ESM
import querystring from 'node:querystring';
assert.ok(querystring.stringify);
```

#### Task 1.4: [S][R:LOW][C:SIMPLE] Basic unit tests
**Duration**: 2 hours
**Dependencies**: [D:1.3] Module registered

**Subtasks**:
- **1.4.1** [1h] Create `test/node/querystring/test_escape_unescape.js`
- **1.4.2** [30min] Test basic encoding/decoding
- **1.4.3** [30min] Test edge cases (empty, unicode, special chars)

**Acceptance Criteria**:
- 15+ test cases for escape/unescape
- All tests passing
- No memory leaks (ASAN clean)

---

### Phase 2: parse() and stringify() Implementation (6 hours)

#### Task 2.1: [S][R:MEDIUM][C:MEDIUM] Implement parse() with default separators
**Duration**: 3 hours
**Dependencies**: [D:1.1] escape/unescape working

**Subtasks**:
- **2.1.1** [1h] Adapt parsing loop from `JSRT_ParseSearchParams()`
- **2.1.2** [1h] Build plain JSObject from parsed parameters
- **2.1.3** [30min] Handle duplicate keys (create arrays)
- **2.1.4** [30min] Implement maxKeys limit (default 1000)

**Implementation Notes**:
```c
// Reuse parsing logic from url_search_params_parser.c
// Key changes:
// 1. Return plain JSObject instead of JSRT_URLSearchParams
// 2. Handle arrays for duplicate keys
// 3. Add maxKeys limit check

static JSValue js_querystring_parse(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
  // Parse arguments: str, sep='&', eq='=', options={maxKeys:1000}
  const char* str = argc > 0 ? JS_ToCString(ctx, argv[0]) : "";
  const char* sep = argc > 1 ? JS_ToCString(ctx, argv[1]) : "&";
  const char* eq = argc > 2 ? JS_ToCString(ctx, argv[2]) : "=";

  // Extract maxKeys from options
  int max_keys = 1000;
  if (argc > 3 && JS_IsObject(argv[3])) {
    JSValue max_keys_val = JS_GetPropertyStr(ctx, argv[3], "maxKeys");
    if (!JS_IsUndefined(max_keys_val)) {
      JS_ToInt32(ctx, &max_keys, max_keys_val);
    }
    JS_FreeValue(ctx, max_keys_val);
  }

  JSValue result = JS_NewObject(ctx);

  // Adapt parsing loop (reuse logic from JSRT_ParseSearchParams)
  // ... parsing implementation ...

  return result;
}
```

**Acceptance Criteria**:
```javascript
assert.deepStrictEqual(
  querystring.parse('foo=bar&abc=xyz'),
  { foo: 'bar', abc: 'xyz' }
);

assert.deepStrictEqual(
  querystring.parse('foo=bar&foo=baz'),
  { foo: ['bar', 'baz'] }
);

assert.deepStrictEqual(
  querystring.parse('a=b&c=d&e=f', null, null, { maxKeys: 2 }),
  { a: 'b', c: 'd' }
);
```

#### Task 2.2: [S][R:MEDIUM][C:MEDIUM] Add custom separator support
**Duration**: 1.5 hours
**Dependencies**: [D:2.1] Basic parse() working

**Subtasks**:
- **2.2.1** [1h] Replace hardcoded `&` with custom sep parameter
- **2.2.2** [30min] Replace hardcoded `=` with custom eq parameter

**Acceptance Criteria**:
```javascript
assert.deepStrictEqual(
  querystring.parse('foo:bar;baz:qux', ';', ':'),
  { foo: 'bar', baz: 'qux' }
);
```

#### Task 2.3: [S][R:MEDIUM][C:MEDIUM] Implement stringify()
**Duration**: 1.5 hours
**Dependencies**: [D:1.1] escape() working

**Subtasks**:
- **2.3.1** [1h] Iterate object properties with `JS_GetOwnPropertyNames()`
- **2.3.2** [30min] Handle different types (string, number, boolean, array, null)

**Implementation Notes**:
```c
static JSValue js_querystring_stringify(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
  if (argc < 1 || !JS_IsObject(argv[0])) {
    return JS_NewString(ctx, "");
  }

  JSValue obj = argv[0];
  const char* sep = argc > 1 ? JS_ToCString(ctx, argv[1]) : "&";
  const char* eq = argc > 2 ? JS_ToCString(ctx, argv[2]) : "=";

  // Get own property names
  JSPropertyEnum* props;
  uint32_t prop_count;
  JS_GetOwnPropertyNames(ctx, &props, &prop_count, obj, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY);

  // Build result string
  char* result = strdup("");

  for (uint32_t i = 0; i < prop_count; i++) {
    JSValue key_val = JS_AtomToString(ctx, props[i].atom);
    const char* key_str = JS_ToCString(ctx, key_val);
    char* encoded_key = url_encode(key_str);

    JSValue val = JS_GetProperty(ctx, obj, props[i].atom);

    // Handle arrays
    if (JS_IsArray(ctx, val)) {
      // Repeat key for each array element
    } else {
      // Convert to string and encode
      const char* val_str = JS_ToCString(ctx, val);
      char* encoded_val = url_encode(val_str);

      // Append to result: key=value
    }

    // Cleanup
  }

  JSValue js_result = JS_NewString(ctx, result);
  free(result);
  return js_result;
}
```

**Acceptance Criteria**:
```javascript
assert.strictEqual(
  querystring.stringify({ foo: 'bar', baz: 'qux' }),
  'foo=bar&baz=qux'
);

assert.strictEqual(
  querystring.stringify({ foo: ['bar', 'baz'] }),
  'foo=bar&foo=baz'
);

assert.strictEqual(
  querystring.stringify({ foo: 123, bar: true }),
  'foo=123&bar=true'
);
```

---

### Phase 3: Edge Cases & Comprehensive Testing (4 hours)

#### Task 3.1: [P][R:MEDIUM][C:MEDIUM] Edge case testing
**Duration**: 2 hours
**Dependencies**: [D:2.1, 2.3] parse/stringify working

**Test Categories**:
- **3.1.1** Empty inputs (`''`, `{}`)
- **3.1.2** Special characters (`&=+%`)
- **3.1.3** Unicode handling (emoji, non-ASCII)
- **3.1.4** Invalid UTF-8 sequences
- **3.1.5** Null and undefined values
- **3.1.6** Very long strings (>10KB)
- **3.1.7** maxKeys boundary conditions

**Test File**: `test/node/querystring/test_edge_cases.js`

**Acceptance Criteria**:
- 30+ edge case tests
- All tests passing
- No crashes on invalid input

#### Task 3.2: [P][R:LOW][C:SIMPLE] Compatibility tests
**Duration**: 1 hour
**Dependencies**: [D:2.1, 2.3] parse/stringify working

**Subtasks**:
- **3.2.1** Test round-trip: `parse(stringify(obj))`
- **3.2.2** Compare with URLSearchParams behavior
- **3.2.3** Test encoding differences (`+` vs `%20`)

**Test File**: `test/node/querystring/test_compatibility.js`

**Acceptance Criteria**:
```javascript
// Round-trip test
const obj = { foo: 'hello world', bar: 'test' };
const encoded = querystring.stringify(obj);
const decoded = querystring.parse(encoded);
assert.deepStrictEqual(decoded, obj);

// Encoding difference verification
assert(querystring.stringify({ q: 'a b' }).includes('+'));
assert(new URLSearchParams({ q: 'a b' }).toString().includes('%20'));
```

#### Task 3.3: [P][R:LOW][C:SIMPLE] CommonJS and ESM tests
**Duration**: 1 hour
**Dependencies**: [D:1.3] Module registered

**Subtasks**:
- **3.3.1** Create `test/node/querystring/test_querystring_cjs.js`
- **3.3.2** Create `test/node/querystring/test_querystring_esm.mjs`
- **3.3.3** Test all 6 API functions in both modes

**Acceptance Criteria**:
- Both CommonJS and ESM tests passing
- All 6 functions accessible
- Aliases work correctly

---

### Phase 4: Documentation & Optimization (2 hours)

#### Task 4.1: [P][R:LOW][C:SIMPLE] Code cleanup
**Duration**: 1 hour
**Dependencies**: [D:3.1, 3.2, 3.3] All tests passing

**Subtasks**:
- **4.1.1** [30min] Run `make format`
- **4.1.2** [15min] Add function-level comments
- **4.1.3** [15min] Remove debug logging

**Acceptance Criteria**:
- Code properly formatted
- Functions documented
- Clean build with no warnings

#### Task 4.2: [P][R:LOW][C:SIMPLE] Update documentation
**Duration**: 1 hour
**Dependencies**: [D:4.1] Code cleaned

**Subtasks**:
- **4.2.1** [30min] Update this plan with completion status
- **4.2.2** [30min] Add usage examples to comments

**Acceptance Criteria**:
- Plan document updated
- Implementation documented
- Examples added

---

## 🧪 Testing Strategy

### Test Coverage Requirements

#### Unit Tests (MANDATORY)
**Coverage**: 100% of all 6 API functions
**Location**: `test/node/querystring/`
**Execution**: `make test`

**Test Files Structure**:
```
test/node/querystring/
├── test_escape_unescape.js       [15 tests] Phase 1
├── test_parse_basic.js           [20 tests] Phase 2
├── test_stringify_basic.js       [20 tests] Phase 2
├── test_custom_separators.js     [10 tests] Phase 2
├── test_edge_cases.js            [30 tests] Phase 3
├── test_compatibility.js         [15 tests] Phase 3
├── test_querystring_cjs.js       [20 tests] Phase 3
└── test_querystring_esm.mjs      [20 tests] Phase 3

Total: 150 tests across 8 files
```

#### Test Categories

**1. Basic Functionality** (40 tests)
```javascript
// escape/unescape
test('escape converts space to plus', () => {
  assert.strictEqual(querystring.escape('hello world'), 'hello+world');
});

// parse
test('parse handles simple query', () => {
  assert.deepStrictEqual(
    querystring.parse('foo=bar&baz=qux'),
    { foo: 'bar', baz: 'qux' }
  );
});

// stringify
test('stringify converts object to query', () => {
  assert.strictEqual(
    querystring.stringify({ foo: 'bar', baz: 'qux' }),
    'foo=bar&baz=qux'
  );
});
```

**2. Edge Cases** (30 tests)
```javascript
test('parse handles empty string', () => {
  assert.deepStrictEqual(querystring.parse(''), {});
});

test('stringify handles empty object', () => {
  assert.strictEqual(querystring.stringify({}), '');
});

test('parse handles missing equals sign', () => {
  assert.deepStrictEqual(querystring.parse('foo&bar'), { foo: '', bar: '' });
});

test('stringify handles null values', () => {
  assert.strictEqual(querystring.stringify({ foo: null }), 'foo=');
});
```

**3. Array Handling** (15 tests)
```javascript
test('parse creates array for duplicate keys', () => {
  assert.deepStrictEqual(
    querystring.parse('foo=bar&foo=baz&foo=qux'),
    { foo: ['bar', 'baz', 'qux'] }
  );
});

test('stringify repeats key for array values', () => {
  assert.strictEqual(
    querystring.stringify({ foo: ['bar', 'baz'] }),
    'foo=bar&foo=baz'
  );
});
```

**4. Custom Separators** (10 tests)
```javascript
test('parse supports custom separators', () => {
  assert.deepStrictEqual(
    querystring.parse('foo:bar;baz:qux', ';', ':'),
    { foo: 'bar', baz: 'qux' }
  );
});

test('stringify supports custom separators', () => {
  assert.strictEqual(
    querystring.stringify({ foo: 'bar', baz: 'qux' }, ';', ':'),
    'foo:bar;baz:qux'
  );
});
```

**5. maxKeys Limit** (10 tests)
```javascript
test('parse respects maxKeys limit', () => {
  const result = querystring.parse('a=1&b=2&c=3&d=4', null, null, { maxKeys: 2 });
  assert.strictEqual(Object.keys(result).length, 2);
});

test('parse uses default maxKeys of 1000', () => {
  // Create query string with 1001 parameters
  const qs = Array.from({ length: 1001 }, (_, i) => `k${i}=v${i}`).join('&');
  const result = querystring.parse(qs);
  assert.strictEqual(Object.keys(result).length, 1000);
});
```

**6. Unicode & Encoding** (20 tests)
```javascript
test('handles unicode correctly', () => {
  const str = 'emoji=🎉&chinese=你好';
  const encoded = querystring.stringify({ emoji: '🎉', chinese: '你好' });
  const decoded = querystring.parse(encoded);
  assert.deepStrictEqual(decoded, { emoji: '🎉', chinese: '你好' });
});

test('handles invalid UTF-8 gracefully', () => {
  // Should not crash, replace with U+FFFD
  const result = querystring.unescape('%FF%FE');
  assert.ok(result.includes('\uFFFD'));
});
```

**7. Type Coercion** (15 tests)
```javascript
test('stringify converts number to string', () => {
  assert.strictEqual(querystring.stringify({ num: 42 }), 'num=42');
});

test('stringify converts boolean to string', () => {
  assert.strictEqual(
    querystring.stringify({ bool: true }),
    'bool=true'
  );
});

test('stringify converts bigint to string', () => {
  assert.strictEqual(
    querystring.stringify({ big: 9007199254740991n }),
    'big=9007199254740991'
  );
});
```

**8. Round-Trip Testing** (10 tests)
```javascript
test('parse and stringify are inverses', () => {
  const obj = { foo: 'bar', baz: 'hello world', num: '123' };
  const encoded = querystring.stringify(obj);
  const decoded = querystring.parse(encoded);
  assert.deepStrictEqual(decoded, obj);
});
```

#### Memory Safety Tests
**Tool**: AddressSanitizer (ASAN)
**Command**: `make jsrt_m && ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m test/node/querystring/test_*.js`

**Success Criteria**:
- Zero memory leaks
- Zero buffer overflows
- Zero use-after-free errors

#### Compliance Tests
**WPT**: Run `make wpt` to ensure no regressions
**Baseline**: Maintain current WPT pass rate

---

## 🎯 Success Criteria

### Functional Requirements
- ✅ All 6 API methods implemented (parse, stringify, escape, unescape, decode, encode)
- ✅ Both CommonJS and ESM support
- ✅ Custom separator support (sep, eq parameters)
- ✅ maxKeys limit (default 1000)
- ✅ Array handling for duplicate keys
- ✅ Type coercion (number, boolean, null, undefined)

### Quality Requirements
- ✅ 100% unit test pass rate (150 tests)
- ✅ Zero memory leaks (ASAN clean)
- ✅ WPT baseline maintained
- ✅ Code properly formatted (`make format`)

### Performance Requirements
- ⚡ encode/decode: <1ms for typical query strings (<1KB)
- ⚡ parse/stringify: <5ms for 100 parameters
- ⚡ No performance regression vs baseline

### Compatibility Requirements
- ✅ Matches Node.js querystring behavior exactly
- ✅ Encoding uses `+` for space (not `%20`)
- ✅ Handles all Node.js edge cases correctly
- ✅ maxKeys default of 1000

---

## 📈 Dependency Graph

```
Phase 1: Foundation
  [1.1 escape/unescape] → [1.2 aliases] → [1.3 module registration] → [1.4 basic tests]
         ↓
Phase 2: Core Functions
  [2.1 parse basic] → [2.2 custom separators]
         ↓
  [2.3 stringify]
         ↓
Phase 3: Testing (Parallel)
  [3.1 edge cases] [3.2 compatibility] [3.3 ESM/CJS]
         ↓              ↓                    ↓
         └──────────────┴────────────────────┘
                        ↓
Phase 4: Cleanup (Parallel)
  [4.1 code cleanup] [4.2 documentation]
```

### Critical Path
`1.1 → 1.2 → 1.3 → 2.1 → 2.2 → 2.3 → 3.* → 4.*`

**Estimated Total Duration**: 20 hours

### Parallel Execution Opportunities
- Phase 3: All testing tasks can run in parallel
- Phase 4: Code cleanup and documentation can run in parallel

---

## 💡 Implementation Tips

### Code Reuse Examples

#### Escape/Unescape (Trivial Wrappers)
```c
// querystring.escape() - direct wrapper
static JSValue js_querystring_escape(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
  if (argc < 1) return JS_NewString(ctx, "");

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) return JS_EXCEPTION;

  char* encoded = url_encode(str);  // ✅ Reuse from src/url/
  JS_FreeCString(ctx, str);

  if (!encoded) return JS_ThrowOutOfMemory(ctx);

  JSValue result = JS_NewString(ctx, encoded);
  free(encoded);
  return result;
}

// querystring.unescape() - direct wrapper
static JSValue js_querystring_unescape(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
  if (argc < 1) return JS_NewString(ctx, "");

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) return JS_EXCEPTION;

  size_t len = strlen(str);
  size_t out_len;
  char* decoded = url_decode_query_with_length_and_output_len(str, len, &out_len);  // ✅ Reuse
  JS_FreeCString(ctx, str);

  if (!decoded) return JS_ThrowOutOfMemory(ctx);

  JSValue result = JS_NewStringLen(ctx, decoded, out_len);
  free(decoded);
  return result;
}
```

#### Parse Logic (Adapted from URLSearchParams)
```c
// Adapted from JSRT_ParseSearchParams() in url_search_params_parser.c
static JSValue js_querystring_parse(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
  // Extract arguments
  const char* str = argc > 0 ? JS_ToCString(ctx, argv[0]) : "";
  const char* sep = argc > 1 && !JS_IsNull(argv[1]) ? JS_ToCString(ctx, argv[1]) : "&";
  const char* eq = argc > 2 && !JS_IsNull(argv[2]) ? JS_ToCString(ctx, argv[2]) : "=";

  // Extract maxKeys from options
  int max_keys = 1000;
  if (argc > 3 && JS_IsObject(argv[3])) {
    JSValue mk_val = JS_GetPropertyStr(ctx, argv[3], "maxKeys");
    if (!JS_IsUndefined(mk_val)) {
      JS_ToInt32(ctx, &max_keys, mk_val);
    }
    JS_FreeValue(ctx, mk_val);
  }

  JSValue result = JS_NewObject(ctx);

  // Skip leading separator
  size_t str_len = strlen(str);
  const char* start = str;
  if (str_len > 0 && *start == *sep) {
    start++;
    str_len--;
  }

  // Parse loop (adapted from JSRT_ParseSearchParams)
  size_t i = 0;
  int key_count = 0;

  while (i < str_len && key_count < max_keys) {
    // Find next separator
    size_t param_start = i;
    while (i < str_len && start[i] != *sep) {
      i++;
    }
    size_t param_len = i - param_start;

    if (param_len > 0) {
      // Find equals sign
      size_t eq_pos = param_start;
      while (eq_pos < param_start + param_len && start[eq_pos] != *eq) {
        eq_pos++;
      }

      // Extract key and value
      size_t key_len = eq_pos - param_start;
      size_t val_len = (eq_pos < param_start + param_len)
                       ? param_start + param_len - eq_pos - 1
                       : 0;

      // Decode key and value using existing functions
      size_t decoded_key_len, decoded_val_len;
      char* key = url_decode_query_with_length_and_output_len(
        start + param_start, key_len, &decoded_key_len);
      char* val = (val_len > 0)
                  ? url_decode_query_with_length_and_output_len(
                      start + eq_pos + 1, val_len, &decoded_val_len)
                  : strdup("");

      if (key && val) {
        // Check if key already exists
        JSValue existing = JS_GetPropertyStr(ctx, result, key);

        if (JS_IsUndefined(existing)) {
          // First occurrence - set as string
          JS_SetPropertyStr(ctx, result, key, JS_NewString(ctx, val));
        } else if (JS_IsArray(ctx, existing)) {
          // Already array - append
          JSValue val_js = JS_NewString(ctx, val);
          JS_SetPropertyUint32(ctx, existing,
                               JS_GetPropertyStr(ctx, existing, "length"),
                               val_js);
        } else {
          // Second occurrence - convert to array
          JSValue arr = JS_NewArray(ctx);
          JS_SetPropertyUint32(ctx, arr, 0, existing);
          JS_SetPropertyUint32(ctx, arr, 1, JS_NewString(ctx, val));
          JS_SetPropertyStr(ctx, result, key, arr);
        }

        JS_FreeValue(ctx, existing);
        key_count++;
      }

      free(key);
      free(val);
    }

    // Move past separator
    if (i < str_len) i++;
  }

  // Cleanup
  if (argc > 0) JS_FreeCString(ctx, str);
  if (argc > 1 && !JS_IsNull(argv[1])) JS_FreeCString(ctx, sep);
  if (argc > 2 && !JS_IsNull(argv[2])) JS_FreeCString(ctx, eq);

  return result;
}
```

### Memory Management Pattern
```c
// Always follow this pattern
char* c_string = url_encode(input);
if (!c_string) {
  return JS_ThrowOutOfMemory(ctx);
}

JSValue js_value = JS_NewString(ctx, c_string);
free(c_string);  // ✅ Free C string immediately

return js_value;
```

---

## 📚 References

### Node.js Documentation
- **Official Docs**: https://nodejs.org/api/querystring.html
- **Stability**: 2 - Stable (Legacy API)

### jsrt Resources
- **URL Implementation**: `/home/lei/work/jsrt/src/url/`
- **Encoding Functions**: `/home/lei/work/jsrt/src/url/encoding/url_basic_encoding.c`
- **URLSearchParams**: `/home/lei/work/jsrt/src/url/search_params/`
- **Build Commands**: `make`, `make test`, `make format`

### Related Standards
- **WHATWG URL Standard**: https://url.spec.whatwg.org/
- **RFC 3986**: URI Generic Syntax
- **application/x-www-form-urlencoded**: HTML5 spec

---

## 🔄 Progress Tracking

### Phase 1: Foundation ⏳ NOT STARTED
| Task | Status | Start | Completion | Notes |
|------|--------|-------|------------|-------|
| 1.1 escape/unescape | ⏳ PENDING | - | - | - |
| 1.2 aliases | ⏳ PENDING | - | - | - |
| 1.3 module registration | ⏳ PENDING | - | - | - |
| 1.4 basic tests | ⏳ PENDING | - | - | - |

### Phase 2: Core Functions ⏳ NOT STARTED
| Task | Status | Start | Completion | Notes |
|------|--------|-------|------------|-------|
| 2.1 parse basic | ⏳ PENDING | - | - | - |
| 2.2 custom separators | ⏳ PENDING | - | - | - |
| 2.3 stringify | ⏳ PENDING | - | - | - |

### Phase 3: Testing ⏳ NOT STARTED
| Task | Status | Start | Completion | Notes |
|------|--------|-------|------------|-------|
| 3.1 edge cases | ⏳ PENDING | - | - | - |
| 3.2 compatibility | ⏳ PENDING | - | - | - |
| 3.3 ESM/CJS | ⏳ PENDING | - | - | - |

### Phase 4: Cleanup ⏳ NOT STARTED
| Task | Status | Start | Completion | Notes |
|------|--------|-------|------------|-------|
| 4.1 code cleanup | ⏳ PENDING | - | - | - |
| 4.2 documentation | ⏳ PENDING | - | - | - |

---

## 📊 Metrics

### Lines of Code Estimate
- **node_querystring.c**: ~400 lines
- **Test files**: ~800 lines (8 files × 100 lines)
- **Total new code**: ~1200 lines
- **Code reused**: ~200 lines from src/url/

### API Coverage
- **Target**: 6 methods (parse, stringify, escape, unescape, encode, decode)
- **Implemented**: 0/6 (0%)
- **Tested**: 0/6 (0%)

### Test Coverage
- **Target**: 150 tests
- **Written**: 0/150 (0%)
- **Passing**: 0/150 (0%)

---

## 🎉 Completion Criteria

This implementation will be considered **COMPLETE** when:

1. ✅ All 6 API methods implemented and working
2. ✅ 150+ tests written and passing (100% pass rate)
3. ✅ Both CommonJS and ESM support verified
4. ✅ Zero memory leaks (ASAN validation)
5. ✅ WPT baseline maintained (no regressions)
6. ✅ Code properly formatted (`make format`)
7. ✅ All builds pass (`make test && make wpt && make clean && make`)
8. ✅ Documentation updated

---

**End of Plan Document**
