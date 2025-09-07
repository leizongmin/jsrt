#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "../util/debug.h"
#include "node_modules.h"

// URL decode helper function
static char* url_decode(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  char* decoded = malloc(len + 1);
  if (!decoded)
    return NULL;

  size_t i = 0, j = 0;
  while (i < len) {
    if (str[i] == '%' && i + 2 < len) {
      // Decode %XX hex sequence
      char hex[3] = {str[i + 1], str[i + 2], '\0'};
      char* endptr;
      unsigned long val = strtoul(hex, &endptr, 16);
      if (endptr == hex + 2) {
        decoded[j++] = (char)val;
        i += 3;
      } else {
        decoded[j++] = str[i++];
      }
    } else if (str[i] == '+') {
      decoded[j++] = ' ';
      i++;
    } else {
      decoded[j++] = str[i++];
    }
  }
  decoded[j] = '\0';
  return decoded;
}

// URL encode helper function for querystring formatting (uses + for spaces)
static char* url_encode_form(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  char* encoded = malloc(len * 3 + 1);  // Worst case: every char becomes %XX
  if (!encoded)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded[j++] = c;
    } else if (c == ' ') {
      encoded[j++] = '+';
    } else {
      sprintf(&encoded[j], "%%%02X", c);
      j += 3;
    }
  }
  encoded[j] = '\0';
  return encoded;
}

// URL encode helper function for escape() (uses %20 for spaces like encodeURIComponent)
static char* url_encode(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  char* encoded = malloc(len * 3 + 1);  // Worst case: every char becomes %XX
  if (!encoded)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded[j++] = c;
    } else {
      sprintf(&encoded[j], "%%%02X", c);
      j += 3;
    }
  }
  encoded[j] = '\0';
  return encoded;
}

// querystring.parse(str[, sep[, eq[, options]]])
static JSValue js_querystring_parse(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "parse() requires a string argument");
  }

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_ThrowTypeError(ctx, "First argument must be a string");
  }

  // Default separators
  const char* sep = "&";
  const char* eq = "=";

  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    sep = JS_ToCString(ctx, argv[1]);
    if (!sep) {
      JS_FreeCString(ctx, str);
      return JS_ThrowTypeError(ctx, "sep must be a string");
    }
  }

  if (argc > 2 && !JS_IsUndefined(argv[2])) {
    eq = JS_ToCString(ctx, argv[2]);
    if (!eq) {
      JS_FreeCString(ctx, str);
      if (argc > 1)
        JS_FreeCString(ctx, sep);
      return JS_ThrowTypeError(ctx, "eq must be a string");
    }
  }

  JSValue result = JS_NewObject(ctx);

  // Parse the query string
  char* str_copy = strdup(str);
  if (!str_copy) {
    JS_FreeCString(ctx, str);
    if (argc > 1)
      JS_FreeCString(ctx, sep);
    if (argc > 2)
      JS_FreeCString(ctx, eq);
    JS_FreeValue(ctx, result);
    return JS_ThrowOutOfMemory(ctx);
  }

  char* saveptr1;
  char* pair = strtok_r(str_copy, sep, &saveptr1);

  while (pair) {
    char* eq_pos = strstr(pair, eq);
    if (eq_pos) {
      *eq_pos = '\0';
      char* key = url_decode(pair);
      char* value = url_decode(eq_pos + strlen(eq));

      if (key) {
        // Check if key already exists
        JSValue existing = JS_GetPropertyStr(ctx, result, key);
        if (!JS_IsUndefined(existing)) {
          // Convert to array or append to existing array
          if (JS_IsArray(ctx, existing)) {
            // Append to existing array
            JSValue len_val = JS_GetPropertyStr(ctx, existing, "length");
            uint32_t len;
            JS_ToUint32(ctx, &len, len_val);
            JS_SetPropertyUint32(ctx, existing, len, JS_NewString(ctx, value ? value : ""));
            JS_FreeValue(ctx, len_val);
            JS_FreeValue(ctx, existing);
          } else {
            // Convert to array
            JSValue array = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, array, 0, existing);  // existing is moved to array
            JS_SetPropertyUint32(ctx, array, 1, JS_NewString(ctx, value ? value : ""));
            JS_SetPropertyStr(ctx, result, key, array);
            // Don't free existing here as it's now owned by the array
          }
        } else {
          JS_SetPropertyStr(ctx, result, key, JS_NewString(ctx, value ? value : ""));
          JS_FreeValue(ctx, existing);
        }

        free(key);
        if (value)
          free(value);
      }
    } else {
      // Key without value
      char* key = url_decode(pair);
      if (key) {
        JS_SetPropertyStr(ctx, result, key, JS_NewString(ctx, ""));
        free(key);
      }
    }

    pair = strtok_r(NULL, sep, &saveptr1);
  }

  free(str_copy);
  JS_FreeCString(ctx, str);
  if (argc > 1)
    JS_FreeCString(ctx, sep);
  if (argc > 2)
    JS_FreeCString(ctx, eq);

  return result;
}

