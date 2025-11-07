#include <string.h>
#include "../runtime.h"
#include "../std/assert.h"
#include "../util/debug.h"
#include "node_modules.h"

// StringDecoder implementation - decodes Buffer objects to strings
// This is a minimal implementation focused on npm package compatibility

// Forward declarations
static JSValue js_string_decoder_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_string_decoder_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_string_decoder_text(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_string_decoder_fillLast(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// StringDecoder structure
typedef struct {
  char* remaining;
  size_t remaining_len;
  char* charBuffer;
  size_t charBufferLen;
  size_t charBufferReceived;
  int lastChar;
  int lastNeed;
} StringDecoder;

// StringDecoder constructor
static JSValue js_string_decoder_ctor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue encoding = JS_UNDEFINED;
  const char* encoding_str = "utf8";

  if (argc > 0) {
    encoding = argv[0];
    if (!JS_IsUndefined(encoding) && !JS_IsNull(encoding)) {
      encoding_str = JS_ToCString(ctx, encoding);
      if (!encoding_str) {
        return JS_EXCEPTION;
      }
    }
  }

  // Create StringDecoder object
  StringDecoder* decoder = js_malloc(ctx, sizeof(StringDecoder));
  if (!decoder) {
    if (!JS_IsUndefined(encoding)) {
      JS_FreeCString(ctx, encoding_str);
    }
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize decoder
  memset(decoder, 0, sizeof(StringDecoder));

  // Set encoding-specific properties
  JSValue obj = JS_NewObjectClass(ctx, 1);
  if (JS_IsException(obj)) {
    js_free(ctx, decoder);
    if (!JS_IsUndefined(encoding)) {
      JS_FreeCString(ctx, encoding_str);
    }
    return obj;
  }

  // Store decoder in object
  JS_SetOpaque(obj, decoder);

  // Add encoding property
  JS_SetPropertyStr(ctx, obj, "encoding", JS_NewString(ctx, encoding_str));

  // Set methods on the StringDecoder object
  JS_SetPropertyStr(ctx, obj, "write", JS_NewCFunction(ctx, js_string_decoder_write, "write", 3));
  JS_SetPropertyStr(ctx, obj, "end", JS_NewCFunction(ctx, js_string_decoder_end, "end", 0));
  JS_SetPropertyStr(ctx, obj, "text", JS_NewCFunction(ctx, js_string_decoder_text, "text", 3));
  JS_SetPropertyStr(ctx, obj, "fillLast", JS_NewCFunction(ctx, js_string_decoder_fillLast, "fillLast", 0));

  if (!JS_IsUndefined(encoding)) {
    JS_FreeCString(ctx, encoding_str);
  }

  return obj;
}

// StringDecoder finalizer
static void js_string_decoder_finalizer(JSRuntime* rt, JSValue val) {
  StringDecoder* decoder = JS_GetOpaque(val, 1);
  if (decoder) {
    if (decoder->remaining) {
      js_free_rt(rt, decoder->remaining);
    }
    if (decoder->charBuffer) {
      js_free_rt(rt, decoder->charBuffer);
    }
    js_free_rt(rt, decoder);
  }
}

// Decode buffer to string
static JSValue js_string_decoder_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue buffer;
  int32_t start = 0;
  int32_t end = -1;

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "write() requires at least one argument");
  }

  buffer = argv[0];

  // Handle Buffer or Uint8Array
  if (!JS_IsObject(buffer)) {
    return JS_ThrowTypeError(ctx, "First argument must be a Buffer or Uint8Array");
  }

  if (argc > 1) {
    if (JS_ToInt32(ctx, &start, argv[1])) {
      return JS_EXCEPTION;
    }
  }

  if (argc > 2) {
    if (JS_ToInt32(ctx, &end, argv[2])) {
      return JS_EXCEPTION;
    }
  }

  // Get buffer data
  size_t buf_size;
  uint8_t* buf_data = JS_GetArrayBuffer(ctx, &buf_size, JS_GetPropertyStr(ctx, buffer, "buffer"));
  if (!buf_data) {
    return JS_ThrowTypeError(ctx, "Unable to get buffer data");
  }

  // Get byteOffset and length
  int32_t byteOffset, length;
  if (JS_ToInt32(ctx, &byteOffset, JS_GetPropertyStr(ctx, buffer, "byteOffset"))) {
    byteOffset = 0;
  }
  if (JS_ToInt32(ctx, &length, JS_GetPropertyStr(ctx, buffer, "length"))) {
    length = buf_size - byteOffset;
  }

  // Adjust for start and end
  if (end == -1) {
    end = length;
  }

  // Clamp values
  start = (start < 0) ? 0 : (start > length) ? length : start;
  end = (end < 0) ? 0 : (end > length) ? length : end;

  if (start >= end) {
    return JS_NewString(ctx, "");
  }

  // Convert to string (simplified - just decode as UTF8)
  const char* data = (const char*)buf_data + byteOffset + start;
  size_t len = end - start;

  return JS_NewStringLen(ctx, data, len);
}

