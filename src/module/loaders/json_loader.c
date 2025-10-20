/**
 * JSON Module Loader Implementation
 */

#include "json_loader.h"

#include <stdlib.h>
#include <string.h>

#include "../../util/file.h"
#include "../core/module_cache.h"
#include "../protocols/protocol_dispatcher.h"
#include "../util/module_debug.h"
#include "../util/module_errors.h"

JSValue jsrt_load_json_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* resolved_path,
                              const char* specifier) {
  if (!ctx || !loader || !resolved_path) {
    MODULE_DEBUG_ERROR("Invalid arguments to jsrt_load_json_module");
    return JS_EXCEPTION;
  }

  MODULE_DEBUG_LOADER("Loading JSON module: %s", resolved_path);

  // Load file content via protocol dispatcher
  JSRT_ReadFileResult file_result = jsrt_load_content_by_protocol(resolved_path);
  if (file_result.error != JSRT_READ_FILE_OK) {
    MODULE_DEBUG_ERROR("Failed to load JSON file: %s", resolved_path);
    return jsrt_module_throw_error(ctx, JSRT_MODULE_NOT_FOUND, "Cannot load JSON file '%s': %s", resolved_path,
                                   JSRT_ReadFileErrorToString(file_result.error));
  }

  MODULE_DEBUG_LOADER("JSON file loaded, size: %zu bytes", file_result.size);

  // Parse JSON content
  JSValue json_obj = JS_ParseJSON(ctx, file_result.data, file_result.size, resolved_path);
  JSRT_ReadFileResultFree(&file_result);

  if (JS_IsException(json_obj)) {
    MODULE_DEBUG_ERROR("Failed to parse JSON file: %s", resolved_path);
    // Get the exception for better error message
    JSValue exception = JS_GetException(ctx);
    JS_FreeValue(ctx, json_obj);
    return exception;
  }

  MODULE_DEBUG_LOADER("JSON parsed successfully");

  // Cache the result
  if (loader->enable_cache && loader->cache) {
    jsrt_module_cache_put(loader->cache, resolved_path, json_obj);
    MODULE_DEBUG_LOADER("Cached JSON module: %s", resolved_path);
  }

  return json_obj;
}
