---
Created: 2025-10-07T00:00:00Z
Last Updated: 2025-10-07T00:00:00Z
Status: 📋 PLANNING - Ready for Implementation
Overall Progress: 0/86 tasks (0%)
API Coverage: Basic implementation started - needs enhancement
Current Phase: Phase 1 - Analysis
---

# Node.js util Module Comprehensive Implementation Plan

## 📋 Executive Summary

### Objective
Enhance and complete the Node.js-compatible `node:util` module in jsrt to provide full utility function coverage including string formatting, object inspection, type checking, async utilities, deprecation, and text encoding.

### Current Status
- ✅ **Basic util module exists** - `src/node/node_util.c` with 10 functions
- ✅ **format()** - Implemented with %s, %d, %j placeholders
- ✅ **inspect()** - Basic JSON.stringify wrapper
- ✅ **Type checkers** - isArray, isObject, isString, etc. (8 functions)
- ✅ **promisify()** - Stub implementation (needs completion)
- ✅ **TextEncoder/TextDecoder** - Fully implemented in `src/std/encoding.c`
- ❌ **formatWithOptions()** - Not implemented
- ❌ **util.types.*** - Not implemented (modern type checking API)
- ❌ **callbackify()** - Not implemented
- ❌ **deprecate()** - Not implemented
- ❌ **debuglog()** - Not implemented
- ❌ **inherits()** - Not implemented
- ❌ **isDeepStrictEqual()** - Not implemented
- ❌ **inspect() enhancements** - No options, colors, depth, circular detection

### Key Success Factors
1. **Maximum Code Reuse**: Leverage existing encoding.c, console.c formatting
2. **Priority Implementation**: Focus on most-used APIs first
3. **Memory Safety**: Proper QuickJS memory management patterns
4. **Node.js Compatibility**: Match Node.js behavior exactly
5. **Test Coverage**: 150+ comprehensive tests

### Implementation Strategy
- **Phase 1**: TextEncoder/TextDecoder export (2 hours)
- **Phase 2**: Enhanced format() with %i, %f, %o, %O placeholders (4 hours)
- **Phase 3**: formatWithOptions() (2 hours)
- **Phase 4**: Enhanced inspect() with options (10 hours)
- **Phase 5**: util.types.* API (6 hours)
- **Phase 6**: promisify() and callbackify() (8 hours)
- **Phase 7**: deprecate() and debuglog() (4 hours)
- **Phase 8**: Additional utilities (inherits, isDeepStrictEqual) (4 hours)
- **Phase 9**: Testing and edge cases (8 hours)
- **Phase 10**: Documentation and cleanup (2 hours)
- **Total Estimated Time**: 50 hours

---

## 🔍 Current Implementation Analysis

### Existing Infrastructure

#### 1. node_util.c (Current State)
**File**: `src/node/node_util.c` (~354 lines)

**Implemented Functions**:
```c
✅ js_util_format(ctx, argv)              // Lines 8-165
   - Supports: %s (string), %d (number), %j (JSON), %% (escape)
   - Missing: %i, %f, %o, %O placeholders
   - Missing: Options parameter for formatWithOptions

✅ js_util_inspect(ctx, argv)             // Lines 168-193
   - Basic: Uses JSON.stringify with 2-space indent
   - Missing: Custom options (colors, depth, showHidden, etc.)
   - Missing: Circular reference detection
   - Missing: Special handling for functions, symbols, etc.

✅ js_util_is_array()                     // Lines 196-201
✅ js_util_is_object()                    // Lines 204-211
✅ js_util_is_string()                    // Lines 214-219
✅ js_util_is_number()                    // Lines 222-227
✅ js_util_is_boolean()                   // Lines 230-235
✅ js_util_is_function()                  // Lines 238-243
✅ js_util_is_null()                      // Lines 246-251
✅ js_util_is_undefined()                 // Lines 254-259

⚠️  js_util_promisify()                   // Lines 262-274
   - Stub: Returns original function
   - Needs: Full Promise wrapper implementation

✅ JSRT_InitNodeUtil()                    // Lines 277-298
   - Exports all functions above
   - Missing: util.types namespace
   - Missing: TextEncoder/TextDecoder exports

✅ js_node_util_init()                    // Lines 301-354
   - ES module exports
   - Needs: Additional exports for new APIs
```

#### 2. encoding.c (Reusable Assets)
**File**: `src/std/encoding.c` (~1527 lines)

**Available for Export**:
```c
✅ JSRT_TextEncoderClassID                // Line 538
✅ JSRT_TextDecoderClassID                // Line 539
✅ JSRT_TextEncoderConstructor()          // Lines 558-581
✅ JSRT_TextDecoderConstructor()          // Lines 983-1050
✅ JSRT_TextEncoderEncode()               // Lines 689-832
✅ JSRT_TextEncoderEncodeInto()           // Lines 834-962
✅ JSRT_TextDecoderDecode()               // Lines 1052-1489
✅ JSRT_RuntimeSetupStdEncoding()         // Lines 1492-1522
```

**Key Insight**: TextEncoder/TextDecoder are already fully implemented and WPT-compliant. Just need to export them from util module.

#### 3. console.c (Reusable Patterns)
**File**: `src/std/console.c`

**Reusable Functions**:
```c
✅ JSRT_GetJSValuePrettyString(dbuf, ctx, val, options, colors)
   - Used for console.log formatting
   - Can be adapted for util.inspect()
   - Already handles objects, arrays, colors

✅ jsrt_console_output()                  // Lines 144-177
   - Formatting with colors and indentation
   - Pattern for inspect() color support
```

#### 4. QuickJS Type Checking APIs
**Available from QuickJS**:
```c
✅ JS_IsArray(ctx, val)
✅ JS_IsString(val)
✅ JS_IsNumber(val)
✅ JS_IsBool(val)
✅ JS_IsNull(val)
✅ JS_IsUndefined(val)
✅ JS_IsObject(val)
✅ JS_IsFunction(ctx, val)
✅ JS_IsError(ctx, val)
✅ JS_IsException(val)
✅ JS_IsConstructor(ctx, val)

// Additional checks needed:
JS_TAG_INT              // Internal tag checking
JS_TAG_BIG_INT
JS_TAG_SYMBOL
JS_TAG_FLOAT64
JS_TAG_OBJECT           // Check subtype for Date, RegExp, etc.
```

### What Needs to be Built

**Enhanced/New in**: `src/node/node_util.c`

```c
// Phase 2: Enhanced format()
❌ format() enhancements
   - Add %i (integer) placeholder
   - Add %f (float) placeholder
   - Add %o (object with options) placeholder
   - Add %O (object with options, break lines) placeholder
   - Better error handling

// Phase 3: formatWithOptions()
❌ js_util_format_with_options(ctx, argv)
   - First arg: options object { colors, depth, ... }
   - Rest: same as format()

// Phase 4: Enhanced inspect()
❌ js_util_inspect_enhanced(ctx, argv)
   - Parse options: { colors, depth, showHidden, maxArrayLength, ... }
   - Circular reference detection (visited set)
   - Custom formatting for:
     * Functions (show source or [Function: name])
     * Symbols
     * Dates (show value)
     * RegExp (show pattern)
     * Errors (show message)
     * TypedArrays
     * Maps/Sets
     * Promises (show state)
   - Color support (when colors: true)
   - Depth limiting
   - Array truncation (maxArrayLength)

// Phase 5: util.types.* namespace
❌ js_util_types_is_promise(ctx, argv)
❌ js_util_types_is_date(ctx, argv)
❌ js_util_types_is_regexp(ctx, argv)
❌ js_util_types_is_map(ctx, argv)
❌ js_util_types_is_set(ctx, argv)
❌ js_util_types_is_typed_array(ctx, argv)
❌ js_util_types_is_uint8_array(ctx, argv)
❌ js_util_types_is_int8_array(ctx, argv)
❌ js_util_types_is_uint16_array(ctx, argv)
// ... etc for all typed array types
❌ js_util_types_is_array_buffer(ctx, argv)
❌ js_util_types_is_shared_array_buffer(ctx, argv)
❌ js_util_types_is_data_view(ctx, argv)
❌ js_util_types_is_big_int(ctx, argv)
❌ js_util_types_is_symbol(ctx, argv)
❌ js_util_types_is_weak_map(ctx, argv)
❌ js_util_types_is_weak_set(ctx, argv)
❌ js_util_types_is_proxy(ctx, argv)
❌ js_util_types_is_native_error(ctx, argv)

// Phase 6: Async utilities
❌ js_util_promisify_full(ctx, argv)
   - Wrap callback function in Promise
   - Handle (err, result) callback pattern
   - Support custom symbols
   - Error handling

❌ js_util_callbackify(ctx, argv)
   - Convert Promise-returning function to callback style
   - Handle promise resolution/rejection
   - Call callback(err, result)

// Phase 7: Deprecation and debugging
❌ js_util_deprecate(ctx, argv)
   - Wrap function to show deprecation warning
   - Track shown warnings (only show once)
   - Support deprecation codes (DEP0001, etc.)
   - Use process.emitWarning() if available

❌ js_util_debuglog(ctx, argv)
   - Create conditional logger based on NODE_DEBUG env
   - Return function that logs if section enabled
   - Support callback for expensive operations

// Phase 8: Additional utilities
❌ js_util_inherits(ctx, argv)
   - Set up prototype chain
   - Deprecated in favor of ES6 classes
   - Still needed for legacy compatibility

❌ js_util_is_deep_strict_equal(ctx, argv)
   - Deep equality comparison
   - Strict mode (like assert.deepStrictEqual)
   - Handle circular references
   - Type-aware comparison

// Helper functions
❌ js_util_get_constructor_name(ctx, val)
❌ js_util_detect_circular_refs(ctx, val, visited)
❌ js_util_format_function(ctx, val, options)
❌ js_util_format_error(ctx, val, options)
❌ js_util_apply_colors(str, type)

// Module initialization updates
❌ Update JSRT_InitNodeUtil() to include:
   - util.types namespace object
   - TextEncoder/TextDecoder exports
   - All new functions
   - Proper CommonJS exports

❌ Update js_node_util_init() to include:
   - ES module exports for new APIs
   - Named exports for TextEncoder/TextDecoder
```

