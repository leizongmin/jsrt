#include "url.h"

#include <ctype.h>
#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"

// Forward declare class IDs
static JSClassID JSRT_URLClassID;
static JSClassID JSRT_URLSearchParamsClassID;

// URL component structure
typedef struct {
  char* href;
  char* protocol;
  char* host;
  char* hostname;
  char* port;
  char* pathname;
  char* search;
  char* hash;
  char* origin;
} JSRT_URL;

// Simple URL parser (basic implementation)
static JSRT_URL* JSRT_ParseURL(const char* url, const char* base) {
  JSRT_URL* parsed = malloc(sizeof(JSRT_URL));
  memset(parsed, 0, sizeof(JSRT_URL));

  // Initialize with empty strings to prevent NULL dereference
  parsed->href = strdup(url);
  parsed->protocol = strdup("");
  parsed->host = strdup("");
  parsed->hostname = strdup("");
  parsed->port = strdup("");
  parsed->pathname = strdup("");
  parsed->search = strdup("");
  parsed->hash = strdup("");
  parsed->origin = strdup("");

  // Simple parsing - this is a basic implementation
  // In a production system, you'd want a full URL parser

  char* url_copy = strdup(url);
  char* ptr = url_copy;

  // Extract protocol
  char* protocol_end = strstr(ptr, "://");
  if (protocol_end) {
    *protocol_end = '\0';
    free(parsed->protocol);
    parsed->protocol = malloc(strlen(ptr) + 2);
    sprintf(parsed->protocol, "%s:", ptr);
    ptr = protocol_end + 3;
  }

  // Extract hash first (fragment)
  char* hash_start = strchr(ptr, '#');
  if (hash_start) {
    free(parsed->hash);
    parsed->hash = strdup(hash_start);
    *hash_start = '\0';
  }

  // Extract search (query)
  char* search_start = strchr(ptr, '?');
  if (search_start) {
    free(parsed->search);
    parsed->search = strdup(search_start);
    *search_start = '\0';
  }

  // Extract host and pathname
  char* path_start = strchr(ptr, '/');
  if (path_start) {
    free(parsed->pathname);
    parsed->pathname = strdup(path_start);
    *path_start = '\0';
  } else {
    free(parsed->pathname);
    parsed->pathname = strdup("/");
  }

  // What's left is the host
  if (strlen(ptr) > 0) {
    free(parsed->host);
    parsed->host = strdup(ptr);

    // Check for port
    char* port_start = strchr(ptr, ':');
    if (port_start) {
      *port_start = '\0';
      free(parsed->hostname);
      parsed->hostname = strdup(ptr);
      free(parsed->port);
      parsed->port = strdup(port_start + 1);
      *port_start = ':';  // Restore for host
    } else {
      free(parsed->hostname);
      parsed->hostname = strdup(ptr);
    }
  }

  // Build origin
  if (strlen(parsed->protocol) > 0 && strlen(parsed->host) > 0) {
    free(parsed->origin);
    parsed->origin = malloc(strlen(parsed->protocol) + strlen(parsed->host) + 4);
    sprintf(parsed->origin, "%s//%s", parsed->protocol, parsed->host);
  }

  free(url_copy);
  return parsed;
}

static void JSRT_FreeURL(JSRT_URL* url) {
  if (url) {
    free(url->href);
    free(url->protocol);
    free(url->host);
    free(url->hostname);
    free(url->port);
    free(url->pathname);
    free(url->search);
    free(url->hash);
    free(url->origin);
    free(url);
  }
}

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

static JSValue JSRT_URLConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "URL constructor requires at least 1 argument");
  }

  const char* url_str = JS_ToCString(ctx, argv[0]);
  if (!url_str) {
    return JS_EXCEPTION;
  }

  const char* base_str = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    base_str = JS_ToCString(ctx, argv[1]);
    if (!base_str) {
      JS_FreeCString(ctx, url_str);
      return JS_EXCEPTION;
    }
  }

  JSRT_URL* url = JSRT_ParseURL(url_str, base_str);

  JSValue obj = JS_NewObjectClass(ctx, JSRT_URLClassID);
  JS_SetOpaque(obj, url);

  JS_FreeCString(ctx, url_str);
  if (base_str) {
    JS_FreeCString(ctx, base_str);
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
  return JS_NewString(ctx, url->pathname);
}

static JSValue JSRT_URLGetSearch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->search);
}

static JSValue JSRT_URLGetHash(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->hash);
}

static JSValue JSRT_URLGetOrigin(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->origin);
}

static JSValue JSRT_URLToString(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLGetHref(ctx, this_val, argc, argv);
}

static JSValue JSRT_URLToJSON(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLGetHref(ctx, this_val, argc, argv);
}

