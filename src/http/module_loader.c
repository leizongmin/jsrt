#include "module_loader.h"
#include "cache.h"
#include "security.h"
#include "../util/http_client.h"
#include "../util/debug.h"
#include "../../deps/quickjs/quickjs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global HTTP module cache
static JSRT_HttpCache* g_http_cache = NULL;

// Helper function to compile module from string
static JSModuleDef* compile_module_from_string(JSContext* ctx, const char* url, const char* source, size_t source_len) {
  JSValue func_val = JS_Eval(ctx, source, source_len, url, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
  
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
      if (cache_size == 0) cache_size = 100;
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
  if (relative_path[0] == '.' && (relative_path[1] == '/' || 
      (relative_path[1] == '.' && relative_path[2] == '/'))) {
    
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
  JSRT_HttpSecurityResult content_result = jsrt_http_validate_response_content(
    response.content_type, response.body_size);
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
  jsrt_http_cache_put(g_http_cache, url, response.body, response.body_size, 
                      response.etag, response.last_modified);

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
    
    // For CommonJS, wrap the cached content and evaluate it
    char* wrapped = wrap_as_commonjs_module(cached->data);
    if (!wrapped) {
      return JS_ThrowOutOfMemory(ctx);
    }
    
    JSValue result = JS_Eval(ctx, wrapped, strlen(wrapped), url, JS_EVAL_TYPE_MODULE);
    free(wrapped);
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
  JSRT_HttpSecurityResult content_result = jsrt_http_validate_response_content(
    response.content_type, response.body_size);
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
  jsrt_http_cache_put(g_http_cache, url, response.body, response.body_size, 
                      response.etag, response.last_modified);

  // For CommonJS, wrap the content and evaluate it
  char* wrapped = wrap_as_commonjs_module(response.body);
  if (!wrapped) {
    JSRT_HttpResponseFree(&response);
    return JS_ThrowOutOfMemory(ctx);
  }
  
  JSValue result = JS_Eval(ctx, wrapped, strlen(wrapped), url, JS_EVAL_TYPE_MODULE);
  
  free(wrapped);
  JSRT_HttpResponseFree(&response);

  if (JS_IsException(result)) {
    JSRT_Debug("jsrt_require_http_module: failed to evaluate module from '%s'", url);
    return result;
  }

  JSRT_Debug("jsrt_require_http_module: successfully loaded CommonJS module from '%s'", url);
  return result;
}