---

## 📊 Complete API Specification

### Priority 1: Essential APIs (Must Have)

#### 1. util.format(format[, ...args])
**Current**: Implemented with %s, %d, %j, %%
**Enhancement Needed**: Add %i, %f, %o, %O

**Node.js Signature**: `format(format: string, ...args: any[]): string`

**Behavior**:
```javascript
util.format('Hello %s', 'world')
// Returns: 'Hello world'

util.format('%d + %d = %d', 1, 2, 3)
// Returns: '1 + 2 = 3'

util.format('%i', 42.7)
// Returns: '42' (integer)

util.format('%f', 42.7)
// Returns: '42.7' (float)

util.format('%j', {foo: 'bar'})
// Returns: '{"foo":"bar"}' (JSON)

util.format('%o', {foo: 'bar'})
// Returns: "{ foo: 'bar' }" (object inspect, single line)

util.format('%O', {foo: 'bar'})
// Returns: "{\n  foo: 'bar'\n}" (object inspect, multi-line)

util.format('Extra %s args', 'are', 'appended')
// Returns: 'Extra are args appended'

util.format('100%')
// Returns: '100%' (no replacement)

util.format('%% escapes')
// Returns: '% escapes'
```

**Placeholder Types**:
- `%s` - String (String(arg))
- `%d` - Number (Number(arg), decimal)
- `%i` - Integer (parseInt(arg, 10))
- `%f` - Float (parseFloat(arg))
- `%j` - JSON (JSON.stringify(arg))
- `%o` - Object (util.inspect(arg), single line)
- `%O` - Object (util.inspect(arg), multi-line)
- `%%` - Literal % sign

**Edge Cases**:
- Missing args → placeholder becomes literal
- Extra args → appended with spaces
- Non-string format → coerce to string
- Circular objects in %j → [Circular]
- Invalid placeholders → kept as-is

**Implementation Notes**:
- Enhance existing js_util_format()
- Add support for %i, %f, %o, %O
- Use inspect() for %o and %O
- Better error handling for invalid JSON

---

#### 2. util.formatWithOptions(inspectOptions, format[, ...args])
**Current**: Not implemented
**Priority**: HIGH

**Node.js Signature**: `formatWithOptions(inspectOptions: object, format: string, ...args: any[]): string`

**Behavior**:
```javascript
util.formatWithOptions({ colors: true }, 'Hello %s', 'world')
// Returns: 'Hello world' (with ANSI colors if supported)

util.formatWithOptions({ depth: 1 }, '%O', {a: {b: {c: 1}}})
// Returns: '{ a: { b: [Object] } }' (depth limited)

util.formatWithOptions({ maxArrayLength: 2 }, '%o', [1,2,3,4,5])
// Returns: '[ 1, 2, ... 3 more items ]'
```

**inspectOptions Object**:
- `colors` (boolean): Use ANSI colors
- `depth` (number): Recursion depth (default: 2)
- `showHidden` (boolean): Show non-enumerable properties
- `maxArrayLength` (number): Max array elements (default: 100)
- `breakLength` (number): Line break threshold (default: 60)
- `compact` (boolean|number): Compact formatting

**Implementation Notes**:
- Similar to format() but parse options first
- Pass options to inspect() calls
- Validate options object

---

#### 3. util.inspect(object[, options])
**Current**: Basic JSON.stringify wrapper
**Enhancement Needed**: Full inspect implementation

**Node.js Signature**: `inspect(object: any, options?: InspectOptions): string`

**InspectOptions Interface**:
```typescript
interface InspectOptions {
  colors?: boolean;          // Use ANSI colors
  depth?: number | null;     // Recursion depth (default: 2, null = infinite)
  showHidden?: boolean;      // Show non-enumerable (default: false)
  showProxy?: boolean;       // Show Proxy target (default: false)
  maxArrayLength?: number;   // Max array elements (default: 100)
  maxStringLength?: number;  // Max string length (default: 10000)
  breakLength?: number;      // Line break threshold (default: 80)
  compact?: boolean | number; // Compact mode (default: 3)
  sorted?: boolean | Function; // Sort object keys
  getters?: boolean | 'get' | 'set'; // Show getters/setters
  numericSeparator?: boolean; // Use _ separator in numbers (default: false)
}
```

**Behavior**:
```javascript
util.inspect({foo: 'bar'})
// Returns: "{ foo: 'bar' }"

util.inspect({foo: 'bar'}, {colors: true})
// Returns: "{ foo: \x1b[32m'bar'\x1b[39m }" (ANSI colored)

util.inspect({a: {b: {c: {d: 1}}}}, {depth: 2})
// Returns: "{ a: { b: { c: [Object] } } }"

util.inspect([1, 2, 3, 4, 5], {maxArrayLength: 2})
// Returns: "[ 1, 2, ... 3 more items ]"

function myFunc() {}
util.inspect(myFunc)
// Returns: "[Function: myFunc]"

util.inspect(new Date('2025-01-01'))
// Returns: "2025-01-01T00:00:00.000Z"

util.inspect(/abc/gi)
// Returns: "/abc/gi"

util.inspect(Symbol('test'))
// Returns: "Symbol(test)"

const circular = {a: 1};
circular.self = circular;
util.inspect(circular)
// Returns: "<ref *1> { a: 1, self: [Circular *1] }"
```

**Type-Specific Formatting**:
- **Strings**: Single quotes, escaped special chars
- **Numbers**: Decimal notation, NaN, Infinity
- **BigInt**: Number + 'n' suffix
- **Boolean**: 'true' or 'false'
- **null**: 'null'
- **undefined**: 'undefined'
- **Symbol**: 'Symbol(description)'
- **Function**: '[Function: name]' or '[Function (anonymous)]'
- **Date**: ISO string
- **RegExp**: Source with flags
- **Error**: 'Error: message'
- **Arrays**: `[ elem1, elem2, ... ]`
- **Objects**: `{ key: value, ... }`
- **Map**: `Map(2) { key1 => value1, key2 => value2 }`
- **Set**: `Set(3) { value1, value2, value3 }`
- **TypedArray**: `Uint8Array(3) [ 1, 2, 3 ]`
- **ArrayBuffer**: `ArrayBuffer { [Uint8Contents]: <01 02 03>, byteLength: 3 }`
- **Promise**: `Promise { <pending> }` or `Promise { <fulfilled>: value }`
- **Proxy**: `Proxy [ target, handler ]`
- **WeakMap/WeakSet**: `WeakMap { <items unknown> }`

**Color Scheme** (ANSI codes):
- String: green (32)
- Number: yellow (33)
- Boolean: yellow (33)
- null: bold (1)
- undefined: dim (2)
- Symbol: green (32)
- Date: magenta (35)
- RegExp: red (31)
- Special: cyan (36)

**Circular Reference Handling**:
```javascript
const obj = {a: 1};
obj.self = obj;
util.inspect(obj)
// Returns: "<ref *1> { a: 1, self: [Circular *1] }"

const a = {}, b = {};
a.b = b;
b.a = a;
util.inspect({a, b})
// Returns: "{ a: <ref *1> { b: { a: [Circular *1] } }, b: <ref *2> { a: [Circular *2] } }"
```

**Implementation Strategy**:
1. Create visited Set to track circular references
2. Implement recursive descent with depth tracking
3. Type detection and custom formatting per type
4. Apply options (colors, depth, maxArrayLength, etc.)
5. Handle special constructors (Date, RegExp, Error, etc.)
6. Line breaking and indentation logic
7. Property enumeration (own, enumerable, hidden)

