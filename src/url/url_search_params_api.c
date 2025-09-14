#include "url.h"

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

// URLSearchParams.toString() method
static JSValue JSRT_URLSearchParamsToString(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  if (!search_params->params) {
    return JS_NewString(ctx, "");
  }

  // Calculate total length needed accounting for URL encoding expansion
  // In worst case, every byte could become %XX (3 bytes), plus separators
  size_t total_len = 0;
  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    // Worst case: every character in name and value becomes %XX (3x expansion)
    // Plus '=' and '&' separators (which don't need encoding in query strings)
    total_len += (param->name_len * 3) + (param->value_len * 3) + 2;
    param = param->next;
  }

  if (total_len == 0) {
    return JS_NewString(ctx, "");
  }

  // Add extra buffer for safety
  total_len += 1;  // For null terminator
  char* result = malloc(total_len);
  if (!result) {
    return JS_ThrowOutOfMemory(ctx);
  }
  char* ptr = result;
  param = search_params->params;
  int first = 1;

  while (param) {
    // Check bounds before writing separator
    if (!first) {
      if (ptr >= result + total_len - 1) {
        free(result);
        return JS_ThrowInternalError(ctx, "Buffer overflow in URLSearchParams toString");
      }
      *ptr++ = '&';
    }
    first = 0;

    // Encode name
    char* encoded_name = url_encode_with_len(param->name, param->name_len);
    if (encoded_name) {
      size_t encoded_name_len = strlen(encoded_name);
      // Check bounds before copying encoded name
      if (ptr + encoded_name_len >= result + total_len - 1) {
        free(encoded_name);
        free(result);
        return JS_ThrowInternalError(ctx, "Buffer overflow in URLSearchParams toString");
      }
      memcpy(ptr, encoded_name, encoded_name_len);
      ptr += encoded_name_len;
      free(encoded_name);
    }

    // Check bounds before writing '='
    if (ptr >= result + total_len - 1) {
      free(result);
      return JS_ThrowInternalError(ctx, "Buffer overflow in URLSearchParams toString");
    }
    *ptr++ = '=';

    // Encode value
    char* encoded_value = url_encode_with_len(param->value, param->value_len);
    if (encoded_value) {
      size_t encoded_value_len = strlen(encoded_value);
      // Check bounds before copying encoded value
      if (ptr + encoded_value_len >= result + total_len - 1) {
        free(encoded_value);
        free(result);
        return JS_ThrowInternalError(ctx, "Buffer overflow in URLSearchParams toString");
      }
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

// URLSearchParams.keys() method - returns an iterator of all keys
static JSValue JSRT_URLSearchParamsKeys(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  JSValue array = JS_NewArray(ctx);
  int index = 0;

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    JSValue key = JS_NewStringLen(ctx, param->name, param->name_len);
    JS_SetPropertyUint32(ctx, array, index++, key);
    param = param->next;
  }

  // Return an iterator for the array
  JSValue iterator_symbol = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Symbol");
  JSValue iterator_atom_val = JS_GetPropertyStr(ctx, iterator_symbol, "iterator");
  JS_FreeValue(ctx, iterator_symbol);

  JSValue iterator_method = JS_GetProperty(ctx, array, JS_ValueToAtom(ctx, iterator_atom_val));
  JS_FreeValue(ctx, iterator_atom_val);

  JSValue result = JS_Call(ctx, iterator_method, array, 0, NULL);
  JS_FreeValue(ctx, iterator_method);
  JS_FreeValue(ctx, array);

  return result;
}

// URLSearchParams.values() method - returns an iterator of all values
static JSValue JSRT_URLSearchParamsValues(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  JSValue array = JS_NewArray(ctx);
  int index = 0;

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    JSValue value = JS_NewStringLen(ctx, param->value, param->value_len);
    JS_SetPropertyUint32(ctx, array, index++, value);
    param = param->next;
  }

  // Return an iterator for the array
  JSValue iterator_symbol = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Symbol");
  JSValue iterator_atom_val = JS_GetPropertyStr(ctx, iterator_symbol, "iterator");
  JS_FreeValue(ctx, iterator_symbol);

  JSValue iterator_method = JS_GetProperty(ctx, array, JS_ValueToAtom(ctx, iterator_atom_val));
  JS_FreeValue(ctx, iterator_atom_val);

  JSValue result = JS_Call(ctx, iterator_method, array, 0, NULL);
  JS_FreeValue(ctx, iterator_method);
  JS_FreeValue(ctx, array);

  return result;
}

