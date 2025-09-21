#include "../url.h"

// URLSearchParams.get() method
static JSValue JSRT_URLSearchParamsGet(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "get() requires 1 argument");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  size_t name_len;
  const char* name = JS_ToCStringLen(ctx, &name_len, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (param->name_len == name_len && memcmp(param->name, name, name_len) == 0) {
      JS_FreeCString(ctx, name);
      return JS_NewStringLen(ctx, param->value, param->value_len);
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
  return JS_NULL;
}

// URLSearchParams.getAll() method
static JSValue JSRT_URLSearchParamsGetAll(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "getAll() requires 1 argument");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  size_t name_len;
  const char* name = JS_ToCStringLen(ctx, &name_len, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  JSValue result_array = JS_NewArray(ctx);
  int index = 0;

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (param->name_len == name_len && memcmp(param->name, name, name_len) == 0) {
      JSValue value = JS_NewStringLen(ctx, param->value, param->value_len);
      JS_SetPropertyUint32(ctx, result_array, index++, value);
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
  return result_array;
}

// URLSearchParams.set() method
static JSValue JSRT_URLSearchParamsSet(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "set() requires 2 arguments");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  size_t name_len, value_len;
  const char* name = JS_ToCStringLen(ctx, &name_len, argv[0]);
  const char* value = JS_ToCStringLen(ctx, &value_len, argv[1]);
  if (!name || !value) {
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);
    return JS_EXCEPTION;
  }

  // Find the first parameter with the same name and update it
  // Remove any subsequent parameters with the same name
  JSRT_URLSearchParam* first_match = NULL;
  JSRT_URLSearchParam** current = &search_params->params;

  while (*current) {
    if ((*current)->name_len == name_len && memcmp((*current)->name, name, name_len) == 0) {
      if (!first_match) {
        // This is the first match - update its value
        first_match = *current;
        free(first_match->value);
        first_match->value = malloc(value_len + 1);
        if (!first_match->value) {
          JS_FreeCString(ctx, name);
          JS_FreeCString(ctx, value);
          return JS_ThrowOutOfMemory(ctx);
        }
        memcpy(first_match->value, value, value_len);
        first_match->value[value_len] = '\0';
        first_match->value_len = value_len;
        current = &(*current)->next;
      } else {
        // This is a subsequent match - remove it
        JSRT_URLSearchParam* to_remove = *current;
        *current = (*current)->next;
        free(to_remove->name);
        free(to_remove->value);
        free(to_remove);
      }
    } else {
      current = &(*current)->next;
    }
  }

  // If no existing parameter was found, add a new one
  if (!first_match) {
    JSRT_AddSearchParamWithLength(search_params, name, name_len, value, value_len);
  }
  update_parent_url_href(search_params);

  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);
  return JS_UNDEFINED;
}

// URLSearchParams.append() method
static JSValue JSRT_URLSearchParamsAppend(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "append() requires 2 arguments");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  size_t name_len, value_len;
  const char* name = JS_ToCStringLen(ctx, &name_len, argv[0]);
  const char* value = JS_ToCStringLen(ctx, &value_len, argv[1]);
  if (!name || !value) {
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);
    return JS_EXCEPTION;
  }

  JSRT_AddSearchParamWithLength(search_params, name, name_len, value, value_len);
  update_parent_url_href(search_params);

  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);
  return JS_UNDEFINED;
}

