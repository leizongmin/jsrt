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

// Helper function to strip tab and newline characters from URL strings
// According to URL spec, these characters should be removed: tab (0x09), LF (0x0A), CR (0x0D)
static char* JSRT_StripURLControlCharacters(const char* input) {
  if (!input)
    return NULL;

  size_t len = strlen(input);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    char c = input[i];
    // Skip tab, line feed, and carriage return
    if (c != 0x09 && c != 0x0A && c != 0x0D) {
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

  const char* url_str_raw = JS_ToCString(ctx, argv[0]);
  if (!url_str_raw) {
    return JS_EXCEPTION;
  }

  // Strip control characters as per URL specification
  char* url_str = JSRT_StripURLControlCharacters(url_str_raw);
  JS_FreeCString(ctx, url_str_raw);
  if (!url_str) {
    return JS_EXCEPTION;
  }

  const char* base_str_raw = NULL;
  char* base_str = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    base_str_raw = JS_ToCString(ctx, argv[1]);
    if (!base_str_raw) {
      free(url_str);
      return JS_EXCEPTION;
    }
    base_str = JSRT_StripURLControlCharacters(base_str_raw);
    JS_FreeCString(ctx, base_str_raw);
    if (!base_str) {
      free(url_str);
      return JS_EXCEPTION;
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
  return JS_NewString(ctx, url->username ? url->username : "");
}

static JSValue JSRT_URLGetPassword(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->password ? url->password : "");
}

static JSValue JSRT_URLGetHost(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->host);
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
  return JS_NewString(ctx, url->port);
}

static JSValue JSRT_URLGetPathname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // For non-special schemes, return the raw pathname without encoding
  // For special schemes, return the percent-encoded pathname
  if (is_special_scheme(url->protocol)) {
    char* encoded_pathname = url_component_encode(url->pathname);
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
  } else {
    size_t len = strlen(new_search);
    url->search = malloc(len + 2);
    url->search[0] = '?';
    strcpy(url->search + 1, new_search);
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
      cached_params->params = new_params->params;
      new_params->params = NULL;  // Prevent double free
      JSRT_FreeSearchParams(new_params);
    }
  }

  // Rebuild href with new search component
  free(url->href);
  size_t href_len = strlen(url->protocol) + 2 + strlen(url->host) + strlen(url->pathname);
  if (url->search && strlen(url->search) > 0) {
    href_len += strlen(url->search);
  }
  if (url->hash && strlen(url->hash) > 0) {
    href_len += strlen(url->hash);
  }

  url->href = malloc(href_len + 1);
  strcpy(url->href, url->protocol);
  strcat(url->href, "//");
  strcat(url->href, url->host);
  strcat(url->href, url->pathname);

  if (url->search && strlen(url->search) > 0) {
    strcat(url->href, url->search);
  }

  if (url->hash && strlen(url->hash) > 0) {
    strcat(url->href, url->hash);
  }

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

  if (argc >= 1 && !JS_IsUndefined(argv[0])) {
    JSValue init = argv[0];

    // Check if it's already a URLSearchParams object
    if (JS_GetOpaque2(ctx, init, JSRT_URLSearchParamsClassID)) {
      JSRT_URLSearchParams* src = JS_GetOpaque2(ctx, init, JSRT_URLSearchParamsClassID);
      search_params = JSRT_CreateEmptySearchParams();

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
    }
    // Check if it's DOMException or DOMException.prototype (should throw due to branding checks)
    else if (!JS_IsString(init)) {
      // First check if this is a DOMException or DOMException.prototype
      JSValue global = JS_GetGlobalObject(ctx);
      JSValue dom_exception = JS_GetPropertyStr(ctx, global, "DOMException");
      if (!JS_IsUndefined(dom_exception)) {
        JSValue dom_exception_proto = JS_GetPropertyStr(ctx, dom_exception, "prototype");
        if (JS_SameValue(ctx, init, dom_exception_proto) || JS_SameValue(ctx, init, dom_exception)) {
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
        // It's an array or iterable - use sequence parser
        search_params = JSRT_ParseSearchParamsFromSequence(ctx, init);
        if (!search_params) {
          // If an exception is already pending (e.g., TypeError for invalid pairs), return it
          if (JS_HasException(ctx)) {
            return JS_EXCEPTION;
          }
          return JS_ThrowTypeError(ctx, "Invalid sequence argument to URLSearchParams constructor");
        }
      } else {
        // It's a plain object - use record parser
        search_params = JSRT_ParseSearchParamsFromRecord(ctx, init);
        if (!search_params) {
          // If an exception is already pending, return it
          if (JS_HasException(ctx)) {
            return JS_EXCEPTION;
          }
          return JS_ThrowTypeError(ctx, "Invalid record argument to URLSearchParams constructor");
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
    }
  } else {
    search_params = JSRT_CreateEmptySearchParams();
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