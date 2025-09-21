#include "../url.h"

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

void JSRT_RegisterURLSearchParamsIterators(JSContext* ctx, JSValue proto) {
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
}