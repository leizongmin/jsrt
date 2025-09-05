#include "url.h"

#include <ctype.h>
#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"
#include "formdata.h"

// Forward declare class IDs
static JSClassID JSRT_URLClassID;
static JSClassID JSRT_URLSearchParamsClassID;

// External class ID from formdata.c
extern JSClassID JSRT_FormDataClassID;

// Forward declare structures
struct JSRT_URL;

// URLSearchParams parameter structure
typedef struct JSRT_URLSearchParam {
  char* name;
  char* value;
  size_t name_len;   // Length of name (may contain null bytes)
  size_t value_len;  // Length of value (may contain null bytes)
  struct JSRT_URLSearchParam* next;
} JSRT_URLSearchParam;

// URLSearchParams structure
typedef struct JSRT_URLSearchParams {
  JSRT_URLSearchParam* params;
  struct JSRT_URL* parent_url;  // Reference to parent URL for href updates
  JSContext* ctx;               // Context for parent URL updates
} JSRT_URLSearchParams;

// URL component structure
typedef struct JSRT_URL {
  char* href;
  char* protocol;
  char* host;
  char* hostname;
  char* port;
  char* pathname;
  char* search;
  char* hash;
  char* origin;
  JSValue search_params;  // Cached URLSearchParams object
  JSContext* ctx;         // Context for managing search_params
} JSRT_URL;

// Forward declarations
static JSRT_URL* JSRT_ParseURL(const char* url, const char* base);
static int validate_url_characters(const char* url);
static void JSRT_FreeURL(JSRT_URL* url);
static JSValue JSRT_URLSearchParamsToString(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static char* url_encode_with_len(const char* str, size_t len);
static JSRT_URLSearchParams* JSRT_ParseSearchParams(const char* search_string, size_t string_len);
static JSRT_URLSearchParams* JSRT_ParseSearchParamsFromFormData(JSContext* ctx, JSValueConst formdata_val);
static void JSRT_FreeSearchParams(JSRT_URLSearchParams* search_params);

// Simple URL parser (basic implementation)
static int is_valid_scheme(const char* scheme) {
  if (!scheme || strlen(scheme) == 0)
    return 0;

  // First character must be a letter
  if (!((scheme[0] >= 'a' && scheme[0] <= 'z') || (scheme[0] >= 'A' && scheme[0] <= 'Z'))) {
    return 0;
  }

  // Rest can be letters, digits, +, -, .
  for (size_t i = 1; i < strlen(scheme); i++) {
    char c = scheme[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '-' ||
          c == '.')) {
      return 0;
    }
  }
  return 1;
}

static int is_default_port(const char* scheme, const char* port) {
  if (!port || strlen(port) == 0)
    return 1;  // No port specified, so it's implicit default

  if (strcmp(scheme, "https") == 0 && strcmp(port, "443") == 0)
    return 1;
  if (strcmp(scheme, "http") == 0 && strcmp(port, "80") == 0)
    return 1;
  if (strcmp(scheme, "ws") == 0 && strcmp(port, "80") == 0)
    return 1;
  if (strcmp(scheme, "wss") == 0 && strcmp(port, "443") == 0)
    return 1;
  if (strcmp(scheme, "ftp") == 0 && strcmp(port, "21") == 0)
    return 1;

  return 0;
}

static char* compute_origin(const char* protocol, const char* hostname, const char* port) {
  if (!protocol || !hostname || strlen(protocol) == 0 || strlen(hostname) == 0) {
    return strdup("null");
  }

  // Extract scheme without colon
  char* scheme = strdup(protocol);
  if (strlen(scheme) > 0 && scheme[strlen(scheme) - 1] == ':') {
    scheme[strlen(scheme) - 1] = '\0';
  }

  // Some schemes always have null origin
  if (strcmp(scheme, "file") == 0 || strcmp(scheme, "data") == 0 || strcmp(scheme, "javascript") == 0 ||
      strcmp(scheme, "blob") == 0) {
    free(scheme);
    return strdup("null");
  }

  char* origin;
  if (is_default_port(scheme, port) || !port || strlen(port) == 0) {
    // Omit default port
    origin = malloc(strlen(protocol) + strlen(hostname) + 4);
    sprintf(origin, "%s//%s", protocol, hostname);
  } else {
    // Include custom port
    origin = malloc(strlen(protocol) + strlen(hostname) + strlen(port) + 5);
    sprintf(origin, "%s//%s:%s", protocol, hostname, port);
  }

  free(scheme);
  return origin;
}

