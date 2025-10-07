#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "../url/url.h"
#include "../util/debug.h"
#include "node_modules.h"

// querystring.escape(str) - uses form encoding (space -> +, * preserved)
// Directly reuses url_encode() from src/url/encoding/url_basic_encoding.c
static JSValue js_querystring_escape(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewString(ctx, "");
  }

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_EXCEPTION;
  }

  // Reuse WPT-compliant url_encode() which already implements querystring semantics
  // (space -> +, * preserved, etc.)
  char* encoded = url_encode(str);
  JS_FreeCString(ctx, str);

  if (!encoded) {
    return JS_ThrowOutOfMemory(ctx);
  }

  JSValue result = JS_NewString(ctx, encoded);
  free(encoded);
  return result;
}

// querystring.unescape(str) - decodes with + -> space
// Directly reuses url_decode_query_with_length_and_output_len() from src/url/
static JSValue js_querystring_unescape(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewString(ctx, "");
  }

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_EXCEPTION;
  }

  size_t len = strlen(str);
  size_t out_len;

  // Reuse WPT-compliant url_decode_query_with_length_and_output_len()
  // which handles + -> space and proper UTF-8 validation
  char* decoded = url_decode_query_with_length_and_output_len(str, len, &out_len);
  JS_FreeCString(ctx, str);

  if (!decoded) {
    return JS_ThrowOutOfMemory(ctx);
  }

  JSValue result = JS_NewStringLen(ctx, decoded, out_len);
  free(decoded);
  return result;
}

// querystring.parse(str[, sep[, eq[, options]]])
// Adapted from URLSearchParams parsing logic with custom separators and maxKeys support
static JSValue js_querystring_parse(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Handle empty or missing argument
  if (argc < 1 || JS_IsUndefined(argv[0]) || JS_IsNull(argv[0])) {
    return JS_NewObject(ctx);
  }

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_EXCEPTION;
  }

  // Default separators
  const char* sep = "&";
  const char* eq = "=";
  int sep_needs_free = 0;
  int eq_needs_free = 0;

  // Get custom sep parameter (defaults to & if null or undefined)
  if (argc > 1 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
    sep = JS_ToCString(ctx, argv[1]);
    if (!sep) {
      JS_FreeCString(ctx, str);
      return JS_EXCEPTION;
    }
    sep_needs_free = 1;
  }

  // Get custom eq parameter (defaults to = if null or undefined)
  if (argc > 2 && !JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
    eq = JS_ToCString(ctx, argv[2]);
    if (!eq) {
      JS_FreeCString(ctx, str);
      if (sep_needs_free)
        JS_FreeCString(ctx, sep);
      return JS_EXCEPTION;
    }
    eq_needs_free = 1;
  }

  // Extract maxKeys from options (default 1000)
  int max_keys = 1000;
  if (argc > 3 && JS_IsObject(argv[3])) {
    JSValue max_keys_val = JS_GetPropertyStr(ctx, argv[3], "maxKeys");
    if (!JS_IsUndefined(max_keys_val) && !JS_IsNull(max_keys_val)) {
      int32_t mk;
      if (JS_ToInt32(ctx, &mk, max_keys_val) == 0 && mk >= 0) {
        max_keys = mk;
      }
    }
    JS_FreeValue(ctx, max_keys_val);
  }

  JSValue result = JS_NewObject(ctx);

  size_t str_len = strlen(str);
  size_t sep_len = strlen(sep);
  size_t eq_len = strlen(eq);

  // Skip leading separator
  const char* start = str;
  if (str_len > 0 && sep_len > 0 && strncmp(start, sep, sep_len) == 0) {
    start += sep_len;
    str_len -= sep_len;
  }

  // Parse loop
  size_t i = 0;
  int key_count = 0;

  while (i < str_len && (max_keys == 0 || key_count < max_keys)) {
    // Find next separator
    size_t param_start = i;
    size_t next_sep = i;

    // Find next occurrence of separator
    while (next_sep < str_len) {
      if (sep_len > 0 && next_sep + sep_len <= str_len && strncmp(start + next_sep, sep, sep_len) == 0) {
        break;
      }
      next_sep++;
    }

    size_t param_len = next_sep - param_start;

    if (param_len > 0) {
      // Find equals sign within this parameter
      size_t eq_pos = param_start;
      int found_eq = 0;

      while (eq_pos < param_start + param_len) {
        if (eq_len > 0 && eq_pos + eq_len <= param_start + param_len && strncmp(start + eq_pos, eq, eq_len) == 0) {
          found_eq = 1;
          break;
        }
        eq_pos++;
      }

      // Extract key and value
      size_t key_len = found_eq ? (eq_pos - param_start) : param_len;
      size_t val_start = found_eq ? (eq_pos + eq_len) : param_start + param_len;
      size_t val_len = found_eq ? (param_start + param_len - val_start) : 0;

      // Decode key and value using existing WPT-compliant functions
      size_t decoded_key_len, decoded_val_len;
      char* key = url_decode_query_with_length_and_output_len(start + param_start, key_len, &decoded_key_len);
      char* val = (val_len > 0)
                      ? url_decode_query_with_length_and_output_len(start + val_start, val_len, &decoded_val_len)
                      : strdup("");

      if (key && val) {
        // Check if key already exists
        JSValue existing = JS_GetPropertyStr(ctx, result, key);

        if (JS_IsUndefined(existing)) {
          // First occurrence - set as string
          JS_SetPropertyStr(ctx, result, key, JS_NewString(ctx, val));
          JS_FreeValue(ctx, existing);
          key_count++;  // Only increment for new unique keys
        } else if (JS_IsArray(ctx, existing)) {
          // Already array - append (don't increment key_count)
          JSValue len_val = JS_GetPropertyStr(ctx, existing, "length");
          uint32_t len;
          JS_ToUint32(ctx, &len, len_val);
          JS_SetPropertyUint32(ctx, existing, len, JS_NewString(ctx, val));
          JS_FreeValue(ctx, len_val);
          JS_FreeValue(ctx, existing);
        } else {
          // Second occurrence - convert to array (don't increment key_count)
          JSValue arr = JS_NewArray(ctx);
          JS_SetPropertyUint32(ctx, arr, 0, JS_DupValue(ctx, existing));  // Duplicate the value
          JS_SetPropertyUint32(ctx, arr, 1, JS_NewString(ctx, val));
          JS_SetPropertyStr(ctx, result, key, arr);
          JS_FreeValue(ctx, existing);
        }
      }

      free(key);
      free(val);
    }

    // Move to next parameter
    i = next_sep;
    if (i < str_len && sep_len > 0) {
      i += sep_len;
    }
  }

  // Cleanup
  JS_FreeCString(ctx, str);
  if (sep_needs_free)
    JS_FreeCString(ctx, sep);
  if (eq_needs_free)
    JS_FreeCString(ctx, eq);

  return result;
}

