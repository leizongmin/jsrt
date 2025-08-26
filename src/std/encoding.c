#include "encoding.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/dbuf.h"
#include "../util/debug.h"

// TextEncoder implementation
typedef struct {
  const char *encoding;
} JSRT_TextEncoder;

static void JSRT_TextEncoderFinalize(JSRuntime *rt, JSValue val) {
  JSRT_TextEncoder *encoder = JS_GetOpaque(val, 0);
  if (encoder) {
    free(encoder);
  }
}

static JSClassDef JSRT_TextEncoderClass = {
    .class_name = "TextEncoder",
    .finalizer = JSRT_TextEncoderFinalize,
};

static JSClassID JSRT_TextEncoderClassID;

static JSValue JSRT_TextEncoderConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  JSValue obj;
  JSRT_TextEncoder *encoder;

  encoder = malloc(sizeof(JSRT_TextEncoder));
  if (!encoder) {
    return JS_EXCEPTION;
  }

  encoder->encoding = "utf-8";  // Only UTF-8 supported for now

  obj = JS_NewObjectClass(ctx, JSRT_TextEncoderClassID);
  if (JS_IsException(obj)) {
    free(encoder);
    return JS_EXCEPTION;
  }

  JS_SetOpaque(obj, encoder);

  // Set encoding property
  JS_DefinePropertyValueStr(ctx, obj, "encoding", JS_NewString(ctx, "utf-8"), JS_PROP_C_W_E);

  return obj;
}

static JSValue JSRT_TextEncoderEncode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_TextEncoder *encoder = JS_GetOpaque2(ctx, this_val, JSRT_TextEncoderClassID);
  if (!encoder) {
    return JS_EXCEPTION;
  }

  const char *input = "";
  size_t input_len = 0;

  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    input = JS_ToCStringLen(ctx, &input_len, argv[0]);
    if (!input) {
      return JS_EXCEPTION;
    }
  }

  // Create Uint8Array with the UTF-8 bytes
  JSValue array_buffer = JS_NewArrayBufferCopy(ctx, (const uint8_t *)input, input_len);
  if (JS_IsException(array_buffer)) {
    JS_FreeCString(ctx, input);
    return JS_EXCEPTION;
  }

  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Uint8Array");
  JSValue uint8_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);
  JS_FreeValue(ctx, uint8_array_ctor);
  JS_FreeValue(ctx, array_buffer);
  JS_FreeCString(ctx, input);

  return uint8_array;
}

static JSValue JSRT_TextEncoderEncodeInto(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_TextEncoder *encoder = JS_GetOpaque2(ctx, this_val, JSRT_TextEncoderClassID);
  if (!encoder) {
    return JS_EXCEPTION;
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "encodeInto requires 2 arguments");
  }

  const char *input = "";
  size_t input_len = 0;

  if (!JS_IsUndefined(argv[0])) {
    input = JS_ToCStringLen(ctx, &input_len, argv[0]);
    if (!input) {
      return JS_EXCEPTION;
    }
  }

  // Get destination Uint8Array
  JSValue destination = argv[1];
  size_t byte_offset, byte_length;
  JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, destination, &byte_offset, &byte_length, NULL);
  if (JS_IsException(array_buffer)) {
    JS_FreeCString(ctx, input);
    return JS_ThrowTypeError(ctx, "destination must be a Uint8Array");
  }

  uint8_t *buffer = JS_GetArrayBuffer(ctx, &byte_length, array_buffer);
  if (!buffer) {
    JS_FreeCString(ctx, input);
    return JS_EXCEPTION;
  }

  // Copy UTF-8 bytes into destination buffer
  size_t bytes_written = input_len < byte_length ? input_len : byte_length;
  memcpy(buffer + byte_offset, input, bytes_written);

  JS_FreeCString(ctx, input);

  // Return result object with read and written properties
  JSValue result = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, result, "read", JS_NewUint32(ctx, input_len));
  JS_SetPropertyStr(ctx, result, "written", JS_NewUint32(ctx, bytes_written));

  return result;
}

// TextDecoder implementation
typedef struct {
  const char *encoding;
  bool fatal;
  bool ignore_bom;
} JSRT_TextDecoder;

static void JSRT_TextDecoderFinalize(JSRuntime *rt, JSValue val) {
  JSRT_TextDecoder *decoder = JS_GetOpaque(val, 0);
  if (decoder) {
    free(decoder);
  }
}

static JSClassDef JSRT_TextDecoderClass = {
    .class_name = "TextDecoder",
    .finalizer = JSRT_TextDecoderFinalize,
};

static JSClassID JSRT_TextDecoderClassID;

static JSValue JSRT_TextDecoderConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  JSValue obj;
  JSRT_TextDecoder *decoder;

  decoder = malloc(sizeof(JSRT_TextDecoder));
  if (!decoder) {
    return JS_EXCEPTION;
  }

  decoder->encoding = "utf-8";  // Only UTF-8 supported for now
  decoder->fatal = false;
  decoder->ignore_bom = false;

  // Parse options if provided
  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue fatal_val = JS_GetPropertyStr(ctx, argv[1], "fatal");
    if (!JS_IsUndefined(fatal_val)) {
      decoder->fatal = JS_ToBool(ctx, fatal_val);
    }
    JS_FreeValue(ctx, fatal_val);

    JSValue ignore_bom_val = JS_GetPropertyStr(ctx, argv[1], "ignoreBOM");
    if (!JS_IsUndefined(ignore_bom_val)) {
      decoder->ignore_bom = JS_ToBool(ctx, ignore_bom_val);
    }
    JS_FreeValue(ctx, ignore_bom_val);
  }

  obj = JS_NewObjectClass(ctx, JSRT_TextDecoderClassID);
  if (JS_IsException(obj)) {
    free(decoder);
    return JS_EXCEPTION;
  }

  JS_SetOpaque(obj, decoder);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "encoding", JS_NewString(ctx, "utf-8"), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, obj, "fatal", JS_NewBool(ctx, decoder->fatal), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, obj, "ignoreBOM", JS_NewBool(ctx, decoder->ignore_bom), JS_PROP_C_W_E);

  return obj;
}