**Implementation Complexity**: COMPLEX (10 hours)

---

#### 4. util.types.* Type Checking API
**Current**: Not implemented (legacy is* functions exist)
**Priority**: HIGH (modern API)

**Node.js API**: `util.types` namespace with 30+ type checkers

**Complete Types List**:
```javascript
// Primitives & Wrappers
util.types.isExternal(value)           // Native external value
util.types.isAnyArrayBuffer(value)     // ArrayBuffer or SharedArrayBuffer
util.types.isArrayBuffer(value)        // ArrayBuffer
util.types.isArgumentsObject(value)    // arguments object
util.types.isAsyncFunction(value)      // async function
util.types.isBigIntObject(value)       // new BigInt()
util.types.isBooleanObject(value)      // new Boolean()
util.types.isBoxedPrimitive(value)     // String/Number/Boolean/Symbol/BigInt object
util.types.isDataView(value)           // DataView
util.types.isDate(value)               // Date object
util.types.isGeneratorFunction(value)  // function*
util.types.isGeneratorObject(value)    // Generator instance
util.types.isMap(value)                // Map
util.types.isMapIterator(value)        // Map iterator
util.types.isModuleNamespaceObject(value) // import * as ns
util.types.isNativeError(value)        // Error, TypeError, etc.
util.types.isNumberObject(value)       // new Number()
util.types.isPromise(value)            // Promise
util.types.isProxy(value)              // Proxy
util.types.isRegExp(value)             // RegExp
util.types.isSet(value)                // Set
util.types.isSetIterator(value)        // Set iterator
util.types.isSharedArrayBuffer(value)  // SharedArrayBuffer
util.types.isStringObject(value)       // new String()
util.types.isSymbolObject(value)       // Object(Symbol())
util.types.isTypedArray(value)         // Any TypedArray
util.types.isUint8Array(value)         // Uint8Array
util.types.isUint8ClampedArray(value)  // Uint8ClampedArray
util.types.isUint16Array(value)        // Uint16Array
util.types.isUint32Array(value)        // Uint32Array
util.types.isInt8Array(value)          // Int8Array
util.types.isInt16Array(value)         // Int16Array
util.types.isInt32Array(value)         // Int32Array
util.types.isFloat32Array(value)       // Float32Array
util.types.isFloat64Array(value)       // Float64Array
util.types.isBigInt64Array(value)      // BigInt64Array
util.types.isBigUint64Array(value)     // BigUint64Array
util.types.isWeakMap(value)            // WeakMap
util.types.isWeakSet(value)            // WeakSet
util.types.isKeyObject(value)          // KeyObject (crypto)
util.types.isCryptoKey(value)          // CryptoKey (Web Crypto)
```

**Example Usage**:
```javascript
util.types.isDate(new Date())          // true
util.types.isDate({})                  // false

util.types.isPromise(Promise.resolve()) // true
util.types.isPromise({then: () => {}}) // false (not a real Promise)

util.types.isRegExp(/abc/)             // true
util.types.isRegExp(new RegExp('abc')) // true

util.types.isTypedArray(new Uint8Array()) // true
util.types.isUint8Array(new Uint8Array()) // true
util.types.isUint16Array(new Uint8Array()) // false

util.types.isNativeError(new Error())      // true
util.types.isNativeError(new TypeError())  // true
util.types.isNativeError({message: 'x'})   // false
```

**Implementation Strategy**:
1. Use QuickJS internal tag system (JS_TAG_*)
2. Check object constructors via JS_GetPropertyStr(ctx, global, "Date") etc.
3. Use JS_IsInstanceOf() for class checks
4. Special handling for typed arrays (check byteLength property)
5. Create util.types namespace object with all checkers

**QuickJS Type Detection Patterns**:
```c
// Date detection
JSValue date_ctor = JS_GetPropertyStr(ctx, global, "Date");
int is_date = JS_IsInstanceOf(ctx, val, date_ctor);

// RegExp detection
JSValue regexp_ctor = JS_GetPropertyStr(ctx, global, "RegExp");
int is_regexp = JS_IsInstanceOf(ctx, val, regexp_ctor);

// Promise detection
JSValue promise_ctor = JS_GetPropertyStr(ctx, global, "Promise");
int is_promise = JS_IsInstanceOf(ctx, val, promise_ctor);

// TypedArray detection
JSValue uint8array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
int is_uint8 = JS_IsInstanceOf(ctx, val, uint8array_ctor);

// Map/Set detection
JSValue map_ctor = JS_GetPropertyStr(ctx, global, "Map");
int is_map = JS_IsInstanceOf(ctx, val, map_ctor);

// BigInt primitive (not object)
int is_bigint = (JS_VALUE_GET_TAG(val) == JS_TAG_BIG_INT);

// Symbol primitive
int is_symbol = (JS_VALUE_GET_TAG(val) == JS_TAG_SYMBOL);
```

**Implementation Complexity**: MEDIUM (6 hours)

---

#### 5. util.TextEncoder / util.TextDecoder
**Current**: Fully implemented in encoding.c, not exported from util
**Priority**: HIGH (easy win)

**Node.js Signature**:
```javascript
new util.TextEncoder()
new util.TextDecoder([encoding[, options]])
```

**Behavior**:
```javascript
const encoder = new util.TextEncoder();
const bytes = encoder.encode('hello');
// Returns: Uint8Array(5) [ 104, 101, 108, 108, 111 ]

const decoder = new util.TextDecoder();
const text = decoder.decode(bytes);
// Returns: 'hello'
```

**Implementation**:
- Import from encoding.c
- Re-export in util module
- Add to both CommonJS and ES module exports

**Implementation Complexity**: TRIVIAL (1 hour)

---

### Priority 2: Common APIs (Should Have)

#### 6. util.promisify(original)
**Current**: Stub implementation
**Priority**: HIGH

**Node.js Signature**: `promisify<T>(fn: (...args: any[], callback: (err: Error, result: T) => void) => void): (...args: any[]) => Promise<T>`

**Behavior**:
```javascript
const fs = require('fs');
const readFilePromise = util.promisify(fs.readFile);

// Original callback style:
fs.readFile('file.txt', 'utf8', (err, data) => {
  if (err) throw err;
  console.log(data);
});

// Promisified:
readFilePromise('file.txt', 'utf8')
  .then(data => console.log(data))
  .catch(err => console.error(err));

// Or with async/await:
async function readIt() {
  const data = await readFilePromise('file.txt', 'utf8');
  console.log(data);
}
```

**Custom Promisify Support**:
```javascript
// Functions can define custom promisify behavior
function myFunc(arg, callback) {
  callback(null, 'result: ' + arg);
}

// Add custom promisify symbol
myFunc[util.promisify.custom] = function(arg) {
  return Promise.resolve('custom: ' + arg);
};

const promisified = util.promisify(myFunc);
// Will use the custom version
```

**Error Handling**:
```javascript
function errorFunc(callback) {
  callback(new Error('Something failed'));
}

const promisified = util.promisify(errorFunc);
promisified().catch(err => {
  console.error(err.message); // 'Something failed'
});
```

**Implementation Strategy**:
1. Check for util.promisify.custom symbol on function
2. If exists, return that function
3. Otherwise, create wrapper:
   - Return new Promise
   - Call original with custom callback
   - Callback resolves/rejects promise
   - Handle (err, result) pattern
4. Preserve function name and length

**Implementation Complexity**: MEDIUM (4 hours)

---

#### 7. util.callbackify(original)
**Current**: Not implemented
**Priority**: MEDIUM

**Node.js Signature**: `callbackify<T>(fn: (...args: any[]) => Promise<T>): (...args: any[], callback: (err: Error, result: T) => void) => void`

**Behavior**:
```javascript
async function fetchData(url) {
  const response = await fetch(url);
  return response.json();
}

const callbackFetch = util.callbackify(fetchData);

callbackFetch('https://api.example.com', (err, data) => {
  if (err) throw err;
  console.log(data);
});
```

**Error Handling**:
```javascript
async function mayFail() {
  throw new Error('Failed!');
}

const callbackVersion = util.callbackify(mayFail);
callbackVersion((err, result) => {
  console.error(err.message); // 'Failed!'
  console.log(result);        // undefined
});
```

**Special Case - Rejected with non-Error**:
```javascript
async function rejectsWithValue() {
  throw 'not an error object';
}

const cb = util.callbackify(rejectsWithValue);
cb((err) => {
  console.log(err instanceof Error);     // true
  console.log(err.message);              // 'Promise was rejected with a falsy value'
  console.log(err.reason);               // 'not an error object'
});
```

