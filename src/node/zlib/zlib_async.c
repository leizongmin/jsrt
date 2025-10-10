#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../runtime.h"
#include "../../util/macro.h"
#include "zlib_internal.h"

// Async work structure
typedef struct {
  uv_work_t work;
  JSContext* ctx;
  JSValue callback;
  uint8_t* input;
  size_t input_len;
  uint8_t* output;
  size_t output_len;
  ZlibOptions opts;
  int format;
  bool is_deflate;
  int error_code;
  char error_msg[256];
} ZlibAsyncWork;

// Worker thread function - performs compression/decompression
static void zlib_async_work_cb(uv_work_t* req) {
  ZlibAsyncWork* work = (ZlibAsyncWork*)req;

  // Create a temporary context for compression
  ZlibContext* zctx = malloc(sizeof(ZlibContext));
  if (!zctx) {
    work->error_code = Z_MEM_ERROR;
    snprintf(work->error_msg, sizeof(work->error_msg), "Out of memory");
    return;
  }

  memset(zctx, 0, sizeof(ZlibContext));
  memcpy(&zctx->opts, &work->opts, sizeof(ZlibOptions));

  int ret;
  if (work->is_deflate) {
    ret = zlib_init_deflate(zctx, &work->opts, work->format);
  } else {
    ret = zlib_init_inflate(zctx, &work->opts, work->format);
  }

  if (ret != Z_OK) {
    work->error_code = ret;
    snprintf(work->error_msg, sizeof(work->error_msg), "Failed to initialize: %s", zlib_error_message(ret));
    free(zctx);
    return;
  }

  if (work->is_deflate) {
    // Deflate (compression)
    size_t output_capacity = deflateBound(&zctx->strm, work->input_len);
    work->output = (uint8_t*)malloc(output_capacity);
    if (!work->output) {
      work->error_code = Z_MEM_ERROR;
      snprintf(work->error_msg, sizeof(work->error_msg), "Out of memory");
      zlib_cleanup(zctx);
      free(zctx);
      return;
    }

    zctx->strm.next_in = work->input;
    zctx->strm.avail_in = work->input_len;
    zctx->strm.next_out = work->output;
    zctx->strm.avail_out = output_capacity;

    ret = deflate(&zctx->strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
      work->error_code = ret;
      snprintf(work->error_msg, sizeof(work->error_msg), "Deflate failed: %s", zlib_error_message(ret));
      free(work->output);
      work->output = NULL;
      zlib_cleanup(zctx);
      free(zctx);
      return;
    }

    work->output_len = zctx->strm.total_out;
  } else {
    // Inflate (decompression)
    size_t chunk_size = work->opts.chunkSize;
    size_t output_capacity = chunk_size;
    work->output = (uint8_t*)malloc(output_capacity);
    if (!work->output) {
      work->error_code = Z_MEM_ERROR;
      snprintf(work->error_msg, sizeof(work->error_msg), "Out of memory");
      zlib_cleanup(zctx);
      free(zctx);
      return;
    }

    size_t output_size = 0;
    zctx->strm.next_in = work->input;
    zctx->strm.avail_in = work->input_len;

    do {
      if (output_size + chunk_size > output_capacity) {
        output_capacity = output_capacity * 2;
        uint8_t* new_buffer = (uint8_t*)realloc(work->output, output_capacity);
        if (!new_buffer) {
          work->error_code = Z_MEM_ERROR;
          snprintf(work->error_msg, sizeof(work->error_msg), "Out of memory");
          free(work->output);
          work->output = NULL;
          zlib_cleanup(zctx);
          free(zctx);
          return;
        }
        work->output = new_buffer;
      }

      zctx->strm.next_out = work->output + output_size;
      zctx->strm.avail_out = chunk_size;

      ret = inflate(&zctx->strm, Z_NO_FLUSH);

      if (ret != Z_OK && ret != Z_STREAM_END) {
        work->error_code = ret;
        snprintf(work->error_msg, sizeof(work->error_msg), "Inflate failed: %s", zlib_error_message(ret));
        free(work->output);
        work->output = NULL;
        zlib_cleanup(zctx);
        free(zctx);
        return;
      }

      output_size += (chunk_size - zctx->strm.avail_out);

    } while (ret != Z_STREAM_END && zctx->strm.avail_in > 0);

    work->output_len = output_size;
  }

  work->error_code = Z_OK;
  zlib_cleanup(zctx);
  free(zctx);
}

