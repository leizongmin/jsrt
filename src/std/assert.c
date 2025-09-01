#include "assert.h"

#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/colorize.h"

// Forward declarations
static JSValue jsrt_assert(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_assert_ok(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_assert_equal(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_assert_notEqual(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_assert_strictEqual(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_assert_notStrictEqual(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_assert_deepEqual(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_assert_notDeepEqual(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_assert_throws(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_assert_doesNotThrow(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// Helper function to format assertion error message
static void print_assertion_error(JSContext *ctx, const char *message, JSValueConst actual, JSValueConst expected) {
  printf("%sAssertionError: %s", jsrt_colorize(JSRT_ColorizeRed, -1, JSRT_ColorizeBold), JSRT_ColorizeClear);
  printf("%s\n", message);

  if (!JS_IsUndefined(actual)) {
    const char *actual_str = JS_ToCString(ctx, actual);
    if (actual_str) {
      printf("  actual: %s\n", actual_str);
      JS_FreeCString(ctx, actual_str);
    }
  }

  if (!JS_IsUndefined(expected)) {
    const char *expected_str = JS_ToCString(ctx, expected);
    if (expected_str) {
      printf("  expected: %s\n", expected_str);
      JS_FreeCString(ctx, expected_str);
    }
  }
}

// Helper function to throw AssertionError
static JSValue throw_assertion_error(JSContext *ctx, const char *message) {
  JSValue error = JS_NewError(ctx);
  JS_SetPropertyStr(ctx, error, "name", JS_NewString(ctx, "AssertionError"));
  JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, message));
  return JS_Throw(ctx, error);
}

// assert(value[, message])
static JSValue jsrt_assert(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return throw_assertion_error(ctx, "No assertion provided");
  }

  int32_t value = JS_ToBool(ctx, argv[0]);
  if (value <= 0) {
    const char *message = "Assertion failed";
    if (argc > 1 && JS_IsString(argv[1])) {
      message = JS_ToCString(ctx, argv[1]);
    }

    print_assertion_error(ctx, message, argv[0], JS_UNDEFINED);
    JSValue error = throw_assertion_error(ctx, message);

    if (argc > 1 && JS_IsString(argv[1])) {
      JS_FreeCString(ctx, message);
    }

    return error;
  }

  return JS_UNDEFINED;
}

// assert.ok(value[, message]) - alias for assert()
static JSValue jsrt_assert_ok(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  return jsrt_assert(ctx, this_val, argc, argv);
}

// assert.equal(actual, expected[, message])
static JSValue jsrt_assert_equal(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.equal requires at least 2 arguments");
  }

  JSValue val1 = JS_DupValue(ctx, argv[0]);
  JSValue val2 = JS_DupValue(ctx, argv[1]);
  int equal = JS_StrictEq(ctx, val1, val2);
  // For loose equality, convert to string and compare
  if (equal != 1) {
    const char *str1 = JS_ToCString(ctx, val1);
    const char *str2 = JS_ToCString(ctx, val2);
    if (str1 && str2) {
      equal = (strcmp(str1, str2) == 0) ? 1 : 0;
    }
    if (str1) JS_FreeCString(ctx, str1);
    if (str2) JS_FreeCString(ctx, str2);
  }
  JS_FreeValue(ctx, val1);
  JS_FreeValue(ctx, val2);
  equal = (equal == 1);
  if (!equal) {
    const char *message = "Expected values to be equal (==)";
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = throw_assertion_error(ctx, message);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return error;
  }

  return JS_UNDEFINED;
}

// assert.notEqual(actual, expected[, message])
static JSValue jsrt_assert_notEqual(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.notEqual requires at least 2 arguments");
  }

  JSValue val1 = JS_DupValue(ctx, argv[0]);
  JSValue val2 = JS_DupValue(ctx, argv[1]);
  int equal = JS_StrictEq(ctx, val1, val2);
  // For loose equality, convert to string and compare
  if (equal != 1) {
    const char *str1 = JS_ToCString(ctx, val1);
    const char *str2 = JS_ToCString(ctx, val2);
    if (str1 && str2) {
      equal = (strcmp(str1, str2) == 0) ? 1 : 0;
    }
    if (str1) JS_FreeCString(ctx, str1);
    if (str2) JS_FreeCString(ctx, str2);
  }
  JS_FreeValue(ctx, val1);
  JS_FreeValue(ctx, val2);
  equal = (equal == 1);
  if (equal) {
    const char *message = "Expected values to be not equal (!=)";
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = throw_assertion_error(ctx, message);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return error;
  }

  return JS_UNDEFINED;
}

// assert.strictEqual(actual, expected[, message])
static JSValue jsrt_assert_strictEqual(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.strictEqual requires at least 2 arguments");
  }

  int equal = JS_StrictEq(ctx, argv[0], argv[1]);
  if (equal != 1) {
    const char *message = "Expected values to be strictly equal (===)";
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = throw_assertion_error(ctx, message);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return error;
  }

  return JS_UNDEFINED;
}

// assert.notStrictEqual(actual, expected[, message])
static JSValue jsrt_assert_notStrictEqual(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.notStrictEqual requires at least 2 arguments");
  }

  int equal = JS_StrictEq(ctx, argv[0], argv[1]);
  if (equal == 1) {
    const char *message = "Expected values to be not strictly equal (!==)";
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = throw_assertion_error(ctx, message);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return error;
  }

  return JS_UNDEFINED;
}

// Simple deep equal implementation - only handles basic types
static int deep_equal(JSContext *ctx, JSValueConst a, JSValueConst b) {
  // First try strict equality
  int strict = JS_StrictEq(ctx, a, b);
  if (strict == 1) return 1;
  if (strict == -1) return 0;  // error

  // Check types
  if (JS_VALUE_GET_TAG(a) != JS_VALUE_GET_TAG(b)) return 0;

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
static JSValue jsrt_assert_deepEqual(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.deepEqual requires at least 2 arguments");
  }

  if (!deep_equal(ctx, argv[0], argv[1])) {
    const char *message = "Expected values to be deeply equal";
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = throw_assertion_error(ctx, message);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return error;
  }

  return JS_UNDEFINED;
}

// assert.notDeepEqual(actual, expected[, message])
static JSValue jsrt_assert_notDeepEqual(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) {
    return throw_assertion_error(ctx, "assert.notDeepEqual requires at least 2 arguments");
  }

  if (deep_equal(ctx, argv[0], argv[1])) {
    const char *message = "Expected values to be not deeply equal";
    if (argc > 2 && JS_IsString(argv[2])) {
      message = JS_ToCString(ctx, argv[2]);
    }

    print_assertion_error(ctx, message, argv[0], argv[1]);
    JSValue error = throw_assertion_error(ctx, message);

    if (argc > 2 && JS_IsString(argv[2])) {
      JS_FreeCString(ctx, message);
    }

    return error;
  }

  return JS_UNDEFINED;
}

// assert.throws(block[, error][, message])
static JSValue jsrt_assert_throws(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return throw_assertion_error(ctx, "assert.throws requires at least 1 argument");
  }

  if (!JS_IsFunction(ctx, argv[0])) {
    return throw_assertion_error(ctx, "assert.throws expects a function as first argument");
  }

  JSValue result = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, NULL);

  if (!JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    const char *message = "Expected function to throw";
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
static JSValue jsrt_assert_doesNotThrow(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return throw_assertion_error(ctx, "assert.doesNotThrow requires at least 1 argument");
  }

  if (!JS_IsFunction(ctx, argv[0])) {
    return throw_assertion_error(ctx, "assert.doesNotThrow expects a function as first argument");
  }

  JSValue result = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, NULL);

  if (JS_IsException(result)) {
    const char *message = "Expected function not to throw";
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

// Initialize assert module
void JSRT_RuntimeSetupStdAssert(JSRT_Runtime *rt) {
  // Create assert object
  JSValue assert = JS_NewObject(rt->ctx);

  // Set main assert function (callable object)
  JS_SetPropertyStr(rt->ctx, rt->global, "assert", JS_NewCFunction(rt->ctx, jsrt_assert, "assert", 2));

  // Create assert module object with methods
  JSValue assert_module = JS_NewObject(rt->ctx);

  // Set methods on assert module
  JS_SetPropertyStr(rt->ctx, assert_module, "ok", JS_NewCFunction(rt->ctx, jsrt_assert_ok, "ok", 2));
  JS_SetPropertyStr(rt->ctx, assert_module, "equal", JS_NewCFunction(rt->ctx, jsrt_assert_equal, "equal", 3));
  JS_SetPropertyStr(rt->ctx, assert_module, "notEqual", JS_NewCFunction(rt->ctx, jsrt_assert_notEqual, "notEqual", 3));
  JS_SetPropertyStr(rt->ctx, assert_module, "strictEqual",
                    JS_NewCFunction(rt->ctx, jsrt_assert_strictEqual, "strictEqual", 3));
  JS_SetPropertyStr(rt->ctx, assert_module, "notStrictEqual",
                    JS_NewCFunction(rt->ctx, jsrt_assert_notStrictEqual, "notStrictEqual", 3));
  JS_SetPropertyStr(rt->ctx, assert_module, "deepEqual",
                    JS_NewCFunction(rt->ctx, jsrt_assert_deepEqual, "deepEqual", 3));
  JS_SetPropertyStr(rt->ctx, assert_module, "notDeepEqual",
                    JS_NewCFunction(rt->ctx, jsrt_assert_notDeepEqual, "notDeepEqual", 3));
  JS_SetPropertyStr(rt->ctx, assert_module, "throws", JS_NewCFunction(rt->ctx, jsrt_assert_throws, "throws", 3));
  JS_SetPropertyStr(rt->ctx, assert_module, "doesNotThrow",
                    JS_NewCFunction(rt->ctx, jsrt_assert_doesNotThrow, "doesNotThrow", 3));

  // Also set methods on global assert function
  JSValue global_assert = JS_GetPropertyStr(rt->ctx, rt->global, "assert");
  JS_SetPropertyStr(rt->ctx, global_assert, "ok", JS_NewCFunction(rt->ctx, jsrt_assert_ok, "ok", 2));
  JS_SetPropertyStr(rt->ctx, global_assert, "equal", JS_NewCFunction(rt->ctx, jsrt_assert_equal, "equal", 3));
  JS_SetPropertyStr(rt->ctx, global_assert, "notEqual", JS_NewCFunction(rt->ctx, jsrt_assert_notEqual, "notEqual", 3));
  JS_SetPropertyStr(rt->ctx, global_assert, "strictEqual",
                    JS_NewCFunction(rt->ctx, jsrt_assert_strictEqual, "strictEqual", 3));
  JS_SetPropertyStr(rt->ctx, global_assert, "notStrictEqual",
                    JS_NewCFunction(rt->ctx, jsrt_assert_notStrictEqual, "notStrictEqual", 3));
  JS_SetPropertyStr(rt->ctx, global_assert, "deepEqual",
                    JS_NewCFunction(rt->ctx, jsrt_assert_deepEqual, "deepEqual", 3));
  JS_SetPropertyStr(rt->ctx, global_assert, "notDeepEqual",
                    JS_NewCFunction(rt->ctx, jsrt_assert_notDeepEqual, "notDeepEqual", 3));
  JS_SetPropertyStr(rt->ctx, global_assert, "throws", JS_NewCFunction(rt->ctx, jsrt_assert_throws, "throws", 3));
  JS_SetPropertyStr(rt->ctx, global_assert, "doesNotThrow",
                    JS_NewCFunction(rt->ctx, jsrt_assert_doesNotThrow, "doesNotThrow", 3));

  JS_FreeValue(rt->ctx, global_assert);
  JS_FreeValue(rt->ctx, assert_module);
}