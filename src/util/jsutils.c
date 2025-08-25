#include "jsutils.h"

#include <quickjs.h>

#include "macro.h"

const char* JSRT_GetTypeofJSValue(JSContext* ctx, JSValue value) {
  uint32_t tag = JS_VALUE_GET_NORM_TAG(value);
  switch (tag) {
    case JS_TAG_BIG_INT:
    case JS_TAG_SHORT_BIG_INT:
      return "bigint";
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
      return "number";
    case JS_TAG_UNDEFINED:
      return "undefined";
    case JS_TAG_BOOL:
      return "boolean";
    case JS_TAG_STRING:
      return "string";
    case JS_TAG_OBJECT: {
      if (JS_IsFunction(ctx, value)) {
        return "function";
      } else {
        goto obj_type;
      }
    } break;
    case JS_TAG_NULL:
    obj_type:
      return "object";
    case JS_TAG_SYMBOL:
      return "symbol";
    default:
      return "unknown";
  }
}
