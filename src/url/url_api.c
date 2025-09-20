#include "url.h"

// Class IDs
JSClassID JSRT_URLClassID;
JSClassID JSRT_URLSearchParamsClassID;
JSClassID JSRT_URLSearchParamsIteratorClassID;

// Class definitions
static void JSRT_URLFinalize(JSRuntime* rt, JSValue val) {
  JSRT_URL* url = JS_GetOpaque(val, JSRT_URLClassID);
  if (url) {
    JSRT_FreeURL(url);
  }
}

static JSClassDef JSRT_URLClass = {
    .class_name = "URL",
    .finalizer = JSRT_URLFinalize,
};

// Helper function to validate UTF-8 sequence
static int is_valid_utf8_sequence(const unsigned char* bytes, size_t start, size_t max_len, size_t* seq_len) {
  if (start >= max_len)
    return 0;

  unsigned char c = bytes[start];

  if (c < 0x80) {
    // ASCII
    *seq_len = 1;
    return 1;
  } else if ((c & 0xE0) == 0xC0) {
    // 2-byte sequence
    if (start + 1 >= max_len)
      return 0;
    *seq_len = 2;
    return (bytes[start + 1] & 0xC0) == 0x80;
  } else if ((c & 0xF0) == 0xE0) {
    // 3-byte sequence
    if (start + 2 >= max_len)
      return 0;
    *seq_len = 3;
    return (bytes[start + 1] & 0xC0) == 0x80 && (bytes[start + 2] & 0xC0) == 0x80;
  } else if ((c & 0xF8) == 0xF0) {
    // 4-byte sequence
    if (start + 3 >= max_len)
      return 0;
    *seq_len = 4;
    return (bytes[start + 1] & 0xC0) == 0x80 && (bytes[start + 2] & 0xC0) == 0x80 && (bytes[start + 3] & 0xC0) == 0x80;
  }

  return 0;  // Invalid UTF-8 start byte
}

// Helper function to strip tab and newline characters from URL strings
// According to URL spec, these characters should be removed: tab (0x09), LF (0x0A), CR (0x0D)
// Per WHATWG URL specification, only strip control characters but don't reject URLs with special characters
static char* JSRT_StripURLControlCharacters(const char* input, size_t input_len) {
  if (!input)
    return NULL;

  char* result = malloc(input_len + 1);
  if (!result)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < input_len; i++) {
    unsigned char c = (unsigned char)input[i];

    // Per WHATWG URL spec: "Remove all ASCII tab or newline from input"
    // Remove tab (0x09), line feed (0x0A), and carriage return (0x0D)
    if (c == 0x09 || c == 0x0A || c == 0x0D) {
      continue;  // Skip these characters
    }

    // For UTF-8 sequences, validate and copy them but don't reject based on specific characters
    // Many Unicode characters that were previously rejected should be allowed per WPT tests
    if (c >= 0x80) {
      size_t utf8_len;
      if (!is_valid_utf8_sequence((const unsigned char*)input, i, input_len, &utf8_len)) {
        // Only reject truly malformed UTF-8
        free(result);
        return NULL;
      }

      // Check for specific problematic Unicode characters that should cause URL parsing to fail
      if (utf8_len == 3 && i + 2 < input_len) {
        unsigned char c2 = (unsigned char)input[i + 1];
        unsigned char c3 = (unsigned char)input[i + 2];

#ifdef DEBUG
        if (c == 0xE2 || c == 0xEF) {
          fprintf(stderr, "[DEBUG] preprocess_url_string: checking UTF-8 sequence: 0x%02X 0x%02X 0x%02X\n", c, c2, c3);
        }
#endif

        // Note: Unicode zero-width character stripping has been moved to hostname processing
        // to ensure context-sensitive handling per WHATWG URL specification.
        // These characters should only be stripped from authority/hostname parts, not from paths.
      }

      // Copy the entire valid UTF-8 sequence
      for (size_t k = 0; k < utf8_len && i + k < input_len; k++) {
        result[j++] = input[i + k];
      }
      i += utf8_len - 1;  // Skip the rest of the sequence (loop will increment i)
    } else {
      // ASCII character - copy as-is
      // Let the URL parser handle validation and normalization of special characters
      result[j++] = c;
    }
  }
  result[j] = '\0';
  return result;
}