static JSRT_URL* resolve_relative_url(const char* url, const char* base) {
  // Parse base URL first
  JSRT_URL* base_url = JSRT_ParseURL(base, NULL);
  if (!base_url || strlen(base_url->protocol) == 0 || strlen(base_url->host) == 0) {
    if (base_url)
      JSRT_FreeURL(base_url);
    return NULL;  // Invalid base URL
  }

  JSRT_URL* result = malloc(sizeof(JSRT_URL));
  memset(result, 0, sizeof(JSRT_URL));

  // Initialize result with base URL components
  result->protocol = strdup(base_url->protocol);
  result->host = strdup(base_url->host);
  result->hostname = strdup(base_url->hostname);
  result->port = strdup(base_url->port);
  result->search_params = JS_UNDEFINED;
  result->ctx = NULL;

  if (url[0] == '/') {
    // Absolute path
    result->pathname = strdup(url);
    result->search = strdup("");
    result->hash = strdup("");
  } else {
    // Relative path - for now, just use the relative path as pathname
    // A full implementation would resolve . and .. components
    result->pathname = malloc(strlen(base_url->pathname) + strlen(url) + 2);
    sprintf(result->pathname, "%s/%s", base_url->pathname, url);
    result->search = strdup("");
    result->hash = strdup("");
  }

  // Build origin and href
  result->origin = malloc(strlen(result->protocol) + strlen(result->host) + 4);
  sprintf(result->origin, "%s//%s", result->protocol, result->host);

  result->href =
      malloc(strlen(result->origin) + strlen(result->pathname) + strlen(result->search) + strlen(result->hash) + 1);
  sprintf(result->href, "%s%s%s%s", result->origin, result->pathname, result->search, result->hash);

  JSRT_FreeURL(base_url);
  return result;
}

// Validate URL characters according to WPT specification
// Reject ASCII tab (0x09), LF (0x0A), and CR (0x0D)
static int validate_url_characters(const char* url) {
  for (const char* p = url; *p; p++) {
    // Check for ASCII tab, LF, CR which should be rejected
    if (*p == 0x09 || *p == 0x0A || *p == 0x0D) {
      return 0;  // Invalid character found
    }
    // Optionally check for other control characters
    if (*p < 0x20 && *p != 0x09 && *p != 0x0A && *p != 0x0D) {
      return 0;  // Other control characters
    }
  }
  return 1;  // Valid
}

