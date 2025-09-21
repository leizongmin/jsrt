#include "../url.h"

JSRT_URLSearchParams* JSRT_ParseSearchParams(const char* search_string, size_t string_len) {
  JSRT_URLSearchParams* search_params = malloc(sizeof(JSRT_URLSearchParams));
  if (!search_params) {
    return NULL;
  }

  search_params->params = NULL;
  search_params->parent_url = NULL;
  search_params->ctx = NULL;

  if (!search_string || string_len == 0) {
    return search_params;
  }

  const char* start = search_string;
  size_t start_len = string_len;
  if (start_len > 0 && start[0] == '?') {
    start++;  // Skip leading '?'
    start_len--;
  }

  JSRT_URLSearchParam** tail = &search_params->params;

  // Parse parameters manually without relying on C string functions
  size_t i = 0;
  while (i < start_len) {
    // Find the end of this parameter (next '&' or end of string)
    size_t param_start = i;
    while (i < start_len && start[i] != '&') {
      i++;
    }
    size_t param_len = i - param_start;

    if (param_len > 0) {
      // Find '=' in this parameter
      size_t eq_pos = param_start;
      while (eq_pos < param_start + param_len && start[eq_pos] != '=') {
        eq_pos++;
      }

      JSRT_URLSearchParam* param = malloc(sizeof(JSRT_URLSearchParam));
      if (!param) {
        JSRT_FreeSearchParams(search_params);
        return NULL;
      }
      param->next = NULL;

      if (eq_pos < param_start + param_len) {
        // Has '=' - split into name and value
        size_t name_len = eq_pos - param_start;
        size_t value_len = param_start + param_len - eq_pos - 1;

        param->name = url_decode_query_with_length_and_output_len(start + param_start, name_len, &param->name_len);
        param->value = url_decode_query_with_length_and_output_len(start + eq_pos + 1, value_len, &param->value_len);
      } else {
        // No '=' - name only, empty value
        param->name = url_decode_query_with_length_and_output_len(start + param_start, param_len, &param->name_len);
        param->value = strdup("");
        param->value_len = 0;
      }

      *tail = param;
      tail = &param->next;
    }

    // Move past the '&' for next iteration
    if (i < start_len) {
      i++;
    }
  }

  return search_params;
}