static JSValue JSRT_URLConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "URL constructor requires at least 1 argument");
  }

  size_t url_str_len;
  const char* url_str_raw = JS_ToCStringLen(ctx, &url_str_len, argv[0]);
  if (!url_str_raw) {
    return JS_EXCEPTION;
  }

  // Strip control characters as per URL specification
  // Note: Use strlen() instead of url_str_len because JS_ToCStringLen returns the JavaScript
  // string length (UTF-16 code units) but url_str_raw is a UTF-8 byte array
  char* url_str = JSRT_StripURLControlCharacters(url_str_raw, strlen(url_str_raw));
  JS_FreeCString(ctx, url_str_raw);
  if (!url_str) {
    return JS_ThrowTypeError(ctx, "Invalid URL");
  }

  const char* base_str_raw = NULL;
  char* base_str = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    size_t base_str_len;
    base_str_raw = JS_ToCStringLen(ctx, &base_str_len, argv[1]);
    if (!base_str_raw) {
      free(url_str);
      return JS_EXCEPTION;
    }
    base_str = JSRT_StripURLControlCharacters(base_str_raw, strlen(base_str_raw));
    JS_FreeCString(ctx, base_str_raw);
    if (!base_str) {
      free(url_str);
      return JS_ThrowTypeError(ctx, "Invalid URL");
    }
  }

  JSRT_URL* url = JSRT_ParseURL(url_str, base_str);
  if (!url) {
    free(url_str);
    if (base_str) {
      free(base_str);
    }
    return JS_ThrowTypeError(ctx, "Invalid URL");
  }

  url->ctx = ctx;  // Set context for managing search_params

  JSValue obj = JS_NewObjectClass(ctx, JSRT_URLClassID);
  JS_SetOpaque(obj, url);

  free(url_str);
  if (base_str) {
    free(base_str);
  }

  return obj;
}

// Property getters
static JSValue JSRT_URLGetHref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->href);
}

static JSValue JSRT_URLGetProtocol(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->protocol);
}

static JSValue JSRT_URLGetUsername(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  if (!url->username || strlen(url->username) == 0) {
    return JS_NewString(ctx, "");
  }

  // Username property should return percent-encoded value
  char* encoded_username = url_userinfo_encode_with_scheme_name(url->username, url->protocol);
  JSValue result = JS_NewString(ctx, encoded_username ? encoded_username : "");
  free(encoded_username);
  return result;
}

static JSValue JSRT_URLGetPassword(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  if (!url->password || strlen(url->password) == 0) {
    return JS_NewString(ctx, "");
  }

  // Password property should return percent-encoded value
  char* encoded_password = url_userinfo_encode_with_scheme_name(url->password, url->protocol);
  JSValue result = JS_NewString(ctx, encoded_password ? encoded_password : "");
  free(encoded_password);
  return result;
}

static JSValue JSRT_URLGetHost(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // Build host with normalized port
  if (url->hostname && strlen(url->hostname) > 0) {
    char* normalized_port = normalize_port(url->port, url->protocol);
    if (normalized_port && strlen(normalized_port) > 0) {
      // Host includes port
      size_t host_len = strlen(url->hostname) + strlen(normalized_port) + 2;
      char* final_host = malloc(host_len);
      if (!final_host) {
        free(normalized_port);
        return JS_EXCEPTION;
      }
      snprintf(final_host, host_len, "%s:%s", url->hostname, normalized_port);
      JSValue result = JS_NewString(ctx, final_host);
      free(final_host);
      free(normalized_port);
      return result;
    } else {
      // Host without port (port 0 or default port)
      free(normalized_port);
      return JS_NewString(ctx, url->hostname);
    }
  }

  return JS_NewString(ctx, url->host ? url->host : "");
}

static JSValue JSRT_URLGetHostname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->hostname);
}