**Implementation Strategy**:
1. Return function that accepts callback as last argument
2. Call original async function
3. Attach .then() handler → callback(null, result)
4. Attach .catch() handler → callback(err)
5. Handle non-Error rejections (wrap in ERR_FALSY_VALUE_REJECTION)
6. Preserve function name

**Implementation Complexity**: MEDIUM (4 hours)

---

#### 8. util.deprecate(fn, msg[, code])
**Current**: Not implemented
**Priority**: MEDIUM

**Node.js Signature**: `deprecate<T extends Function>(fn: T, msg: string, code?: string): T`

**Behavior**:
```javascript
const oldFunction = util.deprecate(() => {
  console.log('doing work');
}, 'oldFunction() is deprecated. Use newFunction() instead', 'DEP0001');

oldFunction();
// Prints: [DEP0001] DeprecationWarning: oldFunction() is deprecated. Use newFunction() instead
// (printed only once per code)
// Then executes: doing work
```

**Warning Tracking**:
```javascript
const deprecated = util.deprecate(() => {}, 'This is deprecated', 'DEP0001');

deprecated(); // Shows warning
deprecated(); // No warning (already shown)
deprecated(); // No warning

// Different deprecation code
const deprecated2 = util.deprecate(() => {}, 'Other deprecation', 'DEP0002');
deprecated2(); // Shows warning (different code)
```

**Environment Variable Control**:
```bash
# Disable all deprecation warnings
NODE_NO_WARNINGS=1 node script.js

# Throw on deprecation warnings
node --throw-deprecation script.js

# Trace deprecation warnings
node --trace-deprecation script.js
```

**Implementation Strategy**:
1. Create wrapper function
2. Track shown warnings in static map (code → shown boolean)
3. On first call with code, emit warning
4. Check process.noDeprecation flag
5. Check process.throwDeprecation flag
6. Use process.emitWarning() if available, else console.warn()
7. Preserve original function properties

**Implementation Complexity**: MEDIUM (3 hours)

---

#### 9. util.debuglog(section[, callback])
**Current**: Not implemented
**Priority**: MEDIUM

**Node.js Signature**: `debuglog(section: string, callback?: () => void): (msg: string, ...args: any[]) => void`

**Behavior**:
```javascript
const debuglog = util.debuglog('http');

debuglog('Starting server on port %d', 8080);
// Only prints if NODE_DEBUG=http or NODE_DEBUG=* is set

// With callback for expensive operations
const debuglog2 = util.debuglog('database', (fn) => {
  fn = (msg, ...args) => {
    console.log('DATABASE:', util.format(msg, ...args));
  };
});
```

**Environment Variable**:
```bash
# Enable debug for specific section
NODE_DEBUG=http node script.js

# Enable multiple sections
NODE_DEBUG=http,database node script.js

# Enable all sections
NODE_DEBUG=* node script.js

# No debug output
node script.js
```

**Performance Consideration**:
```javascript
// Bad: computes expensive value even if debug disabled
debuglog('Result: %j', computeExpensiveResult());

// Good: only computes if debug enabled
if (debuglog.enabled) {
  debuglog('Result: %j', computeExpensiveResult());
}
```

**Implementation Strategy**:
1. Parse NODE_DEBUG environment variable at module init
2. Create set of enabled sections
3. Return function that checks if section enabled
4. If enabled, call util.format() and write to stderr
5. Add .enabled property to returned function
6. Support callback parameter for custom logging

**Implementation Complexity**: SIMPLE (2 hours)

---

### Priority 3: Nice-to-Have APIs

#### 10. util.inherits(constructor, superConstructor)
**Current**: Not implemented
**Priority**: LOW (legacy API)

**Node.js Signature**: `inherits(constructor: Function, superConstructor: Function): void`

**Behavior**:
```javascript
function Animal(name) {
  this.name = name;
}
Animal.prototype.speak = function() {
  console.log(this.name + ' makes a sound');
};

function Dog(name) {
  Animal.call(this, name);
}
util.inherits(Dog, Animal);

Dog.prototype.speak = function() {
  console.log(this.name + ' barks');
};

const dog = new Dog('Rex');
dog.speak(); // 'Rex barks'
console.log(dog instanceof Animal); // true
```

**Note**: Deprecated in favor of ES6 classes with `extends` keyword.

**Implementation Strategy**:
1. Set constructor.super_ = superConstructor
2. Set constructor.prototype = Object.create(superConstructor.prototype)
3. Set constructor.prototype.constructor = constructor

**Implementation Complexity**: SIMPLE (1 hour)

---

#### 11. util.isDeepStrictEqual(val1, val2)
**Current**: Not implemented
**Priority**: MEDIUM

**Node.js Signature**: `isDeepStrictEqual(val1: any, val2: any): boolean`

**Behavior**:
```javascript
util.isDeepStrictEqual({a: 1}, {a: 1})
// Returns: true

util.isDeepStrictEqual({a: 1}, {a: '1'})
// Returns: false (strict equality)

util.isDeepStrictEqual([1, 2, 3], [1, 2, 3])
// Returns: true

util.isDeepStrictEqual(new Date('2025-01-01'), new Date('2025-01-01'))
// Returns: true

util.isDeepStrictEqual(/abc/g, /abc/g)
// Returns: true

// Circular references
const a = {x: 1};
a.self = a;
const b = {x: 1};
b.self = b;
util.isDeepStrictEqual(a, b)
// Returns: true
```

**Comparison Rules**:
- Primitives: === comparison
- Objects: Same constructor, same keys, same values (recursive)
- Arrays: Same length, same elements (recursive)
- Dates: Same getTime()
- RegExp: Same source and flags
- Maps/Sets: Same size, same entries
- TypedArrays: Same type, same values
- Circular: Track visited objects

**Implementation Strategy**:
1. Fast path for primitives (===)
2. Type checking (same constructor)
3. Recursive comparison with visited tracking
4. Special cases for Date, RegExp, Map, Set, TypedArray
5. Object key comparison (own enumerable properties)
6. Array element comparison

**Implementation Complexity**: COMPLEX (4 hours)

---

#### 12. util.getSystemErrorName(err)
**Current**: Not implemented
**Priority**: LOW

**Node.js Signature**: `getSystemErrorName(err: number): string`

**Behavior**:
```javascript
util.getSystemErrorName(-2)   // 'ENOENT'
util.getSystemErrorName(-13)  // 'EACCES'
util.getSystemErrorName(-48)  // 'EADDRINUSE'
util.getSystemErrorName(0)    // undefined (not an error)
```

**Implementation**: Map errno values to error names using libuv.

---

#### 13. util.getSystemErrorMap()
**Current**: Not implemented
**Priority**: LOW

**Node.js Signature**: `getSystemErrorMap(): Map<number, [string, string]>`

**Behavior**:
```javascript
const errorMap = util.getSystemErrorMap();
errorMap.get(-2)
// Returns: ['ENOENT', 'no such file or directory']

errorMap.get(-13)
// Returns: ['EACCES', 'permission denied']
```

**Implementation**: Create Map from libuv error codes and descriptions.

---

#### 14. util.MIMEType (class)
**Current**: Not implemented
**Priority**: LOW

**Node.js Signature**:
```javascript
new util.MIMEType(input: string)
```

**Behavior**:
```javascript
const mime = new util.MIMEType('text/html; charset=utf-8');
console.log(mime.type);      // 'text'
console.log(mime.subtype);   // 'html'
console.log(mime.essence);   // 'text/html'
console.log(mime.params.get('charset')); // 'utf-8'
```

---

#### 15. util.parseArgs([config])
**Current**: Not implemented
**Priority**: LOW

**Node.js Signature**: `parseArgs(config?: ParseArgsConfig): ParseArgsResult`

**Behavior**:
```javascript
const args = util.parseArgs({
  args: ['--foo', 'bar', '-b', 'baz'],
  options: {
    foo: { type: 'string' },
    bar: { type: 'boolean', short: 'b' }
  }
});

// Returns:
// {
//   values: { foo: 'bar', bar: true },
//   positionals: ['baz']
// }
```

---

## 🏗️ Implementation Roadmap

### Phase 1: TextEncoder/TextDecoder Export (2 hours)
**Goal**: Make existing encoding classes available via util module

**Tasks**:
1. ✅ encoding.c is already implemented
2. ⬜ Import classes in node_util.c
3. ⬜ Export via util.TextEncoder
4. ⬜ Export via util.TextDecoder
5. ⬜ Add to CommonJS exports
6. ⬜ Add to ES module exports
7. ⬜ Write basic tests (10 tests)

**Files Modified**:
- `src/node/node_util.c` (~50 lines added)

**Success Criteria**:
- `const { TextEncoder } = require('node:util')` works
- `import { TextEncoder } from 'node:util'` works
- All encoding tests pass

---

### Phase 2: Enhanced format() (4 hours)
**Goal**: Add missing placeholders (%i, %f, %o, %O)

