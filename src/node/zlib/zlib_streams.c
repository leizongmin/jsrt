#include <string.h>
#include "../../util/debug.h"
#include "../stream/stream_internal.h"
#include "zlib_internal.h"

// Zlib stream classes - extends Transform from node:stream
// Provides streaming compression/decompression by wrapping zlib C library

// Zlib stream private data (stored as JS property, not as opaque)
typedef struct {
  ZlibContext* zlib_ctx;
  int format;        // ZLIB_FORMAT_GZIP, ZLIB_FORMAT_DEFLATE, ZLIB_FORMAT_RAW
  bool is_compress;  // true for compression, false for decompression
} ZlibStreamData;

// Helper to get zlib stream data from object
static ZlibStreamData* get_zlib_stream_data(JSContext* ctx, JSValueConst obj) {
  JSValue private_val = JS_GetPropertyStr(ctx, obj, "_zlibData");
  if (JS_IsUndefined(private_val)) {
    JS_FreeValue(ctx, private_val);
    return NULL;
  }

  size_t size;
  uint8_t* buf = JS_GetArrayBuffer(ctx, &size, private_val);
  JS_FreeValue(ctx, private_val);

  if (!buf || size != sizeof(ZlibStreamData)) {
    return NULL;
  }

  return (ZlibStreamData*)buf;
}

// Free function for ArrayBuffer containing ZlibStreamData
static void zlib_stream_data_free(JSRuntime* rt, void* opaque, void* ptr) {
  (void)rt;
  (void)opaque;
  free(ptr);
}

// Helper to set zlib stream data on object
static int set_zlib_stream_data(JSContext* ctx, JSValue obj, ZlibStreamData* data) {
  // Allocate heap memory for the data
  ZlibStreamData* heap_data = malloc(sizeof(ZlibStreamData));
  if (!heap_data) {
    return -1;
  }
  memcpy(heap_data, data, sizeof(ZlibStreamData));

  // Create ArrayBuffer with heap data (will be freed when AB is GC'd)
  JSValue ab = JS_NewArrayBuffer(ctx, (uint8_t*)heap_data, sizeof(ZlibStreamData), zlib_stream_data_free, NULL, 0);
  if (JS_IsException(ab)) {
    free(heap_data);
    return -1;
  }

  int ret = JS_SetPropertyStr(ctx, obj, "_zlibData", ab);
  return ret;
}

// Process data through zlib incrementally
static JSValue zlib_stream_process(JSContext* ctx, ZlibStreamData* zstream, const uint8_t* input, size_t input_len,
                                   bool is_final, uint8_t** output, size_t* output_len) {
  if (!zstream->zlib_ctx) {
    return JS_ThrowInternalError(ctx, "zlib context not initialized");
  }

  ZlibContext* zctx = zstream->zlib_ctx;
  z_stream* strm = &zctx->strm;

  // Set input
  strm->next_in = (Bytef*)input;
  strm->avail_in = input_len;

  // Allocate output buffer (estimate)
  size_t out_capacity = input_len + 1024;  // Extra space for compression overhead
  uint8_t* out_buf = zlib_buffer_acquire(out_capacity, &out_capacity);
  size_t total_out = 0;

  int flush = is_final ? Z_FINISH : Z_NO_FLUSH;

  do {
    // Expand output buffer if needed
    if (total_out >= out_capacity) {
      size_t new_capacity = out_capacity * 2;
      uint8_t* new_buf = zlib_buffer_acquire(new_capacity, &new_capacity);
      memcpy(new_buf, out_buf, total_out);
      zlib_buffer_release(out_buf, out_capacity);
      out_buf = new_buf;
      out_capacity = new_capacity;
    }

    strm->next_out = out_buf + total_out;
    strm->avail_out = out_capacity - total_out;

    int ret = zstream->is_compress ? deflate(strm, flush) : inflate(strm, flush);

    if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
      zlib_buffer_release(out_buf, out_capacity);
      return zlib_throw_error(ctx, ret, zstream->is_compress ? "deflate failed" : "inflate failed");
    }

    total_out = out_capacity - strm->avail_out;

    if (ret == Z_STREAM_END) {
      break;
    }
  } while (strm->avail_in > 0 || (is_final && total_out > 0));

  *output = out_buf;
  *output_len = total_out;

  return JS_UNDEFINED;
}

