#include "assert.h"

#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/colorize.h"

// Forward declarations
static JSValue jsrt_assert(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_ok(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_equal(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_notEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_strictEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_notStrictEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_deepEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_notDeepEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_deepStrictEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_notDeepStrictEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_throws(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_doesNotThrow(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_rejects(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_doesNotReject(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Helper function to format assertion error message
static void print_assertion_error(JSContext* ctx, const char* message, JSValueConst actual, JSValueConst expected) {
  printf("%sAssertionError: %s", jsrt_colorize(JSRT_ColorizeRed, -1, JSRT_ColorizeBold), JSRT_ColorizeClear);
  printf("%s\n", message);

  if (!JS_IsUndefined(actual)) {
    const char* actual_str = JS_ToCString(ctx, actual);
    if (actual_str) {
      printf("  actual: %s\n", actual_str);
      JS_FreeCString(ctx, actual_str);
    }
  }

  if (!JS_IsUndefined(expected)) {
    const char* expected_str = JS_ToCString(ctx, expected);
    if (expected_str) {
      printf("  expected: %s\n", expected_str);
      JS_FreeCString(ctx, expected_str);
    }
  }
}

// Enhanced function to create AssertionError with all Node.js properties
static JSValue create_assertion_error(JSContext* ctx, const char* message, JSValueConst actual, JSValueConst expected,
                                      const char* operator, int generated_message) {
  JSValue error = JS_NewError(ctx);
  JS_SetPropertyStr(ctx, error, "name", JS_NewString(ctx, "AssertionError"));
  JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, message));
  JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ERR_ASSERTION"));

  // Add actual and expected values if provided
  if (!JS_IsUndefined(actual)) {
    JS_SetPropertyStr(ctx, error, "actual", JS_DupValue(ctx, actual));
  }
  if (!JS_IsUndefined(expected)) {
    JS_SetPropertyStr(ctx, error, "expected", JS_DupValue(ctx, expected));
  }

  // Add operator string
  if (operator) {
    JS_SetPropertyStr(ctx, error, "operator", JS_NewString(ctx, operator));
  }

  // Add generatedMessage flag
  JS_SetPropertyStr(ctx, error, "generatedMessage", JS_NewBool(ctx, generated_message));

  return error;
}

// Helper function to throw AssertionError (legacy - uses enhanced version)
static JSValue throw_assertion_error(JSContext* ctx, const char* message) {
  JSValue error = create_assertion_error(ctx, message, JS_UNDEFINED, JS_UNDEFINED, NULL, 1);
  return JS_Throw(ctx, error);
}

// assert(value[, message])
static JSValue jsrt_assert(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return throw_assertion_error(ctx, "No assertion provided");
  }

  int32_t value = JS_ToBool(ctx, argv[0]);
  if (value <= 0) {
    const char* message = "Assertion failed";
    int generated = 1;
    if (argc > 1 && JS_IsString(argv[1])) {
      message = JS_ToCString(ctx, argv[1]);
      generated = 0;
    }

    print_assertion_error(ctx, message, argv[0], JS_UNDEFINED);
    JSValue error = create_assertion_error(ctx, message, argv[0], JS_TRUE, "==", generated);

    if (argc > 1 && JS_IsString(argv[1])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// assert.ok(value[, message]) - alias for assert()
static JSValue jsrt_assert_ok(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return jsrt_assert(ctx, this_val, argc, argv);
}

// assert.equal(actual, expected[, message])
static JSValue jsrt_assert_equal(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.equal requires at least 2 arguments");
  }

  // Use JavaScript evaluation to implement loose equality (==)
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, "__assertTmp1", JS_DupValue(ctx, argv[0]));
  JS_SetPropertyStr(ctx, global, "__assertTmp2", JS_DupValue(ctx, argv[1]));

  JSValue result = JS_Eval(ctx, "__assertTmp1 == __assertTmp2", 26, "<assert>", JS_EVAL_FLAG_STRICT);
  int equal = JS_ToBool(ctx, result);

  JS_DeleteProperty(ctx, global, JS_NewAtom(ctx, "__assertTmp1"), 0);
  JS_DeleteProperty(ctx, global, JS_NewAtom(ctx, "__assertTmp2"), 0);
  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, result);
  equal = (equal == 1);
  if (!equal) {
    const char* message = "Expected values to be equal (==)";
    int generated = 1;
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
      generated = 0;
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = create_assertion_error(ctx, message, argv[0], argv[1], "==", generated);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// assert.notEqual(actual, expected[, message])
static JSValue jsrt_assert_notEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.notEqual requires at least 2 arguments");
  }

  JSValue val1 = JS_DupValue(ctx, argv[0]);
  JSValue val2 = JS_DupValue(ctx, argv[1]);
  int equal = JS_StrictEq(ctx, val1, val2);
  // For loose equality, convert to string and compare
  if (equal != 1) {
    const char* str1 = JS_ToCString(ctx, val1);
    const char* str2 = JS_ToCString(ctx, val2);
    if (str1 && str2) {
      equal = (strcmp(str1, str2) == 0) ? 1 : 0;
    }
    if (str1)
      JS_FreeCString(ctx, str1);
    if (str2)
      JS_FreeCString(ctx, str2);
  }
  JS_FreeValue(ctx, val1);
  JS_FreeValue(ctx, val2);
  equal = (equal == 1);
  if (equal) {
    const char* message = "Expected values to be not equal (!=)";
    int generated = 1;
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
      generated = 0;
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = create_assertion_error(ctx, message, argv[0], argv[1], "!=", generated);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// assert.strictEqual(actual, expected[, message])
static JSValue jsrt_assert_strictEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.strictEqual requires at least 2 arguments");
  }

  int equal = JS_StrictEq(ctx, argv[0], argv[1]);
  if (equal != 1) {
    const char* message = "Expected values to be strictly equal (===)";
    int generated = 1;
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
      generated = 0;
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = create_assertion_error(ctx, message, argv[0], argv[1], "===", generated);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// assert.notStrictEqual(actual, expected[, message])
static JSValue jsrt_assert_notStrictEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.notStrictEqual requires at least 2 arguments");
  }

  int equal = JS_StrictEq(ctx, argv[0], argv[1]);
  if (equal == 1) {
    const char* message = "Expected values to be not strictly equal (!==)";
    int generated = 1;
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
      generated = 0;
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = create_assertion_error(ctx, message, argv[0], argv[1], "!==", generated);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// Helper: Check if value is NaN
static int is_nan(JSContext* ctx, JSValueConst val) {
  if (!JS_IsNumber(val))
    return 0;
  double d;
  if (JS_ToFloat64(ctx, &d, val) < 0)
    return 0;
  return d != d;  // NaN != NaN
}

// Helper: Check if value is -0
static int is_negative_zero(JSContext* ctx, JSValueConst val) {
  if (!JS_IsNumber(val))
    return 0;
  double d;
  if (JS_ToFloat64(ctx, &d, val) < 0)
    return 0;
  return d == 0.0 && 1.0 / d < 0.0;  // 1/-0 = -Infinity
}

// Helper: Deep strict equality with Object.is() semantics for primitives
// Returns: 1 if equal, 0 if not equal, -1 on error
static int deep_strict_equal_internal(JSContext* ctx, JSValueConst a, JSValueConst b, JSValue visited) {
  // Use Object.is() semantics for primitives
  // NaN === NaN (true), -0 !== +0 (false)

  // Check for NaN (Object.is(NaN, NaN) === true)
  int a_is_nan = is_nan(ctx, a);
  int b_is_nan = is_nan(ctx, b);
  if (a_is_nan && b_is_nan)
    return 1;
  if (a_is_nan || b_is_nan)
    return 0;

  // Check for -0 vs +0 (Object.is(-0, +0) === false)
  int a_is_neg_zero = is_negative_zero(ctx, a);
  int b_is_neg_zero = is_negative_zero(ctx, b);
  if (a_is_neg_zero != b_is_neg_zero)
    return 0;

  // Try strict equality first (handles primitives, same objects)
  int strict = JS_StrictEq(ctx, a, b);
  if (strict == 1)
    return 1;
  if (strict == -1)
    return -1;  // error

  // Check if different types
  if (JS_VALUE_GET_TAG(a) != JS_VALUE_GET_TAG(b))
    return 0;

  // If not objects, and not strictly equal, they're different
  if (!JS_IsObject(a) || !JS_IsObject(b))
    return 0;

  // Check for circular references
  JSValue visited_len = JS_GetPropertyStr(ctx, visited, "length");
  int len;
  if (JS_ToInt32(ctx, &len, visited_len) < 0) {
    JS_FreeValue(ctx, visited_len);
    return -1;
  }
  JS_FreeValue(ctx, visited_len);

  // Check if we've seen this pair before (circular reference)
  for (int i = 0; i < len; i++) {
    JSValue pair = JS_GetPropertyUint32(ctx, visited, i);
    JSValue pair_a = JS_GetPropertyUint32(ctx, pair, 0);
    JSValue pair_b = JS_GetPropertyUint32(ctx, pair, 1);

    int same_a = JS_StrictEq(ctx, pair_a, a) == 1;
    int same_b = JS_StrictEq(ctx, pair_b, b) == 1;

    JS_FreeValue(ctx, pair_a);
    JS_FreeValue(ctx, pair_b);
    JS_FreeValue(ctx, pair);

    if (same_a && same_b)
      return 1;  // Circular reference, assume equal
  }

  // Add this pair to visited set
  JSValue pair = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, pair, 0, JS_DupValue(ctx, a));
  JS_SetPropertyUint32(ctx, pair, 1, JS_DupValue(ctx, b));
  JS_SetPropertyUint32(ctx, visited, len, pair);

  // Check if both are arrays
  int a_is_array = JS_IsArray(ctx, a);
  int b_is_array = JS_IsArray(ctx, b);
  if (a_is_array != b_is_array) {
    return 0;
  }

  if (a_is_array) {
    // Compare array lengths
    JSValue a_len = JS_GetPropertyStr(ctx, a, "length");
    JSValue b_len = JS_GetPropertyStr(ctx, b, "length");
    int equal_len = JS_StrictEq(ctx, a_len, b_len) == 1;

    uint32_t arr_len;
    if (JS_ToUint32(ctx, &arr_len, a_len) < 0) {
      JS_FreeValue(ctx, a_len);
      JS_FreeValue(ctx, b_len);
      return -1;
    }

    JS_FreeValue(ctx, a_len);
    JS_FreeValue(ctx, b_len);

    if (!equal_len)
      return 0;

    // Compare array elements
    for (uint32_t i = 0; i < arr_len; i++) {
      JSValue a_elem = JS_GetPropertyUint32(ctx, a, i);
      JSValue b_elem = JS_GetPropertyUint32(ctx, b, i);

      int elem_equal = deep_strict_equal_internal(ctx, a_elem, b_elem, visited);

      JS_FreeValue(ctx, a_elem);
      JS_FreeValue(ctx, b_elem);

      if (elem_equal != 1)
        return elem_equal;  // Not equal or error
    }

    return 1;  // All elements equal
  }

  // Handle special object types
  // Check if Date objects
  JSValue a_time = JS_Invoke(ctx, a, JS_NewAtom(ctx, "getTime"), 0, NULL);
  JSValue b_time = JS_Invoke(ctx, b, JS_NewAtom(ctx, "getTime"), 0, NULL);
  int both_dates = !JS_IsException(a_time) && !JS_IsException(b_time);

  if (both_dates) {
    int time_equal = JS_StrictEq(ctx, a_time, b_time) == 1;
    JS_FreeValue(ctx, a_time);
    JS_FreeValue(ctx, b_time);
    return time_equal ? 1 : 0;
  }
  JS_FreeValue(ctx, a_time);
  JS_FreeValue(ctx, b_time);
  // Clear exceptions if not dates
  if (JS_IsException(a_time))
    JS_GetException(ctx);
  if (JS_IsException(b_time))
    JS_GetException(ctx);

  // Check if RegExp objects
  JSValue a_source = JS_GetPropertyStr(ctx, a, "source");
  JSValue b_source = JS_GetPropertyStr(ctx, b, "source");
  JSValue a_flags = JS_GetPropertyStr(ctx, a, "flags");
  JSValue b_flags = JS_GetPropertyStr(ctx, b, "flags");

  int both_regexp =
      !JS_IsUndefined(a_source) && !JS_IsUndefined(b_source) && !JS_IsUndefined(a_flags) && !JS_IsUndefined(b_flags);

  if (both_regexp) {
    int source_equal = JS_StrictEq(ctx, a_source, b_source) == 1;
    int flags_equal = JS_StrictEq(ctx, a_flags, b_flags) == 1;
    JS_FreeValue(ctx, a_source);
    JS_FreeValue(ctx, b_source);
    JS_FreeValue(ctx, a_flags);
    JS_FreeValue(ctx, b_flags);
    return (source_equal && flags_equal) ? 1 : 0;
  }
  JS_FreeValue(ctx, a_source);
  JS_FreeValue(ctx, b_source);
  JS_FreeValue(ctx, a_flags);
  JS_FreeValue(ctx, b_flags);

  // Check if Set objects
  JSValue a_size = JS_GetPropertyStr(ctx, a, "size");
  JSValue b_size = JS_GetPropertyStr(ctx, b, "size");
  int both_sets = !JS_IsUndefined(a_size) && !JS_IsUndefined(b_size);

  if (both_sets) {
    // For Sets, check size and contents
    int size_equal = JS_StrictEq(ctx, a_size, b_size) == 1;
    JS_FreeValue(ctx, a_size);
    JS_FreeValue(ctx, b_size);

    if (!size_equal)
      return 0;

    // Convert Sets to arrays and compare
    JSValue a_array = JS_Invoke(ctx, JS_GetGlobalObject(ctx), JS_NewAtom(ctx, "Array"), 1, &a);
    JSValue from_func = JS_GetPropertyStr(ctx, a_array, "from");
    JSValue a_arr = JS_Call(ctx, from_func, JS_UNDEFINED, 1, &a);
    JSValue b_arr = JS_Call(ctx, from_func, JS_UNDEFINED, 1, &b);
    JS_FreeValue(ctx, from_func);
    JS_FreeValue(ctx, a_array);

    int result = deep_strict_equal_internal(ctx, a_arr, b_arr, visited);
    JS_FreeValue(ctx, a_arr);
    JS_FreeValue(ctx, b_arr);
    return result;
  }
  JS_FreeValue(ctx, a_size);
  JS_FreeValue(ctx, b_size);

  // For plain objects, compare own properties
  JSPropertyEnum* a_props = NULL;
  JSPropertyEnum* b_props = NULL;
  uint32_t a_count = 0;
  uint32_t b_count = 0;

  if (JS_GetOwnPropertyNames(ctx, &a_props, &a_count, a, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
    return -1;
  }

  if (JS_GetOwnPropertyNames(ctx, &b_props, &b_count, b, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
    js_free(ctx, a_props);
    return -1;
  }

  // Compare number of properties
  if (a_count != b_count) {
    js_free(ctx, a_props);
    js_free(ctx, b_props);
    return 0;
  }

  // Compare each property value
  int result = 1;
  for (uint32_t i = 0; i < a_count && result == 1; i++) {
    JSAtom atom = a_props[i].atom;
    const char* key_str = JS_AtomToCString(ctx, atom);

    // Check if key exists in b
    int has_prop = JS_HasProperty(ctx, b, atom);
    if (has_prop <= 0) {
      JS_FreeCString(ctx, key_str);
      result = 0;
      break;
    }

    // Get property values
    JSValue a_val = JS_GetProperty(ctx, a, atom);
    JSValue b_val = JS_GetProperty(ctx, b, atom);

    int val_equal = deep_strict_equal_internal(ctx, a_val, b_val, visited);

    JS_FreeCString(ctx, key_str);
    JS_FreeValue(ctx, a_val);
    JS_FreeValue(ctx, b_val);

    if (val_equal != 1) {
      result = val_equal;
    }
  }

  js_free(ctx, a_props);
  js_free(ctx, b_props);

  return result;
}

// Simple deep equal implementation - only handles basic types
static int deep_equal(JSContext* ctx, JSValueConst a, JSValueConst b) {
  // First try strict equality
  int strict = JS_StrictEq(ctx, a, b);
  if (strict == 1)
    return 1;
  if (strict == -1)
    return 0;  // error

  // Check types
  if (JS_VALUE_GET_TAG(a) != JS_VALUE_GET_TAG(b))
    return 0;

  // For objects and arrays, do a simple JSON comparison
  if (JS_IsObject(a) && JS_IsObject(b)) {
    JSValue json_a = JS_JSONStringify(ctx, a, JS_UNDEFINED, JS_UNDEFINED);
    JSValue json_b = JS_JSONStringify(ctx, b, JS_UNDEFINED, JS_UNDEFINED);

    if (JS_IsException(json_a) || JS_IsException(json_b)) {
      JS_FreeValue(ctx, json_a);
      JS_FreeValue(ctx, json_b);
      return 0;
    }

    int result = JS_StrictEq(ctx, json_a, json_b) == 1;
    JS_FreeValue(ctx, json_a);
    JS_FreeValue(ctx, json_b);
    return result;
  }

  return 0;
}

// assert.deepEqual(actual, expected[, message])
static JSValue jsrt_assert_deepEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.deepEqual requires at least 2 arguments");
  }

  if (!deep_equal(ctx, argv[0], argv[1])) {
    const char* message = "Expected values to be deeply equal";
    int generated = 1;
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
      generated = 0;
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = create_assertion_error(ctx, message, argv[0], argv[1], "deepEqual", generated);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// assert.notDeepEqual(actual, expected[, message])
static JSValue jsrt_assert_notDeepEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.notDeepEqual requires at least 2 arguments");
  }

  if (deep_equal(ctx, argv[0], argv[1])) {
    const char* message = "Expected values to be not deeply equal";
    int generated = 1;
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
      generated = 0;
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = create_assertion_error(ctx, message, argv[0], argv[1], "notDeepEqual", generated);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// assert.deepStrictEqual(actual, expected[, message])
static JSValue jsrt_assert_deepStrictEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.deepStrictEqual requires at least 2 arguments");
  }

  // Create visited array for circular reference detection
  JSValue visited = JS_NewArray(ctx);
  int result = deep_strict_equal_internal(ctx, argv[0], argv[1], visited);
  JS_FreeValue(ctx, visited);

  if (result != 1) {
    const char* message = "Expected values to be deeply and strictly equal";
    int generated = 1;
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
      generated = 0;
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = create_assertion_error(ctx, message, argv[0], argv[1], "deepStrictEqual", generated);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// assert.notDeepStrictEqual(actual, expected[, message])
static JSValue jsrt_assert_notDeepStrictEqual(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.notDeepStrictEqual requires at least 2 arguments");
  }

  // Create visited array for circular reference detection
  JSValue visited = JS_NewArray(ctx);
  int result = deep_strict_equal_internal(ctx, argv[0], argv[1], visited);
  JS_FreeValue(ctx, visited);

  if (result == 1) {
    const char* message = "Expected values not to be deeply and strictly equal";
    int generated = 1;
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
      generated = 0;
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = create_assertion_error(ctx, message, argv[0], argv[1], "notDeepStrictEqual", generated);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// Helper: Validate thrown error against expected criteria
static int validate_thrown_error(JSContext* ctx, JSValue error, JSValueConst expected) {
  // If expected is a constructor function, check instanceof
  if (JS_IsFunction(ctx, expected)) {
    // Check if error is instance of expected constructor
    JSValue instanceof_result = JS_Eval(ctx, "null", 4, "<internal>", 0);

    // Try to use instanceof - get the constructor name
    JSValue error_constructor = JS_GetPropertyStr(ctx, error, "constructor");
    JSValue expected_name = JS_GetPropertyStr(ctx, expected, "name");
    JSValue error_name = JS_GetPropertyStr(ctx, error_constructor, "name");

    const char* expected_name_str = JS_ToCString(ctx, expected_name);
    const char* error_name_str = JS_ToCString(ctx, error_name);

    int match = (expected_name_str && error_name_str && strcmp(expected_name_str, error_name_str) == 0);

    if (expected_name_str)
      JS_FreeCString(ctx, expected_name_str);
    if (error_name_str)
      JS_FreeCString(ctx, error_name_str);

    JS_FreeValue(ctx, error_constructor);
    JS_FreeValue(ctx, expected_name);
    JS_FreeValue(ctx, error_name);
    JS_FreeValue(ctx, instanceof_result);

    return match;
  }

  // If expected is a string, check if error message includes it
  if (JS_IsString(expected)) {
    JSValue error_message = JS_GetPropertyStr(ctx, error, "message");
    const char* error_msg_str = JS_ToCString(ctx, error_message);
    const char* expected_str = JS_ToCString(ctx, expected);

    int match = (error_msg_str && expected_str && strstr(error_msg_str, expected_str) != NULL);

    if (error_msg_str)
      JS_FreeCString(ctx, error_msg_str);
    if (expected_str)
      JS_FreeCString(ctx, expected_str);

    JS_FreeValue(ctx, error_message);

    return match;
  }

  // If expected is a RegExp, test error message against it
  JSValue test_method = JS_GetPropertyStr(ctx, expected, "test");
  if (JS_IsFunction(ctx, test_method)) {
    JSValue error_message = JS_GetPropertyStr(ctx, error, "message");
    JSValue test_args[1] = {error_message};
    JSValue test_result = JS_Call(ctx, test_method, expected, 1, test_args);

    int match = JS_ToBool(ctx, test_result);

    JS_FreeValue(ctx, error_message);
    JS_FreeValue(ctx, test_result);
    JS_FreeValue(ctx, test_method);

    return match;
  }
  JS_FreeValue(ctx, test_method);

  // If expected is an object, check error properties
  if (JS_IsObject(expected)) {
    JSPropertyEnum* props = NULL;
    uint32_t prop_count = 0;

    if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, expected, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
      return 0;
    }

    int all_match = 1;
    for (uint32_t i = 0; i < prop_count; i++) {
      JSValue expected_val = JS_GetProperty(ctx, expected, props[i].atom);
      JSValue error_val = JS_GetProperty(ctx, error, props[i].atom);

      int match = JS_StrictEq(ctx, expected_val, error_val) == 1;

      JS_FreeValue(ctx, expected_val);
      JS_FreeValue(ctx, error_val);

      if (!match) {
        all_match = 0;
        break;
      }
    }

    js_free(ctx, props);
    return all_match;
  }

  // If expected is a function (validator), call it with error
  // This is handled above with JS_IsFunction check

  return 1;  // No validation criteria, accept any error
}

// assert.throws(block[, error][, message])
static JSValue jsrt_assert_throws(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return throw_assertion_error(ctx, "assert.throws requires at least 1 argument");
  }

  if (!JS_IsFunction(ctx, argv[0])) {
    return throw_assertion_error(ctx, "assert.throws expects a function as first argument");
  }

  JSValue result = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, NULL);

  if (!JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    const char* message = "Expected function to throw";
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
    } else if (argc > 1 && JS_IsString(argv[argc - 1])) {
      message = JS_ToCString(ctx, argv[argc - 1]);
    }

    JSValue error = throw_assertion_error(ctx, message);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    } else if (argc > 1 && JS_IsString(argv[argc - 1])) {
      JS_FreeCString(ctx, message);
    }

    return error;
  }

  // Get the thrown error
  JSValue error = JS_GetException(ctx);

  // If error validation is provided (argc >= 2 and argv[1] is not a string message)
  if (argc >= 2 && !JS_IsString(argv[1])) {
    int validation_passed = validate_thrown_error(ctx, error, argv[1]);

    if (!validation_passed) {
      const char* message = "The error thrown does not match the expected error";
      if (argc > 2 && JS_IsString(argv[2])) {
        message = JS_ToCString(ctx, argv[2]);
      }

      JS_FreeValue(ctx, error);
      JSValue err = throw_assertion_error(ctx, message);

      if (argc > 2 && JS_IsString(argv[2])) {
        JS_FreeCString(ctx, message);
      }

      return err;
    }
  }

  // Return the error for inspection (Node.js behavior)
  return error;
}

// assert.doesNotThrow(block[, error][, message])
static JSValue jsrt_assert_doesNotThrow(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return throw_assertion_error(ctx, "assert.doesNotThrow requires at least 1 argument");
  }

  if (!JS_IsFunction(ctx, argv[0])) {
    return throw_assertion_error(ctx, "assert.doesNotThrow expects a function as first argument");
  }

  JSValue result = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, NULL);

  if (JS_IsException(result)) {
    const char* message = "Expected function not to throw";
    if (argc > 1 && JS_IsString(argv[argc - 1])) {
      message = JS_ToCString(ctx, argv[argc - 1]);
    }

    JSValue error = throw_assertion_error(ctx, message);

    if (argc > 1 && JS_IsString(argv[argc - 1])) {
      JS_FreeCString(ctx, message);
    }

    return error;
  }

  JS_FreeValue(ctx, result);
  return JS_UNDEFINED;
}

// assert.fail([message])
static JSValue jsrt_assert_fail(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* message = "Failed";
  int needs_free = 0;

  if (argc > 0 && JS_IsString(argv[0])) {
    message = JS_ToCString(ctx, argv[0]);
    needs_free = 1;
  }

  JSValue error = create_assertion_error(ctx, message, JS_UNDEFINED, JS_UNDEFINED, "fail", 0);

  if (needs_free) {
    JS_FreeCString(ctx, message);
  }

  return JS_Throw(ctx, error);
}

// assert.ifError(value)
static JSValue jsrt_assert_ifError(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_UNDEFINED;
  }

  JSValue value = argv[0];
  int is_falsy = !JS_ToBool(ctx, value);

  if (is_falsy) {
    return JS_UNDEFINED;
  }

  // Value is truthy - throw it or create error from it
  if (JS_IsError(ctx, value)) {
    // It's already an error, throw it
    return JS_Throw(ctx, JS_DupValue(ctx, value));
  } else {
    // Not an error, create AssertionError with the value
    const char* val_str = JS_ToCString(ctx, value);
    char message[256];
    snprintf(message, sizeof(message), "ifError got unwanted exception: %s", val_str ? val_str : "unknown");
    if (val_str) {
      JS_FreeCString(ctx, val_str);
    }

    JSValue error = create_assertion_error(ctx, message, value, JS_UNDEFINED, "ifError", 1);
    return JS_Throw(ctx, error);
  }
}