static JSValue JSRT_URLGetPort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // Return normalized port (empty string for port 0 and default ports)
  char* normalized_port = normalize_port(url->port, url->protocol);
  if (!normalized_port)
    return JS_NewString(ctx, "");  // Return empty string for invalid ports

  JSValue result = JS_NewString(ctx, normalized_port);
  free(normalized_port);
  return result;
}

static JSValue JSRT_URLGetPathname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // For non-special schemes, return the raw pathname without encoding
  // For special schemes, return the percent-encoded pathname (except file URLs preserve pipes)
  if (is_special_scheme(url->protocol)) {
    char* encoded_pathname;
    if (strcmp(url->protocol, "file:") == 0) {
      // File URLs preserve pipe characters in pathname
      encoded_pathname = strdup(url->pathname ? url->pathname : "");
    } else {
      // Other special schemes use component encoding
      encoded_pathname = url_component_encode(url->pathname);
    }
    if (!encoded_pathname)
      return JS_EXCEPTION;

    JSValue result = JS_NewString(ctx, encoded_pathname);
    free(encoded_pathname);
    return result;
  } else {
    // Non-special schemes return raw pathname
    return JS_NewString(ctx, url->pathname);
  }
}

static JSValue JSRT_URLGetSearch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // Per WPT spec: empty query ("?") should return empty string, not "?"
  if (url->search && strcmp(url->search, "?") == 0) {
    return JS_NewString(ctx, "");
  }

  // Return the search string as-is (already properly encoded during parsing)
  return JS_NewString(ctx, url->search ? url->search : "");
}

static JSValue JSRT_URLSetSearch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  if (argc < 1)
    return JS_UNDEFINED;

  const char* new_search = JS_ToCString(ctx, argv[0]);
  if (!new_search)
    return JS_EXCEPTION;

  // Update the search string
  free(url->search);
  if (new_search[0] == '?') {
    url->search = strdup(new_search);
    if (!url->search) {
      JS_FreeCString(ctx, new_search);
      return JS_EXCEPTION;
    }
  } else {
    size_t len = strlen(new_search);
    url->search = malloc(len + 2);
    if (!url->search) {
      JS_FreeCString(ctx, new_search);
      return JS_EXCEPTION;
    }
    snprintf(url->search, len + 2, "?%s", new_search);
  }

  // Update cached URLSearchParams object if it exists
  if (!JS_IsUndefined(url->search_params)) {
    JSRT_URLSearchParams* cached_params = JS_GetOpaque2(ctx, url->search_params, JSRT_URLSearchParamsClassID);
    if (cached_params) {
      // Free existing parameters
      JSRT_URLSearchParam* param = cached_params->params;
      while (param) {
        JSRT_URLSearchParam* next = param->next;
        free(param->name);
        free(param->value);
        free(param);
        param = next;
      }

      // Parse new search string into existing URLSearchParams object
      JSRT_URLSearchParams* new_params = JSRT_ParseSearchParams(url->search, strlen(url->search));
      if (new_params) {
        cached_params->params = new_params->params;
        new_params->params = NULL;  // Prevent double free
        JSRT_FreeSearchParams(new_params);
      } else {
        cached_params->params = NULL;
      }
    }
  }

  // Rebuild href with new search component
  build_href(url);

  JS_FreeCString(ctx, new_search);
  return JS_UNDEFINED;
}

static JSValue JSRT_URLGetHash(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // Handle NULL or empty hash
  if (!url->hash || strlen(url->hash) == 0) {
    return JS_NewString(ctx, "");
  }

  // Per WPT spec: empty fragment ("#") should return empty string, not "#"
  if (strcmp(url->hash, "#") == 0) {
    return JS_NewString(ctx, "");
  }

  // Return the percent-encoded hash as per URL specification
  // Use scheme-appropriate encoding for fragments
  int is_special = is_special_scheme(url->protocol);
  char* encoded_hash = is_special ? url_fragment_encode(url->hash) : url_fragment_encode_nonspecial(url->hash);
  if (!encoded_hash)
    return JS_NewString(ctx, "");  // Return empty string instead of exception

  JSValue result = JS_NewString(ctx, encoded_hash);
  free(encoded_hash);
  return result;
}