// querystring.stringify(obj[, sep[, eq[, options]]])
static JSValue js_querystring_stringify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "stringify() requires an object argument");
  }

  // Default separators
  const char* sep = "&";
  const char* eq = "=";

  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    sep = JS_ToCString(ctx, argv[1]);
    if (!sep) {
      return JS_ThrowTypeError(ctx, "sep must be a string");
    }
  }

  if (argc > 2 && !JS_IsUndefined(argv[2])) {
    eq = JS_ToCString(ctx, argv[2]);
    if (!eq) {
      if (argc > 1)
        JS_FreeCString(ctx, sep);
      return JS_ThrowTypeError(ctx, "eq must be a string");
    }
  }

  // Build result string
  char* result_str = malloc(1);
  if (!result_str) {
    if (argc > 1)
      JS_FreeCString(ctx, sep);
    if (argc > 2)
      JS_FreeCString(ctx, eq);
    return JS_ThrowOutOfMemory(ctx);
  }
  result_str[0] = '\0';
  size_t result_len = 0;

  // Get object properties
  JSPropertyEnum* props;
  uint32_t prop_count;

  if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, argv[0], JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
    for (uint32_t i = 0; i < prop_count; i++) {
      JSValue key_val = JS_AtomToString(ctx, props[i].atom);
      const char* key = JS_ToCString(ctx, key_val);

      if (key) {
        JSValue value_val = JS_GetProperty(ctx, argv[0], props[i].atom);

        if (JS_IsArray(ctx, value_val)) {
          // Handle array values
          JSValue len_val = JS_GetPropertyStr(ctx, value_val, "length");
          uint32_t len;
          if (JS_ToUint32(ctx, &len, len_val) == 0) {
            for (uint32_t j = 0; j < len; j++) {
              JSValue item = JS_GetPropertyUint32(ctx, value_val, j);
              const char* value_str = JS_ToCString(ctx, item);

              if (value_str) {
                char* encoded_key = url_encode_form(key);
                char* encoded_value = url_encode_form(value_str);

                if (encoded_key && encoded_value) {
                  size_t needed =
                      result_len + strlen(encoded_key) + strlen(eq) + strlen(encoded_value) + strlen(sep) + 1;
                  result_str = realloc(result_str, needed);

                  if (result_len > 0) {
                    strcat(result_str, sep);
                    result_len += strlen(sep);
                  }
                  strcat(result_str, encoded_key);
                  strcat(result_str, eq);
                  strcat(result_str, encoded_value);
                  result_len = strlen(result_str);
                }

                if (encoded_key)
                  free(encoded_key);
                if (encoded_value)
                  free(encoded_value);
                JS_FreeCString(ctx, value_str);
              }
              JS_FreeValue(ctx, item);
            }
          }
          JS_FreeValue(ctx, len_val);
        } else {
          // Handle single value
          const char* value_str = JS_ToCString(ctx, value_val);
          if (value_str) {
            char* encoded_key = url_encode_form(key);
            char* encoded_value = url_encode_form(value_str);

            if (encoded_key && encoded_value) {
              size_t needed = result_len + strlen(encoded_key) + strlen(eq) + strlen(encoded_value) + strlen(sep) + 1;
              result_str = realloc(result_str, needed);

              if (result_len > 0) {
                strcat(result_str, sep);
                result_len += strlen(sep);
              }
              strcat(result_str, encoded_key);
              strcat(result_str, eq);
              strcat(result_str, encoded_value);
              result_len = strlen(result_str);
            }

            if (encoded_key)
              free(encoded_key);
            if (encoded_value)
              free(encoded_value);
            JS_FreeCString(ctx, value_str);
          }
        }

        JS_FreeValue(ctx, value_val);
        JS_FreeCString(ctx, key);
      }
      JS_FreeValue(ctx, key_val);
    }

    JS_FreePropertyEnum(ctx, props, prop_count);
  }

  JSValue result = JS_NewString(ctx, result_str);
  free(result_str);

  if (argc > 1)
    JS_FreeCString(ctx, sep);
  if (argc > 2)
    JS_FreeCString(ctx, eq);

  return result;
}

// querystring.escape(str) - alias for encodeURIComponent
static JSValue js_querystring_escape(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "escape() requires a string argument");
  }

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_ThrowTypeError(ctx, "Argument must be a string");
  }

  char* encoded = url_encode(str);
  JS_FreeCString(ctx, str);

  if (!encoded) {
    return JS_ThrowOutOfMemory(ctx);
  }

  JSValue result = JS_NewString(ctx, encoded);
  free(encoded);
  return result;
}

// querystring.unescape(str) - alias for decodeURIComponent
static JSValue js_querystring_unescape(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "unescape() requires a string argument");
  }

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_ThrowTypeError(ctx, "Argument must be a string");
  }

  char* decoded = url_decode(str);
  JS_FreeCString(ctx, str);

  if (!decoded) {
    return JS_ThrowOutOfMemory(ctx);
  }

  JSValue result = JS_NewString(ctx, decoded);
  free(decoded);
  return result;
}

// Initialize querystring module
JSValue JSRT_InitNodeQueryString(JSContext* ctx) {
  JSValue querystring = JS_NewObject(ctx);

  // Add parse and stringify methods
  JS_SetPropertyStr(ctx, querystring, "parse", JS_NewCFunction(ctx, js_querystring_parse, "parse", 4));
  JS_SetPropertyStr(ctx, querystring, "stringify", JS_NewCFunction(ctx, js_querystring_stringify, "stringify", 4));

  // Add escape and unescape methods (aliases)
  JS_SetPropertyStr(ctx, querystring, "escape", JS_NewCFunction(ctx, js_querystring_escape, "escape", 1));
  JS_SetPropertyStr(ctx, querystring, "unescape", JS_NewCFunction(ctx, js_querystring_unescape, "unescape", 1));

  // Add decode and encode as aliases
  JS_SetPropertyStr(ctx, querystring, "decode", JS_NewCFunction(ctx, js_querystring_parse, "decode", 4));
  JS_SetPropertyStr(ctx, querystring, "encode", JS_NewCFunction(ctx, js_querystring_stringify, "encode", 4));

  return querystring;
}

// ES module initialization
int js_node_querystring_init(JSContext* ctx, JSModuleDef* m) {
  JSValue querystring = JSRT_InitNodeQueryString(ctx);
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

  JS_FreeValue(ctx, querystring);
  return 0;
}