// assert.match(string, regexp[, message])
static JSValue jsrt_assert_match(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.match requires at least 2 arguments");
  }

  if (!JS_IsString(argv[0])) {
    return throw_assertion_error(ctx, "assert.match expects a string as first argument");
  }

  // Check if second argument is a RegExp by checking if it has a test method
  JSValue test_method = JS_GetPropertyStr(ctx, argv[1], "test");
  int is_regexp = JS_IsFunction(ctx, test_method);
  JS_FreeValue(ctx, test_method);

  if (!is_regexp) {
    return throw_assertion_error(ctx, "assert.match expects a RegExp as second argument");
  }

  // Call regexp.test(string)
  JSValue test_args[1] = {argv[0]};
  JSValue test_result = JS_Invoke(ctx, argv[1], JS_NewAtom(ctx, "test"), 1, test_args);

  if (JS_IsException(test_result)) {
    return test_result;
  }

  int matches = JS_ToBool(ctx, test_result);
  JS_FreeValue(ctx, test_result);

  if (!matches) {
    const char* message = "The input did not match the regular expression";
    int generated = 1;
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
      generated = 0;
    }

    JSValue error = create_assertion_error(ctx, message, argv[0], argv[1], "match", generated);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// assert.doesNotMatch(string, regexp[, message])
