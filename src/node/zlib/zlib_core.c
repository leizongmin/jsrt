#include <stdlib.h>
#include <string.h>
#include "zlib_internal.h"

// Create new zlib context
ZlibContext* zlib_context_new(JSContext* ctx) {
  ZlibContext* zctx = (ZlibContext*)malloc(sizeof(ZlibContext));
  if (!zctx) {
    return NULL;
  }

  memset(zctx, 0, sizeof(ZlibContext));
  zlib_options_init_defaults(&zctx->opts);
  zctx->initialized = false;
  zctx->is_deflate = false;
  zctx->output_buffer = NULL;
  zctx->output_capacity = 0;
  zctx->output_size = 0;

  return zctx;
}

// Free zlib context
void zlib_context_free(ZlibContext* zctx) {
  if (!zctx) {
    return;
  }

  if (zctx->initialized) {
    zlib_cleanup(zctx);
  }

  if (zctx->output_buffer) {
    free(zctx->output_buffer);
    zctx->output_buffer = NULL;
  }

  zlib_options_cleanup(&zctx->opts);
  free(zctx);
}

// Initialize deflate stream
int zlib_init_deflate(ZlibContext* zctx, const ZlibOptions* opts, int format) {
  if (zctx->initialized) {
    return Z_STREAM_ERROR;
  }

  memset(&zctx->strm, 0, sizeof(z_stream));

  // Copy options
  if (opts) {
    memcpy(&zctx->opts, opts, sizeof(ZlibOptions));
  } else {
    zlib_options_init_defaults(&zctx->opts);
  }

  zctx->is_deflate = true;

  // Adjust windowBits based on format
  int windowBits = zctx->opts.windowBits;
  if (format == ZLIB_FORMAT_GZIP) {
    windowBits += 16;  // Add 16 for gzip format
  } else if (format == ZLIB_FORMAT_RAW) {
    windowBits = -windowBits;  // Negative for raw deflate
  }
  // ZLIB_FORMAT_DEFLATE uses windowBits as-is

  int ret =
      deflateInit2(&zctx->strm, zctx->opts.level, Z_DEFLATED, windowBits, zctx->opts.memLevel, zctx->opts.strategy);

  if (ret == Z_OK) {
    zctx->initialized = true;
  }

  return ret;
}

// Initialize inflate stream
int zlib_init_inflate(ZlibContext* zctx, const ZlibOptions* opts, int format) {
  if (zctx->initialized) {
    return Z_STREAM_ERROR;
  }

  memset(&zctx->strm, 0, sizeof(z_stream));

  // Copy options
  if (opts) {
    memcpy(&zctx->opts, opts, sizeof(ZlibOptions));
  } else {
    zlib_options_init_defaults(&zctx->opts);
  }

  zctx->is_deflate = false;

  // Adjust windowBits based on format
  int windowBits = zctx->opts.windowBits;
  if (format == ZLIB_FORMAT_GZIP) {
    windowBits += 16;  // Add 16 for gzip format
  } else if (format == ZLIB_FORMAT_RAW) {
    windowBits = -windowBits;  // Negative for raw deflate
  }
  // ZLIB_FORMAT_DEFLATE uses windowBits as-is
  // Use +32 to auto-detect format (gzip or deflate)

  int ret = inflateInit2(&zctx->strm, windowBits);

  if (ret == Z_OK) {
    zctx->initialized = true;
  }

  return ret;
}

// Cleanup zlib stream
void zlib_cleanup(ZlibContext* zctx) {
  if (!zctx || !zctx->initialized) {
    return;
  }

  if (zctx->is_deflate) {
    deflateEnd(&zctx->strm);
  } else {
    inflateEnd(&zctx->strm);
  }

  zctx->initialized = false;
}

// Get error message for zlib error code
const char* zlib_error_message(int err_code) {
  switch (err_code) {
    case Z_OK:
      return "OK";
    case Z_STREAM_END:
      return "Stream end";
    case Z_NEED_DICT:
      return "Need dictionary";
    case Z_ERRNO:
      return "File error";
    case Z_STREAM_ERROR:
      return "Stream error";
    case Z_DATA_ERROR:
      return "Data error";
    case Z_MEM_ERROR:
      return "Memory error";
    case Z_BUF_ERROR:
      return "Buffer error";
    case Z_VERSION_ERROR:
      return "Version error";
    default:
      return "Unknown error";
  }
}

// Throw zlib error
JSValue zlib_throw_error(JSContext* ctx, int err_code, const char* msg) {
  const char* err_msg = zlib_error_message(err_code);

  if (msg) {
    return JS_ThrowInternalError(ctx, "%s: %s", msg, err_msg);
  } else {
    return JS_ThrowInternalError(ctx, "%s", err_msg);
  }
}
