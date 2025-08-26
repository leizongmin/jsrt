#include "clone.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"
#include "../util/list.h"

// Circular reference tracking
typedef struct JSRT_CloneEntry {
  JSValue original;
  JSValue clone;
  struct JSRT_CloneEntry *next;
} JSRT_CloneEntry;

typedef struct {
  JSRT_CloneEntry *entries;
} JSRT_CloneMap;

static JSRT_CloneMap *JSRT_CloneMapNew() {
  JSRT_CloneMap *map = malloc(sizeof(JSRT_CloneMap));
  map->entries = NULL;
  return map;
}

static void JSRT_CloneMapFree(JSContext *ctx, JSRT_CloneMap *map) {
  if (map) {
    JSRT_CloneEntry *entry = map->entries;
    while (entry) {
      JSRT_CloneEntry *next = entry->next;
      JS_FreeValue(ctx, entry->original);
      JS_FreeValue(ctx, entry->clone);
      free(entry);
      entry = next;
    }
    free(map);
  }
}

static JSValue JSRT_CloneMapGet(JSRT_CloneMap *map, JSContext *ctx, JSValue original) {
  JSRT_CloneEntry *entry = map->entries;
  while (entry) {
    if (JS_VALUE_GET_PTR(entry->original) == JS_VALUE_GET_PTR(original)) {
      return JS_DupValue(ctx, entry->clone);
    }
    entry = entry->next;
  }
  return JS_UNDEFINED;
}

static void JSRT_CloneMapSet(JSRT_CloneMap *map, JSContext *ctx, JSValue original, JSValue clone) {
  JSRT_CloneEntry *entry = malloc(sizeof(JSRT_CloneEntry));
  entry->original = JS_DupValue(ctx, original);
  entry->clone = JS_DupValue(ctx, clone);
  entry->next = map->entries;
  map->entries = entry;
}

// Forward declaration
static JSValue JSRT_CloneValueInternal(JSContext *ctx, JSValue value, JSRT_CloneMap *map);

static JSValue JSRT_CloneArray(JSContext *ctx, JSValue array, JSRT_CloneMap *map) {
  JSValue length_val = JS_GetPropertyStr(ctx, array, "length");
  if (JS_IsException(length_val)) {
    return JS_EXCEPTION;
  }

  uint32_t length;
  if (JS_ToUint32(ctx, &length, length_val)) {
    JS_FreeValue(ctx, length_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length_val);

  JSValue cloned_array = JS_NewArray(ctx);
  if (JS_IsException(cloned_array)) {
    return JS_EXCEPTION;
  }

  // Add to map before cloning elements to handle circular references
  JSRT_CloneMapSet(map, ctx, array, cloned_array);

  for (uint32_t i = 0; i < length; i++) {
    JSValue element = JS_GetPropertyUint32(ctx, array, i);
    if (JS_IsException(element)) {
      JS_FreeValue(ctx, cloned_array);
      return JS_EXCEPTION;
    }

    JSValue cloned_element = JSRT_CloneValueInternal(ctx, element, map);
    JS_FreeValue(ctx, element);

    if (JS_IsException(cloned_element)) {
      JS_FreeValue(ctx, cloned_array);
      return JS_EXCEPTION;
    }

    if (JS_SetPropertyUint32(ctx, cloned_array, i, cloned_element) < 0) {
      JS_FreeValue(ctx, cloned_element);
      JS_FreeValue(ctx, cloned_array);
      return JS_EXCEPTION;
    }
  }

  return cloned_array;
}

static JSValue JSRT_CloneObject(JSContext *ctx, JSValue object, JSRT_CloneMap *map) {
  JSValue cloned_object = JS_NewObject(ctx);
  if (JS_IsException(cloned_object)) {
    return JS_EXCEPTION;
  }

  // Add to map before cloning properties to handle circular references
  JSRT_CloneMapSet(map, ctx, object, cloned_object);

  // Get property names
  JSPropertyEnum *props = NULL;
  uint32_t prop_count = 0;

  if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, object,
                             JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK | JS_GPN_ENUM_ONLY) < 0) {
    JS_FreeValue(ctx, cloned_object);
    return JS_EXCEPTION;
  }

  for (uint32_t i = 0; i < prop_count; i++) {
    JSAtom prop_atom = props[i].atom;
    JSValue prop_value = JS_GetProperty(ctx, object, prop_atom);

    if (JS_IsException(prop_value)) {
      JS_FreeValue(ctx, cloned_object);
      js_free(ctx, props);
      return JS_EXCEPTION;
    }

    JSValue cloned_value = JSRT_CloneValueInternal(ctx, prop_value, map);
    JS_FreeValue(ctx, prop_value);

    if (JS_IsException(cloned_value)) {
      JS_FreeValue(ctx, cloned_object);
      js_free(ctx, props);
      return JS_EXCEPTION;
    }

    if (JS_SetProperty(ctx, cloned_object, prop_atom, cloned_value) < 0) {
      JS_FreeValue(ctx, cloned_value);
      JS_FreeValue(ctx, cloned_object);
      js_free(ctx, props);
      return JS_EXCEPTION;
    }
  }

  js_free(ctx, props);
  return cloned_object;
}