static JSValue jsrt_assert_doesNotMatch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.doesNotMatch requires at least 2 arguments");
  }

  if (!JS_IsString(argv[0])) {
    return throw_assertion_error(ctx, "assert.doesNotMatch expects a string as first argument");
  }

  // Check if second argument is a RegExp
  JSValue test_method = JS_GetPropertyStr(ctx, argv[1], "test");
  int is_regexp = JS_IsFunction(ctx, test_method);
  JS_FreeValue(ctx, test_method);

  if (!is_regexp) {
    return throw_assertion_error(ctx, "assert.doesNotMatch expects a RegExp as second argument");
  }

  // Call regexp.test(string)
  JSValue test_args[1] = {argv[0]};
  JSValue test_result = JS_Invoke(ctx, argv[1], JS_NewAtom(ctx, "test"), 1, test_args);

  if (JS_IsException(test_result)) {
    return test_result;
  }

  int matches = JS_ToBool(ctx, test_result);
  JS_FreeValue(ctx, test_result);

  if (matches) {
    const char* message = "The input was expected to not match the regular expression";
    int generated = 1;
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
      generated = 0;
    }

    JSValue error = create_assertion_error(ctx, message, argv[0], argv[1], "doesNotMatch", generated);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// assert.rejects(asyncFn[, error][, message])