JSRT_URLSearchParams* JSRT_ParseSearchParamsFromSequence(JSContext* ctx, JSValue seq) {
  JSRT_URLSearchParams* search_params = JSRT_CreateEmptySearchParams();

  // TODO: Check if it has Symbol.iterator (should use iterator protocol)
  // Try iterator protocol first if the object has Symbol.iterator
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue symbol_obj = JS_GetPropertyStr(ctx, global, "Symbol");
  JSValue iterator_symbol = JS_GetPropertyStr(ctx, symbol_obj, "iterator");
  JS_FreeValue(ctx, symbol_obj);
  JS_FreeValue(ctx, global);

  if (!JS_IsUndefined(iterator_symbol)) {
    JSAtom iterator_atom = JS_ValueToAtom(ctx, iterator_symbol);
    int has_iterator = JS_HasProperty(ctx, seq, iterator_atom);

    if (has_iterator) {
      // Use iterator protocol
      JSValue iterator_method = JS_GetProperty(ctx, seq, iterator_atom);
      JS_FreeAtom(ctx, iterator_atom);
      JS_FreeValue(ctx, iterator_symbol);
      if (JS_IsException(iterator_method)) {
        JSRT_FreeSearchParams(search_params);
        return NULL;
      }

      JSValue iterator = JS_Call(ctx, iterator_method, seq, 0, NULL);
      JS_FreeValue(ctx, iterator_method);
      if (JS_IsException(iterator)) {
        JSRT_FreeSearchParams(search_params);
        return NULL;
      }

      // Iterate through the iterator
      JSValue next_method = JS_GetPropertyStr(ctx, iterator, "next");
      if (JS_IsException(next_method)) {
        JS_FreeValue(ctx, iterator);
        JSRT_FreeSearchParams(search_params);
        return NULL;
      }

      while (true) {
        JSValue result = JS_Call(ctx, next_method, iterator, 0, NULL);
        if (JS_IsException(result)) {
          JS_FreeValue(ctx, next_method);
          JS_FreeValue(ctx, iterator);
          JSRT_FreeSearchParams(search_params);
          return NULL;
        }

        JSValue done = JS_GetPropertyStr(ctx, result, "done");
        bool is_done = JS_ToBool(ctx, done);
        JS_FreeValue(ctx, done);

        if (is_done) {
          JS_FreeValue(ctx, result);
          break;
        }

        JSValue item = JS_GetPropertyStr(ctx, result, "value");
        JS_FreeValue(ctx, result);

        // Process the item (same validation as array-like)
        JSValue item_length_val = JS_GetPropertyStr(ctx, item, "length");
        int32_t item_length;
        if (JS_IsException(item_length_val) || JS_ToInt32(ctx, &item_length, item_length_val)) {
          JS_FreeValue(ctx, item);
          if (!JS_IsException(item_length_val))
            JS_FreeValue(ctx, item_length_val);
          JS_FreeValue(ctx, next_method);
          JS_FreeValue(ctx, iterator);
          JSRT_FreeSearchParams(search_params);
          return NULL;
        }

        if (item_length != 2) {
          JS_FreeValue(ctx, item);
          JS_FreeValue(ctx, item_length_val);
          JS_FreeValue(ctx, next_method);
          JS_FreeValue(ctx, iterator);
          JSRT_FreeSearchParams(search_params);
          JS_ThrowTypeError(ctx, "Iterator value a 0 is not an entry object");
          return NULL;
        }
        JS_FreeValue(ctx, item_length_val);

        JSValue name_val = JS_GetPropertyUint32(ctx, item, 0);
        JSValue value_val = JS_GetPropertyUint32(ctx, item, 1);

        const char* name_str = JS_ToCString(ctx, name_val);
        const char* value_str = JS_ToCString(ctx, value_val);

        if (name_str && value_str) {
          JSRT_AddSearchParam(search_params, name_str, value_str);
        }

        if (name_str)
          JS_FreeCString(ctx, name_str);
        if (value_str)
          JS_FreeCString(ctx, value_str);
        JS_FreeValue(ctx, name_val);
        JS_FreeValue(ctx, value_val);
        JS_FreeValue(ctx, item);
      }

      JS_FreeValue(ctx, next_method);
      JS_FreeValue(ctx, iterator);
      return search_params;
    } else {
      JS_FreeAtom(ctx, iterator_atom);
      JS_FreeValue(ctx, iterator_symbol);
    }
  } else {
    JS_FreeValue(ctx, iterator_symbol);
  }

  // Fall back to array-like sequence handling
  JSValue length_val = JS_GetPropertyStr(ctx, seq, "length");
  if (JS_IsException(length_val)) {
    JSRT_FreeSearchParams(search_params);
    return NULL;
  }

  int32_t length;
  if (JS_ToInt32(ctx, &length, length_val)) {
    JS_FreeValue(ctx, length_val);
    JSRT_FreeSearchParams(search_params);
    return NULL;
  }
  JS_FreeValue(ctx, length_val);

  for (int32_t i = 0; i < length; i++) {
    JSValue item = JS_GetPropertyUint32(ctx, seq, i);
    if (JS_IsException(item)) {
      JSRT_FreeSearchParams(search_params);
      return NULL;
    }

    // Each item should be a sequence with exactly 2 elements
    JSValue item_length_val = JS_GetPropertyStr(ctx, item, "length");
    int32_t item_length;
    if (JS_IsException(item_length_val) || JS_ToInt32(ctx, &item_length, item_length_val)) {
      JS_FreeValue(ctx, item);
      if (!JS_IsException(item_length_val))
        JS_FreeValue(ctx, item_length_val);
      JSRT_FreeSearchParams(search_params);
      return NULL;
    }

    if (item_length != 2) {
      JS_FreeValue(ctx, item);
      JS_FreeValue(ctx, item_length_val);
      JSRT_FreeSearchParams(search_params);
      JS_ThrowTypeError(ctx, "Iterator value a 0 is not an entry object");
      return NULL;
    }
    JS_FreeValue(ctx, item_length_val);

    JSValue name_val = JS_GetPropertyUint32(ctx, item, 0);
    JSValue value_val = JS_GetPropertyUint32(ctx, item, 1);

    const char* name_str = JS_ToCString(ctx, name_val);
    const char* value_str = JS_ToCString(ctx, value_val);

    if (name_str && value_str) {
      JSRT_AddSearchParam(search_params, name_str, value_str);
    }

    if (name_str)
      JS_FreeCString(ctx, name_str);
    if (value_str)
      JS_FreeCString(ctx, value_str);
    JS_FreeValue(ctx, name_val);
    JS_FreeValue(ctx, value_val);
    JS_FreeValue(ctx, item);
  }

  return search_params;
}

