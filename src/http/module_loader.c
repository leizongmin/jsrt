#include "module_loader.h"
#include "../../deps/quickjs/quickjs.h"
#include "../util/debug.h"
#include "../util/http_client.h"
#include "cache.h"
#include "security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global HTTP module cache
static JSRT_HttpCache* g_http_cache = NULL;

// Helper function to clean HTTP response content for JavaScript parsing
static char* clean_js_content(const char* source, size_t source_len, size_t* cleaned_len) {
  if (!source || source_len == 0) {
    *cleaned_len = 0;
    return NULL;
  }

  const char* start = source;
  size_t len = source_len;

  // Skip UTF-8 BOM if present (0xEF 0xBB 0xBF)
  if (len >= 3 && (unsigned char)start[0] == 0xEF && (unsigned char)start[1] == 0xBB &&
      (unsigned char)start[2] == 0xBF) {
    start += 3;
    len -= 3;
  }

  // Allocate cleaned content
  char* cleaned = malloc(len + 1);
  if (!cleaned) {
    *cleaned_len = 0;
    return NULL;
  }

  // Copy content, normalizing line endings and removing potential problematic characters
  size_t write_pos = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)start[i];

    // Skip null bytes and other problematic control characters
    if (c == 0 || (c < 32 && c != '\t' && c != '\n' && c != '\r')) {
      continue;
    }

    // Normalize Windows line endings to Unix
    if (c == '\r' && i + 1 < len && start[i + 1] == '\n') {
      cleaned[write_pos++] = '\n';
      i++;  // skip the \n
    } else if (c == '\r') {
      cleaned[write_pos++] = '\n';
    } else if (c >= 32 || c == '\t' || c == '\n') {
      // Only allow printable characters, tabs, and newlines
      cleaned[write_pos++] = c;
    }
  }

  cleaned[write_pos] = '\0';
  *cleaned_len = write_pos;
  return cleaned;
}