**Tasks**:
1. ⬜ Add %i (integer) support
   - Parse as parseInt(value, 10)
   - Handle NaN case
2. ⬜ Add %f (float) support
   - Parse as parseFloat(value)
   - Handle NaN, Infinity
3. ⬜ Add %o (object single-line) support
   - Call inspect() without depth/breaking
4. ⬜ Add %O (object multi-line) support
   - Call inspect() with breaking enabled
5. ⬜ Improve error handling
6. ⬜ Write comprehensive tests (30 tests)

**Files Modified**:
- `src/node/node_util.c` (~100 lines modified)

**Success Criteria**:
- All placeholders work correctly
- Edge cases handled (NaN, Infinity, circular objects)
- Tests cover all placeholder types

---

### Phase 3: formatWithOptions() (2 hours)
**Goal**: Implement format with custom inspect options

**Tasks**:
1. ⬜ Implement js_util_format_with_options()
2. ⬜ Parse options object (colors, depth, etc.)
3. ⬜ Pass options to inspect() calls
4. ⬜ Validate options
5. ⬜ Write tests (15 tests)

**Files Modified**:
- `src/node/node_util.c` (~80 lines added)

**Success Criteria**:
- Options correctly applied to formatting
- Colors work when enabled
- Depth limiting works

---

### Phase 4: Enhanced inspect() (10 hours)
**Goal**: Full-featured inspect with all options

**Tasks**:
1. ⬜ Implement options parsing
   - colors, depth, showHidden, maxArrayLength, etc.
2. ⬜ Implement circular reference detection
   - Visited set with WeakMap or object tracking
   - Mark circular references
3. ⬜ Implement type-specific formatting:
   - ⬜ Functions: [Function: name] or source
   - ⬜ Dates: ISO string or custom format
   - ⬜ RegExp: /pattern/flags
   - ⬜ Errors: Error: message + stack
   - ⬜ Symbols: Symbol(description)
   - ⬜ BigInt: 123n
   - ⬜ Maps: Map(2) { key => value, ... }
   - ⬜ Sets: Set(3) { value1, value2, ... }
   - ⬜ TypedArrays: Uint8Array(3) [ 1, 2, 3 ]
   - ⬜ ArrayBuffer: with hex contents
   - ⬜ Promises: Promise { <state> }
   - ⬜ Proxies: Proxy [ target, handler ]
4. ⬜ Implement color support
   - ANSI escape codes
   - Color scheme per type
5. ⬜ Implement depth limiting
   - Track current depth
   - Replace deep objects with [Object]
6. ⬜ Implement array truncation
   - maxArrayLength option
   - Show "... N more items"
7. ⬜ Implement string truncation
   - maxStringLength option
8. ⬜ Implement line breaking
   - breakLength threshold
   - Compact vs expanded mode
9. ⬜ Write comprehensive tests (50 tests)

**Files Modified**:
- `src/node/node_util.c` (~500 lines added)

**Success Criteria**:
- All options work correctly
- Circular references detected and marked
- All types formatted correctly
- Colors work when enabled
- Performance acceptable for large objects

---

### Phase 5: util.types.* API (6 hours)
**Goal**: Implement modern type checking API

**Tasks**:
1. ⬜ Create util.types namespace object
2. ⬜ Implement type checkers (30+ functions):
   - ⬜ isDate, isRegExp, isPromise
   - ⬜ isMap, isSet, isWeakMap, isWeakSet
   - ⬜ isTypedArray (generic)
   - ⬜ isUint8Array, isInt8Array, etc. (9 typed array types)
   - ⬜ isArrayBuffer, isSharedArrayBuffer, isDataView
   - ⬜ isBigIntObject, isNumberObject, isStringObject, etc.
   - ⬜ isNativeError
   - ⬜ isProxy (if possible with QuickJS)
   - ⬜ isAsyncFunction, isGeneratorFunction
   - ⬜ Other types as needed
3. ⬜ Implement constructor checking helpers
4. ⬜ Export types namespace
5. ⬜ Write comprehensive tests (60 tests)

**Files Modified**:
- `src/node/node_util.c` (~400 lines added)

**Success Criteria**:
- All type checkers work correctly
- No false positives/negatives
- Performance acceptable
- Tests cover all edge cases

---

### Phase 6: promisify() and callbackify() (8 hours)
**Goal**: Full async utilities implementation

**Tasks - promisify()**:
1. ⬜ Check for custom promisify symbol
2. ⬜ Create Promise wrapper function
3. ⬜ Handle (err, result) callback pattern
4. ⬜ Handle errors correctly
5. ⬜ Preserve function name and length
6. ⬜ Add util.promisify.custom symbol
7. ⬜ Write tests (25 tests)

**Tasks - callbackify()**:
1. ⬜ Create callback wrapper function
2. ⬜ Handle Promise resolution → callback(null, result)
3. ⬜ Handle Promise rejection → callback(err)
4. ⬜ Handle non-Error rejections (wrap in ERR_FALSY_VALUE_REJECTION)
5. ⬜ Preserve function name
6. ⬜ Write tests (20 tests)

**Files Modified**:
- `src/node/node_util.c` (~250 lines added)

**Success Criteria**:
- promisify() works with callback functions
- callbackify() works with async functions
- Roundtrip works: callbackify(promisify(fn)) ≈ fn
- Custom promisify symbol works
- Error handling correct
- Tests cover all cases

---

### Phase 7: deprecate() and debuglog() (4 hours)
**Goal**: Deprecation and debugging utilities

**Tasks - deprecate()**:
1. ⬜ Create wrapper function
2. ⬜ Track shown warnings (static map)
3. ⬜ Emit warning on first call
4. ⬜ Check environment flags (NODE_NO_WARNINGS, etc.)
5. ⬜ Use process.emitWarning() or fallback to console.warn()
6. ⬜ Preserve function properties
7. ⬜ Write tests (15 tests)

**Tasks - debuglog()**:
1. ⬜ Parse NODE_DEBUG environment variable
2. ⬜ Create enabled sections set
3. ⬜ Return logging function
4. ⬜ Add .enabled property
5. ⬜ Support callback parameter
6. ⬜ Format messages with util.format()
7. ⬜ Write tests (10 tests)

**Files Modified**:
- `src/node/node_util.c` (~200 lines added)

**Success Criteria**:
- deprecate() shows warnings correctly
- debuglog() respects NODE_DEBUG
- Performance acceptable
- Tests cover all cases

---

### Phase 8: Additional Utilities (4 hours)
**Goal**: Implement remaining utility functions

**Tasks**:
1. ⬜ Implement util.inherits()
   - Set prototype chain
   - Set super_ property
   - Write tests (10 tests)
2. ⬜ Implement util.isDeepStrictEqual()
   - Primitive comparison
   - Recursive object/array comparison
   - Circular reference handling
   - Special type handling (Date, RegExp, etc.)
   - Write tests (25 tests)
3. ⬜ (Optional) Implement util.getSystemErrorName()
4. ⬜ (Optional) Implement util.getSystemErrorMap()

**Files Modified**:
- `src/node/node_util.c` (~300 lines added)

**Success Criteria**:
- All functions work correctly
- Tests comprehensive

---

### Phase 9: Testing and Edge Cases (8 hours)
**Goal**: Comprehensive test coverage and edge case handling

**Tasks**:
1. ⬜ Write integration tests (30 tests)
2. ⬜ Test edge cases:
   - ⬜ null, undefined handling
   - ⬜ Large objects/arrays
   - ⬜ Deeply nested structures
   - ⬜ Circular references
   - ⬜ Special characters in strings
   - ⬜ Binary data
   - ⬜ Memory limits
3. ⬜ Test error conditions
4. ⬜ Memory leak testing (ASAN)
5. ⬜ Performance testing
6. ⬜ Cross-platform testing

**Test Files**:
- `test/node/test_node_util_format.js` (format tests)
- `test/node/test_node_util_inspect.js` (inspect tests)
- `test/node/test_node_util_types.js` (types tests)
- `test/node/test_node_util_async.js` (promisify/callbackify tests)
- `test/node/test_node_util_misc.js` (other utilities)
- `test/node/test_node_util_encoding.js` (TextEncoder/Decoder tests)

**Success Criteria**:
- 150+ total tests
- All edge cases covered
- No memory leaks (ASAN clean)
- No regressions in existing tests

---

### Phase 10: Documentation and Cleanup (2 hours)
**Goal**: Polish and document

**Tasks**:
1. ⬜ Code cleanup and formatting
2. ⬜ Add code comments
3. ⬜ Update module exports
4. ⬜ Verify CommonJS and ES module support
5. ⬜ Run make format
6. ⬜ Run make test (verify all pass)
7. ⬜ Run make wpt (verify no regressions)
8. ⬜ Update this plan document with completion status

**Success Criteria**:
- All code formatted
- All tests pass
- No WPT regressions
- Documentation complete