// End decoding and return any remaining bytes
static JSValue js_string_decoder_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  StringDecoder* decoder = JS_GetOpaque(this_val, 1);
  if (!decoder) {
    return JS_ThrowTypeError(ctx, "Invalid StringDecoder object");
  }

  // Return any remaining bytes as string
  if (decoder->remaining && decoder->remaining_len > 0) {
    JSValue result = JS_NewStringLen(ctx, decoder->remaining, decoder->remaining_len);
    // Clear remaining data
    js_free(ctx, decoder->remaining);
    decoder->remaining = NULL;
    decoder->remaining_len = 0;
    return result;
  }

  return JS_NewString(ctx, "");
}

// Get text from buffer (Node.js 12+ method)
static JSValue js_string_decoder_text(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue buffer;
  int32_t start = 0;
  int32_t end = -1;

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "text() requires at least one argument");
  }

  buffer = argv[0];

  if (argc > 1) {
    if (JS_ToInt32(ctx, &start, argv[1])) {
      return JS_EXCEPTION;
    }
  }

  if (argc > 2) {
    if (JS_ToInt32(ctx, &end, argv[2])) {
      return JS_EXCEPTION;
    }
  }

  // Similar to write but simpler - just decode and return
  return js_string_decoder_write(ctx, this_val, argc, argv);
}

// Fill the buffer (Node.js 12+ method)
static JSValue js_string_decoder_fillLast(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  StringDecoder* decoder = JS_GetOpaque(this_val, 1);
  if (!decoder) {
    return JS_ThrowTypeError(ctx, "Invalid StringDecoder object");
  }

  // This is a no-op in this simplified implementation
  return JS_UNDEFINED;
}

// StringDecoder module initialization (CommonJS)
JSValue JSRT_InitNodeStringDecoder(JSContext* ctx) {
  JSValue string_decoder_obj = JS_NewObject(ctx);

  // StringDecoder constructor
  JSValue ctor = JS_NewCFunction2(ctx, js_string_decoder_ctor, "StringDecoder", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, string_decoder_obj, "StringDecoder", ctor);

  // StringDecoder prototype methods
  JSValue proto = JS_GetPropertyStr(ctx, ctor, "prototype");
  JS_SetPropertyStr(ctx, proto, "write", JS_NewCFunction(ctx, js_string_decoder_write, "write", 3));
  JS_SetPropertyStr(ctx, proto, "end", JS_NewCFunction(ctx, js_string_decoder_end, "end", 0));
  JS_SetPropertyStr(ctx, proto, "text", JS_NewCFunction(ctx, js_string_decoder_text, "text", 3));
  JS_SetPropertyStr(ctx, proto, "fillLast", JS_NewCFunction(ctx, js_string_decoder_fillLast, "fillLast", 0));
  JS_FreeValue(ctx, proto);

  return string_decoder_obj;
}

// StringDecoder module initialization (ES Module)
int js_node_string_decoder_init(JSContext* ctx, JSModuleDef* m) {
  JSValue string_decoder_obj = JSRT_InitNodeStringDecoder(ctx);

  // Add exports
  JS_SetModuleExport(ctx, m, "StringDecoder", JS_GetPropertyStr(ctx, string_decoder_obj, "StringDecoder"));
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, string_decoder_obj));

  JS_FreeValue(ctx, string_decoder_obj);
  return 0;
}