// _transform implementation for zlib streams
static JSValue zlib_stream_transform(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "_transform requires 3 arguments");
  }

  // Get zlib stream data
  ZlibStreamData* zstream = get_zlib_stream_data(ctx, this_val);
  if (!zstream) {
    return JS_ThrowTypeError(ctx, "Not a zlib stream");
  }

  JSValue chunk = argv[0];
  // JSValue encoding = argv[1];  // Unused
  JSValue callback = argv[2];

  // Get input data
  const uint8_t* input;
  size_t input_len;
  size_t size;
  uint8_t* buf = JS_GetArrayBuffer(ctx, &size, chunk);

  if (buf) {
    input = buf;
    input_len = size;
  } else {
    JSValue buffer = JS_GetTypedArrayBuffer(ctx, chunk, NULL, NULL, NULL);
    if (!JS_IsException(buffer)) {
      buf = JS_GetArrayBuffer(ctx, &size, buffer);
      JS_FreeValue(ctx, buffer);
      if (buf) {
        input = buf;
        input_len = size;
      } else {
        JSValue args[] = {JS_NewString(ctx, "Invalid chunk type")};
        JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
        JS_FreeValue(ctx, args[0]);
        return result;
      }
    } else {
      JSValue args[] = {JS_NewString(ctx, "Invalid chunk type")};
      JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
      JS_FreeValue(ctx, args[0]);
      return result;
    }
  }

  // Process chunk through zlib
  uint8_t* output;
  size_t output_len;
  JSValue err = zlib_stream_process(ctx, zstream, input, input_len, false, &output, &output_len);

  if (JS_IsException(err)) {
    JSValue args[] = {err};
    JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, err);
    return result;
  }

  // Push output to readable side if any
  if (output_len > 0) {
    JSValue output_buf = JS_NewArrayBufferCopy(ctx, output, output_len);
    zlib_buffer_release(output, output_len);

    JSValue push_fn = JS_GetPropertyStr(ctx, this_val, "push");
    if (JS_IsFunction(ctx, push_fn)) {
      JSValue push_args[] = {output_buf};
      JSValue push_result = JS_Call(ctx, push_fn, this_val, 1, push_args);
      JS_FreeValue(ctx, push_result);
    }
    JS_FreeValue(ctx, push_fn);
    JS_FreeValue(ctx, output_buf);
  } else {
    zlib_buffer_release(output, output_len);
  }

  // Call callback to signal completion
  JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 0, NULL);
  return result;
}

// _flush implementation for zlib streams
static JSValue zlib_stream_flush(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "_flush requires 1 argument");
  }

  ZlibStreamData* zstream = get_zlib_stream_data(ctx, this_val);
  if (!zstream) {
    return JS_ThrowTypeError(ctx, "Not a zlib stream");
  }

  JSValue callback = argv[0];

  // Flush remaining data
  uint8_t* output;
  size_t output_len;
  JSValue err = zlib_stream_process(ctx, zstream, NULL, 0, true, &output, &output_len);

  if (JS_IsException(err)) {
    JSValue args[] = {err};
    JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, err);
    return result;
  }

  // Push final output
  if (output_len > 0) {
    JSValue output_buf = JS_NewArrayBufferCopy(ctx, output, output_len);
    zlib_buffer_release(output, output_len);

    JSValue push_fn = JS_GetPropertyStr(ctx, this_val, "push");
    if (JS_IsFunction(ctx, push_fn)) {
      JSValue push_args[] = {output_buf};
      JSValue push_result = JS_Call(ctx, push_fn, this_val, 1, push_args);
      JS_FreeValue(ctx, push_result);
    }
    JS_FreeValue(ctx, push_fn);
    JS_FreeValue(ctx, output_buf);
  } else {
    zlib_buffer_release(output, output_len);
  }

  // Push null to signal end
  JSValue push_fn = JS_GetPropertyStr(ctx, this_val, "push");
  if (JS_IsFunction(ctx, push_fn)) {
    JSValue push_args[] = {JS_NULL};
    JSValue push_result = JS_Call(ctx, push_fn, this_val, 1, push_args);
    JS_FreeValue(ctx, push_result);
  }
  JS_FreeValue(ctx, push_fn);

  // Call callback
  JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 0, NULL);
  return result;
}

