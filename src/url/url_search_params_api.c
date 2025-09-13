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

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (param->name_len == strlen(name) && memcmp(param->name, name, param->name_len) == 0) {
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

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  JSValue result_array = JS_NewArray(ctx);
  int index = 0;

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (param->name_len == strlen(name) && memcmp(param->name, name, param->name_len) == 0) {
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

  const char* name = JS_ToCString(ctx, argv[0]);
  const char* value = JS_ToCString(ctx, argv[1]);
  if (!name || !value) {
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);
    return JS_EXCEPTION;
  }

  size_t name_len = strlen(name);
  size_t value_len = strlen(value);

  // Remove all existing parameters with the same name
  JSRT_URLSearchParam** current = &search_params->params;
  while (*current) {
    if ((*current)->name_len == name_len && memcmp((*current)->name, name, name_len) == 0) {
      JSRT_URLSearchParam* to_remove = *current;
      *current = (*current)->next;
      free(to_remove->name);
      free(to_remove->value);
      free(to_remove);
    } else {
      current = &(*current)->next;
    }
  }

  // Add the new parameter
  JSRT_AddSearchParamWithLength(search_params, name, name_len, value, value_len);
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

  const char* name = JS_ToCString(ctx, argv[0]);
  const char* value = JS_ToCString(ctx, argv[1]);
  if (!name || !value) {
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);
    return JS_EXCEPTION;
  }

  JSRT_AddSearchParamWithLength(search_params, name, strlen(name), value, strlen(value));
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

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  size_t name_len = strlen(name);
  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (param->name_len == name_len && memcmp(param->name, name, name_len) == 0) {
      JS_FreeCString(ctx, name);
      return JS_NewBool(ctx, 1);
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
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

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  size_t name_len = strlen(name);
  JSRT_URLSearchParam** current = &search_params->params;
  int removed = 0;

  while (*current) {
    if ((*current)->name_len == name_len && memcmp((*current)->name, name, name_len) == 0) {
      JSRT_URLSearchParam* to_remove = *current;
      *current = (*current)->next;
      free(to_remove->name);
      free(to_remove->value);
      free(to_remove);
      removed = 1;
    } else {
      current = &(*current)->next;
    }
  }

  if (removed) {
    update_parent_url_href(search_params);
  }

  JS_FreeCString(ctx, name);
  return JS_UNDEFINED;
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

  // Calculate total length needed
  size_t total_len = 0;
  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    total_len += param->name_len + param->value_len + 2;  // +2 for '=' and '&'
    param = param->next;
  }

  if (total_len == 0) {
    return JS_NewString(ctx, "");
  }

  char* result = malloc(total_len);
  char* ptr = result;
  param = search_params->params;
  int first = 1;

  while (param) {
    if (!first) {
      *ptr++ = '&';
    }
    first = 0;

    // Encode name
    char* encoded_name = url_encode_with_len(param->name, param->name_len);
    if (encoded_name) {
      size_t encoded_name_len = strlen(encoded_name);
      memcpy(ptr, encoded_name, encoded_name_len);
      ptr += encoded_name_len;
      free(encoded_name);
    }

    *ptr++ = '=';

    // Encode value
    char* encoded_value = url_encode_with_len(param->value, param->value_len);
    if (encoded_value) {
      size_t encoded_value_len = strlen(encoded_value);
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

// Register URLSearchParams methods with the prototype
void JSRT_RegisterURLSearchParamsMethods(JSContext* ctx, JSValue proto) {
  JS_SetPropertyStr(ctx, proto, "get", JS_NewCFunction(ctx, JSRT_URLSearchParamsGet, "get", 1));
  JS_SetPropertyStr(ctx, proto, "getAll", JS_NewCFunction(ctx, JSRT_URLSearchParamsGetAll, "getAll", 1));
  JS_SetPropertyStr(ctx, proto, "set", JS_NewCFunction(ctx, JSRT_URLSearchParamsSet, "set", 2));
  JS_SetPropertyStr(ctx, proto, "append", JS_NewCFunction(ctx, JSRT_URLSearchParamsAppend, "append", 2));
  JS_SetPropertyStr(ctx, proto, "has", JS_NewCFunction(ctx, JSRT_URLSearchParamsHas, "has", 1));
  JS_SetPropertyStr(ctx, proto, "delete", JS_NewCFunction(ctx, JSRT_URLSearchParamsDelete, "delete", 1));
  JS_SetPropertyStr(ctx, proto, "toString", JS_NewCFunction(ctx, JSRT_URLSearchParamsToString, "toString", 0));
}