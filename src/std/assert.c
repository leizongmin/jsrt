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
static JSValue jsrt_assert_throws(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_assert_doesNotThrow(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

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
    if (argc > 1 && JS_IsString(argv[argc - 1])) {
      message = JS_ToCString(ctx, argv[argc - 1]);
    }

    JSValue error = throw_assertion_error(ctx, message);

    if (argc > 1 && JS_IsString(argv[argc - 1])) {
      JS_FreeCString(ctx, message);
    }

    return error;
  }

  // Clear the exception since we expected it
  JS_GetException(ctx);
  return JS_UNDEFINED;
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
  JS_SetPropertyStr(ctx, assert_func, "throws", JS_NewCFunction(ctx, jsrt_assert_throws, "throws", 3));
  JS_SetPropertyStr(ctx, assert_func, "doesNotThrow",
                    JS_NewCFunction(ctx, jsrt_assert_doesNotThrow, "doesNotThrow", 3));
  JS_SetPropertyStr(ctx, assert_func, "fail", JS_NewCFunction(ctx, jsrt_assert_fail, "fail", 1));
  JS_SetPropertyStr(ctx, assert_func, "ifError", JS_NewCFunction(ctx, jsrt_assert_ifError, "ifError", 1));
  JS_SetPropertyStr(ctx, assert_func, "match", JS_NewCFunction(ctx, jsrt_assert_match, "match", 3));
  JS_SetPropertyStr(ctx, assert_func, "doesNotMatch",
                    JS_NewCFunction(ctx, jsrt_assert_doesNotMatch, "doesNotMatch", 3));

  // Create assert.strict namespace
  JSValue strict_obj = JS_NewObject(ctx);

  // In strict mode, equal() maps to strictEqual(), deepEqual() maps to deepStrictEqual()
  JS_SetPropertyStr(ctx, strict_obj, "ok", JS_GetPropertyStr(ctx, assert_func, "ok"));
  JS_SetPropertyStr(ctx, strict_obj, "equal", JS_GetPropertyStr(ctx, assert_func, "strictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "notEqual", JS_GetPropertyStr(ctx, assert_func, "notStrictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "strictEqual", JS_GetPropertyStr(ctx, assert_func, "strictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "notStrictEqual", JS_GetPropertyStr(ctx, assert_func, "notStrictEqual"));
  JS_SetPropertyStr(ctx, strict_obj, "deepEqual",
                    JS_GetPropertyStr(ctx, assert_func, "deepEqual"));  // Will be deepStrictEqual in Phase 4
  JS_SetPropertyStr(ctx, strict_obj, "notDeepEqual",
                    JS_GetPropertyStr(ctx, assert_func, "notDeepEqual"));  // Will be notDeepStrictEqual in Phase 4
  JS_SetPropertyStr(ctx, strict_obj, "throws", JS_GetPropertyStr(ctx, assert_func, "throws"));
  JS_SetPropertyStr(ctx, strict_obj, "doesNotThrow", JS_GetPropertyStr(ctx, assert_func, "doesNotThrow"));
  JS_SetPropertyStr(ctx, strict_obj, "fail", JS_GetPropertyStr(ctx, assert_func, "fail"));
  JS_SetPropertyStr(ctx, strict_obj, "ifError", JS_GetPropertyStr(ctx, assert_func, "ifError"));
  JS_SetPropertyStr(ctx, strict_obj, "match", JS_GetPropertyStr(ctx, assert_func, "match"));
  JS_SetPropertyStr(ctx, strict_obj, "doesNotMatch", JS_GetPropertyStr(ctx, assert_func, "doesNotMatch"));

  JS_SetPropertyStr(ctx, assert_func, "strict", strict_obj);

  return assert_func;
}