// Returns a Promise that resolves if the async function/promise rejects
static JSValue jsrt_assert_rejects(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "assert.rejects requires at least 1 argument");
  }

  // Get the promise - either call the function or use the promise directly
  JSValue promise;
  if (JS_IsFunction(ctx, argv[0])) {
    promise = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, NULL);
    if (JS_IsException(promise)) {
      return promise;
    }
  } else {
    promise = JS_DupValue(ctx, argv[0]);
  }

  // Check if it's a promise (has then method)
  JSValue then_method = JS_GetPropertyStr(ctx, promise, "then");
  if (!JS_IsFunction(ctx, then_method)) {
    JS_FreeValue(ctx, then_method);
    JS_FreeValue(ctx, promise);
    return JS_ThrowTypeError(ctx, "assert.rejects expects a Promise or async function");
  }
  JS_FreeValue(ctx, then_method);

  // Create a new promise to return
  JSValue promise_constructor = JS_GetGlobalObject(ctx);
  JSValue promise_ctor = JS_GetPropertyStr(ctx, promise_constructor, "Promise");
  JS_FreeValue(ctx, promise_constructor);

  // Create executor function for the returned promise
  const char* executor_code =
      "(resolve, reject, original_promise, expected, message) => {\n"
      "  original_promise.then(\n"
      "    (value) => {\n"
      "      const err = new Error(message || 'Expected promise to be rejected');\n"
      "      err.name = 'AssertionError';\n"
      "      err.code = 'ERR_ASSERTION';\n"
      "      reject(err);\n"
      "    },\n"
      "    (error) => {\n"
      "      // Promise rejected as expected\n"
      "      // TODO: Validate error against expected if provided\n"
      "      resolve(error);\n"
      "    }\n"
      "  );\n"
      "}";

  JSValue executor_func = JS_Eval(ctx, executor_code, strlen(executor_code), "<rejects>", JS_EVAL_TYPE_GLOBAL);

  if (JS_IsException(executor_func)) {
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, promise_ctor);
    return executor_func;
  }

  // Build arguments for executor: (original_promise, expected, message)
  JSValue expected = argc >= 2 ? JS_DupValue(ctx, argv[1]) : JS_UNDEFINED;
  JSValue message = argc >= 3 && JS_IsString(argv[2]) ? JS_DupValue(ctx, argv[2]) : JS_NewString(ctx, "");

  // Call executor with Promise.then
  JSValue then = JS_GetPropertyStr(ctx, promise, "then");

  // Setup callbacks
  const char* resolve_code =
      "(resolve, reject, expected, message, error) => {\n"
      "  resolve(error);\n"
      "}";

  const char* reject_code =
      "(resolve, reject, expected, message, value) => {\n"
      "  const err = new Error(message || 'Expected promise to be rejected');\n"
      "  err.name = 'AssertionError';\n"
      "  err.code = 'ERR_ASSERTION';\n"
      "  reject(err);\n"
      "}";

  // Use a simpler approach - wrap in JS
  const char* wrapper_code =
      "(promise, expected, message) => {\n"
      "  return promise.then(\n"
      "    (value) => {\n"
      "      const err = new Error(message || 'Expected promise to be rejected');\n"
      "      err.name = 'AssertionError';\n"
      "      err.code = 'ERR_ASSERTION';\n"
      "      err.actual = value;\n"
      "      err.operator = 'rejects';\n"
      "      err.generatedMessage = !message;\n"
      "      throw err;\n"
      "    },\n"
      "    (error) => {\n"
      "      return error;\n"
      "    }\n"
      "  );\n"
      "}";

  JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<rejects-wrapper>", JS_EVAL_TYPE_GLOBAL);

  if (JS_IsException(wrapper)) {
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, expected);
    JS_FreeValue(ctx, message);
    JS_FreeValue(ctx, executor_func);
    JS_FreeValue(ctx, promise_ctor);
    JS_FreeValue(ctx, then);
    return wrapper;
  }

  JSValue args[3] = {promise, expected, message};
  JSValue result = JS_Call(ctx, wrapper, JS_UNDEFINED, 3, args);

  JS_FreeValue(ctx, wrapper);
  JS_FreeValue(ctx, promise);
  JS_FreeValue(ctx, expected);
  JS_FreeValue(ctx, message);
  JS_FreeValue(ctx, executor_func);
  JS_FreeValue(ctx, promise_ctor);
  JS_FreeValue(ctx, then);

  return result;
}