// URLSearchParams.has() method
static JSValue JSRT_URLSearchParamsHas(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "has() requires 1 argument");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  size_t name_len;
  const char* name = JS_ToCStringLen(ctx, &name_len, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  const char* value = NULL;
  size_t value_len = 0;
  int check_value = 0;

  // If second argument is provided, check both name and value
  // Special case: if second argument is undefined, treat it as "don't check value"
  if (argc >= 2) {
    if (JS_IsUndefined(argv[1])) {
      // undefined means "ignore value, just check name exists"
      check_value = 0;
    } else {
      value = JS_ToCStringLen(ctx, &value_len, argv[1]);
      if (!value) {
        JS_FreeCString(ctx, name);
        return JS_EXCEPTION;
      }
      check_value = 1;
    }
  }
  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (param->name_len == name_len && memcmp(param->name, name, name_len) == 0) {
      if (check_value) {
        // Check if value also matches
        if (param->value_len == value_len && memcmp(param->value, value, value_len) == 0) {
          JS_FreeCString(ctx, name);
          JS_FreeCString(ctx, value);
          return JS_NewBool(ctx, 1);
        }
        // Name matches but value doesn't, continue searching
      } else {
        // Only checking name, found a match
        JS_FreeCString(ctx, name);
        return JS_NewBool(ctx, 1);
      }
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
  if (value) {
    JS_FreeCString(ctx, value);
  }
  return JS_NewBool(ctx, 0);
}

// URLSearchParams.delete() method
static JSValue JSRT_URLSearchParamsDelete(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "delete() requires 1 argument");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  size_t name_len;
  const char* name = JS_ToCStringLen(ctx, &name_len, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  const char* value = NULL;
  size_t value_len = 0;
  int check_value = 0;

  // If second argument is provided, delete only matching name-value pairs
  if (argc >= 2) {
    value = JS_ToCStringLen(ctx, &value_len, argv[1]);
    if (!value) {
      JS_FreeCString(ctx, name);
      return JS_EXCEPTION;
    }
    check_value = 1;
  }

  JSRT_URLSearchParam** current = &search_params->params;
  int removed = 0;

  while (*current) {
    if ((*current)->name_len == name_len && memcmp((*current)->name, name, name_len) == 0) {
      if (check_value) {
        // Check if value also matches
        if ((*current)->value_len == value_len && memcmp((*current)->value, value, value_len) == 0) {
          JSRT_URLSearchParam* to_remove = *current;
          *current = (*current)->next;
          free(to_remove->name);
          free(to_remove->value);
          free(to_remove);
          removed = 1;
        } else {
          // Name matches but value doesn't, continue to next
          current = &(*current)->next;
        }
      } else {
        // Only checking name, remove this entry
        JSRT_URLSearchParam* to_remove = *current;
        *current = (*current)->next;
        free(to_remove->name);
        free(to_remove->value);
        free(to_remove);
        removed = 1;
      }
    } else {
      current = &(*current)->next;
    }
  }

  if (removed) {
    update_parent_url_href(search_params);
  }

  JS_FreeCString(ctx, name);
  if (value) {
    JS_FreeCString(ctx, value);
  }
  return JS_UNDEFINED;
}

// URLSearchParams.size getter
static JSValue JSRT_URLSearchParamsGetSize(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  // Count the number of parameters
  int count = 0;
  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    count++;
    param = param->next;
  }

  return JS_NewInt32(ctx, count);
}

// Safe bounds checking macro to prevent buffer overflows
#define SAFE_BUFFER_CHECK(ptr, result, total_len, needed_space)                         \
  do {                                                                                  \
    if ((ptr) + (needed_space) > (result) + (total_len) - 1) {                          \
      free(result);                                                                     \
      return JS_ThrowInternalError(ctx, "Buffer overflow in URLSearchParams toString"); \
    }                                                                                   \
  } while (0)