// Create a zlib stream base (compression or decompression)
static JSValue create_zlib_stream(JSContext* ctx, int format, bool is_compress, JSValueConst options) {
  // Get Transform constructor - first try from global
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue transform_ctor = JS_GetPropertyStr(ctx, global_obj, "Transform");

  // If not in global, try to require node:stream module
  if (!JS_IsFunction(ctx, transform_ctor)) {
    JS_FreeValue(ctx, transform_ctor);

    // Try to load stream module
    JSValue require_fn = JS_GetPropertyStr(ctx, global_obj, "require");
    if (JS_IsFunction(ctx, require_fn)) {
      JSValue stream_name = JS_NewString(ctx, "node:stream");
      JSValue stream_module = JS_Call(ctx, require_fn, JS_UNDEFINED, 1, &stream_name);
      JS_FreeValue(ctx, stream_name);
      JS_FreeValue(ctx, require_fn);

      if (!JS_IsException(stream_module)) {
        transform_ctor = JS_GetPropertyStr(ctx, stream_module, "Transform");
        JS_FreeValue(ctx, stream_module);
      } else {
        JS_FreeValue(ctx, stream_module);
      }
    } else {
      JS_FreeValue(ctx, require_fn);
    }
  }

  JS_FreeValue(ctx, global_obj);

  if (!JS_IsFunction(ctx, transform_ctor)) {
    JS_FreeValue(ctx, transform_ctor);
    return JS_ThrowReferenceError(ctx, "Transform class not available");
  }

  // Create Transform instance
  // Pass undefined if no options, otherwise dup the options
  JSValue obj;
  if (JS_IsUndefined(options)) {
    obj = JS_CallConstructor(ctx, transform_ctor, 0, NULL);
  } else {
    JSValue opts_arg = JS_DupValue(ctx, options);
    obj = JS_CallConstructor(ctx, transform_ctor, 1, &opts_arg);
    JS_FreeValue(ctx, opts_arg);
  }
  JS_FreeValue(ctx, transform_ctor);

  if (JS_IsException(obj)) {
    return obj;
  }

  // Allocate zlib stream data
  ZlibStreamData zstream_data;
  memset(&zstream_data, 0, sizeof(ZlibStreamData));

  zstream_data.format = format;
  zstream_data.is_compress = is_compress;

  // Parse options
  ZlibOptions opts;
  if (JS_IsUndefined(options) || JS_IsNull(options)) {
    zlib_options_init_defaults(&opts);
  } else if (zlib_parse_options(ctx, options, &opts) < 0) {
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
  }

  // Acquire zlib context
  zstream_data.zlib_ctx = zlib_context_acquire(ctx);
  if (!zstream_data.zlib_ctx) {
    zlib_options_cleanup(&opts);
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize zlib context
  int ret;
  if (is_compress) {
    ret = zlib_init_deflate(zstream_data.zlib_ctx, &opts, format);
  } else {
    ret = zlib_init_inflate(zstream_data.zlib_ctx, &opts, format);
  }

  zlib_options_cleanup(&opts);

  if (ret != Z_OK) {
    zlib_context_release(zstream_data.zlib_ctx);
    JS_FreeValue(ctx, obj);
    return zlib_throw_error(ctx, ret, is_compress ? "deflateInit failed" : "inflateInit failed");
  }

  // Store zlib stream data as property
  if (set_zlib_stream_data(ctx, obj, &zstream_data) < 0) {
    zlib_context_release(zstream_data.zlib_ctx);
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Override _transform and _flush methods
  JS_SetPropertyStr(ctx, obj, "_transform", JS_NewCFunction(ctx, zlib_stream_transform, "_transform", 3));
  JS_SetPropertyStr(ctx, obj, "_flush", JS_NewCFunction(ctx, zlib_stream_flush, "_flush", 1));

  return obj;
}

// Gzip stream
static JSValue js_create_gzip(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue options = argc > 0 ? argv[0] : JS_UNDEFINED;
  return create_zlib_stream(ctx, ZLIB_FORMAT_GZIP, true, options);
}

// Gunzip stream
static JSValue js_create_gunzip(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue options = argc > 0 ? argv[0] : JS_UNDEFINED;
  return create_zlib_stream(ctx, ZLIB_FORMAT_GZIP, false, options);
}

// Deflate stream
static JSValue js_create_deflate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue options = argc > 0 ? argv[0] : JS_UNDEFINED;
  return create_zlib_stream(ctx, ZLIB_FORMAT_DEFLATE, true, options);
}

// Inflate stream
static JSValue js_create_inflate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue options = argc > 0 ? argv[0] : JS_UNDEFINED;
  return create_zlib_stream(ctx, ZLIB_FORMAT_DEFLATE, false, options);
}