static JSValue JSRT_CloneDate(JSContext *ctx, JSValue date) {
  // Get the time value from the Date object
  JSValue getTime = JS_GetPropertyStr(ctx, date, "getTime");
  if (JS_IsException(getTime)) {
    return JS_EXCEPTION;
  }

  JSValue time_val = JS_Call(ctx, getTime, date, 0, NULL);
  JS_FreeValue(ctx, getTime);

  if (JS_IsException(time_val)) {
    return JS_EXCEPTION;
  }

  // Create new Date with the same time
  JSValue date_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Date");
  JSValue cloned_date = JS_CallConstructor(ctx, date_ctor, 1, &time_val);
  JS_FreeValue(ctx, date_ctor);
  JS_FreeValue(ctx, time_val);

  return cloned_date;
}

static JSValue JSRT_CloneRegExp(JSContext *ctx, JSValue regexp) {
  // Get source and flags
  JSValue source = JS_GetPropertyStr(ctx, regexp, "source");
  JSValue flags = JS_GetPropertyStr(ctx, regexp, "flags");

  if (JS_IsException(source) || JS_IsException(flags)) {
    JS_FreeValue(ctx, source);
    JS_FreeValue(ctx, flags);
    return JS_EXCEPTION;
  }

  // Create new RegExp
  JSValue regexp_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "RegExp");
  JSValue args[2] = {source, flags};
  JSValue cloned_regexp = JS_CallConstructor(ctx, regexp_ctor, 2, args);

  JS_FreeValue(ctx, regexp_ctor);
  JS_FreeValue(ctx, source);
  JS_FreeValue(ctx, flags);

  return cloned_regexp;
}

static JSValue JSRT_CloneValueInternal(JSContext *ctx, JSValue value, JSRT_CloneMap *map) {
  uint32_t tag = JS_VALUE_GET_NORM_TAG(value);

  // Check for circular reference
  JSValue existing = JSRT_CloneMapGet(map, ctx, value);
  if (!JS_IsUndefined(existing)) {
    return existing;
  }

  switch (tag) {
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
    case JS_TAG_BOOL:
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
    case JS_TAG_STRING:
      // Primitive values can be directly duplicated
      return JS_DupValue(ctx, value);

    case JS_TAG_OBJECT: {
      // Check specific object types
      JSValue constructor = JS_GetPropertyStr(ctx, value, "constructor");
      if (!JS_IsException(constructor)) {
        JSValue name = JS_GetPropertyStr(ctx, constructor, "name");
        if (!JS_IsException(name)) {
          const char *name_str = JS_ToCString(ctx, name);
          if (name_str) {
            if (strcmp(name_str, "Array") == 0) {
              JS_FreeCString(ctx, name_str);
              JS_FreeValue(ctx, name);
              JS_FreeValue(ctx, constructor);
              return JSRT_CloneArray(ctx, value, map);
            } else if (strcmp(name_str, "Date") == 0) {
              JS_FreeCString(ctx, name_str);
              JS_FreeValue(ctx, name);
              JS_FreeValue(ctx, constructor);
              return JSRT_CloneDate(ctx, value);
            } else if (strcmp(name_str, "RegExp") == 0) {
              JS_FreeCString(ctx, name_str);
              JS_FreeValue(ctx, name);
              JS_FreeValue(ctx, constructor);
              return JSRT_CloneRegExp(ctx, value);
            }
            JS_FreeCString(ctx, name_str);
          }
        }
        JS_FreeValue(ctx, name);
      }
      JS_FreeValue(ctx, constructor);

      // Check if it's an array using Array.isArray
      JSValue array_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Array");
      JSValue isArray = JS_GetPropertyStr(ctx, array_ctor, "isArray");
      JSValue is_array_result = JS_Call(ctx, isArray, JS_UNDEFINED, 1, &value);
      JS_FreeValue(ctx, array_ctor);
      JS_FreeValue(ctx, isArray);

      if (!JS_IsException(is_array_result) && JS_ToBool(ctx, is_array_result)) {
        JS_FreeValue(ctx, is_array_result);
        return JSRT_CloneArray(ctx, value, map);
      }
      JS_FreeValue(ctx, is_array_result);

      // Default to object cloning
      return JSRT_CloneObject(ctx, value, map);
    }

    default:
      // Unsupported type
      return JS_ThrowTypeError(ctx, "Cannot clone this type of value");
  }
}

static JSValue JSRT_StructuredClone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "structuredClone requires 1 argument");
  }

  JSRT_CloneMap *map = JSRT_CloneMapNew();
  JSValue result = JSRT_CloneValueInternal(ctx, argv[0], map);
  JSRT_CloneMapFree(ctx, map);

  return result;
}

// Setup function
void JSRT_RuntimeSetupStdClone(JSRT_Runtime *rt) {
  JSContext *ctx = rt->ctx;

  JSRT_Debug("JSRT_RuntimeSetupStdClone: initializing Structured Clone API");

  // Add structuredClone as a global function
  JS_SetPropertyStr(ctx, rt->global, "structuredClone",
                    JS_NewCFunction(ctx, JSRT_StructuredClone, "structuredClone", 1));

  JSRT_Debug("Structured Clone API setup completed");
}