// URLSearchParams.toString() method
static JSValue JSRT_URLSearchParamsToString(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  if (!search_params->params) {
    return JS_NewString(ctx, "");
  }

  // Calculate total length needed with overflow protection
  // In worst case, every byte could become %XX (3 bytes), plus separators
  size_t total_len = 1;  // Start with space for null terminator
  size_t param_count = 0;
  JSRT_URLSearchParam* param = search_params->params;

  while (param) {
    // Check for reasonable parameter limits to prevent abuse
    if (param_count > 10000) {
      return JS_ThrowInternalError(ctx, "Too many URLSearchParams entries");
    }

    // Check individual parameter sizes
    if (param->name_len > SIZE_MAX / 6 || param->value_len > SIZE_MAX / 6) {
      return JS_ThrowInternalError(ctx, "URLSearchParam entry too large");
    }

    // Calculate space needed for this parameter with overflow protection
    size_t param_space = 0;

    // Worst case: every character in name and value becomes %XX (3x expansion)
    // Plus '=' and '&' separators
    size_t name_space = param->name_len * 3;
    size_t value_space = param->value_len * 3;

    // Check for overflow in individual calculations
    if (name_space / 3 != param->name_len || value_space / 3 != param->value_len) {
      return JS_ThrowInternalError(ctx, "Integer overflow in URLSearchParams toString calculation");
    }

    param_space = name_space + value_space + 2;  // +2 for '=' and '&'

    // Check for overflow when adding to total
    if (total_len > SIZE_MAX - param_space) {
      return JS_ThrowInternalError(ctx, "Buffer size overflow in URLSearchParams toString");
    }

    total_len += param_space;
    param_count++;
    param = param->next;
  }

  if (total_len <= 1) {
    return JS_NewString(ctx, "");
  }

  // Additional safety check for final allocation size
  if (total_len > 100 * 1024 * 1024) {  // 100MB limit
    return JS_ThrowInternalError(ctx, "URLSearchParams string too large");
  }

  char* result = malloc(total_len);
  if (!result) {
    return JS_ThrowOutOfMemory(ctx);
  }

  char* ptr = result;
  param = search_params->params;
  int first = 1;

  while (param) {
    // Add separator for non-first parameters
    if (!first) {
      SAFE_BUFFER_CHECK(ptr, result, total_len, 1);
      *ptr++ = '&';
    }
    first = 0;

    // Encode name with bounds checking
    char* encoded_name = url_encode_with_len(param->name, param->name_len);
    if (encoded_name) {
      size_t encoded_name_len = strlen(encoded_name);
      SAFE_BUFFER_CHECK(ptr, result, total_len, encoded_name_len);
      memcpy(ptr, encoded_name, encoded_name_len);
      ptr += encoded_name_len;
      free(encoded_name);
    }

    // Add equals sign
    SAFE_BUFFER_CHECK(ptr, result, total_len, 1);
    *ptr++ = '=';

    // Encode value with bounds checking
    char* encoded_value = url_encode_with_len(param->value, param->value_len);
    if (encoded_value) {
      size_t encoded_value_len = strlen(encoded_value);
      SAFE_BUFFER_CHECK(ptr, result, total_len, encoded_value_len);
      memcpy(ptr, encoded_value, encoded_value_len);
      ptr += encoded_value_len;
      free(encoded_value);
    }

    param = param->next;
  }

  *ptr = '\0';
  JSValue js_result = JS_NewString(ctx, result);
  free(result);
  return js_result;
}

void JSRT_RegisterURLSearchParamsMethods(JSContext* ctx, JSValue proto) {
  JS_SetPropertyStr(ctx, proto, "get", JS_NewCFunction(ctx, JSRT_URLSearchParamsGet, "get", 1));
  JS_SetPropertyStr(ctx, proto, "getAll", JS_NewCFunction(ctx, JSRT_URLSearchParamsGetAll, "getAll", 1));
  JS_SetPropertyStr(ctx, proto, "set", JS_NewCFunction(ctx, JSRT_URLSearchParamsSet, "set", 2));
  JS_SetPropertyStr(ctx, proto, "append", JS_NewCFunction(ctx, JSRT_URLSearchParamsAppend, "append", 2));
  JS_SetPropertyStr(ctx, proto, "has", JS_NewCFunction(ctx, JSRT_URLSearchParamsHas, "has", 1));
  JS_SetPropertyStr(ctx, proto, "delete", JS_NewCFunction(ctx, JSRT_URLSearchParamsDelete, "delete", 1));
  JS_SetPropertyStr(ctx, proto, "toString", JS_NewCFunction(ctx, JSRT_URLSearchParamsToString, "toString", 0));

  // size getter property
  JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "size"),
                          JS_NewCFunction(ctx, JSRT_URLSearchParamsGetSize, "get size", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
}