// Completion callback - runs in main thread
static void zlib_async_after_work_cb(uv_work_t* req, int status) {
  ZlibAsyncWork* work = (ZlibAsyncWork*)req;
  JSContext* ctx = work->ctx;

  JSValue result;
  JSValue error = JS_UNDEFINED;

  if (status != 0 || work->error_code != Z_OK) {
    // Error occurred
    if (work->error_msg[0]) {
      error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, work->error_msg));
    } else {
      error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Unknown error"));
    }
    result = JS_UNDEFINED;
  } else {
    // Success - create result buffer
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, work->output, work->output_len);

    // Wrap in Uint8Array
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue uint8_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
    JSValue args[1] = {array_buffer};
    result = JS_CallConstructor(ctx, uint8_ctor, 1, args);

    JS_FreeValue(ctx, array_buffer);
    JS_FreeValue(ctx, uint8_ctor);
    JS_FreeValue(ctx, global);
  }

  // Call the callback
  JSValue callback_args[2] = {error, result};
  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 2, callback_args);

  // Clean up
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, error);
  JS_FreeValue(ctx, result);
  JS_FreeValue(ctx, work->callback);

  if (work->input) {
    free(work->input);
  }
  if (work->output) {
    free(work->output);
  }
  // Don't call zlib_options_cleanup as we didn't allocate the dictionary
  free(work);
}

// Queue async compression work
JSValue zlib_async_deflate(JSContext* ctx, const uint8_t* input, size_t input_len, const ZlibOptions* opts, int format,
                           JSValue callback) {
  ZlibAsyncWork* work = (ZlibAsyncWork*)malloc(sizeof(ZlibAsyncWork));
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  memset(work, 0, sizeof(ZlibAsyncWork));
  work->ctx = ctx;
  work->callback = JS_DupValue(ctx, callback);
  work->is_deflate = true;
  work->format = format;

  // Copy input data
  work->input = (uint8_t*)malloc(input_len);
  if (!work->input) {
    JS_FreeValue(ctx, work->callback);
    free(work);
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(work->input, input, input_len);
  work->input_len = input_len;

  // Copy options
  if (opts) {
    memcpy(&work->opts, opts, sizeof(ZlibOptions));
    // Don't copy dictionary pointer - we don't use it yet
    work->opts.has_dictionary = false;
    work->opts.dictionary = NULL;
    work->opts.dictionary_len = 0;
  } else {
    zlib_options_init_defaults(&work->opts);
  }

  // Get event loop from runtime
  JSRuntime* rt = JS_GetRuntime(ctx);
  JSRT_Runtime* jsrt_rt = (JSRT_Runtime*)JS_GetRuntimeOpaque(rt);
  if (!jsrt_rt || !jsrt_rt->uv_loop) {
    free(work->input);
    JS_FreeValue(ctx, work->callback);
    free(work);
    return JS_ThrowInternalError(ctx, "Event loop not available");
  }

  // Queue work
  int ret = uv_queue_work(jsrt_rt->uv_loop, &work->work, zlib_async_work_cb, zlib_async_after_work_cb);
  if (ret != 0) {
    free(work->input);
    JS_FreeValue(ctx, work->callback);
    free(work);
    return JS_ThrowInternalError(ctx, "Failed to queue async work");
  }

  return JS_UNDEFINED;
}

// Queue async decompression work
JSValue zlib_async_inflate(JSContext* ctx, const uint8_t* input, size_t input_len, const ZlibOptions* opts, int format,
                           JSValue callback) {
  ZlibAsyncWork* work = (ZlibAsyncWork*)malloc(sizeof(ZlibAsyncWork));
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  memset(work, 0, sizeof(ZlibAsyncWork));
  work->ctx = ctx;
  work->callback = JS_DupValue(ctx, callback);
  work->is_deflate = false;
  work->format = format;

  // Copy input data
  work->input = (uint8_t*)malloc(input_len);
  if (!work->input) {
    JS_FreeValue(ctx, work->callback);
    free(work);
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(work->input, input, input_len);
  work->input_len = input_len;

  // Copy options
  if (opts) {
    memcpy(&work->opts, opts, sizeof(ZlibOptions));
    // Don't copy dictionary pointer - we don't use it yet
    work->opts.has_dictionary = false;
    work->opts.dictionary = NULL;
    work->opts.dictionary_len = 0;
  } else {
    zlib_options_init_defaults(&work->opts);
  }

  // Get event loop from runtime
  JSRuntime* rt = JS_GetRuntime(ctx);
  JSRT_Runtime* jsrt_rt = (JSRT_Runtime*)JS_GetRuntimeOpaque(rt);
  if (!jsrt_rt || !jsrt_rt->uv_loop) {
    free(work->input);
    JS_FreeValue(ctx, work->callback);
    free(work);
    return JS_ThrowInternalError(ctx, "Event loop not available");
  }

  // Queue work
  int ret = uv_queue_work(jsrt_rt->uv_loop, &work->work, zlib_async_work_cb, zlib_async_after_work_cb);
  if (ret != 0) {
    free(work->input);
    JS_FreeValue(ctx, work->callback);
    free(work);
    return JS_ThrowInternalError(ctx, "Failed to queue async work");
  }

  return JS_UNDEFINED;
}