static JSRT_URL* JSRT_ParseURL(const char* url, const char* base) {
  // Validate URL characters first
  if (!validate_url_characters(url)) {
    return NULL;  // Invalid characters detected
  }
  if (base && !validate_url_characters(base)) {
    return NULL;  // Invalid characters in base URL
  }

  // Handle relative URLs with base
  if (base && (url[0] == '/' || (url[0] != '\0' && strstr(url, "://") == NULL))) {
    return resolve_relative_url(url, base);
  }

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
  parsed->search_params = JS_UNDEFINED;
  parsed->ctx = NULL;

  // Simple parsing - this is a basic implementation
  // In a production system, you'd want a full URL parser

  char* url_copy = strdup(url);
  char* ptr = url_copy;

  // Handle special schemes first
  if (strncmp(ptr, "data:", 5) == 0) {
    free(parsed->protocol);
    parsed->protocol = strdup("data:");
    free(parsed->pathname);
    parsed->pathname = strdup(ptr + 5);  // Everything after "data:"
    free(url_copy);
    // Build origin
    free(parsed->origin);
    parsed->origin = compute_origin(parsed->protocol, parsed->hostname, parsed->port);
    return parsed;
  }

  // Extract protocol for regular URLs
  char* protocol_end = strstr(ptr, "://");
  if (protocol_end) {
    *protocol_end = '\0';
    free(parsed->protocol);
    parsed->protocol = malloc(strlen(ptr) + 2);
    sprintf(parsed->protocol, "%s:", ptr);
    ptr = protocol_end + 3;
  } else if (strncmp(ptr, "file:", 5) == 0) {
    free(parsed->protocol);
    parsed->protocol = strdup("file:");
    ptr += 5;
    // Handle file:///path format - skip extra slashes
    while (*ptr == '/')
      ptr++;
    free(parsed->pathname);
    parsed->pathname = malloc(strlen(ptr) + 2);
    sprintf(parsed->pathname, "/%s", ptr);
    free(url_copy);
    // Build origin
    free(parsed->origin);
    parsed->origin = compute_origin(parsed->protocol, parsed->hostname, parsed->port);
    return parsed;
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

  // Build origin using the improved function
  free(parsed->origin);
  parsed->origin = compute_origin(parsed->protocol, parsed->hostname, parsed->port);

  free(url_copy);

  // Validate the parsed URL
  if (strlen(parsed->protocol) > 0) {
    // Extract scheme without the colon
    char* scheme = strdup(parsed->protocol);
    if (strlen(scheme) > 0 && scheme[strlen(scheme) - 1] == ':') {
      scheme[strlen(scheme) - 1] = '\0';
    }

    if (!is_valid_scheme(scheme)) {
      free(scheme);
      JSRT_FreeURL(parsed);
      return NULL;  // Invalid scheme
    }

    // Some schemes don't require hosts (file:, data:, etc.)
    if (strcmp(scheme, "file") != 0 && strcmp(scheme, "data") != 0 && strcmp(scheme, "javascript") != 0 &&
        strcmp(scheme, "blob") != 0) {
      // These schemes require a host
      if (strlen(parsed->host) == 0) {
        free(scheme);
        JSRT_FreeURL(parsed);
        return NULL;  // Missing required host
      }
    }

    free(scheme);
  } else {
    // No protocol found - this is not a valid absolute URL
    JSRT_FreeURL(parsed);
    return NULL;
  }

  return parsed;
}

static void JSRT_FreeURL(JSRT_URL* url) {
  if (url) {
    if (url->ctx && !JS_IsUndefined(url->search_params)) {
      JS_FreeValue(url->ctx, url->search_params);
    }
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
  if (!url) {
    JS_FreeCString(ctx, url_str);
    if (base_str) {
      JS_FreeCString(ctx, base_str);
    }
    return JS_ThrowTypeError(ctx, "Invalid URL");
  }

  url->ctx = ctx;  // Set context for managing search_params

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
  return JS_NewString(ctx, url->hash);
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

// URLSearchParams implementation

// Helper function to create a parameter with proper length handling
static JSRT_URLSearchParam* create_url_param(const char* name, size_t name_len, const char* value, size_t value_len) {
  JSRT_URLSearchParam* param = malloc(sizeof(JSRT_URLSearchParam));
  param->name = malloc(name_len + 1);
  memcpy(param->name, name, name_len);
  param->name[name_len] = '\0';
  param->name_len = name_len;
  param->value = malloc(value_len + 1);
  memcpy(param->value, value, value_len);
  param->value[value_len] = '\0';
  param->value_len = value_len;
  param->next = NULL;
  return param;
}

// Helper function to update parent URL's href when URLSearchParams change
static void update_parent_url_href(JSRT_URLSearchParams* search_params) {
  if (!search_params || !search_params->parent_url) {
    return;
  }

  JSRT_URL* url = search_params->parent_url;

  // Generate new search string directly from parameters
  JSContext* ctx = search_params->ctx;
  if (!ctx) {
    return;  // No context available for updates
  }

  // Build search string manually to avoid circular references
  char* new_search_str = NULL;
  if (!search_params->params) {
    new_search_str = strdup("");
  } else {
    // Calculate required length
    size_t total_len = 0;
    JSRT_URLSearchParam* param = search_params->params;
    bool first = true;
    while (param) {
      char* encoded_name = url_encode_with_len(param->name, param->name_len);
      char* encoded_value = url_encode_with_len(param->value, param->value_len);
      size_t pair_len = strlen(encoded_name) + strlen(encoded_value) + 1;  // name=value
      if (!first)
        pair_len += 1;  // &
      total_len += pair_len;
      free(encoded_name);
      free(encoded_value);
      param = param->next;
      first = false;
    }

    // Build the string
    new_search_str = malloc(total_len + 1);
    new_search_str[0] = '\0';
    param = search_params->params;
    first = true;
    while (param) {
      char* encoded_name = url_encode_with_len(param->name, param->name_len);
      char* encoded_value = url_encode_with_len(param->value, param->value_len);

      if (!first)
        strcat(new_search_str, "&");
      strcat(new_search_str, encoded_name);
      strcat(new_search_str, "=");
      strcat(new_search_str, encoded_value);

      free(encoded_name);
      free(encoded_value);
      param = param->next;
      first = false;
    }
  }

  // Update URL's search component
  free(url->search);
  if (strlen(new_search_str) > 0) {
    url->search = malloc(strlen(new_search_str) + 2);  // +1 for '?', +1 for '\0'
    sprintf(url->search, "?%s", new_search_str);
  } else {
    url->search = strdup("");
  }

  // Rebuild href
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

  free(new_search_str);
}

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

static int hex_to_int(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}

static char* url_decode_with_length_and_output_len(const char* str, size_t len, size_t* output_len) {
  char* decoded = malloc(len + 1);
  size_t i = 0, j = 0;

  while (i < len) {
    if (str[i] == '%' && i + 2 < len) {
      int h1 = hex_to_int(str[i + 1]);
      int h2 = hex_to_int(str[i + 2]);
      if (h1 >= 0 && h2 >= 0) {
        decoded[j++] = (char)((h1 << 4) | h2);
        i += 3;
        continue;
      }
    } else if (str[i] == '+') {
      // + should decode to space in query parameters
      decoded[j++] = ' ';
      i++;
      continue;
    }
    decoded[j++] = str[i++];
  }
  decoded[j] = '\0';
  if (output_len) {
    *output_len = j;
  }
  return decoded;
}

static char* url_decode_with_length(const char* str, size_t len) {
  return url_decode_with_length_and_output_len(str, len, NULL);
}

static char* url_decode(const char* str) {
  size_t len = strlen(str);
  return url_decode_with_length(str, len);
}

static JSRT_URLSearchParams* JSRT_ParseSearchParams(const char* search_string, size_t string_len) {
  JSRT_URLSearchParams* search_params = malloc(sizeof(JSRT_URLSearchParams));
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
      param->next = NULL;

      if (eq_pos < param_start + param_len) {
        // Has '=' - split into name and value
        size_t name_len = eq_pos - param_start;
        size_t value_len = param_start + param_len - eq_pos - 1;

        param->name = url_decode_with_length_and_output_len(start + param_start, name_len, &param->name_len);
        param->value = url_decode_with_length_and_output_len(start + eq_pos + 1, value_len, &param->value_len);
      } else {
        // No '=' - name only, empty value
        param->name = url_decode_with_length_and_output_len(start + param_start, param_len, &param->name_len);
        param->value = strdup("");
        param->value_len = 0;
      }

      // Lengths are already set by url_decode_with_length_and_output_len

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

static JSRT_URLSearchParams* JSRT_CreateEmptySearchParams() {
  JSRT_URLSearchParams* search_params = malloc(sizeof(JSRT_URLSearchParams));
  search_params->params = NULL;
  search_params->parent_url = NULL;
  search_params->ctx = NULL;
  return search_params;
}

static void JSRT_AddSearchParam(JSRT_URLSearchParams* search_params, const char* name, const char* value) {
  size_t name_len = strlen(name);
  size_t value_len = strlen(value);
  JSRT_URLSearchParam* param = create_url_param(name, name_len, value, value_len);

  // Add at end to maintain insertion order
  if (!search_params->params) {
    search_params->params = param;
  } else {
    JSRT_URLSearchParam* tail = search_params->params;
    while (tail->next) {
      tail = tail->next;
    }
    tail->next = param;
  }
}

static JSRT_URLSearchParams* JSRT_ParseSearchParamsFromSequence(JSContext* ctx, JSValue seq) {
  JSRT_URLSearchParams* search_params = JSRT_CreateEmptySearchParams();

  // TODO: Check if it has Symbol.iterator (should use iterator protocol)
  // For now, skip iterator protocol and use array-like handling only
  if (false) {
    // Use iterator protocol
    JSAtom iterator_symbol2 = JS_NewAtom(ctx, "Symbol.iterator");
    JSValue iterator_method = JS_GetProperty(ctx, seq, iterator_symbol2);
    JS_FreeAtom(ctx, iterator_symbol2);
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

static JSRT_URLSearchParams* JSRT_ParseSearchParamsFromRecord(JSContext* ctx, JSValue record) {
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

    const char* name_str = JS_AtomToCString(ctx, properties[i].atom);
    const char* value_str = JS_ToCString(ctx, value);

    if (name_str && value_str) {
      JSRT_AddSearchParam(search_params, name_str, value_str);
    }

    if (name_str)
      JS_FreeCString(ctx, name_str);
    if (value_str)
      JS_FreeCString(ctx, value_str);
    JS_FreeValue(ctx, value);
  }

  JS_FreePropertyEnum(ctx, properties, count);
  return search_params;
}

static JSRT_URLSearchParams* JSRT_ParseSearchParamsFromFormData(JSContext* ctx, JSValueConst formdata_val) {
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
    // Check if it's an iterable (has Symbol.iterator) or array-like sequence
    else if (!JS_IsString(init)) {
      JSAtom length_atom = JS_NewAtom(ctx, "length");
      int has_length = JS_HasProperty(ctx, init, length_atom);
      JS_FreeAtom(ctx, length_atom);

      // Functions have a length property but should typically be treated as records
      // unless they are specifically array-like (like Arguments object)
      // For WPT compliance, functions like DOMException should be treated as records
      bool is_function = JS_IsFunction(ctx, init);

      // Check if it's truly array-like by verifying it has numeric indices AND
      // those indices contain valid [name, value] pairs
      bool is_array_like = false;
      // If it's a real Array, always treat as sequence (let sequence parser handle validation)
      if (JS_IsArray(ctx, init)) {
        is_array_like = true;
      } else if (has_length && !is_function) {
        // For non-arrays with length property, only treat as sequence if it looks like
        // it contains [name, value] pairs (to distinguish from objects like {length: 2, 0: 'a', 1: 'b'})
        JSValue length_val = JS_GetPropertyStr(ctx, init, "length");
        int32_t length;
        if (!JS_IsException(length_val) && JS_ToInt32(ctx, &length, length_val) == 0 && length > 0) {
          // Check if first element exists and is a valid [name, value] pair
          JSValue first_element = JS_GetPropertyUint32(ctx, init, 0);
          if (!JS_IsUndefined(first_element)) {
            // Check if first element is array-like with length 2 (valid pair)
            JSValue elem_length_val = JS_GetPropertyStr(ctx, first_element, "length");
            int32_t elem_length;
            if (!JS_IsException(elem_length_val) && JS_ToInt32(ctx, &elem_length, elem_length_val) == 0 &&
                elem_length == 2) {
              // This looks like a valid sequence of [name, value] pairs
              is_array_like = true;
            }
            JS_FreeValue(ctx, elem_length_val);
          }
          JS_FreeValue(ctx, first_element);
        }
        JS_FreeValue(ctx, length_val);
      }

      if (is_array_like) {
        // Try to parse as sequence
        search_params = JSRT_ParseSearchParamsFromSequence(ctx, init);
        if (!search_params) {
          // If an exception is already pending (e.g., TypeError for invalid pairs), return it
          if (JS_HasException(ctx)) {
            return JS_EXCEPTION;
          }
          return JS_ThrowTypeError(ctx, "Invalid sequence argument to URLSearchParams constructor");
        }
      } else if (JS_IsObject(init)) {
        // Check if it's a record/object (not null, not array, not string)
        // This now includes functions with enumerable properties (like DOMException)

        // Special case: DOMException.prototype should throw TypeError due to branding checks
        JSValue dom_exception_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "DOMException");
        if (!JS_IsUndefined(dom_exception_ctor)) {
          JSValue dom_exception_proto = JS_GetPropertyStr(ctx, dom_exception_ctor, "prototype");
          if (JS_SameValue(ctx, init, dom_exception_proto)) {
            JS_FreeValue(ctx, dom_exception_ctor);
            JS_FreeValue(ctx, dom_exception_proto);
            return JS_ThrowTypeError(
                ctx, "Constructing a URLSearchParams from DOMException.prototype should throw due to branding checks");
          }
          JS_FreeValue(ctx, dom_exception_proto);
        }
        JS_FreeValue(ctx, dom_exception_ctor);

        search_params = JSRT_ParseSearchParamsFromRecord(ctx, init);
        if (!search_params) {
          return JS_ThrowTypeError(ctx, "Invalid record argument to URLSearchParams constructor");
        }
      } else {
        // Otherwise, treat as string
        size_t init_len;
        const char* init_str = JS_ToCStringLen(ctx, &init_len, init);
        if (!init_str) {
          return JS_EXCEPTION;
        }
        search_params = JSRT_ParseSearchParams(init_str, init_len);
        JS_FreeCString(ctx, init_str);
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
      return JS_NewStringLen(ctx, param->value, param->value_len);
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

  size_t name_len, value_len;
  const char* name = JS_ToCStringLen(ctx, &name_len, argv[0]);
  const char* value = JS_ToCStringLen(ctx, &value_len, argv[1]);
  if (!name || !value) {
    return JS_EXCEPTION;
  }

  // Find first parameter with this name and update its value
  // Remove any subsequent parameters with the same name
  JSRT_URLSearchParam* first_match = NULL;
  JSRT_URLSearchParam** current = &search_params->params;

  while (*current) {
    if (strcmp((*current)->name, name) == 0) {
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

  // If no parameter with this name existed, add new one at the end
  if (!first_match) {
    JSRT_URLSearchParam* new_param = create_url_param(name, name_len, value, value_len);

    if (!search_params->params) {
      search_params->params = new_param;
    } else {
      JSRT_URLSearchParam* tail = search_params->params;
      while (tail->next) {
        tail = tail->next;
      }
      tail->next = new_param;
    }
  }

  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);

  // Update parent URL's href if connected
  update_parent_url_href(search_params);

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

  size_t name_len, value_len;
  const char* name = JS_ToCStringLen(ctx, &name_len, argv[0]);
  const char* value = JS_ToCStringLen(ctx, &value_len, argv[1]);
  if (!name || !value) {
    return JS_EXCEPTION;
  }

  JSRT_URLSearchParam* new_param = create_url_param(name, name_len, value, value_len);

  // Add at end to maintain insertion order
  if (!search_params->params) {
    search_params->params = new_param;
  } else {
    JSRT_URLSearchParam* tail = search_params->params;
    while (tail->next) {
      tail = tail->next;
    }
    tail->next = new_param;
  }

  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);

  // Update parent URL's href if connected
  update_parent_url_href(search_params);

  return JS_UNDEFINED;
}

static JSValue JSRT_URLSearchParamsHas(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "has() requires at least 1 argument");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  const char* value = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    value = JS_ToCString(ctx, argv[1]);
    if (!value) {
      JS_FreeCString(ctx, name);
      return JS_EXCEPTION;
    }
  }

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (strcmp(param->name, name) == 0) {
      if (value) {
        // Two-argument form: check both name and value
        if (strcmp(param->value, value) == 0) {
          JS_FreeCString(ctx, name);
          JS_FreeCString(ctx, value);
          return JS_NewBool(ctx, true);
        }
      } else {
        // One-argument form (or undefined second arg): just check name
        JS_FreeCString(ctx, name);
        return JS_NewBool(ctx, true);
      }
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
  if (value) {
    JS_FreeCString(ctx, value);
  }
  return JS_NewBool(ctx, false);
}

static JSValue JSRT_URLSearchParamsDelete(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "delete() requires at least 1 argument");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  const char* value = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    value = JS_ToCString(ctx, argv[1]);
    if (!value) {
      JS_FreeCString(ctx, name);
      return JS_EXCEPTION;
    }
  }

  JSRT_URLSearchParam** current = &search_params->params;
  while (*current) {
    if (strcmp((*current)->name, name) == 0) {
      if (value) {
        // Two-argument form: only delete if both name and value match
        if (strcmp((*current)->value, value) == 0) {
          JSRT_URLSearchParam* to_remove = *current;
          *current = (*current)->next;
          free(to_remove->name);
          free(to_remove->value);
          free(to_remove);
        } else {
          current = &(*current)->next;
        }
      } else {
        // One-argument form: delete all parameters with this name
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

  JS_FreeCString(ctx, name);
  if (value) {
    JS_FreeCString(ctx, value);
  }

  // Update parent URL's href if connected
  update_parent_url_href(search_params);

  return JS_UNDEFINED;
}

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
  int array_index = 0;

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (strcmp(param->name, name) == 0) {
      JS_SetPropertyUint32(ctx, result_array, array_index++, JS_NewStringLen(ctx, param->value, param->value_len));
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
  return result_array;
}

static JSValue JSRT_URLSearchParamsGetSize(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  int count = 0;
  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    count++;
    param = param->next;
  }

  return JS_NewInt32(ctx, count);
}

static char* url_encode_with_len(const char* str, size_t len) {
  static const char hex_chars[] = "0123456789ABCDEF";
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*') {
      encoded_len++;
    } else if (c == ' ') {
      encoded_len++;  // space becomes +
    } else {
      encoded_len += 3;  // %XX
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*') {
      encoded[j++] = c;
    } else if (c == ' ') {
      encoded[j++] = '+';
    } else {
      encoded[j++] = '%';
      encoded[j++] = hex_chars[c >> 4];
      encoded[j++] = hex_chars[c & 15];
    }
  }
  encoded[j] = '\0';
  return encoded;
}

// Backward compatibility wrapper
static char* url_encode(const char* str) {
  return url_encode_with_len(str, strlen(str));
}

static JSValue JSRT_URLSearchParamsToString(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  if (!search_params->params) {
    return JS_NewString(ctx, "");
  }

  // Build result string using a dynamic approach
  char* result = malloc(1);
  result[0] = '\0';
  size_t result_len = 0;

  JSRT_URLSearchParam* param = search_params->params;
  bool first = true;
  while (param) {
    char* encoded_name = url_encode_with_len(param->name, param->name_len);
    char* encoded_value = url_encode_with_len(param->value, param->value_len);

    size_t pair_len = strlen(encoded_name) + strlen(encoded_value) + 1;  // name=value
    if (!first)
      pair_len += 1;  // &

    result = realloc(result, result_len + pair_len + 1);

    if (!first) {
      strcat(result, "&");
    }
    strcat(result, encoded_name);
    strcat(result, "=");
    strcat(result, encoded_value);

    result_len += pair_len;
    first = false;

    free(encoded_name);
    free(encoded_value);
    param = param->next;
  }

  JSValue js_result = JS_NewString(ctx, result);
  free(result);
  return js_result;
}

// URLSearchParams Iterator Implementation
typedef struct {
  JSRT_URLSearchParams* params;
  JSRT_URLSearchParam* current;
  int type;  // 0=entries, 1=keys, 2=values
} JSRT_URLSearchParamsIterator;

static JSClassID JSRT_URLSearchParamsIteratorClassID;

static void JSRT_URLSearchParamsIteratorFinalizer(JSRuntime* rt, JSValue val) {
  JSRT_URLSearchParamsIterator* iterator = JS_GetOpaque(val, JSRT_URLSearchParamsIteratorClassID);
  if (iterator) {
    free(iterator);
  }
}

static JSClassDef JSRT_URLSearchParamsIteratorClass = {
    "URLSearchParamsIterator",
    .finalizer = JSRT_URLSearchParamsIteratorFinalizer,
};

static JSValue JSRT_URLSearchParamsIteratorNext(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParamsIterator* iterator = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsIteratorClassID);
  if (!iterator) {
    return JS_EXCEPTION;
  }

  JSValue result = JS_NewObject(ctx);

  if (!iterator->current) {
    // Iterator is done
    JS_SetPropertyStr(ctx, result, "done", JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, result, "value", JS_UNDEFINED);
  } else {
    // Return current item
    JS_SetPropertyStr(ctx, result, "done", JS_NewBool(ctx, false));

    JSValue value;
    switch (iterator->type) {
      case 0:  // entries - return [name, value]
        value = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, value, 0, JS_NewString(ctx, iterator->current->name));
        JS_SetPropertyUint32(ctx, value, 1, JS_NewString(ctx, iterator->current->value));
        break;
      case 1:  // keys - return name
        value = JS_NewString(ctx, iterator->current->name);
        break;
      case 2:  // values - return value
        value = JS_NewString(ctx, iterator->current->value);
        break;
      default:
        value = JS_UNDEFINED;
        break;
    }
    JS_SetPropertyStr(ctx, result, "value", value);

    // Advance to next parameter
    iterator->current = iterator->current->next;
  }

  return result;
}

static JSValue JSRT_URLSearchParamsCreateIterator(JSContext* ctx, JSValueConst this_val, int type) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  JSRT_URLSearchParamsIterator* iterator = malloc(sizeof(JSRT_URLSearchParamsIterator));
  if (!iterator) {
    return JS_EXCEPTION;
  }

  iterator->params = search_params;
  iterator->current = search_params->params;
  iterator->type = type;

  JSValue iterator_obj = JS_NewObjectClass(ctx, JSRT_URLSearchParamsIteratorClassID);
  if (JS_IsException(iterator_obj)) {
    free(iterator);
    return JS_EXCEPTION;
  }

  JS_SetOpaque(iterator_obj, iterator);

  // Add next method
  JS_SetPropertyStr(ctx, iterator_obj, "next", JS_NewCFunction(ctx, JSRT_URLSearchParamsIteratorNext, "next", 0));

  return iterator_obj;
}