static JSValue JSRT_TextDecoderDecode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_TextDecoder *decoder = JS_GetOpaque2(ctx, this_val, JSRT_TextDecoderClassID);
  if (!decoder) {
    return JS_EXCEPTION;
  }

  if (argc == 0 || JS_IsUndefined(argv[0])) {
    return JS_NewString(ctx, "");
  }

  uint8_t *input_data = NULL;
  size_t input_len = 0;

  // Try to get as ArrayBuffer first
  input_data = JS_GetArrayBuffer(ctx, &input_len, argv[0]);

  if (input_data) {
    // ArrayBuffer path
    if (input_len == 0) {
      return JS_NewString(ctx, "");
    }

    // Skip BOM if not ignoring it and present
    size_t start_pos = 0;
    if (!decoder->ignore_bom && input_len >= 3 && input_data[0] == 0xEF && input_data[1] == 0xBB &&
        input_data[2] == 0xBF) {
      start_pos = 3;
    }

    // For UTF-8, we can directly create a string from the bytes
    // QuickJS expects valid UTF-8, so we'll validate if fatal is true
    if (decoder->fatal) {
      // Validate UTF-8 sequence
      const uint8_t *p = input_data + start_pos;
      const uint8_t *end = input_data + input_len;
      while (p < end) {
        const uint8_t *p_next;
        int c = unicode_from_utf8(p, end - p, &p_next);
        if (c < 0) {
          return JS_ThrowTypeError(ctx, "Invalid UTF-8 sequence");
        }
        p = p_next;
      }
    }

    return JS_NewStringLen(ctx, (const char *)(input_data + start_pos), input_len - start_pos);
  }

  // Try to get as typed array - testing just the call
  size_t byte_offset, bytes_per_element;
  JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[0], &byte_offset, &input_len, &bytes_per_element);
  if (JS_IsException(array_buffer)) {
    return JS_ThrowTypeError(ctx, "input must be an ArrayBuffer or typed array");
  }

  if (input_len == 0) {
    JS_FreeValue(ctx, array_buffer);
    return JS_NewString(ctx, "");
  }

  size_t buffer_size;
  uint8_t *buffer = JS_GetArrayBuffer(ctx, &buffer_size, array_buffer);
  if (!buffer) {
    JS_FreeValue(ctx, array_buffer);
    return JS_ThrowTypeError(ctx, "Failed to get buffer from typed array");
  }

  input_data = buffer + byte_offset;

  // Skip BOM if not ignoring it and present
  size_t start_pos = 0;
  if (!decoder->ignore_bom && input_len >= 3 && input_data[0] == 0xEF && input_data[1] == 0xBB &&
      input_data[2] == 0xBF) {
    start_pos = 3;
  }

  // For UTF-8, we can directly create a string from the bytes
  // QuickJS expects valid UTF-8, so we'll validate if fatal is true
  if (decoder->fatal) {
    // Validate UTF-8 sequence
    const uint8_t *p = input_data + start_pos;
    const uint8_t *end = input_data + input_len;
    while (p < end) {
      const uint8_t *p_next;
      int c = unicode_from_utf8(p, end - p, &p_next);
      if (c < 0) {
        JS_FreeValue(ctx, array_buffer);
        return JS_ThrowTypeError(ctx, "Invalid UTF-8 sequence");
      }
      p = p_next;
    }
  }

  // Create string from the buffer
  JSValue result = JS_NewStringLen(ctx, (const char *)(input_data + start_pos), input_len - start_pos);
  JS_FreeValue(ctx, array_buffer);
  return result;
}

// Setup functions
void JSRT_RuntimeSetupStdEncoding(JSRT_Runtime *rt) {
  JSContext *ctx = rt->ctx;

  // Register TextEncoder class
  JS_NewClassID(&JSRT_TextEncoderClassID);
  JS_NewClass(rt->rt, JSRT_TextEncoderClassID, &JSRT_TextEncoderClass);

  JSValue text_encoder_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, text_encoder_proto, "encode", JS_NewCFunction(ctx, JSRT_TextEncoderEncode, "encode", 1));
  JS_SetPropertyStr(ctx, text_encoder_proto, "encodeInto",
                    JS_NewCFunction(ctx, JSRT_TextEncoderEncodeInto, "encodeInto", 2));
  JS_SetClassProto(ctx, JSRT_TextEncoderClassID, text_encoder_proto);

  JSValue text_encoder_ctor =
      JS_NewCFunction2(ctx, JSRT_TextEncoderConstructor, "TextEncoder", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "TextEncoder", text_encoder_ctor);

  // Register TextDecoder class
  JS_NewClassID(&JSRT_TextDecoderClassID);
  JS_NewClass(rt->rt, JSRT_TextDecoderClassID, &JSRT_TextDecoderClass);

  JSValue text_decoder_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, text_decoder_proto, "decode", JS_NewCFunction(ctx, JSRT_TextDecoderDecode, "decode", 1));
  JS_SetClassProto(ctx, JSRT_TextDecoderClassID, text_decoder_proto);

  JSValue text_decoder_ctor =
      JS_NewCFunction2(ctx, JSRT_TextDecoderConstructor, "TextDecoder", 2, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "TextDecoder", text_decoder_ctor);

  JSRT_Debug("Encoding API setup completed");
}