// Helper structure to store encoded key-value pairs for two-pass approach
typedef struct {
  char* encoded_key;
  char* encoded_value;
} EncodedPair;

// querystring.stringify(obj[, sep[, eq[, options]]])
static JSValue js_querystring_stringify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1 || JS_IsUndefined(argv[0]) || JS_IsNull(argv[0])) {
    return JS_NewString(ctx, "");
  }

  if (!JS_IsObject(argv[0])) {
    return JS_NewString(ctx, "");
  }

  // Default separators
  const char* sep = "&";
  const char* eq = "=";
  int sep_needs_free = 0;
  int eq_needs_free = 0;

  if (argc > 1 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
    sep = JS_ToCString(ctx, argv[1]);
    if (!sep) {
      return JS_EXCEPTION;
    }
    sep_needs_free = 1;
  }

  if (argc > 2 && !JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
    eq = JS_ToCString(ctx, argv[2]);
    if (!eq) {
      if (sep_needs_free)
        JS_FreeCString(ctx, sep);
      return JS_EXCEPTION;
    }
    eq_needs_free = 1;
  }

  size_t sep_len = strlen(sep);
  size_t eq_len = strlen(eq);

  // Get object properties
  JSPropertyEnum* props;
  uint32_t prop_count;

  if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, argv[0], JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) != 0) {
    if (sep_needs_free)
      JS_FreeCString(ctx, sep);
    if (eq_needs_free)
      JS_FreeCString(ctx, eq);
    return JS_NewString(ctx, "");
  }

  // Allocate array to store encoded pairs
  size_t pairs_capacity = prop_count * 2;  // Initial capacity (account for potential array values)
  size_t pairs_count = 0;
  EncodedPair* pairs = malloc(pairs_capacity * sizeof(EncodedPair));
  if (!pairs) {
    JS_FreePropertyEnum(ctx, props, prop_count);
    if (sep_needs_free)
      JS_FreeCString(ctx, sep);
    if (eq_needs_free)
      JS_FreeCString(ctx, eq);
    return JS_ThrowOutOfMemory(ctx);
  }

  // First pass: encode all key-value pairs and calculate total size
  size_t total_size = 0;

  for (uint32_t i = 0; i < prop_count; i++) {
    JSValue key_val = JS_AtomToString(ctx, props[i].atom);
    const char* key = JS_ToCString(ctx, key_val);

    if (key) {
      JSValue value_val = JS_GetProperty(ctx, argv[0], props[i].atom);

      // Handle arrays - repeat key for each element
      if (JS_IsArray(ctx, value_val)) {
        JSValue len_val = JS_GetPropertyStr(ctx, value_val, "length");
        uint32_t len;
        if (JS_ToUint32(ctx, &len, len_val) == 0) {
          for (uint32_t j = 0; j < len; j++) {
            // Expand pairs array if needed
            if (pairs_count >= pairs_capacity) {
              pairs_capacity *= 2;
              EncodedPair* new_pairs = realloc(pairs, pairs_capacity * sizeof(EncodedPair));
              if (!new_pairs) {
                // Cleanup on realloc failure
                for (size_t k = 0; k < pairs_count; k++) {
                  free(pairs[k].encoded_key);
                  free(pairs[k].encoded_value);
                }
                free(pairs);
                JS_FreeValue(ctx, len_val);
                JS_FreeValue(ctx, value_val);
                JS_FreeCString(ctx, key);
                JS_FreeValue(ctx, key_val);
                JS_FreePropertyEnum(ctx, props, prop_count);
                if (sep_needs_free)
                  JS_FreeCString(ctx, sep);
                if (eq_needs_free)
                  JS_FreeCString(ctx, eq);
                return JS_ThrowOutOfMemory(ctx);
              }
              pairs = new_pairs;
            }

            JSValue item = JS_GetPropertyUint32(ctx, value_val, j);

            // Convert value to string
            const char* value_str = NULL;
            int value_str_needs_free = 0;
            if (JS_IsNull(item) || JS_IsUndefined(item)) {
              value_str = "";
            } else {
              value_str = JS_ToCString(ctx, item);
              value_str_needs_free = 1;
            }

            if (value_str) {
              char* encoded_key = url_encode(key);
              char* encoded_value = url_encode(value_str);

              if (encoded_key && encoded_value) {
                pairs[pairs_count].encoded_key = encoded_key;
                pairs[pairs_count].encoded_value = encoded_value;
                total_size += strlen(encoded_key) + eq_len + strlen(encoded_value);
                if (pairs_count > 0) {
                  total_size += sep_len;
                }
                pairs_count++;
              } else {
                // Cleanup on encoding failure
                if (encoded_key)
                  free(encoded_key);
                if (encoded_value)
                  free(encoded_value);
              }

              if (value_str_needs_free)
                JS_FreeCString(ctx, value_str);
            }
            JS_FreeValue(ctx, item);
          }
        }
        JS_FreeValue(ctx, len_val);
      } else {
        // Expand pairs array if needed
        if (pairs_count >= pairs_capacity) {
          pairs_capacity *= 2;
          EncodedPair* new_pairs = realloc(pairs, pairs_capacity * sizeof(EncodedPair));
          if (!new_pairs) {
            // Cleanup on realloc failure
            for (size_t k = 0; k < pairs_count; k++) {
              free(pairs[k].encoded_key);
              free(pairs[k].encoded_value);
            }
            free(pairs);
            JS_FreeValue(ctx, value_val);
            JS_FreeCString(ctx, key);
            JS_FreeValue(ctx, key_val);
            JS_FreePropertyEnum(ctx, props, prop_count);
            if (sep_needs_free)
              JS_FreeCString(ctx, sep);
            if (eq_needs_free)
              JS_FreeCString(ctx, eq);
            return JS_ThrowOutOfMemory(ctx);
          }
          pairs = new_pairs;
        }

        // Handle single value
        const char* value_str = NULL;
        int value_str_needs_free = 0;
        if (JS_IsNull(value_val) || JS_IsUndefined(value_val)) {
          value_str = "";
        } else {
          value_str = JS_ToCString(ctx, value_val);
          value_str_needs_free = 1;
        }

        if (value_str) {
          char* encoded_key = url_encode(key);
          char* encoded_value = url_encode(value_str);

          if (encoded_key && encoded_value) {
            pairs[pairs_count].encoded_key = encoded_key;
            pairs[pairs_count].encoded_value = encoded_value;
            total_size += strlen(encoded_key) + eq_len + strlen(encoded_value);
            if (pairs_count > 0) {
              total_size += sep_len;
            }
            pairs_count++;
          } else {
            // Cleanup on encoding failure
            if (encoded_key)
              free(encoded_key);
            if (encoded_value)
              free(encoded_value);
          }

          if (value_str_needs_free)
            JS_FreeCString(ctx, value_str);
        }
      }

      JS_FreeValue(ctx, value_val);
      JS_FreeCString(ctx, key);
    }
    JS_FreeValue(ctx, key_val);
  }

  JS_FreePropertyEnum(ctx, props, prop_count);

  // Second pass: build result string with single allocation
  char* result_str = malloc(total_size + 1);
  if (!result_str) {
    // Cleanup on allocation failure
    for (size_t k = 0; k < pairs_count; k++) {
      free(pairs[k].encoded_key);
      free(pairs[k].encoded_value);
    }
    free(pairs);
    if (sep_needs_free)
      JS_FreeCString(ctx, sep);
    if (eq_needs_free)
      JS_FreeCString(ctx, eq);
    return JS_ThrowOutOfMemory(ctx);
  }

  char* p = result_str;
  for (size_t i = 0; i < pairs_count; i++) {
    if (i > 0) {
      memcpy(p, sep, sep_len);
      p += sep_len;
    }
    size_t key_len = strlen(pairs[i].encoded_key);
    memcpy(p, pairs[i].encoded_key, key_len);
    p += key_len;
    memcpy(p, eq, eq_len);
    p += eq_len;
    size_t val_len = strlen(pairs[i].encoded_value);
    memcpy(p, pairs[i].encoded_value, val_len);
    p += val_len;

    // Free encoded strings
    free(pairs[i].encoded_key);
    free(pairs[i].encoded_value);
  }
  *p = '\0';

  free(pairs);

  JSValue result = JS_NewString(ctx, result_str);
  free(result_str);

  if (sep_needs_free)
    JS_FreeCString(ctx, sep);
  if (eq_needs_free)
    JS_FreeCString(ctx, eq);

  return result;
}