static JSValue JSRT_URLSearchParamsEntries(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLSearchParamsCreateIterator(ctx, this_val, 0);
}

static JSValue JSRT_URLSearchParamsKeys(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLSearchParamsCreateIterator(ctx, this_val, 1);
}

static JSValue JSRT_URLSearchParamsValues(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLSearchParamsCreateIterator(ctx, this_val, 2);
}

static JSValue JSRT_URLSearchParamsSymbolIterator(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLSearchParamsEntries(ctx, this_val, argc, argv);
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
  JSValue set_search = JS_NewCFunction(ctx, JSRT_URLSetSearch, "set search", 1);
  JSValue get_hash = JS_NewCFunction(ctx, JSRT_URLGetHash, "get hash", 0);
  JSValue get_origin = JS_NewCFunction(ctx, JSRT_URLGetOrigin, "get origin", 0);
  JSValue get_search_params = JS_NewCFunction(ctx, JSRT_URLGetSearchParams, "get searchParams", 0);

  JSAtom href_atom = JS_NewAtom(ctx, "href");
  JSAtom protocol_atom = JS_NewAtom(ctx, "protocol");
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
  JS_SetPropertyStr(ctx, search_params_proto, "get", JS_NewCFunction(ctx, JSRT_URLSearchParamsGet, "get", 1));
  JS_SetPropertyStr(ctx, search_params_proto, "getAll", JS_NewCFunction(ctx, JSRT_URLSearchParamsGetAll, "getAll", 1));
  JS_SetPropertyStr(ctx, search_params_proto, "set", JS_NewCFunction(ctx, JSRT_URLSearchParamsSet, "set", 2));
  JS_SetPropertyStr(ctx, search_params_proto, "append", JS_NewCFunction(ctx, JSRT_URLSearchParamsAppend, "append", 2));
  JS_SetPropertyStr(ctx, search_params_proto, "has", JS_NewCFunction(ctx, JSRT_URLSearchParamsHas, "has", 2));
  JS_SetPropertyStr(ctx, search_params_proto, "delete", JS_NewCFunction(ctx, JSRT_URLSearchParamsDelete, "delete", 2));
  JS_SetPropertyStr(ctx, search_params_proto, "toString",
                    JS_NewCFunction(ctx, JSRT_URLSearchParamsToString, "toString", 0));

  // Add size property as getter
  JSValue get_size = JS_NewCFunction(ctx, JSRT_URLSearchParamsGetSize, "get size", 0);
  JSAtom size_atom = JS_NewAtom(ctx, "size");
  JS_DefinePropertyGetSet(ctx, search_params_proto, size_atom, get_size, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, size_atom);

  // Add iterator methods
  JS_SetPropertyStr(ctx, search_params_proto, "entries",
                    JS_NewCFunction(ctx, JSRT_URLSearchParamsEntries, "entries", 0));
  JS_SetPropertyStr(ctx, search_params_proto, "keys", JS_NewCFunction(ctx, JSRT_URLSearchParamsKeys, "keys", 0));
  JS_SetPropertyStr(ctx, search_params_proto, "values", JS_NewCFunction(ctx, JSRT_URLSearchParamsValues, "values", 0));

  // Add Symbol.iterator as entries method (per WHATWG spec)
  JSValue symbol_iterator = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Symbol");
  if (!JS_IsException(symbol_iterator)) {
    JSValue iterator_symbol = JS_GetPropertyStr(ctx, symbol_iterator, "iterator");
    if (!JS_IsException(iterator_symbol) && !JS_IsUndefined(iterator_symbol)) {
      JS_DefinePropertyValue(ctx, search_params_proto, JS_ValueToAtom(ctx, iterator_symbol),
                             JS_NewCFunction(ctx, JSRT_URLSearchParamsSymbolIterator, "[Symbol.iterator]", 0),
                             JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
      JS_FreeValue(ctx, iterator_symbol);
    }
    JS_FreeValue(ctx, symbol_iterator);
  }

  // Register iterator class
  JS_NewClassID(&JSRT_URLSearchParamsIteratorClassID);
  JS_NewClass(rt->rt, JSRT_URLSearchParamsIteratorClassID, &JSRT_URLSearchParamsIteratorClass);

  JS_SetClassProto(ctx, JSRT_URLSearchParamsClassID, search_params_proto);

  JSValue search_params_ctor =
      JS_NewCFunction2(ctx, JSRT_URLSearchParamsConstructor, "URLSearchParams", 1, JS_CFUNC_constructor, 0);

  // Set the constructor's prototype property
  JS_SetPropertyStr(ctx, search_params_ctor, "prototype", JS_DupValue(ctx, search_params_proto));

  // Set the prototype's constructor property
  JS_SetPropertyStr(ctx, search_params_proto, "constructor", JS_DupValue(ctx, search_params_ctor));

  JS_SetPropertyStr(ctx, rt->global, "URLSearchParams", search_params_ctor);

  JSRT_Debug("URL/URLSearchParams API setup completed");
}