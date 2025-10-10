#include <string.h>
#include "../../util/macro.h"
#include "../node_modules.h"
#include "zlib_internal.h"

// Helper to get buffer data from JSValue
static int get_buffer_data(JSContext* ctx, JSValueConst val, const uint8_t** data, size_t* len) {
  size_t size;
  uint8_t* buf = JS_GetArrayBuffer(ctx, &size, val);

  if (buf) {
    *data = buf;
    *len = size;
    return 0;
  }

  // Try to get as typed array
  JSValue buffer = JS_GetTypedArrayBuffer(ctx, val, NULL, NULL, NULL);
  if (!JS_IsException(buffer)) {
    buf = JS_GetArrayBuffer(ctx, &size, buffer);
    JS_FreeValue(ctx, buffer);
    if (buf) {
      *data = buf;
      *len = size;
      return 0;
    }
  }

  JS_ThrowTypeError(ctx, "argument must be a Buffer or Uint8Array");
  return -1;
}

// gzipSync implementation
static JSValue js_zlib_gzip_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "gzipSync requires at least 1 argument");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  ZlibOptions opts;
  if (argc > 1 && zlib_parse_options(ctx, argv[1], &opts) < 0) {
    return JS_EXCEPTION;
  } else if (argc <= 1) {
    zlib_options_init_defaults(&opts);
  }

  JSValue result = zlib_deflate_sync(ctx, input, input_len, &opts, ZLIB_FORMAT_GZIP);

  if (argc > 1) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// gunzipSync implementation
static JSValue js_zlib_gunzip_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "gunzipSync requires at least 1 argument");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  ZlibOptions opts;
  if (argc > 1 && zlib_parse_options(ctx, argv[1], &opts) < 0) {
    return JS_EXCEPTION;
  } else if (argc <= 1) {
    zlib_options_init_defaults(&opts);
  }

  JSValue result = zlib_inflate_sync(ctx, input, input_len, &opts, ZLIB_FORMAT_GZIP);

  if (argc > 1) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// deflateSync implementation
static JSValue js_zlib_deflate_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "deflateSync requires at least 1 argument");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  ZlibOptions opts;
  if (argc > 1 && zlib_parse_options(ctx, argv[1], &opts) < 0) {
    return JS_EXCEPTION;
  } else if (argc <= 1) {
    zlib_options_init_defaults(&opts);
  }

  JSValue result = zlib_deflate_sync(ctx, input, input_len, &opts, ZLIB_FORMAT_DEFLATE);

  if (argc > 1) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// inflateSync implementation
static JSValue js_zlib_inflate_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "inflateSync requires at least 1 argument");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  ZlibOptions opts;
  if (argc > 1 && zlib_parse_options(ctx, argv[1], &opts) < 0) {
    return JS_EXCEPTION;
  } else if (argc <= 1) {
    zlib_options_init_defaults(&opts);
  }

  JSValue result = zlib_inflate_sync(ctx, input, input_len, &opts, ZLIB_FORMAT_DEFLATE);

  if (argc > 1) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// deflateRawSync implementation
static JSValue js_zlib_deflate_raw_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "deflateRawSync requires at least 1 argument");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  ZlibOptions opts;
  if (argc > 1 && zlib_parse_options(ctx, argv[1], &opts) < 0) {
    return JS_EXCEPTION;
  } else if (argc <= 1) {
    zlib_options_init_defaults(&opts);
  }

  JSValue result = zlib_deflate_sync(ctx, input, input_len, &opts, ZLIB_FORMAT_RAW);

  if (argc > 1) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// inflateRawSync implementation
static JSValue js_zlib_inflate_raw_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "inflateRawSync requires at least 1 argument");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  ZlibOptions opts;
  if (argc > 1 && zlib_parse_options(ctx, argv[1], &opts) < 0) {
    return JS_EXCEPTION;
  } else if (argc <= 1) {
    zlib_options_init_defaults(&opts);
  }

  JSValue result = zlib_inflate_sync(ctx, input, input_len, &opts, ZLIB_FORMAT_RAW);

  if (argc > 1) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// unzipSync implementation (auto-detect gzip or deflate)