// URLSearchParams.entries() method - returns an iterator of all [key, value] pairs
static JSValue JSRT_URLSearchParamsEntries(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  JSValue array = JS_NewArray(ctx);
  int index = 0;

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    JSValue pair = JS_NewArray(ctx);
    JSValue key = JS_NewStringLen(ctx, param->name, param->name_len);
    JSValue value = JS_NewStringLen(ctx, param->value, param->value_len);
    JS_SetPropertyUint32(ctx, pair, 0, key);
    JS_SetPropertyUint32(ctx, pair, 1, value);
    JS_SetPropertyUint32(ctx, array, index++, pair);
    param = param->next;
  }

  // Return an iterator for the array
  JSValue iterator_symbol = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Symbol");
  JSValue iterator_atom_val = JS_GetPropertyStr(ctx, iterator_symbol, "iterator");
  JS_FreeValue(ctx, iterator_symbol);

  JSValue iterator_method = JS_GetProperty(ctx, array, JS_ValueToAtom(ctx, iterator_atom_val));
  JS_FreeValue(ctx, iterator_atom_val);

  JSValue result = JS_Call(ctx, iterator_method, array, 0, NULL);
  JS_FreeValue(ctx, iterator_method);
  JS_FreeValue(ctx, array);

  return result;
}

// URLSearchParams[Symbol.iterator] method - same as entries()
static JSValue JSRT_URLSearchParamsSymbolIterator(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLSearchParamsEntries(ctx, this_val, argc, argv);
}

// Register URLSearchParams methods with the prototype
void JSRT_RegisterURLSearchParamsMethods(JSContext* ctx, JSValue proto) {
  JS_SetPropertyStr(ctx, proto, "get", JS_NewCFunction(ctx, JSRT_URLSearchParamsGet, "get", 1));
  JS_SetPropertyStr(ctx, proto, "getAll", JS_NewCFunction(ctx, JSRT_URLSearchParamsGetAll, "getAll", 1));
  JS_SetPropertyStr(ctx, proto, "set", JS_NewCFunction(ctx, JSRT_URLSearchParamsSet, "set", 2));
  JS_SetPropertyStr(ctx, proto, "append", JS_NewCFunction(ctx, JSRT_URLSearchParamsAppend, "append", 2));
  JS_SetPropertyStr(ctx, proto, "has", JS_NewCFunction(ctx, JSRT_URLSearchParamsHas, "has", 2));
  JS_SetPropertyStr(ctx, proto, "delete", JS_NewCFunction(ctx, JSRT_URLSearchParamsDelete, "delete", 2));
  JS_SetPropertyStr(ctx, proto, "toString", JS_NewCFunction(ctx, JSRT_URLSearchParamsToString, "toString", 0));

  // Add iterator methods
  JS_SetPropertyStr(ctx, proto, "keys", JS_NewCFunction(ctx, JSRT_URLSearchParamsKeys, "keys", 0));
  JS_SetPropertyStr(ctx, proto, "values", JS_NewCFunction(ctx, JSRT_URLSearchParamsValues, "values", 0));
  JS_SetPropertyStr(ctx, proto, "entries", JS_NewCFunction(ctx, JSRT_URLSearchParamsEntries, "entries", 0));

  // Add Symbol.iterator method
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue symbol_obj = JS_GetPropertyStr(ctx, global, "Symbol");
  JSValue iterator_symbol = JS_GetPropertyStr(ctx, symbol_obj, "iterator");
  JS_FreeValue(ctx, symbol_obj);
  JS_FreeValue(ctx, global);

  if (!JS_IsUndefined(iterator_symbol)) {
    JSValue iterator_method = JS_NewCFunction(ctx, JSRT_URLSearchParamsSymbolIterator, "[Symbol.iterator]", 0);
    JS_SetProperty(ctx, proto, JS_ValueToAtom(ctx, iterator_symbol), iterator_method);
  }
  JS_FreeValue(ctx, iterator_symbol);

  // Add size property as getter
  JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "size"),
                          JS_NewCFunction(ctx, JSRT_URLSearchParamsGetSize, "get size", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
}