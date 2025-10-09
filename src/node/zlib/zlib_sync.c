#include <stdlib.h>
#include <string.h>
#include "zlib_internal.h"

// Synchronous deflate operation
JSValue zlib_deflate_sync(JSContext* ctx, const uint8_t* input, size_t input_len, const ZlibOptions* opts, int format) {
  ZlibContext* zctx = zlib_context_new(ctx);
  if (!zctx) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize deflate stream
  int ret = zlib_init_deflate(zctx, opts, format);
  if (ret != Z_OK) {
    zlib_context_free(zctx);
    return zlib_throw_error(ctx, ret, "Failed to initialize deflate");
  }

  // Estimate output size (worst case: slightly larger than input)
  size_t output_capacity = deflateBound(&zctx->strm, input_len);
  uint8_t* output_buffer = (uint8_t*)malloc(output_capacity);
  if (!output_buffer) {
    zlib_context_free(zctx);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Set up input and output
  zctx->strm.next_in = (Bytef*)input;
  zctx->strm.avail_in = input_len;
  zctx->strm.next_out = output_buffer;
  zctx->strm.avail_out = output_capacity;

  // Perform compression
  ret = deflate(&zctx->strm, Z_FINISH);
  if (ret != Z_STREAM_END) {
    free(output_buffer);
    zlib_context_free(zctx);
    return zlib_throw_error(ctx, ret, "Deflate failed");
  }

  size_t output_size = zctx->strm.total_out;

  // Create result buffer - just return the ArrayBuffer wrapped as Buffer
  JSValue array_buffer = JS_NewArrayBufferCopy(ctx, output_buffer, output_size);
  free(output_buffer);
  zlib_context_free(zctx);

  if (JS_IsException(array_buffer)) {
    return array_buffer;
  }

  // Wrap in Uint8Array to make it Buffer-like
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue uint8_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  JSValue args[1] = {array_buffer};
  JSValue result = JS_CallConstructor(ctx, uint8_ctor, 1, args);

  JS_FreeValue(ctx, array_buffer);
  JS_FreeValue(ctx, uint8_ctor);
  JS_FreeValue(ctx, global);

  return result;
}

// Synchronous inflate operation
JSValue zlib_inflate_sync(JSContext* ctx, const uint8_t* input, size_t input_len, const ZlibOptions* opts, int format) {
  ZlibContext* zctx = zlib_context_new(ctx);
  if (!zctx) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize inflate stream
  int ret = zlib_init_inflate(zctx, opts, format);
  if (ret != Z_OK) {
    zlib_context_free(zctx);
    return zlib_throw_error(ctx, ret, "Failed to initialize inflate");
  }

  // Use chunk-based decompression since we don't know output size
  size_t chunk_size = opts ? opts->chunkSize : 16 * 1024;
  size_t output_capacity = chunk_size;
  uint8_t* output_buffer = (uint8_t*)malloc(output_capacity);
  if (!output_buffer) {
    zlib_context_free(zctx);
    return JS_ThrowOutOfMemory(ctx);
  }

  size_t output_size = 0;

  // Set up input
  zctx->strm.next_in = (Bytef*)input;
  zctx->strm.avail_in = input_len;

  // Decompress in chunks
  do {
    // Ensure we have space
    if (output_size + chunk_size > output_capacity) {
      output_capacity = output_capacity * 2;
      uint8_t* new_buffer = (uint8_t*)realloc(output_buffer, output_capacity);
      if (!new_buffer) {
        free(output_buffer);
        zlib_context_free(zctx);
        return JS_ThrowOutOfMemory(ctx);
      }
      output_buffer = new_buffer;
    }

    zctx->strm.next_out = output_buffer + output_size;
    zctx->strm.avail_out = chunk_size;

    ret = inflate(&zctx->strm, Z_NO_FLUSH);

    if (ret != Z_OK && ret != Z_STREAM_END) {
      free(output_buffer);
      zlib_context_free(zctx);
      return zlib_throw_error(ctx, ret, "Inflate failed");
    }

    output_size += (chunk_size - zctx->strm.avail_out);

  } while (ret != Z_STREAM_END && zctx->strm.avail_in > 0);

  // Create result buffer - just return the ArrayBuffer wrapped as Uint8Array
  JSValue array_buffer = JS_NewArrayBufferCopy(ctx, output_buffer, output_size);
  free(output_buffer);
  zlib_context_free(zctx);

  if (JS_IsException(array_buffer)) {
    return array_buffer;
  }

  // Wrap in Uint8Array to make it Buffer-like
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue uint8_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  JSValue args[1] = {array_buffer};
  JSValue result = JS_CallConstructor(ctx, uint8_ctor, 1, args);

  JS_FreeValue(ctx, array_buffer);
  JS_FreeValue(ctx, uint8_ctor);
  JS_FreeValue(ctx, global);

  return result;
}