// DeflateRaw stream
static JSValue js_create_deflate_raw(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue options = argc > 0 ? argv[0] : JS_UNDEFINED;
  return create_zlib_stream(ctx, ZLIB_FORMAT_RAW, true, options);
}

// InflateRaw stream
static JSValue js_create_inflate_raw(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue options = argc > 0 ? argv[0] : JS_UNDEFINED;
  return create_zlib_stream(ctx, ZLIB_FORMAT_RAW, false, options);
}

// Unzip stream (auto-detect)
static JSValue js_create_unzip(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue options = argc > 0 ? argv[0] : JS_UNDEFINED;

  // For unzip, we need to set windowBits to enable auto-detection
  // Create a modified options object
  JSValue modified_opts = JS_DupValue(ctx, options);
  if (JS_IsUndefined(modified_opts)) {
    modified_opts = JS_NewObject(ctx);
  }

  // Set windowBits to 15 + 32 for auto-detection
  JS_SetPropertyStr(ctx, modified_opts, "windowBits", JS_NewInt32(ctx, 47));  // 15 + 32

  JSValue result = create_zlib_stream(ctx, ZLIB_FORMAT_DEFLATE, false, modified_opts);
  JS_FreeValue(ctx, modified_opts);

  return result;
}

// Initialize zlib stream classes
void zlib_export_streams(JSContext* ctx, JSValue exports) {
  // Export factory functions
  JS_SetPropertyStr(ctx, exports, "createGzip", JS_NewCFunction(ctx, js_create_gzip, "createGzip", 1));
  JS_SetPropertyStr(ctx, exports, "createGunzip", JS_NewCFunction(ctx, js_create_gunzip, "createGunzip", 1));
  JS_SetPropertyStr(ctx, exports, "createDeflate", JS_NewCFunction(ctx, js_create_deflate, "createDeflate", 1));
  JS_SetPropertyStr(ctx, exports, "createInflate", JS_NewCFunction(ctx, js_create_inflate, "createInflate", 1));
  JS_SetPropertyStr(ctx, exports, "createDeflateRaw",
                    JS_NewCFunction(ctx, js_create_deflate_raw, "createDeflateRaw", 1));
  JS_SetPropertyStr(ctx, exports, "createInflateRaw",
                    JS_NewCFunction(ctx, js_create_inflate_raw, "createInflateRaw", 1));
  JS_SetPropertyStr(ctx, exports, "createUnzip", JS_NewCFunction(ctx, js_create_unzip, "createUnzip", 1));
}