// assert.doesNotReject(asyncFn[, error][, message])
// Returns a Promise that resolves if the async function/promise resolves
static JSValue jsrt_assert_doesNotReject(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "assert.doesNotReject requires at least 1 argument");
  }

  // Get the promise - either call the function or use the promise directly
  JSValue promise;
  if (JS_IsFunction(ctx, argv[0])) {
    promise = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, NULL);
    if (JS_IsException(promise)) {
      return promise;
    }
  } else {
    promise = JS_DupValue(ctx, argv[0]);
  }

  // Check if it's a promise
  JSValue then_method = JS_GetPropertyStr(ctx, promise, "then");
  if (!JS_IsFunction(ctx, then_method)) {
    JS_FreeValue(ctx, then_method);
    JS_FreeValue(ctx, promise);
    return JS_ThrowTypeError(ctx, "assert.doesNotReject expects a Promise or async function");
  }
  JS_FreeValue(ctx, then_method);

  // Wrapper to handle rejection
  const char* wrapper_code =
      "(promise, message) => {\n"
      "  return promise.then(\n"
      "    (value) => {\n"
      "      return value;\n"
      "    },\n"
      "    (error) => {\n"
      "      const err = new Error(message || 'Expected promise not to be rejected');\n"
      "      err.name = 'AssertionError';\n"
      "      err.code = 'ERR_ASSERTION';\n"
      "      err.actual = error;\n"
      "      err.operator = 'doesNotReject';\n"
      "      err.generatedMessage = !message;\n"
      "      throw err;\n"
      "    }\n"
      "  );\n"
      "}";

  JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<doesNotReject-wrapper>", JS_EVAL_TYPE_GLOBAL);

  if (JS_IsException(wrapper)) {
    JS_FreeValue(ctx, promise);
    return wrapper;
  }

  JSValue message = argc >= 2 && JS_IsString(argv[argc - 1]) ? JS_DupValue(ctx, argv[argc - 1]) : JS_NewString(ctx, "");

  JSValue args[2] = {promise, message};
  JSValue result = JS_Call(ctx, wrapper, JS_UNDEFINED, 2, args);

  JS_FreeValue(ctx, wrapper);
  JS_FreeValue(ctx, promise);
  JS_FreeValue(ctx, message);

  return result;
}