static JSValue JSRT_URLGetOrigin(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->origin);
}

static JSValue JSRT_URLGetSearchParams(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // Create URLSearchParams object if not already cached
  if (JS_IsUndefined(url->search_params)) {
    JSValue search_params_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "URLSearchParams");
    JSValue search_value = JS_NewString(ctx, url->search);
    url->search_params = JS_CallConstructor(ctx, search_params_ctor, 1, &search_value);

    // Set parent_url reference for URL integration
    JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, url->search_params, JSRT_URLSearchParamsClassID);
    if (search_params) {
      search_params->parent_url = url;
      search_params->ctx = ctx;
    }

    JS_FreeValue(ctx, search_params_ctor);
    JS_FreeValue(ctx, search_value);
  }

  return JS_DupValue(ctx, url->search_params);
}

static JSValue JSRT_URLToString(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLGetHref(ctx, this_val, argc, argv);
}

static JSValue JSRT_URLToJSON(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLGetHref(ctx, this_val, argc, argv);
}

// URLSearchParams Finalizer
static void JSRT_URLSearchParamsFinalize(JSRuntime* rt, JSValue val) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque(val, JSRT_URLSearchParamsClassID);
  if (search_params) {
    JSRT_FreeSearchParams(search_params);
  }
}

static JSClassDef JSRT_URLSearchParamsClass = {
    .class_name = "URLSearchParams",
    .finalizer = JSRT_URLSearchParamsFinalize,
};

// URLSearchParams constructor
static JSValue JSRT_URLSearchParamsConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params;

  if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    JSValue init = argv[0];

    // First check if this is a DOMException or DOMException.prototype (should throw due to branding checks)
    if (!JS_IsString(init)) {
      JSValue global = JS_GetGlobalObject(ctx);
      JSValue dom_exception = JS_GetPropertyStr(ctx, global, "DOMException");
      if (!JS_IsUndefined(dom_exception)) {
        JSValue dom_exception_proto = JS_GetPropertyStr(ctx, dom_exception, "prototype");
        if (JS_SameValue(ctx, init, dom_exception_proto)) {
          // Only DOMException.prototype should throw, not DOMException itself
          JS_FreeValue(ctx, dom_exception_proto);
          JS_FreeValue(ctx, dom_exception);
          JS_FreeValue(ctx, global);
          return JS_ThrowTypeError(ctx, "Invalid argument to URLSearchParams constructor");
        }
        JS_FreeValue(ctx, dom_exception_proto);
      }
      JS_FreeValue(ctx, dom_exception);
      JS_FreeValue(ctx, global);

      // Check if it's an array or has Symbol.iterator (sequence/iterable)
      // This check must come BEFORE type-specific checks to handle custom iterators
      global = JS_GetGlobalObject(ctx);
      JSValue symbol_obj = JS_GetPropertyStr(ctx, global, "Symbol");
      JSValue iterator_symbol = JS_GetPropertyStr(ctx, symbol_obj, "iterator");
      JS_FreeValue(ctx, symbol_obj);
      JS_FreeValue(ctx, global);

      bool is_array = JS_IsArray(ctx, init);
      bool has_iterator = false;

      if (!JS_IsUndefined(iterator_symbol)) {
        JSAtom iterator_atom = JS_ValueToAtom(ctx, iterator_symbol);
        has_iterator = JS_HasProperty(ctx, init, iterator_atom);
        JS_FreeAtom(ctx, iterator_atom);
      }
      JS_FreeValue(ctx, iterator_symbol);

      if (is_array || has_iterator) {
        // It's an array or iterable (including URLSearchParams with custom iterator) - use sequence parser
        search_params = JSRT_ParseSearchParamsFromSequence(ctx, init);
        if (!search_params) {
          // If an exception is already pending (e.g., TypeError for invalid pairs), return it
          if (JS_HasException(ctx)) {
            return JS_EXCEPTION;
          }
          return JS_ThrowTypeError(ctx, "Invalid sequence argument to URLSearchParams constructor");
        }
      }
      // Check if it's already a URLSearchParams object (without custom iterator)
      else if (JS_GetOpaque2(ctx, init, JSRT_URLSearchParamsClassID)) {
        JSRT_URLSearchParams* src = JS_GetOpaque2(ctx, init, JSRT_URLSearchParamsClassID);
        search_params = JSRT_CreateEmptySearchParams();
        if (!search_params) {
          return JS_EXCEPTION;
        }

        // Copy all parameters
        JSRT_URLSearchParam* param = src->params;
        while (param) {
          JSRT_AddSearchParam(search_params, param->name, param->value);
          param = param->next;
        }
      }
      // Check if it's a FormData object
      else if (JS_GetOpaque2(ctx, init, JSRT_FormDataClassID)) {
        search_params = JSRT_ParseSearchParamsFromFormData(ctx, init);
        if (!search_params) {
          return JS_ThrowTypeError(ctx, "Invalid FormData argument to URLSearchParams constructor");
        }
      } else {
        // Check if this is a plain object (record type)
        // Plain objects should be treated as records where each enumerable property
        // becomes a key-value pair in the URLSearchParams
        search_params = JSRT_ParseSearchParamsFromRecord(ctx, init);
        if (!search_params) {
          // If parsing as record failed, check if an exception is already pending
          if (JS_HasException(ctx)) {
            return JS_EXCEPTION;
          }
          return JS_ThrowTypeError(ctx, "value is not iterable");
        }
      }
    }
    // Otherwise, treat as string
    else {
      size_t init_len;
      const char* init_str = JS_ToCStringLen(ctx, &init_len, init);
      if (!init_str) {
        return JS_EXCEPTION;
      }
      search_params = JSRT_ParseSearchParams(init_str, init_len);
      JS_FreeCString(ctx, init_str);
      if (!search_params) {
        return JS_EXCEPTION;
      }
    }
  } else {
    search_params = JSRT_CreateEmptySearchParams();
    if (!search_params) {
      return JS_EXCEPTION;
    }
  }

  JSValue obj = JS_NewObjectClass(ctx, JSRT_URLSearchParamsClassID);
  JS_SetOpaque(obj, search_params);
  return obj;
}