---

## 📈 Progress Tracking

### Task List

| ID | Phase | Task | Status | Priority | Complexity | Dependencies |
|----|-------|------|--------|----------|------------|--------------|
| 1.1 | Phase 1 | Import TextEncoder/Decoder classes | ⏳ PENDING | HIGH | TRIVIAL | None |
| 1.2 | Phase 1 | Export via util.TextEncoder | ⏳ PENDING | HIGH | TRIVIAL | 1.1 |
| 1.3 | Phase 1 | Add to CommonJS exports | ⏳ PENDING | HIGH | TRIVIAL | 1.2 |
| 1.4 | Phase 1 | Add to ES module exports | ⏳ PENDING | HIGH | TRIVIAL | 1.2 |
| 1.5 | Phase 1 | Write encoding tests | ⏳ PENDING | HIGH | SIMPLE | 1.2 |
| 2.1 | Phase 2 | Add %i placeholder support | ⏳ PENDING | HIGH | SIMPLE | None |
| 2.2 | Phase 2 | Add %f placeholder support | ⏳ PENDING | HIGH | SIMPLE | None |
| 2.3 | Phase 2 | Add %o placeholder support | ⏳ PENDING | HIGH | MEDIUM | None |
| 2.4 | Phase 2 | Add %O placeholder support | ⏳ PENDING | HIGH | MEDIUM | None |
| 2.5 | Phase 2 | Write format tests | ⏳ PENDING | HIGH | SIMPLE | 2.1-2.4 |
| 3.1 | Phase 3 | Implement formatWithOptions() | ⏳ PENDING | HIGH | MEDIUM | 2.1-2.4 |
| 3.2 | Phase 3 | Parse options object | ⏳ PENDING | HIGH | SIMPLE | 3.1 |
| 3.3 | Phase 3 | Write formatWithOptions tests | ⏳ PENDING | HIGH | SIMPLE | 3.1-3.2 |
| 4.1 | Phase 4 | Implement options parsing | ⏳ PENDING | HIGH | MEDIUM | None |
| 4.2 | Phase 4 | Implement circular ref detection | ⏳ PENDING | HIGH | COMPLEX | None |
| 4.3 | Phase 4 | Format functions | ⏳ PENDING | HIGH | MEDIUM | 4.1 |
| 4.4 | Phase 4 | Format dates | ⏳ PENDING | HIGH | SIMPLE | 4.1 |
| 4.5 | Phase 4 | Format RegExp | ⏳ PENDING | HIGH | SIMPLE | 4.1 |
| 4.6 | Phase 4 | Format errors | ⏳ PENDING | HIGH | MEDIUM | 4.1 |
| 4.7 | Phase 4 | Format symbols | ⏳ PENDING | HIGH | SIMPLE | 4.1 |
| 4.8 | Phase 4 | Format BigInt | ⏳ PENDING | HIGH | SIMPLE | 4.1 |
| 4.9 | Phase 4 | Format Maps | ⏳ PENDING | HIGH | MEDIUM | 4.1 |
| 4.10 | Phase 4 | Format Sets | ⏳ PENDING | HIGH | MEDIUM | 4.1 |
| 4.11 | Phase 4 | Format TypedArrays | ⏳ PENDING | HIGH | MEDIUM | 4.1 |
| 4.12 | Phase 4 | Format ArrayBuffer | ⏳ PENDING | HIGH | MEDIUM | 4.1 |
| 4.13 | Phase 4 | Format Promises | ⏳ PENDING | MEDIUM | MEDIUM | 4.1 |
| 4.14 | Phase 4 | Format Proxies | ⏳ PENDING | LOW | COMPLEX | 4.1 |
| 4.15 | Phase 4 | Implement color support | ⏳ PENDING | HIGH | MEDIUM | 4.1 |
| 4.16 | Phase 4 | Implement depth limiting | ⏳ PENDING | HIGH | MEDIUM | 4.1, 4.2 |
| 4.17 | Phase 4 | Implement array truncation | ⏳ PENDING | HIGH | SIMPLE | 4.1 |
| 4.18 | Phase 4 | Implement string truncation | ⏳ PENDING | MEDIUM | SIMPLE | 4.1 |
| 4.19 | Phase 4 | Implement line breaking | ⏳ PENDING | MEDIUM | COMPLEX | 4.1 |
| 4.20 | Phase 4 | Write inspect tests | ⏳ PENDING | HIGH | MEDIUM | 4.1-4.19 |
| 5.1 | Phase 5 | Create util.types namespace | ⏳ PENDING | HIGH | SIMPLE | None |
| 5.2 | Phase 5 | Implement isDate | ⏳ PENDING | HIGH | SIMPLE | 5.1 |
| 5.3 | Phase 5 | Implement isRegExp | ⏳ PENDING | HIGH | SIMPLE | 5.1 |
| 5.4 | Phase 5 | Implement isPromise | ⏳ PENDING | HIGH | SIMPLE | 5.1 |
| 5.5 | Phase 5 | Implement isMap | ⏳ PENDING | HIGH | SIMPLE | 5.1 |
| 5.6 | Phase 5 | Implement isSet | ⏳ PENDING | HIGH | SIMPLE | 5.1 |
| 5.7 | Phase 5 | Implement isWeakMap | ⏳ PENDING | HIGH | SIMPLE | 5.1 |
| 5.8 | Phase 5 | Implement isWeakSet | ⏳ PENDING | HIGH | SIMPLE | 5.1 |
| 5.9 | Phase 5 | Implement isTypedArray (generic) | ⏳ PENDING | HIGH | MEDIUM | 5.1 |
| 5.10 | Phase 5 | Implement typed array checkers | ⏳ PENDING | HIGH | SIMPLE | 5.9 |
| 5.11 | Phase 5 | Implement isArrayBuffer | ⏳ PENDING | HIGH | SIMPLE | 5.1 |
| 5.12 | Phase 5 | Implement isDataView | ⏳ PENDING | HIGH | SIMPLE | 5.1 |
| 5.13 | Phase 5 | Implement primitive object checkers | ⏳ PENDING | MEDIUM | SIMPLE | 5.1 |
| 5.14 | Phase 5 | Implement isNativeError | ⏳ PENDING | HIGH | MEDIUM | 5.1 |
| 5.15 | Phase 5 | Write types tests | ⏳ PENDING | HIGH | MEDIUM | 5.1-5.14 |
| 6.1 | Phase 6 | Check custom promisify symbol | ⏳ PENDING | HIGH | SIMPLE | None |
| 6.2 | Phase 6 | Create Promise wrapper | ⏳ PENDING | HIGH | MEDIUM | None |
| 6.3 | Phase 6 | Handle callback pattern | ⏳ PENDING | HIGH | MEDIUM | 6.2 |
| 6.4 | Phase 6 | Handle errors in promisify | ⏳ PENDING | HIGH | SIMPLE | 6.2 |
| 6.5 | Phase 6 | Preserve function properties | ⏳ PENDING | HIGH | SIMPLE | 6.2 |
| 6.6 | Phase 6 | Add promisify.custom symbol | ⏳ PENDING | HIGH | SIMPLE | 6.1 |
| 6.7 | Phase 6 | Write promisify tests | ⏳ PENDING | HIGH | MEDIUM | 6.1-6.6 |
| 6.8 | Phase 6 | Create callback wrapper | ⏳ PENDING | HIGH | MEDIUM | None |
| 6.9 | Phase 6 | Handle Promise resolution | ⏳ PENDING | HIGH | SIMPLE | 6.8 |
| 6.10 | Phase 6 | Handle Promise rejection | ⏳ PENDING | HIGH | MEDIUM | 6.8 |
| 6.11 | Phase 6 | Handle non-Error rejections | ⏳ PENDING | HIGH | MEDIUM | 6.10 |
| 6.12 | Phase 6 | Write callbackify tests | ⏳ PENDING | HIGH | MEDIUM | 6.8-6.11 |
| 7.1 | Phase 7 | Create deprecate wrapper | ⏳ PENDING | MEDIUM | MEDIUM | None |
| 7.2 | Phase 7 | Track shown warnings | ⏳ PENDING | MEDIUM | SIMPLE | 7.1 |
| 7.3 | Phase 7 | Emit warnings | ⏳ PENDING | MEDIUM | MEDIUM | 7.1 |
| 7.4 | Phase 7 | Check environment flags | ⏳ PENDING | MEDIUM | SIMPLE | 7.1 |
| 7.5 | Phase 7 | Write deprecate tests | ⏳ PENDING | MEDIUM | SIMPLE | 7.1-7.4 |
| 7.6 | Phase 7 | Parse NODE_DEBUG env var | ⏳ PENDING | MEDIUM | SIMPLE | None |
| 7.7 | Phase 7 | Create debuglog function | ⏳ PENDING | MEDIUM | SIMPLE | 7.6 |
| 7.8 | Phase 7 | Add .enabled property | ⏳ PENDING | MEDIUM | TRIVIAL | 7.7 |
| 7.9 | Phase 7 | Support callback parameter | ⏳ PENDING | MEDIUM | SIMPLE | 7.7 |
| 7.10 | Phase 7 | Write debuglog tests | ⏳ PENDING | MEDIUM | SIMPLE | 7.6-7.9 |
| 8.1 | Phase 8 | Implement inherits() | ⏳ PENDING | LOW | SIMPLE | None |
| 8.2 | Phase 8 | Write inherits tests | ⏳ PENDING | LOW | SIMPLE | 8.1 |
| 8.3 | Phase 8 | Implement isDeepStrictEqual | ⏳ PENDING | MEDIUM | COMPLEX | None |
| 8.4 | Phase 8 | Handle circular refs in equality | ⏳ PENDING | MEDIUM | MEDIUM | 8.3 |
| 8.5 | Phase 8 | Write equality tests | ⏳ PENDING | MEDIUM | MEDIUM | 8.3-8.4 |
| 9.1 | Phase 9 | Write integration tests | ⏳ PENDING | HIGH | MEDIUM | All above |
| 9.2 | Phase 9 | Test edge cases | ⏳ PENDING | HIGH | MEDIUM | All above |
| 9.3 | Phase 9 | Memory leak testing | ⏳ PENDING | HIGH | SIMPLE | All above |
| 9.4 | Phase 9 | Performance testing | ⏳ PENDING | MEDIUM | SIMPLE | All above |
| 9.5 | Phase 9 | Cross-platform testing | ⏳ PENDING | HIGH | SIMPLE | All above |
| 10.1 | Phase 10 | Code cleanup | ⏳ PENDING | HIGH | SIMPLE | All above |
| 10.2 | Phase 10 | Add comments | ⏳ PENDING | HIGH | SIMPLE | All above |
| 10.3 | Phase 10 | Update exports | ⏳ PENDING | HIGH | SIMPLE | All above |
| 10.4 | Phase 10 | Run make format | ⏳ PENDING | HIGH | TRIVIAL | 10.1 |
| 10.5 | Phase 10 | Run make test | ⏳ PENDING | HIGH | TRIVIAL | 10.1 |
| 10.6 | Phase 10 | Run make wpt | ⏳ PENDING | HIGH | TRIVIAL | 10.1 |