JSRT_URLSearchParams* JSRT_ParseSearchParamsFromRecord(JSContext* ctx, JSValue record) {
  JSRT_URLSearchParams* search_params = JSRT_CreateEmptySearchParams();

  JSPropertyEnum* properties;
  uint32_t count;

  if (JS_GetOwnPropertyNames(ctx, &properties, &count, record, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
    JSRT_FreeSearchParams(search_params);
    return NULL;
  }

  for (uint32_t i = 0; i < count; i++) {
    JSValue value = JS_GetProperty(ctx, record, properties[i].atom);
    if (JS_IsException(value)) {
      continue;
    }

    // Convert atom to JSValue for surrogate handling
    JSValue name_val = JS_AtomToString(ctx, properties[i].atom);
    if (JS_IsException(name_val)) {
      JS_FreeValue(ctx, value);
      continue;
    }

    // Use surrogate handling helper for both name and value
    size_t name_len, value_len;
    const char* name_str = JSRT_StringToUTF8WithSurrogateReplacement(ctx, name_val, &name_len);
    const char* value_str = JSRT_StringToUTF8WithSurrogateReplacement(ctx, value, &value_len);

    if (name_str && value_str) {
      // For object constructor, later values should overwrite earlier ones for same key
      // But maintain the position of the first occurrence of the key
      JSRT_URLSearchParam* existing = NULL;
      JSRT_URLSearchParam* current = search_params->params;
      while (current) {
        if (current->name_len == name_len && memcmp(current->name, name_str, name_len) == 0) {
          if (!existing) {
            // First match - update this one and remember it
            free(current->value);
            current->value = malloc(value_len + 1);
            if (current->value) {
              memcpy(current->value, value_str, value_len);
              current->value[value_len] = '\0';
              current->value_len = value_len;
            }
            existing = current;
          } else {
            // Additional match - remove this one
            // This is a complex operation, let's use the existing removal logic
            JSRT_URLSearchParam** prev_next = &search_params->params;
            while (*prev_next != current) {
              prev_next = &(*prev_next)->next;
            }

            // Save next pointer before freeing current
            JSRT_URLSearchParam* next_param = current->next;
            *prev_next = next_param;

            // Safely free current node
            if (current->name) {
              free(current->name);
              current->name = NULL;
            }
            if (current->value) {
              free(current->value);
              current->value = NULL;
            }
            free(current);

            // Move to next parameter (safely)
            current = next_param;
            continue;
          }
        }
        current = current->next;
      }

      // If no existing key found, add new parameter
      if (!existing) {
        JSRT_AddSearchParamWithLength(search_params, name_str, name_len, value_str, value_len);
      }
    }

    if (name_str)
      free((char*)name_str);
    if (value_str)
      free((char*)value_str);

    JS_FreeValue(ctx, name_val);
    JS_FreeValue(ctx, value);
  }

  JS_FreePropertyEnum(ctx, properties, count);
  return search_params;
}

JSRT_URLSearchParams* JSRT_ParseSearchParamsFromFormData(JSContext* ctx, JSValueConst formdata_val) {
  JSRT_URLSearchParams* search_params = JSRT_CreateEmptySearchParams();

  // Get the FormData opaque data
  void* formdata_opaque = JS_GetOpaque(formdata_val, JSRT_FormDataClassID);
  if (!formdata_opaque) {
    JSRT_FreeSearchParams(search_params);
    return NULL;
  }

  // Access FormData entries through forEach method
  // Call formData.forEach((value, name) => { add to search_params })
  JSValue forEach_fn = JS_GetPropertyStr(ctx, formdata_val, "forEach");
  if (JS_IsUndefined(forEach_fn)) {
    JSRT_FreeSearchParams(search_params);
    return NULL;
  }

  // Create a callback function to collect the entries
  // We'll use a simpler approach: iterate through the FormData structure directly
  // Since we have access to the FormData structure, we can access it directly
  typedef struct JSRT_FormDataEntry {
    char* name;
    JSValue value;
    char* filename;
    struct JSRT_FormDataEntry* next;
  } JSRT_FormDataEntry;

  typedef struct {
    JSRT_FormDataEntry* entries;
  } JSRT_FormData;

  JSRT_FormData* formdata = (JSRT_FormData*)formdata_opaque;
  JSRT_FormDataEntry* entry = formdata->entries;

  while (entry) {
    if (entry->name) {
      const char* value_str = JS_ToCString(ctx, entry->value);
      if (value_str) {
        JSRT_AddSearchParam(search_params, entry->name, value_str);
        JS_FreeCString(ctx, value_str);
      }
    }
    entry = entry->next;
  }

  JS_FreeValue(ctx, forEach_fn);
  return search_params;
}