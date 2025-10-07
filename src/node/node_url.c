/*
 * Node.js url module implementation for jsrt
 * Provides both WHATWG URL API and Legacy URL API
 *
 * This module maximally reuses existing URL infrastructure from src/url/
 * which is 97% WPT compliant (29/32 tests passing)
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../url/url.h"
#include "../util/debug.h"
#include "node_modules.h"

#ifdef _WIN32
#include <windows.h>
#define PATH_MAX MAX_PATH
#else
#include <limits.h>
#include <unistd.h>
#endif

// Forward declarations
static JSValue js_url_parse(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_url_format(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_url_resolve(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_url_domain_to_ascii(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_url_domain_to_unicode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_url_file_to_path(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_url_path_to_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_url_to_http_options(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Helper function to parse query string into object (for parseQueryString option)
static JSValue parse_query_string_to_object(JSContext* ctx, const char* query) {
  if (!query || !*query) {
    return JS_NewObject(ctx);
  }

  JSValue result = JS_NewObject(ctx);

  // Skip leading '?' if present
  if (query[0] == '?') {
    query++;
  }

  char* query_copy = strdup(query);
  if (!query_copy) {
    JS_FreeValue(ctx, result);
    return JS_ThrowOutOfMemory(ctx);
  }

  char* pair = strtok(query_copy, "&");
  while (pair) {
    char* equals = strchr(pair, '=');
    if (equals) {
      *equals = '\0';
      const char* key = pair;
      const char* value = equals + 1;

      // Decode key and value
      char* decoded_key = url_decode(key);
      char* decoded_value = url_decode(value);

      if (decoded_key && decoded_value) {
        JS_SetPropertyStr(ctx, result, decoded_key, JS_NewString(ctx, decoded_value));
      }

      free(decoded_key);
      free(decoded_value);
    } else {
      // Key without value
      char* decoded_key = url_decode(pair);
      if (decoded_key) {
        JS_SetPropertyStr(ctx, result, decoded_key, JS_NewString(ctx, ""));
        free(decoded_key);
      }
    }
    pair = strtok(NULL, "&");
  }

  free(query_copy);
  return result;
}

// Helper to check if URL has special scheme
static bool has_special_scheme(const char* protocol) {
  return is_special_scheme(protocol);
}

// url.parse(urlString[, parseQueryString[, slashesDenoteHost]])
// Returns legacy Url object format
static JSValue js_url_parse(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewObject(ctx);
  }

  const char* url_str = JS_ToCString(ctx, argv[0]);
  if (!url_str) {
    return JS_NewObject(ctx);
  }

  bool parse_query = false;
  bool slashes_denote = false;

  if (argc >= 2) {
    parse_query = JS_ToBool(ctx, argv[1]);
  }

  if (argc >= 3) {
    slashes_denote = JS_ToBool(ctx, argv[2]);
  }

  // Parse URL using existing WHATWG parser
  JSRT_URL* parsed = JSRT_ParseURL(url_str, NULL);

  // Create legacy format object
  JSValue result = JS_NewObject(ctx);

  if (!parsed) {
    // Return partial object for invalid URLs
    JS_SetPropertyStr(ctx, result, "href", JS_NewString(ctx, url_str));
    JS_SetPropertyStr(ctx, result, "protocol", JS_NULL);
    JS_SetPropertyStr(ctx, result, "slashes", JS_FALSE);
    JS_FreeCString(ctx, url_str);
    return result;
  }

  // Set basic properties
  JS_SetPropertyStr(ctx, result, "href", JS_NewString(ctx, parsed->href));
  JS_SetPropertyStr(ctx, result, "protocol", JS_NewString(ctx, parsed->protocol));
  JS_SetPropertyStr(ctx, result, "hostname", JS_NewString(ctx, parsed->hostname ? parsed->hostname : ""));
  JS_SetPropertyStr(ctx, result, "port", JS_NewString(ctx, parsed->port ? parsed->port : ""));
  JS_SetPropertyStr(ctx, result, "pathname", JS_NewString(ctx, parsed->pathname ? parsed->pathname : ""));
  JS_SetPropertyStr(ctx, result, "hash", JS_NewString(ctx, parsed->hash ? parsed->hash : ""));

  // Compute auth (username:password)
  if ((parsed->username && strlen(parsed->username) > 0) || (parsed->password && strlen(parsed->password) > 0)) {
    size_t auth_len =
        (parsed->username ? strlen(parsed->username) : 0) + (parsed->password ? strlen(parsed->password) : 0) + 2;
    char* auth = malloc(auth_len);
    if (auth) {
      snprintf(auth, auth_len, "%s:%s", parsed->username ? parsed->username : "",
               parsed->password ? parsed->password : "");
      JS_SetPropertyStr(ctx, result, "auth", JS_NewString(ctx, auth));
      free(auth);
    }
  }

  // Compute slashes property
  bool has_slashes = has_special_scheme(parsed->protocol) || (parsed->href && strstr(parsed->href, "//") != NULL);
  JS_SetPropertyStr(ctx, result, "slashes", JS_NewBool(ctx, has_slashes));

  // Handle query string
  if (parsed->search && strlen(parsed->search) > 0) {
    const char* query_str = parsed->search[0] == '?' ? parsed->search + 1 : parsed->search;
    JS_SetPropertyStr(ctx, result, "search", JS_NewString(ctx, parsed->search));

    if (parse_query && strlen(query_str) > 0) {
      // Parse query string into object
      JSValue query_obj = parse_query_string_to_object(ctx, query_str);
      JS_SetPropertyStr(ctx, result, "query", query_obj);
    } else {
      JS_SetPropertyStr(ctx, result, "query", JS_NewString(ctx, query_str));
    }
  } else {
    JS_SetPropertyStr(ctx, result, "search", JS_NULL);
    JS_SetPropertyStr(ctx, result, "query", parse_query ? JS_NewObject(ctx) : JS_NULL);
  }

  // Compute path (pathname + search)
  size_t path_len =
      (parsed->pathname ? strlen(parsed->pathname) : 0) + (parsed->search ? strlen(parsed->search) : 0) + 1;
  char* path = malloc(path_len);
  if (path) {
    snprintf(path, path_len, "%s%s", parsed->pathname ? parsed->pathname : "", parsed->search ? parsed->search : "");
    JS_SetPropertyStr(ctx, result, "path", JS_NewString(ctx, path));
    free(path);
  }

  // Set host (hostname:port)
  if (parsed->hostname) {
    if (parsed->port && strlen(parsed->port) > 0) {
      size_t host_len = strlen(parsed->hostname) + strlen(parsed->port) + 2;
      char* host = malloc(host_len);
      if (host) {
        snprintf(host, host_len, "%s:%s", parsed->hostname, parsed->port);
        JS_SetPropertyStr(ctx, result, "host", JS_NewString(ctx, host));
        free(host);
      }
    } else {
      JS_SetPropertyStr(ctx, result, "host", JS_NewString(ctx, parsed->hostname));
    }
  } else {
    JS_SetPropertyStr(ctx, result, "host", JS_NULL);
  }

  // Cleanup
  JSRT_FreeURL(parsed);
  JS_FreeCString(ctx, url_str);

  return result;
}

// url.format(urlObject)
// Builds URL string from legacy Url object or WHATWG URL object
static JSValue js_url_format(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1 || !JS_IsObject(argv[0])) {
    return JS_NewString(ctx, "");
  }

  JSValue obj = argv[0];

  // Check if it's a WHATWG URL object
  JSRT_URL* url = JS_GetOpaque2(ctx, obj, JSRT_URLClassID);
  if (url) {
    // It's a URL instance - return href directly
    return JS_NewString(ctx, url->href);
  }

  // It's a legacy object - build from components

  // Try href first (highest priority)
  JSValue href_val = JS_GetPropertyStr(ctx, obj, "href");
  if (JS_IsString(href_val)) {
    return href_val;  // Return href directly
  }
  JS_FreeValue(ctx, href_val);

  // Build from components
  char result[4096] = "";

  // 1. Protocol
  JSValue protocol_val = JS_GetPropertyStr(ctx, obj, "protocol");
  if (JS_IsString(protocol_val)) {
    const char* protocol = JS_ToCString(ctx, protocol_val);
    if (protocol) {
      strcat(result, protocol);
      if (strlen(protocol) > 0 && protocol[strlen(protocol) - 1] != ':') {
        strcat(result, ":");
      }
      JS_FreeCString(ctx, protocol);
    }
  }
  JS_FreeValue(ctx, protocol_val);

  // 2. Slashes (default to true for most protocols)
  JSValue slashes_val = JS_GetPropertyStr(ctx, obj, "slashes");
  bool has_slashes = true;  // Default to true
  if (!JS_IsUndefined(slashes_val) && !JS_IsNull(slashes_val)) {
    has_slashes = JS_ToBool(ctx, slashes_val);
  }
  if (has_slashes && strlen(result) > 0) {
    strcat(result, "//");
  }
  JS_FreeValue(ctx, slashes_val);

  // 3. Auth (username:password)
  JSValue auth_val = JS_GetPropertyStr(ctx, obj, "auth");
  if (JS_IsString(auth_val)) {
    const char* auth = JS_ToCString(ctx, auth_val);
    if (auth && strlen(auth) > 0) {
      strcat(result, auth);
      strcat(result, "@");
      JS_FreeCString(ctx, auth);
    }
  }
  JS_FreeValue(ctx, auth_val);

  // 4. Host (or hostname + port)
  JSValue host_val = JS_GetPropertyStr(ctx, obj, "host");
  if (JS_IsString(host_val)) {
    const char* host = JS_ToCString(ctx, host_val);
    if (host) {
      strcat(result, host);
      JS_FreeCString(ctx, host);
    }
  } else {
    // Build from hostname + port
    JSValue hostname_val = JS_GetPropertyStr(ctx, obj, "hostname");
    if (JS_IsString(hostname_val)) {
      const char* hostname = JS_ToCString(ctx, hostname_val);
      if (hostname) {
        strcat(result, hostname);
        JS_FreeCString(ctx, hostname);

        JSValue port_val = JS_GetPropertyStr(ctx, obj, "port");
        if (JS_IsString(port_val)) {
          const char* port = JS_ToCString(ctx, port_val);
          if (port && strlen(port) > 0) {
            strcat(result, ":");
            strcat(result, port);
            JS_FreeCString(ctx, port);
          }
        }
        JS_FreeValue(ctx, port_val);
      }
    }
    JS_FreeValue(ctx, hostname_val);
  }
  JS_FreeValue(ctx, host_val);

  // 5. Path (or pathname + search)
  JSValue path_val = JS_GetPropertyStr(ctx, obj, "path");
  if (JS_IsString(path_val)) {
    const char* path = JS_ToCString(ctx, path_val);
    if (path) {
      strcat(result, path);
      JS_FreeCString(ctx, path);
    }
  } else {
    // Build from pathname + search
    JSValue pathname_val = JS_GetPropertyStr(ctx, obj, "pathname");
    if (JS_IsString(pathname_val)) {
      const char* pathname = JS_ToCString(ctx, pathname_val);
      if (pathname) {
        strcat(result, pathname);
        JS_FreeCString(ctx, pathname);
      }
    }
    JS_FreeValue(ctx, pathname_val);

    JSValue search_val = JS_GetPropertyStr(ctx, obj, "search");
    if (JS_IsString(search_val)) {
      const char* search = JS_ToCString(ctx, search_val);
      if (search) {
        strcat(result, search);
        JS_FreeCString(ctx, search);
      }
    }
    JS_FreeValue(ctx, search_val);
  }
  JS_FreeValue(ctx, path_val);

  // 6. Hash
  JSValue hash_val = JS_GetPropertyStr(ctx, obj, "hash");
  if (JS_IsString(hash_val)) {
    const char* hash = JS_ToCString(ctx, hash_val);
    if (hash) {
      strcat(result, hash);
      JS_FreeCString(ctx, hash);
    }
  }
  JS_FreeValue(ctx, hash_val);

  return JS_NewString(ctx, result);
}

// url.resolve(from, to)
// Resolves 'to' URL relative to 'from' URL
static JSValue js_url_resolve(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_NewString(ctx, "");
  }

  const char* from = JS_ToCString(ctx, argv[0]);
  const char* to = JS_ToCString(ctx, argv[1]);

  if (!from || !to) {
    if (from)
      JS_FreeCString(ctx, from);
    if (to)
      JS_FreeCString(ctx, to);
    return JS_NewString(ctx, "");
  }

  // Check if 'to' is an absolute URL (contains ://)
  if (strstr(to, "://")) {
    // Absolute URL - return as-is
    JSValue result = JS_NewString(ctx, to);
    JS_FreeCString(ctx, from);
    JS_FreeCString(ctx, to);
    return result;
  }

  // Use existing relative URL resolution
  JSRT_URL* resolved = resolve_relative_url(to, from);

  if (!resolved) {
    // Failed to resolve - return 'to' as-is
    JS_FreeCString(ctx, from);
    JSValue result = JS_DupValue(ctx, argv[1]);
    JS_FreeCString(ctx, to);
    return result;
  }

  // Extract href from resolved URL
  JSValue result = JS_NewString(ctx, resolved->href);

  // Cleanup
  JSRT_FreeURL(resolved);
  JS_FreeCString(ctx, from);
  JS_FreeCString(ctx, to);

  return result;
}

// url.domainToASCII(domain)
// Converts Unicode domain to ASCII (Punycode)
static JSValue js_url_domain_to_ascii(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewString(ctx, "");
  }

  const char* domain = JS_ToCString(ctx, argv[0]);
  if (!domain) {
    return JS_NewString(ctx, "");
  }

  // Use existing Punycode encoder
  char* ascii = hostname_to_ascii(domain);
  JS_FreeCString(ctx, domain);

  if (!ascii) {
    return JS_NewString(ctx, "");
  }

  JSValue result = JS_NewString(ctx, ascii);
  free(ascii);

  return result;
}

// url.domainToUnicode(domain)
// Converts ASCII/Punycode domain to Unicode
static JSValue js_url_domain_to_unicode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewString(ctx, "");
  }

  const char* domain = JS_ToCString(ctx, argv[0]);
  if (!domain) {
    return JS_NewString(ctx, "");
  }

  // Use existing Unicode normalizer (decodes Punycode)
  char* unicode = normalize_hostname_unicode(domain);
  JS_FreeCString(ctx, domain);

  if (!unicode) {
    return JS_NewString(ctx, "");
  }

  JSValue result = JS_NewString(ctx, unicode);
  free(unicode);

  return result;
}

// url.fileURLToPath(url)
// Converts file:// URL to platform-specific file path
static JSValue js_url_file_to_path(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "URL is required");
  }

  const char* url_str = NULL;
  JSRT_URL* url_obj = NULL;
  bool free_url_str = false;

  // Accept string or URL object
  if (JS_IsString(argv[0])) {
    url_str = JS_ToCString(ctx, argv[0]);
    free_url_str = true;
  } else {
    url_obj = JS_GetOpaque2(ctx, argv[0], JSRT_URLClassID);
    if (url_obj) {
      url_str = url_obj->href;
    }
  }

  if (!url_str) {
    return JS_ThrowTypeError(ctx, "Invalid URL");
  }

  // Parse URL if string
  JSRT_URL* parsed = url_obj ? url_obj : JSRT_ParseURL(url_str, NULL);
  if (!parsed) {
    if (free_url_str)
      JS_FreeCString(ctx, url_str);
    return JS_ThrowTypeError(ctx, "Invalid URL");
  }

  // Check protocol
  if (strcmp(parsed->protocol, "file:") != 0) {
    if (!url_obj)
      JSRT_FreeURL(parsed);
    if (free_url_str)
      JS_FreeCString(ctx, url_str);
    return JS_ThrowTypeError(ctx, "URL must be a file: URL");
  }

  // Decode pathname
  char* decoded_path = url_decode(parsed->pathname);
  if (!decoded_path) {
    if (!url_obj)
      JSRT_FreeURL(parsed);
    if (free_url_str)
      JS_FreeCString(ctx, url_str);
    return JS_ThrowOutOfMemory(ctx);
  }

  char* result = NULL;

#ifdef _WIN32
  // Windows-specific conversion

  // Handle UNC paths (//hostname/share)
  if (parsed->hostname && strlen(parsed->hostname) > 0) {
    // UNC path: \\hostname\share\path
    size_t len = strlen(parsed->hostname) + strlen(decoded_path) + 10;
    result = malloc(len);
    if (result) {
      snprintf(result, len, "\\\\%s%s", parsed->hostname, decoded_path);

      // Convert / to backslash
      for (char* p = result; *p; p++) {
        if (*p == '/')
          *p = '\\';
      }
    }
  } else {
    // Regular path: C:\path\to\file
    // Remove leading / from /C:/path
    const char* path = decoded_path;
    if (path[0] == '/' && strlen(path) >= 3 && path[2] == ':') {
      path++;  // Skip leading /
    }

    result = strdup(path);
    if (result) {
      // Convert / to backslash
      for (char* p = result; *p; p++) {
        if (*p == '/')
          *p = '\\';
      }
    }
  }
#else
  // Unix-specific conversion
  result = strdup(decoded_path);
#endif

  // Cleanup
  free(decoded_path);
  if (!url_obj)
    JSRT_FreeURL(parsed);
  if (free_url_str)
    JS_FreeCString(ctx, url_str);

  if (!result) {
    return JS_ThrowOutOfMemory(ctx);
  }

  JSValue js_result = JS_NewString(ctx, result);
  free(result);
  return js_result;
}

// url.pathToFileURL(path)
// Converts platform-specific file path to file:// URL object
static JSValue js_url_path_to_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1 || !JS_IsString(argv[0])) {
    return JS_ThrowTypeError(ctx, "Path must be a string");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_ThrowTypeError(ctx, "Path must be a string");
  }

  char url_str[4096] = "file://";

#ifdef _WIN32
  // Windows-specific conversion
  if (path[0] == '\\' && path[1] == '\\') {
    // UNC path: \\hostname\share\path -> file://hostname/share/path
    const char* p = path + 2;
    const char* hostname_end = strchr(p, '\\');
    if (hostname_end) {
      size_t hostname_len = hostname_end - p;
      strncat(url_str, p, hostname_len);
      p = hostname_end;
    }

    // Append path, converting backslash to /
    char* temp_path = strdup(p);
    if (temp_path) {
      for (char* c = temp_path; *c; c++) {
        if (*c == '\\')
          *c = '/';
      }
      // Encode the path
      char* encoded = url_path_encode_file(temp_path);
      if (encoded) {
        strcat(url_str, encoded);
        free(encoded);
      }
      free(temp_path);
    }
  } else {
    // Regular path: C:\path -> file:///C:/path
    strcat(url_str, "/");

    char* temp_path = strdup(path);
    if (temp_path) {
      for (char* c = temp_path; *c; c++) {
        if (*c == '\\')
          *c = '/';
      }
      // Encode the path
      char* encoded = url_path_encode_file(temp_path);
      if (encoded) {
        strcat(url_str, encoded);
        free(encoded);
      }
      free(temp_path);
    }
  }
#else
  // Unix-specific conversion
  strcat(url_str, "/");
  char* encoded = url_path_encode_file(path);
  if (encoded) {
    strcat(url_str, encoded);
    free(encoded);
  }
#endif

  JS_FreeCString(ctx, path);

  // Parse as URL and return URL object
  JSRT_URL* url = JSRT_ParseURL(url_str, NULL);
  if (!url) {
    return JS_ThrowTypeError(ctx, "Failed to create file URL");
  }

  url->ctx = ctx;

  // Create URL object
  JSValue url_obj = JS_NewObjectClass(ctx, JSRT_URLClassID);
  JS_SetOpaque(url_obj, url);

  return url_obj;
}

// url.urlToHttpOptions(url)
// Converts URL object to options object for http.request()
static JSValue js_url_to_http_options(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "URL is required");
  }

  // Extract URL object
  JSRT_URL* url = JS_GetOpaque2(ctx, argv[0], JSRT_URLClassID);
  if (!url) {
    return JS_ThrowTypeError(ctx, "Argument must be a URL object");
  }

  // Build options object
  JSValue options = JS_NewObject(ctx);

  // protocol
  JS_SetPropertyStr(ctx, options, "protocol", JS_NewString(ctx, url->protocol));

  // hostname
  JS_SetPropertyStr(ctx, options, "hostname", JS_NewString(ctx, url->hostname ? url->hostname : ""));

  // port (convert to number if present)
  if (url->port && strlen(url->port) > 0) {
    int port_num = atoi(url->port);
    JS_SetPropertyStr(ctx, options, "port", JS_NewInt32(ctx, port_num));
  }

  // path (pathname + search)
  char path[4096];
  snprintf(path, sizeof(path), "%s%s", url->pathname ? url->pathname : "/", url->search ? url->search : "");
  JS_SetPropertyStr(ctx, options, "path", JS_NewString(ctx, path));

  // hash
  if (url->hash && strlen(url->hash) > 0) {
    JS_SetPropertyStr(ctx, options, "hash", JS_NewString(ctx, url->hash));
  }

  // auth (username:password)
  if (url->username || url->password) {
    char auth[512];
    snprintf(auth, sizeof(auth), "%s:%s", url->username ? url->username : "", url->password ? url->password : "");
    JS_SetPropertyStr(ctx, options, "auth", JS_NewString(ctx, auth));
  }

  return options;
}

// Module function list
static const JSCFunctionListEntry js_url_funcs[] = {
    JS_CFUNC_DEF("parse", 3, js_url_parse),
    JS_CFUNC_DEF("format", 1, js_url_format),
    JS_CFUNC_DEF("resolve", 2, js_url_resolve),
    JS_CFUNC_DEF("domainToASCII", 1, js_url_domain_to_ascii),
    JS_CFUNC_DEF("domainToUnicode", 1, js_url_domain_to_unicode),
    JS_CFUNC_DEF("fileURLToPath", 1, js_url_file_to_path),
    JS_CFUNC_DEF("pathToFileURL", 1, js_url_path_to_file),
    JS_CFUNC_DEF("urlToHttpOptions", 1, js_url_to_http_options),
};

// ES Module initialization
int js_node_url_init(JSContext* ctx, JSModuleDef* m) {
  // Export URL class (get from global object)
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue url_class = JS_GetPropertyStr(ctx, global, "URL");
  JS_SetModuleExport(ctx, m, "URL", url_class);

  // Export URLSearchParams class (get from global object)
  JSValue search_params_class = JS_GetPropertyStr(ctx, global, "URLSearchParams");
  JS_SetModuleExport(ctx, m, "URLSearchParams", search_params_class);
  JS_FreeValue(ctx, global);

  // Export legacy API functions
  size_t func_count = sizeof(js_url_funcs) / sizeof(js_url_funcs[0]);
  JS_SetModuleExportList(ctx, m, js_url_funcs, func_count);

  // Create default export object with all APIs
  JSValue default_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, default_obj, "URL", JS_DupValue(ctx, url_class));
  JS_SetPropertyStr(ctx, default_obj, "URLSearchParams", JS_DupValue(ctx, search_params_class));

  for (size_t i = 0; i < func_count; i++) {
    JSValue func =
        JS_NewCFunction(ctx, js_url_funcs[i].u.func.cfunc.generic, js_url_funcs[i].name, js_url_funcs[i].u.func.length);
    JS_SetPropertyStr(ctx, default_obj, js_url_funcs[i].name, func);
  }

  JS_SetModuleExport(ctx, m, "default", default_obj);

  return 0;
}

// CommonJS module initialization
JSValue JSRT_InitNodeUrl(JSContext* ctx) {
  JSValue module = JS_NewObject(ctx);

  // Export URL class (get from global object)
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue url_class = JS_GetPropertyStr(ctx, global, "URL");
  JS_SetPropertyStr(ctx, module, "URL", url_class);

  // Export URLSearchParams class (get from global object)
  JSValue search_params_class = JS_GetPropertyStr(ctx, global, "URLSearchParams");
  JS_SetPropertyStr(ctx, module, "URLSearchParams", search_params_class);
  JS_FreeValue(ctx, global);

  // Export legacy API functions
  size_t func_count = sizeof(js_url_funcs) / sizeof(js_url_funcs[0]);
  for (size_t i = 0; i < func_count; i++) {
    JSValue func =
        JS_NewCFunction(ctx, js_url_funcs[i].u.func.cfunc.generic, js_url_funcs[i].name, js_url_funcs[i].u.func.length);
    JS_SetPropertyStr(ctx, module, js_url_funcs[i].name, func);
  }

  return module;
}

// Module definition for ES modules
JSModuleDef* js_init_module_node_url(JSContext* ctx, const char* module_name) {
  JSModuleDef* m = JS_NewCModule(ctx, module_name, js_node_url_init);
  if (!m)
    return NULL;

  // Add module exports
  JS_AddModuleExport(ctx, m, "URL");
  JS_AddModuleExport(ctx, m, "URLSearchParams");
  JS_AddModuleExport(ctx, m, "parse");
  JS_AddModuleExport(ctx, m, "format");
  JS_AddModuleExport(ctx, m, "resolve");
  JS_AddModuleExport(ctx, m, "domainToASCII");
  JS_AddModuleExport(ctx, m, "domainToUnicode");
  JS_AddModuleExport(ctx, m, "fileURLToPath");
  JS_AddModuleExport(ctx, m, "pathToFileURL");
  JS_AddModuleExport(ctx, m, "urlToHttpOptions");
  JS_AddModuleExport(ctx, m, "default");

  return m;
}