---

## 🧪 Testing Strategy

### Test Categories

#### 1. Format Tests (30 tests)
**File**: `test/node/test_node_util_format.js`

```javascript
// Basic placeholders
util.format('Hello %s', 'world')
util.format('%d + %d = %d', 1, 2, 3)
util.format('%i', 42.7)  // Integer
util.format('%f', 42.7)  // Float
util.format('%j', {foo: 'bar'})  // JSON
util.format('%o', {foo: 'bar'})  // Object
util.format('%O', {foo: 'bar'})  // Object multiline
util.format('%%')  // Escape

// Edge cases
util.format()  // Empty
util.format('no placeholders')
util.format('%s %s', 'one')  // Missing arg
util.format('%s', 'one', 'two')  // Extra args
util.format('%d', NaN)
util.format('%d', Infinity)
util.format('%j', circular)  // Circular object
util.format('%q', 'unknown')  // Unknown placeholder
```

#### 2. Inspect Tests (50 tests)
**File**: `test/node/test_node_util_inspect.js`

```javascript
// Basic types
util.inspect('string')
util.inspect(123)
util.inspect(true)
util.inspect(null)
util.inspect(undefined)
util.inspect(Symbol('test'))
util.inspect(123n)  // BigInt

// Objects and arrays
util.inspect({a: 1})
util.inspect([1, 2, 3])
util.inspect({a: {b: {c: 1}}})  // Nested

// Options
util.inspect(obj, {colors: true})
util.inspect(obj, {depth: 1})
util.inspect(obj, {showHidden: true})
util.inspect(arr, {maxArrayLength: 2})
util.inspect(str, {maxStringLength: 10})

// Circular references
const circular = {a: 1};
circular.self = circular;
util.inspect(circular)

// Special types
util.inspect(function myFunc() {})
util.inspect(new Date())
util.inspect(/abc/gi)
util.inspect(new Error('test'))
util.inspect(new Map([[1, 2]]))
util.inspect(new Set([1, 2]))
util.inspect(new Uint8Array([1, 2, 3]))
util.inspect(Promise.resolve())
```

#### 3. Types Tests (60 tests)
**File**: `test/node/test_node_util_types.js`

```javascript
// Date
util.types.isDate(new Date())  // true
util.types.isDate({})  // false

// RegExp
util.types.isRegExp(/abc/)  // true
util.types.isRegExp('abc')  // false

// Promise
util.types.isPromise(Promise.resolve())  // true
util.types.isPromise({then: () => {}})  // false

// Map/Set
util.types.isMap(new Map())  // true
util.types.isSet(new Set())  // true
util.types.isWeakMap(new WeakMap())  // true

// TypedArrays
util.types.isTypedArray(new Uint8Array())  // true
util.types.isUint8Array(new Uint8Array())  // true
util.types.isUint16Array(new Uint8Array())  // false

// ArrayBuffer
util.types.isArrayBuffer(new ArrayBuffer(10))  // true
util.types.isDataView(new DataView(new ArrayBuffer(10)))  // true

// Primitives
util.types.isBigIntObject(Object(123n))  // true (NOT primitive BigInt)
util.types.isNumberObject(new Number(123))  // true
util.types.isStringObject(new String('test'))  // true

// Errors
util.types.isNativeError(new Error())  // true
util.types.isNativeError(new TypeError())  // true
util.types.isNativeError({message: 'x'})  // false
```

#### 4. Async Tests (45 tests)
**File**: `test/node/test_node_util_async.js`

```javascript
// promisify basic
function callback(arg, cb) { cb(null, 'result: ' + arg); }
const promisified = util.promisify(callback);
promisified('test').then(result => {
  assert.strictEqual(result, 'result: test');
});

// promisify error
function errorCallback(cb) { cb(new Error('failed')); }
const promisified2 = util.promisify(errorCallback);
promisified2().catch(err => {
  assert.strictEqual(err.message, 'failed');
});

// promisify custom
callback[util.promisify.custom] = (arg) => Promise.resolve('custom: ' + arg);
const custom = util.promisify(callback);
custom('test').then(result => {
  assert.strictEqual(result, 'custom: test');
});

// callbackify basic
async function asyncFunc(arg) { return 'result: ' + arg; }
const callbackified = util.callbackify(asyncFunc);
callbackified('test', (err, result) => {
  assert.strictEqual(err, null);
  assert.strictEqual(result, 'result: test');
});

// callbackify error
async function asyncError() { throw new Error('failed'); }
const callbackified2 = util.callbackify(asyncError);
callbackified2((err, result) => {
  assert.strictEqual(err.message, 'failed');
  assert.strictEqual(result, undefined);
});

// Roundtrip
const original = (arg, cb) => cb(null, arg);
const roundtrip = util.callbackify(util.promisify(original));
roundtrip('test', (err, result) => {
  assert.strictEqual(result, 'test');
});
```

#### 5. Misc Tests (25 tests)
**File**: `test/node/test_node_util_misc.js`

```javascript
// deprecate
const deprecated = util.deprecate(() => 'result', 'deprecated', 'DEP0001');
deprecated();  // Shows warning
deprecated();  // No warning (already shown)

// debuglog (with NODE_DEBUG=test)
const debug = util.debuglog('test');
debug('message');  // Prints if enabled
assert.strictEqual(typeof debug.enabled, 'boolean');

// inherits
function Animal() {}
function Dog() {}
util.inherits(Dog, Animal);
const dog = new Dog();
assert.strictEqual(dog instanceof Animal, true);

// isDeepStrictEqual
assert.strictEqual(util.isDeepStrictEqual({a: 1}, {a: 1}), true);
assert.strictEqual(util.isDeepStrictEqual({a: 1}, {a: '1'}), false);
assert.strictEqual(util.isDeepStrictEqual([1, 2], [1, 2]), true);

// Circular in equality
const a = {x: 1}; a.self = a;
const b = {x: 1}; b.self = b;
assert.strictEqual(util.isDeepStrictEqual(a, b), true);
```

#### 6. Encoding Tests (10 tests)
**File**: `test/node/test_node_util_encoding.js`

```javascript
// TextEncoder export
const { TextEncoder } = require('node:util');
const encoder = new TextEncoder();
const bytes = encoder.encode('hello');
assert.strictEqual(bytes[0], 104);  // 'h'

// TextDecoder export
const { TextDecoder } = require('node:util');
const decoder = new TextDecoder();
const text = decoder.decode(new Uint8Array([104, 101, 108, 108, 111]));
assert.strictEqual(text, 'hello');

// ES module import
import { TextEncoder, TextDecoder } from 'node:util';
```