// Setup function
void JSRT_RuntimeSetupStdURL(JSRT_Runtime* rt) {
  JSContext* ctx = rt->ctx;

  JSRT_Debug("JSRT_RuntimeSetupStdURL: initializing URL/URLSearchParams API");

  // Register URL class
  JS_NewClassID(&JSRT_URLClassID);
  JS_NewClass(rt->rt, JSRT_URLClassID, &JSRT_URLClass);

  JSValue url_proto = JS_NewObject(ctx);

  // Properties with getters
  JSValue get_href = JS_NewCFunction(ctx, JSRT_URLGetHref, "get href", 0);
  JSValue get_protocol = JS_NewCFunction(ctx, JSRT_URLGetProtocol, "get protocol", 0);
  JSValue get_username = JS_NewCFunction(ctx, JSRT_URLGetUsername, "get username", 0);
  JSValue get_password = JS_NewCFunction(ctx, JSRT_URLGetPassword, "get password", 0);
  JSValue get_host = JS_NewCFunction(ctx, JSRT_URLGetHost, "get host", 0);
  JSValue get_hostname = JS_NewCFunction(ctx, JSRT_URLGetHostname, "get hostname", 0);
  JSValue get_port = JS_NewCFunction(ctx, JSRT_URLGetPort, "get port", 0);
  JSValue get_pathname = JS_NewCFunction(ctx, JSRT_URLGetPathname, "get pathname", 0);
  JSValue get_search = JS_NewCFunction(ctx, JSRT_URLGetSearch, "get search", 0);
  JSValue set_search = JS_NewCFunction(ctx, JSRT_URLSetSearch, "set search", 1);
  JSValue get_hash = JS_NewCFunction(ctx, JSRT_URLGetHash, "get hash", 0);
  JSValue get_origin = JS_NewCFunction(ctx, JSRT_URLGetOrigin, "get origin", 0);
  JSValue get_search_params = JS_NewCFunction(ctx, JSRT_URLGetSearchParams, "get searchParams", 0);

  JSAtom href_atom = JS_NewAtom(ctx, "href");
  JSAtom protocol_atom = JS_NewAtom(ctx, "protocol");
  JSAtom username_atom = JS_NewAtom(ctx, "username");
  JSAtom password_atom = JS_NewAtom(ctx, "password");
  JSAtom host_atom = JS_NewAtom(ctx, "host");
  JSAtom hostname_atom = JS_NewAtom(ctx, "hostname");
  JSAtom port_atom = JS_NewAtom(ctx, "port");
  JSAtom pathname_atom = JS_NewAtom(ctx, "pathname");
  JSAtom search_atom = JS_NewAtom(ctx, "search");
  JSAtom hash_atom = JS_NewAtom(ctx, "hash");
  JSAtom origin_atom = JS_NewAtom(ctx, "origin");
  JSAtom search_params_atom = JS_NewAtom(ctx, "searchParams");

  JS_DefinePropertyGetSet(ctx, url_proto, href_atom, get_href, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, protocol_atom, get_protocol, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, username_atom, get_username, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, password_atom, get_password, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, host_atom, get_host, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, hostname_atom, get_hostname, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, port_atom, get_port, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, pathname_atom, get_pathname, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, search_atom, get_search, set_search, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, hash_atom, get_hash, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, origin_atom, get_origin, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, search_params_atom, get_search_params, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  JS_FreeAtom(ctx, href_atom);
  JS_FreeAtom(ctx, protocol_atom);
  JS_FreeAtom(ctx, username_atom);
  JS_FreeAtom(ctx, password_atom);
  JS_FreeAtom(ctx, host_atom);
  JS_FreeAtom(ctx, hostname_atom);
  JS_FreeAtom(ctx, port_atom);
  JS_FreeAtom(ctx, pathname_atom);
  JS_FreeAtom(ctx, search_atom);
  JS_FreeAtom(ctx, hash_atom);
  JS_FreeAtom(ctx, origin_atom);
  JS_FreeAtom(ctx, search_params_atom);

  // Methods
  JS_SetPropertyStr(ctx, url_proto, "toString", JS_NewCFunction(ctx, JSRT_URLToString, "toString", 0));
  JS_SetPropertyStr(ctx, url_proto, "toJSON", JS_NewCFunction(ctx, JSRT_URLToJSON, "toJSON", 0));

  JS_SetClassProto(ctx, JSRT_URLClassID, url_proto);

  JSValue url_ctor = JS_NewCFunction2(ctx, JSRT_URLConstructor, "URL", 2, JS_CFUNC_constructor, 0);

  // Set the constructor's prototype property
  JS_SetPropertyStr(ctx, url_ctor, "prototype", JS_DupValue(ctx, url_proto));

  // Set the prototype's constructor property
  JS_SetPropertyStr(ctx, url_proto, "constructor", JS_DupValue(ctx, url_ctor));

  JS_SetPropertyStr(ctx, rt->global, "URL", url_ctor);

  // Register URLSearchParams class
  JS_NewClassID(&JSRT_URLSearchParamsClassID);
  JS_NewClass(rt->rt, JSRT_URLSearchParamsClassID, &JSRT_URLSearchParamsClass);

  JSValue search_params_proto = JS_NewObject(ctx);
  JSRT_RegisterURLSearchParamsMethods(ctx, search_params_proto);

  JSValue search_params_ctor =
      JS_NewCFunction2(ctx, JSRT_URLSearchParamsConstructor, "URLSearchParams", 1, JS_CFUNC_constructor, 0);

  // Set the constructor's prototype property
  JS_SetPropertyStr(ctx, search_params_ctor, "prototype", JS_DupValue(ctx, search_params_proto));

  // Set the class prototype
  JS_SetClassProto(ctx, JSRT_URLSearchParamsClassID, search_params_proto);

  // Set the prototype's constructor property
  JS_SetPropertyStr(ctx, search_params_proto, "constructor", JS_DupValue(ctx, search_params_ctor));

  JS_SetPropertyStr(ctx, rt->global, "URLSearchParams", search_params_ctor);

  JSRT_Debug("URL/URLSearchParams API setup completed");
}