static JSValue js_zlib_unzip_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "unzipSync requires at least 1 argument");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  ZlibOptions opts;
  if (argc > 1 && zlib_parse_options(ctx, argv[1], &opts) < 0) {
    return JS_EXCEPTION;
  } else if (argc <= 1) {
    zlib_options_init_defaults(&opts);
  }

  // Use windowBits + 32 to auto-detect gzip or deflate format
  // This is a special zlib feature
  opts.windowBits = 15 + 32;  // Enable auto-detection

  JSValue result = zlib_inflate_sync(ctx, input, input_len, &opts, ZLIB_FORMAT_DEFLATE);

  if (argc > 1) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// gzip async implementation
static JSValue js_zlib_gzip(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "gzip requires at least 2 arguments");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  // Check if second arg is options or callback
  JSValue callback;
  ZlibOptions opts;
  if (argc == 2) {
    // buffer, callback
    callback = argv[1];
    zlib_options_init_defaults(&opts);
  } else {
    // buffer, options, callback
    if (zlib_parse_options(ctx, argv[1], &opts) < 0) {
      return JS_EXCEPTION;
    }
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    if (argc > 2) {
      zlib_options_cleanup(&opts);
    }
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  JSValue result = zlib_async_deflate(ctx, input, input_len, &opts, ZLIB_FORMAT_GZIP, callback);

  if (argc > 2) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// gunzip async implementation
static JSValue js_zlib_gunzip(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "gunzip requires at least 2 arguments");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  // Check if second arg is options or callback
  JSValue callback;
  ZlibOptions opts;
  if (argc == 2) {
    // buffer, callback
    callback = argv[1];
    zlib_options_init_defaults(&opts);
  } else {
    // buffer, options, callback
    if (zlib_parse_options(ctx, argv[1], &opts) < 0) {
      return JS_EXCEPTION;
    }
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    if (argc > 2) {
      zlib_options_cleanup(&opts);
    }
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  JSValue result = zlib_async_inflate(ctx, input, input_len, &opts, ZLIB_FORMAT_GZIP, callback);

  if (argc > 2) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// deflate async implementation
static JSValue js_zlib_deflate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "deflate requires at least 2 arguments");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  // Check if second arg is options or callback
  JSValue callback;
  ZlibOptions opts;
  if (argc == 2) {
    callback = argv[1];
    zlib_options_init_defaults(&opts);
  } else {
    if (zlib_parse_options(ctx, argv[1], &opts) < 0) {
      return JS_EXCEPTION;
    }
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    if (argc > 2) {
      zlib_options_cleanup(&opts);
    }
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  JSValue result = zlib_async_deflate(ctx, input, input_len, &opts, ZLIB_FORMAT_DEFLATE, callback);

  if (argc > 2) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// inflate async implementation
static JSValue js_zlib_inflate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "inflate requires at least 2 arguments");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  // Check if second arg is options or callback
  JSValue callback;
  ZlibOptions opts;
  if (argc == 2) {
    callback = argv[1];
    zlib_options_init_defaults(&opts);
  } else {
    if (zlib_parse_options(ctx, argv[1], &opts) < 0) {
      return JS_EXCEPTION;
    }
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    if (argc > 2) {
      zlib_options_cleanup(&opts);
    }
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  JSValue result = zlib_async_inflate(ctx, input, input_len, &opts, ZLIB_FORMAT_DEFLATE, callback);

  if (argc > 2) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// deflateRaw async implementation
static JSValue js_zlib_deflate_raw(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "deflateRaw requires at least 2 arguments");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  JSValue callback;
  ZlibOptions opts;
  if (argc == 2) {
    callback = argv[1];
    zlib_options_init_defaults(&opts);
  } else {
    if (zlib_parse_options(ctx, argv[1], &opts) < 0) {
      return JS_EXCEPTION;
    }
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    if (argc > 2) {
      zlib_options_cleanup(&opts);
    }
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  JSValue result = zlib_async_deflate(ctx, input, input_len, &opts, ZLIB_FORMAT_RAW, callback);

  if (argc > 2) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// inflateRaw async implementation
static JSValue js_zlib_inflate_raw(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "inflateRaw requires at least 2 arguments");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  JSValue callback;
  ZlibOptions opts;
  if (argc == 2) {
    callback = argv[1];
    zlib_options_init_defaults(&opts);
  } else {
    if (zlib_parse_options(ctx, argv[1], &opts) < 0) {
      return JS_EXCEPTION;
    }
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    if (argc > 2) {
      zlib_options_cleanup(&opts);
    }
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  JSValue result = zlib_async_inflate(ctx, input, input_len, &opts, ZLIB_FORMAT_RAW, callback);

  if (argc > 2) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

// unzip async implementation
static JSValue js_zlib_unzip(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "unzip requires at least 2 arguments");
  }

  const uint8_t* input;
  size_t input_len;
  if (get_buffer_data(ctx, argv[0], &input, &input_len) < 0) {
    return JS_EXCEPTION;
  }

  JSValue callback;
  ZlibOptions opts;
  if (argc == 2) {
    callback = argv[1];
    zlib_options_init_defaults(&opts);
  } else {
    if (zlib_parse_options(ctx, argv[1], &opts) < 0) {
      return JS_EXCEPTION;
    }
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    if (argc > 2) {
      zlib_options_cleanup(&opts);
    }
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Use windowBits + 32 for auto-detection
  opts.windowBits = 15 + 32;

  JSValue result = zlib_async_inflate(ctx, input, input_len, &opts, ZLIB_FORMAT_DEFLATE, callback);

  if (argc > 2) {
    zlib_options_cleanup(&opts);
  }

  return result;
}

static const JSCFunctionListEntry js_zlib_funcs[] = {
    JS_CFUNC_DEF("gzipSync", 1, js_zlib_gzip_sync),
    JS_CFUNC_DEF("gunzipSync", 1, js_zlib_gunzip_sync),
    JS_CFUNC_DEF("deflateSync", 1, js_zlib_deflate_sync),
    JS_CFUNC_DEF("inflateSync", 1, js_zlib_inflate_sync),
    JS_CFUNC_DEF("deflateRawSync", 1, js_zlib_deflate_raw_sync),
    JS_CFUNC_DEF("inflateRawSync", 1, js_zlib_inflate_raw_sync),
    JS_CFUNC_DEF("unzipSync", 1, js_zlib_unzip_sync),
    JS_CFUNC_DEF("gzip", 2, js_zlib_gzip),
    JS_CFUNC_DEF("gunzip", 2, js_zlib_gunzip),
    JS_CFUNC_DEF("deflate", 2, js_zlib_deflate),
    JS_CFUNC_DEF("inflate", 2, js_zlib_inflate),
    JS_CFUNC_DEF("deflateRaw", 2, js_zlib_deflate_raw),
    JS_CFUNC_DEF("inflateRaw", 2, js_zlib_inflate_raw),
    JS_CFUNC_DEF("unzip", 2, js_zlib_unzip),
};

static int js_zlib_init_module(JSContext* ctx, JSModuleDef* m) {
  return JS_SetModuleExportList(ctx, m, js_zlib_funcs, countof(js_zlib_funcs));
}

int js_node_zlib_init(JSContext* ctx, JSModuleDef* m) {
  return js_zlib_init_module(ctx, m);
}

JSModuleDef* js_node_zlib_init_module(JSContext* ctx, const char* module_name) {
  JSModuleDef* m = JS_NewCModule(ctx, module_name, js_zlib_init_module);
  if (!m)
    return NULL;
  JS_AddModuleExportList(ctx, m, js_zlib_funcs, countof(js_zlib_funcs));
  return m;
}

JSValue JSRT_InitNodeZlib(JSContext* ctx) {
  JSValue exports = JS_NewObject(ctx);
  if (JS_IsException(exports))
    return JS_EXCEPTION;

  JS_SetPropertyFunctionList(ctx, exports, js_zlib_funcs, countof(js_zlib_funcs));

  // Export constants and utilities
  zlib_export_constants(ctx, exports);
  zlib_export_utilities(ctx, exports);

  // Export stream classes (C implementation)
  zlib_export_streams(ctx, exports);

  return exports;
}