### Test Execution
```bash
# Run all util tests
./target/debug/jsrt test/node/test_node_util_format.js
./target/debug/jsrt test/node/test_node_util_inspect.js
./target/debug/jsrt test/node/test_node_util_types.js
./target/debug/jsrt test/node/test_node_util_async.js
./target/debug/jsrt test/node/test_node_util_misc.js
./target/debug/jsrt test/node/test_node_util_encoding.js

# Memory leak check
./target/debug/jsrt_m test/node/test_node_util_*.js

# Performance test
time ./target/release/jsrt test/node/test_node_util_*.js
```

---

## 💾 Memory Management

### QuickJS Memory Patterns

#### 1. String Handling
```c
// Get C string from JS value
const char* str = JS_ToCString(ctx, js_val);
if (!str) return JS_EXCEPTION;

// Use string...

// MUST free
JS_FreeCString(ctx, str);
```

#### 2. Object Property Access
```c
// Get property
JSValue prop = JS_GetPropertyStr(ctx, obj, "key");

// Check if exception
if (JS_IsException(prop)) {
    return JS_EXCEPTION;
}

// Use property...

// MUST free
JS_FreeValue(ctx, prop);
```

#### 3. Creating New Values
```c
// Create string
JSValue str = JS_NewString(ctx, "hello");

// Create number
JSValue num = JS_NewInt32(ctx, 42);

// Create object
JSValue obj = JS_NewObject(ctx);

// Return (caller will free)
return obj;

// Or if not returning, must free
JS_FreeValue(ctx, obj);
```

#### 4. Array Iteration
```c
JSValue arr = argv[0];
JSValue length_val = JS_GetPropertyStr(ctx, arr, "length");
uint32_t length;
JS_ToUint32(ctx, &length, length_val);
JS_FreeValue(ctx, length_val);

for (uint32_t i = 0; i < length; i++) {
    JSValue elem = JS_GetPropertyUint32(ctx, arr, i);

    // Process elem...

    JS_FreeValue(ctx, elem);
}
```

#### 5. Error Handling
```c
JSValue result = some_function(ctx);
if (JS_IsException(result)) {
    // Don't free exception values
    return JS_EXCEPTION;
}

// Use result...

JS_FreeValue(ctx, result);
return JS_UNDEFINED;
```

### Memory Leak Prevention Checklist

- ⬜ Every JS_ToCString() has matching JS_FreeCString()
- ⬜ Every JS_GetPropertyStr() has matching JS_FreeValue()
- ⬜ Every JS_NewString()/JS_NewObject() is returned or freed
- ⬜ Exception values are NOT freed (JS_EXCEPTION)
- ⬜ Temporary values in loops are freed each iteration
- ⬜ malloc() allocations have matching free()
- ⬜ ASAN testing passes (no leaks detected)

---

## 📝 Implementation Templates

### Template 1: Type Checker Function
```c
static JSValue js_util_types_is_date(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_FALSE;
    }

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue date_ctor = JS_GetPropertyStr(ctx, global, "Date");
    int is_date = JS_IsInstanceOf(ctx, argv[0], date_ctor);

    JS_FreeValue(ctx, date_ctor);
    JS_FreeValue(ctx, global);

    return JS_NewBool(ctx, is_date > 0);
}
```

### Template 2: Circular Reference Detection
```c
typedef struct {
    JSValue* visited;
    size_t count;
    size_t capacity;
} VisitedSet;

static int is_circular(VisitedSet* set, JSContext* ctx, JSValue obj) {
    // Check if already visited
    for (size_t i = 0; i < set->count; i++) {
        if (JS_VALUE_GET_PTR(set->visited[i]) == JS_VALUE_GET_PTR(obj)) {
            return 1;  // Circular
        }
    }

    // Add to visited set
    if (set->count >= set->capacity) {
        set->capacity = set->capacity ? set->capacity * 2 : 8;
        set->visited = realloc(set->visited, set->capacity * sizeof(JSValue));
    }
    set->visited[set->count++] = JS_DupValue(ctx, obj);

    return 0;  // Not circular
}

static void free_visited_set(JSContext* ctx, VisitedSet* set) {
    for (size_t i = 0; i < set->count; i++) {
        JS_FreeValue(ctx, set->visited[i]);
    }
    free(set->visited);
}
```

### Template 3: Inspect with Options
```c
typedef struct {
    bool colors;
    int depth;
    int max_depth;
    bool show_hidden;
    int max_array_length;
    int max_string_length;
} InspectOptions;

static JSValue inspect_value(JSContext* ctx, JSValue val,
                             InspectOptions* opts, VisitedSet* visited) {
    // Check depth limit
    if (opts->depth >= opts->max_depth) {
        return JS_NewString(ctx, "[Object]");
    }

    // Check circular
    if (JS_IsObject(val) && is_circular(visited, ctx, val)) {
        return JS_NewString(ctx, "[Circular]");
    }

    // Type-specific formatting
    if (JS_IsString(val)) {
        // Format string with quotes
    } else if (JS_IsNumber(val)) {
        // Format number
    } else if (JS_IsArray(ctx, val)) {
        // Format array with recursion
        opts->depth++;
        // ... recursive calls
        opts->depth--;
    } else if (JS_IsObject(val)) {
        // Format object with recursion
        opts->depth++;
        // ... recursive calls
        opts->depth--;
    }

    // ... other types

    return result;
}
```

### Template 4: promisify() Wrapper
```c
// Helper function stored as private data
typedef struct {
    JSValue original_fn;
} PromisifyData;

static JSValue promisify_wrapper(JSContext* ctx, JSValueConst this_val,
                                 int argc, JSValueConst* argv) {
    // Get original function from private data
    PromisifyData* data = JS_GetOpaque(this_val, promisify_class_id);

    // Create Promise constructor
    JSValue promise_ctor = JS_GetGlobalObject(ctx);
    JSValue promise = JS_GetPropertyStr(ctx, promise_ctor, "Promise");
    JS_FreeValue(ctx, promise_ctor);

    // Create Promise with executor
    // Executor calls original function with custom callback
    // Callback resolves or rejects the promise

    // Return promise
    return promise;
}
```

---

## 🎯 Success Criteria

### Functionality
- ✅ All Priority 1 APIs fully implemented
- ✅ All Priority 2 APIs fully implemented
- ⚠️ Priority 3 APIs implemented (optional)
- ✅ Both CommonJS and ES module support
- ✅ TextEncoder/TextDecoder exported from util

### Quality
- ✅ 150+ comprehensive tests passing
- ✅ All edge cases handled
- ✅ Circular reference detection works
- ✅ Memory safe (ASAN clean, no leaks)
- ✅ Error handling correct
- ✅ Performance acceptable

### Compatibility
- ✅ Node.js API compatibility verified
- ✅ Behavior matches Node.js exactly
- ✅ Type checking accurate
- ✅ inspect() output similar to Node.js

### Testing
- ✅ `make test` passes (100%)
- ✅ `make wpt` passes (no regressions)
- ✅ `make format` applied
- ✅ ASAN testing clean
- ✅ Cross-platform testing (Linux/macOS/Windows)

---

## 📚 References

### Node.js Documentation
- [util Module](https://nodejs.org/api/util.html)
- [util.format()](https://nodejs.org/api/util.html#utilformatformat-args)
- [util.inspect()](https://nodejs.org/api/util.html#utilinspectobject-options)
- [util.types](https://nodejs.org/api/util.html#utiltypes)
- [util.promisify()](https://nodejs.org/api/util.html#utilpromisifyoriginal)

### Implementation References
- Existing `src/node/node_util.c` (basic implementation)
- `src/std/encoding.c` (TextEncoder/TextDecoder)
- `src/std/console.c` (formatting patterns)
- QuickJS API documentation

### Test References
- `test/node/test_node_util.js` (existing tests)
- `test/node/test_node_util_esm.js` (existing ES module tests)
- Node.js util test suite (for behavior reference)

---

## 🔄 Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-10-07 | Initial comprehensive plan created |

---

## 📊 Summary Statistics

- **Total Phases**: 10
- **Total Tasks**: 86
- **Estimated Hours**: 50
- **Priority 1 APIs**: 5 (format, inspect, types, TextEncoder/Decoder)
- **Priority 2 APIs**: 4 (promisify, callbackify, deprecate, debuglog)
- **Priority 3 APIs**: 4+ (inherits, isDeepStrictEqual, etc.)
- **Test Files**: 6
- **Expected Tests**: 150+
- **Lines of Code**: ~2000 (estimated for full implementation)

---

**Status**: Ready for implementation
**Next Step**: Begin Phase 1 - TextEncoder/TextDecoder Export