// URLSearchParams implementation
typedef struct JSRT_URLSearchParam {
  char* name;
  char* value;
  struct JSRT_URLSearchParam* next;
} JSRT_URLSearchParam;

typedef struct {
  JSRT_URLSearchParam* params;
} JSRT_URLSearchParams;

static void JSRT_FreeSearchParams(JSRT_URLSearchParams* search_params) {
  if (search_params) {
    JSRT_URLSearchParam* param = search_params->params;
    while (param) {
      JSRT_URLSearchParam* next = param->next;
      free(param->name);
      free(param->value);
      free(param);
      param = next;
    }
    free(search_params);
  }
}

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

static JSRT_URLSearchParams* JSRT_ParseSearchParams(const char* search_string) {
  JSRT_URLSearchParams* search_params = malloc(sizeof(JSRT_URLSearchParams));
  search_params->params = NULL;

  if (!search_string || strlen(search_string) == 0) {
    return search_params;
  }

  const char* start = search_string;
  if (start[0] == '?')
    start++;  // Skip leading '?'

  char* params_copy = strdup(start);
  char* param_str = strtok(params_copy, "&");

  JSRT_URLSearchParam** tail = &search_params->params;

  while (param_str) {
    char* eq_pos = strchr(param_str, '=');

    JSRT_URLSearchParam* param = malloc(sizeof(JSRT_URLSearchParam));
    param->next = NULL;

    if (eq_pos) {
      *eq_pos = '\0';
      param->name = strdup(param_str);
      param->value = strdup(eq_pos + 1);
    } else {
      param->name = strdup(param_str);
      param->value = strdup("");
    }

    *tail = param;
    tail = &param->next;

    param_str = strtok(NULL, "&");
  }

  free(params_copy);
  return search_params;
}

