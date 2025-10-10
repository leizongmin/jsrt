#include <zlib.h>
#include "../../util/macro.h"
#include "zlib_internal.h"

// Export zlib constants to JavaScript
void zlib_export_constants(JSContext* ctx, JSValue exports) {
  JSValue constants = JS_NewObject(ctx);

  // Compression levels
  JS_SetPropertyStr(ctx, constants, "Z_NO_COMPRESSION", JS_NewInt32(ctx, Z_NO_COMPRESSION));
  JS_SetPropertyStr(ctx, constants, "Z_BEST_SPEED", JS_NewInt32(ctx, Z_BEST_SPEED));
  JS_SetPropertyStr(ctx, constants, "Z_BEST_COMPRESSION", JS_NewInt32(ctx, Z_BEST_COMPRESSION));
  JS_SetPropertyStr(ctx, constants, "Z_DEFAULT_COMPRESSION", JS_NewInt32(ctx, Z_DEFAULT_COMPRESSION));

  // Compression strategy
  JS_SetPropertyStr(ctx, constants, "Z_FILTERED", JS_NewInt32(ctx, Z_FILTERED));
  JS_SetPropertyStr(ctx, constants, "Z_HUFFMAN_ONLY", JS_NewInt32(ctx, Z_HUFFMAN_ONLY));
  JS_SetPropertyStr(ctx, constants, "Z_RLE", JS_NewInt32(ctx, Z_RLE));
  JS_SetPropertyStr(ctx, constants, "Z_FIXED", JS_NewInt32(ctx, Z_FIXED));
  JS_SetPropertyStr(ctx, constants, "Z_DEFAULT_STRATEGY", JS_NewInt32(ctx, Z_DEFAULT_STRATEGY));

  // Flush values
  JS_SetPropertyStr(ctx, constants, "Z_NO_FLUSH", JS_NewInt32(ctx, Z_NO_FLUSH));
  JS_SetPropertyStr(ctx, constants, "Z_PARTIAL_FLUSH", JS_NewInt32(ctx, Z_PARTIAL_FLUSH));
  JS_SetPropertyStr(ctx, constants, "Z_SYNC_FLUSH", JS_NewInt32(ctx, Z_SYNC_FLUSH));
  JS_SetPropertyStr(ctx, constants, "Z_FULL_FLUSH", JS_NewInt32(ctx, Z_FULL_FLUSH));
  JS_SetPropertyStr(ctx, constants, "Z_FINISH", JS_NewInt32(ctx, Z_FINISH));
  JS_SetPropertyStr(ctx, constants, "Z_BLOCK", JS_NewInt32(ctx, Z_BLOCK));
  JS_SetPropertyStr(ctx, constants, "Z_TREES", JS_NewInt32(ctx, Z_TREES));

  // Return codes
  JS_SetPropertyStr(ctx, constants, "Z_OK", JS_NewInt32(ctx, Z_OK));
  JS_SetPropertyStr(ctx, constants, "Z_STREAM_END", JS_NewInt32(ctx, Z_STREAM_END));
  JS_SetPropertyStr(ctx, constants, "Z_NEED_DICT", JS_NewInt32(ctx, Z_NEED_DICT));
  JS_SetPropertyStr(ctx, constants, "Z_ERRNO", JS_NewInt32(ctx, Z_ERRNO));
  JS_SetPropertyStr(ctx, constants, "Z_STREAM_ERROR", JS_NewInt32(ctx, Z_STREAM_ERROR));
  JS_SetPropertyStr(ctx, constants, "Z_DATA_ERROR", JS_NewInt32(ctx, Z_DATA_ERROR));
  JS_SetPropertyStr(ctx, constants, "Z_MEM_ERROR", JS_NewInt32(ctx, Z_MEM_ERROR));
  JS_SetPropertyStr(ctx, constants, "Z_BUF_ERROR", JS_NewInt32(ctx, Z_BUF_ERROR));
  JS_SetPropertyStr(ctx, constants, "Z_VERSION_ERROR", JS_NewInt32(ctx, Z_VERSION_ERROR));

  // Add zlib version
  JSValue versions = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, versions, "zlib", JS_NewString(ctx, ZLIB_VERSION));
  JS_SetPropertyStr(ctx, exports, "versions", versions);

  // Add constants to exports (both as direct properties and as constants object)
  // Direct properties for compatibility
  JSPropertyEnum* props;
  uint32_t prop_count;
  if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, constants, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
    for (uint32_t i = 0; i < prop_count; i++) {
      JSValue key = JS_AtomToString(ctx, props[i].atom);
      const char* key_str = JS_ToCString(ctx, key);
      if (key_str) {
        JSValue val = JS_GetProperty(ctx, constants, props[i].atom);
        JS_SetPropertyStr(ctx, exports, key_str, JS_DupValue(ctx, val));
        JS_FreeValue(ctx, val);
        JS_FreeCString(ctx, key_str);
      }
      JS_FreeValue(ctx, key);
      JS_FreeAtom(ctx, props[i].atom);
    }
    js_free(ctx, props);
  }

  // Also export as constants object
  JS_SetPropertyStr(ctx, exports, "constants", constants);
}

// Implement crc32 utility
static JSValue js_zlib_crc32(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "crc32 requires at least 1 argument");
  }

  // Get buffer data
  size_t data_len;
  uint8_t* data = NULL;

  // Try to get as ArrayBuffer
  data = JS_GetArrayBuffer(ctx, &data_len, argv[0]);
  if (!data) {
    // Try to get as typed array
    JSValue buffer = JS_GetTypedArrayBuffer(ctx, argv[0], NULL, NULL, NULL);
    if (!JS_IsException(buffer)) {
      data = JS_GetArrayBuffer(ctx, &data_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!data) {
    return JS_ThrowTypeError(ctx, "argument must be a Buffer or Uint8Array");
  }

  // Get initial value if provided
  uint32_t initial = 0;
  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    if (JS_ToUint32(ctx, &initial, argv[1]) < 0) {
      return JS_EXCEPTION;
    }
  }

  // Calculate CRC32
  uint32_t result = crc32(initial, data, data_len);

  return JS_NewUint32(ctx, result);
}

// Implement adler32 utility
static JSValue js_zlib_adler32(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "adler32 requires at least 1 argument");
  }

  // Get buffer data
  size_t data_len;
  uint8_t* data = NULL;

  // Try to get as ArrayBuffer
  data = JS_GetArrayBuffer(ctx, &data_len, argv[0]);
  if (!data) {
    // Try to get as typed array
    JSValue buffer = JS_GetTypedArrayBuffer(ctx, argv[0], NULL, NULL, NULL);
    if (!JS_IsException(buffer)) {
      data = JS_GetArrayBuffer(ctx, &data_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!data) {
    return JS_ThrowTypeError(ctx, "argument must be a Buffer or Uint8Array");
  }

  // Get initial value if provided
  uint32_t initial = 1;  // adler32 starts with 1
  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    if (JS_ToUint32(ctx, &initial, argv[1]) < 0) {
      return JS_EXCEPTION;
    }
  }

  // Calculate Adler-32
  uint32_t result = adler32(initial, data, data_len);

  return JS_NewUint32(ctx, result);
}

// Export utilities
static const JSCFunctionListEntry js_zlib_utils[] = {
    JS_CFUNC_DEF("crc32", 1, js_zlib_crc32),
    JS_CFUNC_DEF("adler32", 1, js_zlib_adler32),
};

void zlib_export_utilities(JSContext* ctx, JSValue exports) {
  JS_SetPropertyFunctionList(ctx, exports, js_zlib_utils, countof(js_zlib_utils));
}