// Helper function to compile module from string
static JSModuleDef* compile_module_from_string(JSContext* ctx, const char* url, const char* source, size_t source_len) {
  // Clean the source content first
  size_t cleaned_len;
  char* cleaned_source = clean_js_content(source, source_len, &cleaned_len);

  if (!cleaned_source) {
    JSRT_Debug("jsrt_load_http_module: failed to clean source from %s", url);
    return NULL;
  }

  JSValue func_val = JS_Eval(ctx, cleaned_source, cleaned_len, url, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

  free(cleaned_source);

  if (JS_IsException(func_val)) {
    JSRT_Debug("jsrt_load_http_module: failed to compile module from %s", url);
    return NULL;
  }

  JSModuleDef* module = JS_VALUE_GET_PTR(func_val);
  JS_FreeValue(ctx, func_val);

  return module;
}

// Helper function to wrap CommonJS content as ES module
static char* wrap_as_commonjs_module(const char* source) {
  size_t source_len = strlen(source);
  size_t wrapper_len = source_len + 512;  // Extra space for wrapper
  char* wrapped = malloc(wrapper_len);

  if (!wrapped) {
    return NULL;
  }

  snprintf(wrapped, wrapper_len,
           "const module = { exports: {} };\n"
           "const exports = module.exports;\n"
           "const require = globalThis.require;\n"
           "\n%s\n"
           "export default module.exports;\n",
           source);

  return wrapped;
}

void jsrt_http_module_init(void) {
  if (!g_http_cache) {
    // Get cache size from environment or use default
    size_t cache_size = 100;  // Default
    const char* cache_size_env = getenv("JSRT_HTTP_MODULES_CACHE_SIZE");
    if (cache_size_env) {
      cache_size = (size_t)atol(cache_size_env);
      if (cache_size == 0)
        cache_size = 100;
    }

    g_http_cache = jsrt_http_cache_create(cache_size);
  }
}

void jsrt_http_module_cleanup(void) {
  if (g_http_cache) {
    jsrt_http_cache_free(g_http_cache);
    g_http_cache = NULL;
  }
}

char* jsrt_resolve_http_relative_import(const char* base_url, const char* relative_path) {
  if (!base_url || !relative_path) {
    return NULL;
  }

  // Handle absolute URLs
  if (jsrt_is_http_url(relative_path)) {
    return strdup(relative_path);
  }

  // Handle relative paths
  if (relative_path[0] == '.' && (relative_path[1] == '/' || (relative_path[1] == '.' && relative_path[2] == '/'))) {
    // Find the last '/' in base_url to get directory
    const char* last_slash = strrchr(base_url, '/');
    if (!last_slash) {
      return NULL;
    }

    size_t base_dir_len = last_slash - base_url + 1;
    size_t relative_len = strlen(relative_path);
    size_t result_len = base_dir_len + relative_len + 1;

    char* result = malloc(result_len);
    if (!result) {
      return NULL;
    }

    // Copy base directory
    strncpy(result, base_url, base_dir_len);
    result[base_dir_len] = '\0';

    // Append relative path, skipping "./"
    const char* rel_start = relative_path;
    if (rel_start[0] == '.' && rel_start[1] == '/') {
      rel_start += 2;
    }

    strcat(result, rel_start);

    // TODO: Handle "../" properly for going up directories
    // For now, just return the basic concatenation

    return result;
  }

  // If not a relative path, return as-is
  return strdup(relative_path);
}

JSModuleDef* jsrt_load_http_module(JSContext* ctx, const char* url) {
  if (!url) {
    JS_ThrowReferenceError(ctx, "Invalid URL provided");
    return NULL;
  }

  JSRT_Debug("jsrt_load_http_module: loading ES module from '%s'", url);

  // Validate URL security
  JSRT_HttpSecurityResult security_result = jsrt_http_validate_url(url);
  if (security_result != JSRT_HTTP_SECURITY_OK) {
    const char* error_msg = "Security validation failed";
    switch (security_result) {
      case JSRT_HTTP_SECURITY_PROTOCOL_FORBIDDEN:
        error_msg = "HTTP module loading is disabled or protocol not allowed";
        break;
      case JSRT_HTTP_SECURITY_DOMAIN_NOT_ALLOWED:
        error_msg = "Domain not in allowlist";
        break;
      case JSRT_HTTP_SECURITY_INVALID_URL:
        error_msg = "Invalid URL format";
        break;
      default:
        break;
    }
    JS_ThrowReferenceError(ctx, "Failed to load module from %s: %s", url, error_msg);
    return NULL;
  }

  // Initialize cache if needed
  jsrt_http_module_init();

  // Check cache first
  JSRT_HttpCacheEntry* cached = jsrt_http_cache_get(g_http_cache, url);
  if (cached && !jsrt_http_cache_is_expired(cached)) {
    JSRT_Debug("jsrt_load_http_module: loading from cache for '%s'", url);
    return compile_module_from_string(ctx, url, cached->data, cached->size);
  }

  // Download module
  JSRT_Debug("jsrt_load_http_module: downloading from '%s'", url);
  JSRT_HttpResponse response = JSRT_HttpGetWithOptions(url, "jsrt/1.0", 30000);

  if (response.error != JSRT_HTTP_OK || response.status != 200) {
    JSRT_HttpResponseFree(&response);
    JS_ThrowReferenceError(ctx, "Failed to load module from %s: HTTP %d", url, response.status);
    return NULL;
  }

  // Validate response content
  JSRT_HttpSecurityResult content_result =
      jsrt_http_validate_response_content(response.content_type, response.body_size);
  if (content_result != JSRT_HTTP_SECURITY_OK) {
    JSRT_HttpResponseFree(&response);
    const char* error_msg = "Content validation failed";
    if (content_result == JSRT_HTTP_SECURITY_SIZE_TOO_LARGE) {
      error_msg = "Module too large";
    } else if (content_result == JSRT_HTTP_SECURITY_CONTENT_TYPE_INVALID) {
      error_msg = "Invalid content type";
    }
    JS_ThrowReferenceError(ctx, "Failed to load module from %s: %s", url, error_msg);
    return NULL;
  }

  // Cache the response
  jsrt_http_cache_put(g_http_cache, url, response.body, response.body_size, response.etag, response.last_modified);

  // Compile module
  JSModuleDef* module = compile_module_from_string(ctx, url, response.body, response.body_size);

  JSRT_HttpResponseFree(&response);

  if (!module) {
    JS_ThrowSyntaxError(ctx, "Failed to compile module from %s", url);
    return NULL;
  }

  JSRT_Debug("jsrt_load_http_module: successfully loaded ES module from '%s'", url);
  return module;
}

JSValue jsrt_require_http_module(JSContext* ctx, const char* url) {
  if (!url) {
    return JS_ThrowTypeError(ctx, "Invalid URL provided");
  }

  JSRT_Debug("jsrt_require_http_module: loading CommonJS module from '%s'", url);

  // Validate URL security
  JSRT_HttpSecurityResult security_result = jsrt_http_validate_url(url);
  if (security_result != JSRT_HTTP_SECURITY_OK) {
    const char* error_msg = "Security validation failed";
    switch (security_result) {
      case JSRT_HTTP_SECURITY_PROTOCOL_FORBIDDEN:
        error_msg = "HTTP module loading is disabled or protocol not allowed";
        break;
      case JSRT_HTTP_SECURITY_DOMAIN_NOT_ALLOWED:
        error_msg = "Domain not in allowlist";
        break;
      case JSRT_HTTP_SECURITY_INVALID_URL:
        error_msg = "Invalid URL format";
        break;
      default:
        break;
    }
    return JS_ThrowReferenceError(ctx, "Failed to require module from %s: %s", url, error_msg);
  }

  // Initialize cache if needed
  jsrt_http_module_init();

  // Check cache first
  JSRT_HttpCacheEntry* cached = jsrt_http_cache_get(g_http_cache, url);
  if (cached && !jsrt_http_cache_is_expired(cached)) {
    JSRT_Debug("jsrt_require_http_module: loading from cache for '%s'", url);

    // For CommonJS, execute the cached code directly and return module.exports
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue module_obj = JS_NewObject(ctx);
    JSValue exports_obj = JS_NewObject(ctx);

    // Set up the CommonJS environment
    JS_SetPropertyStr(ctx, module_obj, "exports", JS_DupValue(ctx, exports_obj));
    JS_SetPropertyStr(ctx, global, "module", JS_DupValue(ctx, module_obj));
    JS_SetPropertyStr(ctx, global, "exports", JS_DupValue(ctx, exports_obj));

    // Clean and execute the cached CommonJS code
    size_t cleaned_len;
    char* cleaned_source = clean_js_content(cached->data, cached->size, &cleaned_len);

    if (!cleaned_source) {
      JSRT_Debug("jsrt_require_http_module: failed to clean cached source from '%s'", url);
      JS_FreeValue(ctx, global);
      JS_FreeValue(ctx, module_obj);
      JS_FreeValue(ctx, exports_obj);
      return JS_ThrowInternalError(ctx, "Failed to clean cached module content");
    }

    JSValue eval_result = JS_Eval(ctx, cleaned_source, cleaned_len, url, JS_EVAL_TYPE_GLOBAL);
    free(cleaned_source);

    if (JS_IsException(eval_result)) {
      JSRT_Debug("jsrt_require_http_module: failed to evaluate cached module from '%s'", url);
      JS_FreeValue(ctx, global);
      JS_FreeValue(ctx, module_obj);
      JS_FreeValue(ctx, exports_obj);
      return eval_result;
    }

    // Get the final exports
    JSValue result = JS_GetPropertyStr(ctx, module_obj, "exports");

    // Clean up
    JS_FreeValue(ctx, eval_result);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, module_obj);
    JS_FreeValue(ctx, exports_obj);

    JSRT_Debug("jsrt_require_http_module: successfully loaded cached CommonJS module from '%s'", url);
    return result;
  }

  // Download module
  JSRT_Debug("jsrt_require_http_module: downloading from '%s'", url);
  JSRT_HttpResponse response = JSRT_HttpGetWithOptions(url, "jsrt/1.0", 30000);

  if (response.error != JSRT_HTTP_OK || response.status != 200) {
    JSRT_HttpResponseFree(&response);
    return JS_ThrowReferenceError(ctx, "Failed to require module from %s: HTTP %d", url, response.status);
  }

  // Validate response content
  JSRT_HttpSecurityResult content_result =
      jsrt_http_validate_response_content(response.content_type, response.body_size);
  if (content_result != JSRT_HTTP_SECURITY_OK) {
    JSRT_HttpResponseFree(&response);
    const char* error_msg = "Content validation failed";
    if (content_result == JSRT_HTTP_SECURITY_SIZE_TOO_LARGE) {
      error_msg = "Module too large";
    } else if (content_result == JSRT_HTTP_SECURITY_CONTENT_TYPE_INVALID) {
      error_msg = "Invalid content type";
    }
    return JS_ThrowReferenceError(ctx, "Failed to require module from %s: %s", url, error_msg);
  }

  // Cache the response
  jsrt_http_cache_put(g_http_cache, url, response.body, response.body_size, response.etag, response.last_modified);

  // For CommonJS, execute the code directly and return module.exports
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue module_obj = JS_NewObject(ctx);
  JSValue exports_obj = JS_NewObject(ctx);

  // Set up the CommonJS environment
  JS_SetPropertyStr(ctx, module_obj, "exports", JS_DupValue(ctx, exports_obj));
  JS_SetPropertyStr(ctx, global, "module", JS_DupValue(ctx, module_obj));
  JS_SetPropertyStr(ctx, global, "exports", JS_DupValue(ctx, exports_obj));

  // Clean and execute the CommonJS code
  size_t cleaned_len;
  char* cleaned_source = clean_js_content(response.body, response.body_size, &cleaned_len);

  JSRT_HttpResponseFree(&response);

  if (!cleaned_source) {
    JSRT_Debug("jsrt_require_http_module: failed to clean source from '%s'", url);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, module_obj);
    JS_FreeValue(ctx, exports_obj);
    return JS_ThrowInternalError(ctx, "Failed to clean module content");
  }

  JSValue eval_result = JS_Eval(ctx, cleaned_source, cleaned_len, url, JS_EVAL_TYPE_GLOBAL);
  free(cleaned_source);

  if (JS_IsException(eval_result)) {
    JSRT_Debug("jsrt_require_http_module: failed to evaluate module from '%s'", url);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, module_obj);
    JS_FreeValue(ctx, exports_obj);
    return eval_result;
  }

  // Get the final exports
  JSValue result = JS_GetPropertyStr(ctx, module_obj, "exports");

  // Clean up
  JS_FreeValue(ctx, eval_result);
  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, module_obj);
  JS_FreeValue(ctx, exports_obj);

  JSRT_Debug("jsrt_require_http_module: successfully loaded CommonJS module from '%s'", url);
  return result;
}