static JSValue JSRT_URLSearchParamsConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params;

  if (argc >= 1 && !JS_IsUndefined(argv[0])) {
    const char* init_str = JS_ToCString(ctx, argv[0]);
    if (!init_str) {
      return JS_EXCEPTION;
    }
    search_params = JSRT_ParseSearchParams(init_str);
    JS_FreeCString(ctx, init_str);
  } else {
    search_params = malloc(sizeof(JSRT_URLSearchParams));
    search_params->params = NULL;
  }

  JSValue obj = JS_NewObjectClass(ctx, JSRT_URLSearchParamsClassID);
  JS_SetOpaque(obj, search_params);
  return obj;
}

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
    if (strcmp(param->name, name) == 0) {
      JS_FreeCString(ctx, name);
      return JS_NewString(ctx, param->value);
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
  return JS_NULL;
}

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
    return JS_EXCEPTION;
  }

  // Remove all existing params with this name
  JSRT_URLSearchParam** current = &search_params->params;
  while (*current) {
    if (strcmp((*current)->name, name) == 0) {
      JSRT_URLSearchParam* to_remove = *current;
      *current = (*current)->next;
      free(to_remove->name);
      free(to_remove->value);
      free(to_remove);
    } else {
      current = &(*current)->next;
    }
  }

  // Add new param
  JSRT_URLSearchParam* new_param = malloc(sizeof(JSRT_URLSearchParam));
  new_param->name = strdup(name);
  new_param->value = strdup(value);
  new_param->next = search_params->params;
  search_params->params = new_param;

  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);
  return JS_UNDEFINED;
}

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
    return JS_EXCEPTION;
  }

  JSRT_URLSearchParam* new_param = malloc(sizeof(JSRT_URLSearchParam));
  new_param->name = strdup(name);
  new_param->value = strdup(value);
  new_param->next = search_params->params;
  search_params->params = new_param;

  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);
  return JS_UNDEFINED;
}

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

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (strcmp(param->name, name) == 0) {
      JS_FreeCString(ctx, name);
      return JS_NewBool(ctx, true);
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
  return JS_NewBool(ctx, false);
}

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

  JSRT_URLSearchParam** current = &search_params->params;
  while (*current) {
    if (strcmp((*current)->name, name) == 0) {
      JSRT_URLSearchParam* to_remove = *current;
      *current = (*current)->next;
      free(to_remove->name);
      free(to_remove->value);
      free(to_remove);
    } else {
      current = &(*current)->next;
    }
  }

  JS_FreeCString(ctx, name);
  return JS_UNDEFINED;
}

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
  int count = 0;
  while (param) {
    total_len += strlen(param->name) + strlen(param->value) + 2;  // name=value&
    param = param->next;
    count++;
  }

  if (count == 0) {
    return JS_NewString(ctx, "");
  }

  char* result = malloc(total_len + 1);
  result[0] = '\0';

  param = search_params->params;
  bool first = true;
  while (param) {
    if (!first) {
      strcat(result, "&");
    }
    strcat(result, param->name);
    strcat(result, "=");
    strcat(result, param->value);
    first = false;
    param = param->next;
  }

  JSValue js_result = JS_NewString(ctx, result);
  free(result);
  return js_result;
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
  JSValue get_host = JS_NewCFunction(ctx, JSRT_URLGetHost, "get host", 0);
  JSValue get_hostname = JS_NewCFunction(ctx, JSRT_URLGetHostname, "get hostname", 0);
  JSValue get_port = JS_NewCFunction(ctx, JSRT_URLGetPort, "get port", 0);
  JSValue get_pathname = JS_NewCFunction(ctx, JSRT_URLGetPathname, "get pathname", 0);
  JSValue get_search = JS_NewCFunction(ctx, JSRT_URLGetSearch, "get search", 0);
  JSValue get_hash = JS_NewCFunction(ctx, JSRT_URLGetHash, "get hash", 0);
  JSValue get_origin = JS_NewCFunction(ctx, JSRT_URLGetOrigin, "get origin", 0);

  JSAtom href_atom = JS_NewAtom(ctx, "href");
  JSAtom protocol_atom = JS_NewAtom(ctx, "protocol");
  JSAtom host_atom = JS_NewAtom(ctx, "host");
  JSAtom hostname_atom = JS_NewAtom(ctx, "hostname");
  JSAtom port_atom = JS_NewAtom(ctx, "port");
  JSAtom pathname_atom = JS_NewAtom(ctx, "pathname");
  JSAtom search_atom = JS_NewAtom(ctx, "search");
  JSAtom hash_atom = JS_NewAtom(ctx, "hash");
  JSAtom origin_atom = JS_NewAtom(ctx, "origin");

  JS_DefinePropertyGetSet(ctx, url_proto, href_atom, get_href, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, protocol_atom, get_protocol, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, host_atom, get_host, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, hostname_atom, get_hostname, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, port_atom, get_port, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, pathname_atom, get_pathname, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, search_atom, get_search, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, hash_atom, get_hash, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, origin_atom, get_origin, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  JS_FreeAtom(ctx, href_atom);
  JS_FreeAtom(ctx, protocol_atom);
  JS_FreeAtom(ctx, host_atom);
  JS_FreeAtom(ctx, hostname_atom);
  JS_FreeAtom(ctx, port_atom);
  JS_FreeAtom(ctx, pathname_atom);
  JS_FreeAtom(ctx, search_atom);
  JS_FreeAtom(ctx, hash_atom);
  JS_FreeAtom(ctx, origin_atom);

  // Methods
  JS_SetPropertyStr(ctx, url_proto, "toString", JS_NewCFunction(ctx, JSRT_URLToString, "toString", 0));
  JS_SetPropertyStr(ctx, url_proto, "toJSON", JS_NewCFunction(ctx, JSRT_URLToJSON, "toJSON", 0));

  JS_SetClassProto(ctx, JSRT_URLClassID, url_proto);

  JSValue url_ctor = JS_NewCFunction2(ctx, JSRT_URLConstructor, "URL", 2, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "URL", url_ctor);

  // Register URLSearchParams class
  JS_NewClassID(&JSRT_URLSearchParamsClassID);
  JS_NewClass(rt->rt, JSRT_URLSearchParamsClassID, &JSRT_URLSearchParamsClass);

  JSValue search_params_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, search_params_proto, "get", JS_NewCFunction(ctx, JSRT_URLSearchParamsGet, "get", 1));
  JS_SetPropertyStr(ctx, search_params_proto, "set", JS_NewCFunction(ctx, JSRT_URLSearchParamsSet, "set", 2));
  JS_SetPropertyStr(ctx, search_params_proto, "append", JS_NewCFunction(ctx, JSRT_URLSearchParamsAppend, "append", 2));
  JS_SetPropertyStr(ctx, search_params_proto, "has", JS_NewCFunction(ctx, JSRT_URLSearchParamsHas, "has", 1));
  JS_SetPropertyStr(ctx, search_params_proto, "delete", JS_NewCFunction(ctx, JSRT_URLSearchParamsDelete, "delete", 1));
  JS_SetPropertyStr(ctx, search_params_proto, "toString",
                    JS_NewCFunction(ctx, JSRT_URLSearchParamsToString, "toString", 0));

  JS_SetClassProto(ctx, JSRT_URLSearchParamsClassID, search_params_proto);

  JSValue search_params_ctor =
      JS_NewCFunction2(ctx, JSRT_URLSearchParamsConstructor, "URLSearchParams", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "URLSearchParams", search_params_ctor);

  JSRT_Debug("URL/URLSearchParams API setup completed");
}