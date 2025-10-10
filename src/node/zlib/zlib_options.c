#include <stdlib.h>
#include <string.h>
#include "../../util/macro.h"
#include "zlib_internal.h"

// Initialize default options
void zlib_options_init_defaults(ZlibOptions* opts) {
  opts->level = Z_DEFAULT_COMPRESSION;
  opts->windowBits = 15;  // Default window size
  opts->memLevel = 8;     // Default memory level
  opts->strategy = Z_DEFAULT_STRATEGY;
  opts->chunkSize = 16 * 1024;  // 16KB default chunk size
  opts->flush = Z_NO_FLUSH;
  opts->finishFlush = Z_FINISH;
  opts->has_dictionary = false;
  opts->dictionary = NULL;
  opts->dictionary_len = 0;
}

// Parse options from JSValue
int zlib_parse_options(JSContext* ctx, JSValue opts_val, ZlibOptions* opts) {
  // Initialize with defaults first
  zlib_options_init_defaults(opts);

  // If no options provided, use defaults
  if (JS_IsUndefined(opts_val) || JS_IsNull(opts_val)) {
    return 0;
  }

  // Options must be an object
  if (!JS_IsObject(opts_val)) {
    JS_ThrowTypeError(ctx, "options must be an object");
    return -1;
  }

  JSValue val;

  // Parse level (0-9, or -1 for default)
  val = JS_GetPropertyStr(ctx, opts_val, "level");
  if (!JS_IsUndefined(val) && !JS_IsNull(val)) {
    int32_t level;
    if (JS_ToInt32(ctx, &level, val) < 0) {
      JS_FreeValue(ctx, val);
      return -1;
    }
    if (level < -1 || level > 9) {
      JS_FreeValue(ctx, val);
      JS_ThrowRangeError(ctx, "level must be between -1 and 9");
      return -1;
    }
    opts->level = level;
  }
  JS_FreeValue(ctx, val);

  // Parse windowBits (8-15)
  val = JS_GetPropertyStr(ctx, opts_val, "windowBits");
  if (!JS_IsUndefined(val) && !JS_IsNull(val)) {
    int32_t windowBits;
    if (JS_ToInt32(ctx, &windowBits, val) < 0) {
      JS_FreeValue(ctx, val);
      return -1;
    }
    // Allow negative for raw deflate, and +16 for gzip
    if (abs(windowBits) < 8 || abs(windowBits) > 15 + 16) {
      JS_FreeValue(ctx, val);
      JS_ThrowRangeError(ctx, "windowBits must be between 8 and 15");
      return -1;
    }
    opts->windowBits = windowBits;
  }
  JS_FreeValue(ctx, val);

  // Parse memLevel (1-9)
  val = JS_GetPropertyStr(ctx, opts_val, "memLevel");
  if (!JS_IsUndefined(val) && !JS_IsNull(val)) {
    int32_t memLevel;
    if (JS_ToInt32(ctx, &memLevel, val) < 0) {
      JS_FreeValue(ctx, val);
      return -1;
    }
    if (memLevel < 1 || memLevel > 9) {
      JS_FreeValue(ctx, val);
      JS_ThrowRangeError(ctx, "memLevel must be between 1 and 9");
      return -1;
    }
    opts->memLevel = memLevel;
  }
  JS_FreeValue(ctx, val);

  // Parse strategy
  val = JS_GetPropertyStr(ctx, opts_val, "strategy");
  if (!JS_IsUndefined(val) && !JS_IsNull(val)) {
    int32_t strategy;
    if (JS_ToInt32(ctx, &strategy, val) < 0) {
      JS_FreeValue(ctx, val);
      return -1;
    }
    opts->strategy = strategy;
  }
  JS_FreeValue(ctx, val);

  // Parse chunkSize
  val = JS_GetPropertyStr(ctx, opts_val, "chunkSize");
  if (!JS_IsUndefined(val) && !JS_IsNull(val)) {
    int64_t chunkSize;
    if (JS_ToInt64(ctx, &chunkSize, val) < 0) {
      JS_FreeValue(ctx, val);
      return -1;
    }
    if (chunkSize <= 0) {
      JS_FreeValue(ctx, val);
      JS_ThrowRangeError(ctx, "chunkSize must be positive");
      return -1;
    }
    opts->chunkSize = (size_t)chunkSize;
  }
  JS_FreeValue(ctx, val);

  return 0;
}

// Clean up options
void zlib_options_cleanup(ZlibOptions* opts) {
  if (opts->dictionary) {
    free(opts->dictionary);
    opts->dictionary = NULL;
  }
  opts->dictionary_len = 0;
  opts->has_dictionary = false;
}