// Initialize querystring module (CommonJS)
JSValue JSRT_InitNodeQueryString(JSContext* ctx) {
  JSValue querystring = JS_NewObject(ctx);

  // Add parse and stringify methods
  JS_SetPropertyStr(ctx, querystring, "parse", JS_NewCFunction(ctx, js_querystring_parse, "parse", 4));
  JS_SetPropertyStr(ctx, querystring, "stringify", JS_NewCFunction(ctx, js_querystring_stringify, "stringify", 4));

  // Add escape and unescape methods
  JS_SetPropertyStr(ctx, querystring, "escape", JS_NewCFunction(ctx, js_querystring_escape, "escape", 1));
  JS_SetPropertyStr(ctx, querystring, "unescape", JS_NewCFunction(ctx, js_querystring_unescape, "unescape", 1));

  // Add decode and encode as aliases to parse and stringify
  JS_SetPropertyStr(ctx, querystring, "decode", JS_NewCFunction(ctx, js_querystring_parse, "decode", 4));
  JS_SetPropertyStr(ctx, querystring, "encode", JS_NewCFunction(ctx, js_querystring_stringify, "encode", 4));

  return querystring;
}

// ES module initialization
int js_node_querystring_init(JSContext* ctx, JSModuleDef* m) {
  JSValue querystring = JSRT_InitNodeQueryString(ctx);

  // Export default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, querystring));

  // Export individual functions
  JSValue parse = JS_GetPropertyStr(ctx, querystring, "parse");
  JS_SetModuleExport(ctx, m, "parse", JS_DupValue(ctx, parse));
  JS_FreeValue(ctx, parse);

  JSValue stringify = JS_GetPropertyStr(ctx, querystring, "stringify");
  JS_SetModuleExport(ctx, m, "stringify", JS_DupValue(ctx, stringify));
  JS_FreeValue(ctx, stringify);

  JSValue escape = JS_GetPropertyStr(ctx, querystring, "escape");
  JS_SetModuleExport(ctx, m, "escape", JS_DupValue(ctx, escape));
  JS_FreeValue(ctx, escape);

  JSValue unescape = JS_GetPropertyStr(ctx, querystring, "unescape");
  JS_SetModuleExport(ctx, m, "unescape", JS_DupValue(ctx, unescape));
  JS_FreeValue(ctx, unescape);

  // Export decode and encode aliases
  JSValue decode = JS_GetPropertyStr(ctx, querystring, "decode");
  JS_SetModuleExport(ctx, m, "decode", JS_DupValue(ctx, decode));
  JS_FreeValue(ctx, decode);

  JSValue encode = JS_GetPropertyStr(ctx, querystring, "encode");
  JS_SetModuleExport(ctx, m, "encode", JS_DupValue(ctx, encode));
  JS_FreeValue(ctx, encode);

  JS_FreeValue(ctx, querystring);
  return 0;
}