// Create assert module for jsrt:assert
JSValue JSRT_CreateAssertModule(JSContext* ctx) {
  // Create assert function that is also callable
  JSValue assert_func = JS_NewCFunction(ctx, jsrt_assert, "assert", 2);

  // Set methods on the assert function object
  JS_SetPropertyStr(ctx, assert_func, "ok", JS_NewCFunction(ctx, jsrt_assert_ok, "ok", 2));
  JS_SetPropertyStr(ctx, assert_func, "equal", JS_NewCFunction(ctx, jsrt_assert_equal, "equal", 3));
  JS_SetPropertyStr(ctx, assert_func, "notEqual", JS_NewCFunction(ctx, jsrt_assert_notEqual, "notEqual", 3));
  JS_SetPropertyStr(ctx, assert_func, "strictEqual", JS_NewCFunction(ctx, jsrt_assert_strictEqual, "strictEqual", 3));
  JS_SetPropertyStr(ctx, assert_func, "notStrictEqual",
                    JS_NewCFunction(ctx, jsrt_assert_notStrictEqual, "notStrictEqual", 3));
  JS_SetPropertyStr(ctx, assert_func, "deepEqual", JS_NewCFunction(ctx, jsrt_assert_deepEqual, "deepEqual", 3));
  JS_SetPropertyStr(ctx, assert_func, "notDeepEqual",
                    JS_NewCFunction(ctx, jsrt_assert_notDeepEqual, "notDeepEqual", 3));
  JS_SetPropertyStr(ctx, assert_func, "deepStrictEqual",
                    JS_NewCFunction(ctx, jsrt_assert_deepStrictEqual, "deepStrictEqual", 3));
  JS_SetPropertyStr(ctx, assert_func, "notDeepStrictEqual",
                    JS_NewCFunction(ctx, jsrt_assert_notDeepStrictEqual, "notDeepStrictEqual", 3));
  JS_SetPropertyStr(ctx, assert_func, "throws", JS_NewCFunction(ctx, jsrt_assert_throws, "throws", 3));
  JS_SetPropertyStr(ctx, assert_func, "doesNotThrow",
                    JS_NewCFunction(ctx, jsrt_assert_doesNotThrow, "doesNotThrow", 3));
  JS_SetPropertyStr(ctx, assert_func, "fail", JS_NewCFunction(ctx, jsrt_assert_fail, "fail", 1));
  JS_SetPropertyStr(ctx, assert_func, "ifError", JS_NewCFunction(ctx, jsrt_assert_ifError, "ifError", 1));
  JS_SetPropertyStr(ctx, assert_func, "match", JS_NewCFunction(ctx, jsrt_assert_match, "match", 3));
  JS_SetPropertyStr(ctx, assert_func, "doesNotMatch",
                    JS_NewCFunction(ctx, jsrt_assert_doesNotMatch, "doesNotMatch", 3));
  JS_SetPropertyStr(ctx, assert_func, "rejects", JS_NewCFunction(ctx, jsrt_assert_rejects, "rejects", 3));
  JS_SetPropertyStr(ctx, assert_func, "doesNotReject",
                    JS_NewCFunction(ctx, jsrt_assert_doesNotReject, "doesNotReject", 3));

  // Create assert.strict namespace
  JSValue strict_obj = JS_NewObject(ctx);

  // In strict mode, equal() maps to strictEqual(), deepEqual() maps to deepStrictEqual()
  JS_SetPropertyStr(ctx, strict_obj, "ok", JS_GetPropertyStr(ctx, assert_func, "ok"));
  JS_SetPropertyStr(ctx, strict_obj, "equal", JS_GetPropertyStr(ctx, assert_func, "strictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "notEqual", JS_GetPropertyStr(ctx, assert_func, "notStrictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "strictEqual", JS_GetPropertyStr(ctx, assert_func, "strictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "notStrictEqual", JS_GetPropertyStr(ctx, assert_func, "notStrictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "deepEqual", JS_GetPropertyStr(ctx, assert_func, "deepStrictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "notDeepEqual", JS_GetPropertyStr(ctx, assert_func, "notDeepStrictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "deepStrictEqual", JS_GetPropertyStr(ctx, assert_func, "deepStrictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "notDeepStrictEqual", JS_GetPropertyStr(ctx, assert_func, "notDeepStrictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "throws", JS_GetPropertyStr(ctx, assert_func, "throws"));
  JS_SetPropertyStr(ctx, strict_obj, "doesNotThrow", JS_GetPropertyStr(ctx, assert_func, "doesNotThrow"));
  JS_SetPropertyStr(ctx, strict_obj, "fail", JS_GetPropertyStr(ctx, assert_func, "fail"));
  JS_SetPropertyStr(ctx, strict_obj, "ifError", JS_GetPropertyStr(ctx, assert_func, "ifError"));
  JS_SetPropertyStr(ctx, strict_obj, "match", JS_GetPropertyStr(ctx, assert_func, "match"));
  JS_SetPropertyStr(ctx, strict_obj, "doesNotMatch", JS_GetPropertyStr(ctx, assert_func, "doesNotMatch"));
  JS_SetPropertyStr(ctx, strict_obj, "rejects", JS_GetPropertyStr(ctx, assert_func, "rejects"));
  JS_SetPropertyStr(ctx, strict_obj, "doesNotReject", JS_GetPropertyStr(ctx, assert_func, "doesNotReject"));

  JS_SetPropertyStr(ctx, assert_func, "strict", strict_obj);

  return assert_func;
}