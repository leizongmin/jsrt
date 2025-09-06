#include <stdlib.h>
#include <string.h>
#include "../util/debug.h"
#include "node_modules.h"

// Forward declarations
static JSValue js_buffer_to_string(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Helper to get buffer data from JSValue
static uint8_t* get_buffer_data(JSContext* ctx, JSValue obj, size_t* size) {
  size_t byte_offset;
  JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, obj, &byte_offset, size, NULL);
  if (JS_IsException(array_buffer)) {
    return NULL;
  }

  size_t buffer_size;
  uint8_t* buffer = JS_GetArrayBuffer(ctx, &buffer_size, array_buffer);
  JS_FreeValue(ctx, array_buffer);

  if (!buffer) {
    return NULL;
  }

  return buffer + byte_offset;
}

// Helper to create Uint8Array from ArrayBuffer with Buffer methods
static JSValue create_uint8_array(JSContext* ctx, JSValue array_buffer) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  JSValue uint8_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);

  // Add Buffer-specific methods to the Uint8Array instance
  if (!JS_IsException(uint8_array)) {
    // Add toString method that converts bytes to string
    JSValue toString_func = JS_NewCFunction(ctx, js_buffer_to_string, "toString", 1);
    JS_SetPropertyStr(ctx, uint8_array, "toString", toString_func);
  }

  JS_FreeValue(ctx, uint8_array_ctor);
  JS_FreeValue(ctx, global);
  return uint8_array;
}

// Buffer.prototype.toString([encoding])
static JSValue js_buffer_to_string(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Get buffer data
  size_t buffer_size;
  uint8_t* buffer_data = get_buffer_data(ctx, this_val, &buffer_size);

  if (!buffer_data) {
    return JS_ThrowTypeError(ctx, "Invalid buffer object");
  }

  // For now, just convert bytes to UTF-8 string (ignoring encoding parameter)
  return JS_NewStringLen(ctx, (const char*)buffer_data, buffer_size);
}

// Buffer.alloc(size[, fill[, encoding]])
static JSValue js_buffer_alloc(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Buffer.alloc() requires at least 1 argument");
  }

  int32_t size;
  if (JS_ToInt32(ctx, &size, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  if (size < 0) {
    return JS_ThrowRangeError(ctx, "Invalid buffer size");
  }

  // Create ArrayBuffer with zeros
  uint8_t* data = malloc(size);
  if (size > 0 && !data) {
    return JS_ThrowOutOfMemory(ctx);
  }
  memset(data, 0, size);

  JSValue buffer = JS_NewArrayBuffer(ctx, data, size, NULL, NULL, false);
  if (JS_IsException(buffer)) {
    free(data);
    return JS_EXCEPTION;
  }

  JSValue uint8_array = create_uint8_array(ctx, buffer);
  JS_FreeValue(ctx, buffer);

  // Fill buffer if fill value provided
  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    size_t buf_size;
    uint8_t* buf_data = get_buffer_data(ctx, uint8_array, &buf_size);

    if (buf_data && buf_size > 0) {
      if (JS_IsString(argv[1])) {
        const char* fill_str = JS_ToCString(ctx, argv[1]);
        if (fill_str) {
          size_t fill_len = strlen(fill_str);
          if (fill_len > 0) {
            for (size_t i = 0; i < buf_size; i++) {
              buf_data[i] = fill_str[i % fill_len];
            }
          }
          JS_FreeCString(ctx, fill_str);
        }
      } else {
        int32_t fill_val;
        if (JS_ToInt32(ctx, &fill_val, argv[1]) >= 0) {
          memset(buf_data, fill_val & 0xFF, buf_size);
        }
      }
    }
  }

  return uint8_array;
}

// Buffer.allocUnsafe(size)
static JSValue js_buffer_alloc_unsafe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Buffer.allocUnsafe() requires 1 argument");
  }

  int32_t size;
  if (JS_ToInt32(ctx, &size, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  if (size < 0) {
    return JS_ThrowRangeError(ctx, "Invalid buffer size");
  }

  // Create ArrayBuffer without initializing memory
  uint8_t* data = malloc(size);
  if (size > 0 && !data) {
    return JS_ThrowOutOfMemory(ctx);
  }

  JSValue buffer = JS_NewArrayBuffer(ctx, data, size, NULL, NULL, false);
  if (JS_IsException(buffer)) {
    free(data);
    return JS_EXCEPTION;
  }

  JSValue uint8_array = create_uint8_array(ctx, buffer);
  JS_FreeValue(ctx, buffer);

  return uint8_array;
}

// Buffer.from(array)
// Buffer.from(string[, encoding])
static JSValue js_buffer_from(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Buffer.from() requires at least 1 argument");
  }

  JSValue arg = argv[0];

  // Handle string input
  if (JS_IsString(arg)) {
    const char* str = JS_ToCString(ctx, arg);
    if (!str) {
      return JS_EXCEPTION;
    }

    size_t str_len = strlen(str);
    JSValue buffer = JS_NewArrayBufferCopy(ctx, (const uint8_t*)str, str_len);
    JS_FreeCString(ctx, str);

    if (JS_IsException(buffer)) {
      return JS_EXCEPTION;
    }

    JSValue uint8_array = create_uint8_array(ctx, buffer);
    JS_FreeValue(ctx, buffer);
    return uint8_array;
  }

  // Handle array input
  if (JS_IsArray(ctx, arg)) {
    JSValue length_val = JS_GetPropertyStr(ctx, arg, "length");
    int32_t length;
    if (JS_ToInt32(ctx, &length, length_val) < 0) {
      JS_FreeValue(ctx, length_val);
      return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, length_val);

    if (length < 0) {
      return JS_ThrowRangeError(ctx, "Invalid array length");
    }

    uint8_t* data = malloc(length);
    if (length > 0 && !data) {
      return JS_ThrowOutOfMemory(ctx);
    }

    for (int32_t i = 0; i < length; i++) {
      JSValue item = JS_GetPropertyUint32(ctx, arg, i);
      int32_t val;
      if (JS_ToInt32(ctx, &val, item) >= 0) {
        data[i] = val & 0xFF;
      } else {
        data[i] = 0;
      }
      JS_FreeValue(ctx, item);
    }

    JSValue buffer = JS_NewArrayBuffer(ctx, data, length, NULL, NULL, false);
    if (JS_IsException(buffer)) {
      free(data);
      return JS_EXCEPTION;
    }

    JSValue uint8_array = create_uint8_array(ctx, buffer);
    JS_FreeValue(ctx, buffer);
    return uint8_array;
  }

  // Handle Uint8Array or other TypedArray input
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  if (JS_IsInstanceOf(ctx, arg, uint8_array_ctor) > 0) {
    // It's already a Uint8Array, just return it as is (or duplicate it)
    JS_FreeValue(ctx, uint8_array_ctor);
    JS_FreeValue(ctx, global);
    return JS_DupValue(ctx, arg);
  }
  JS_FreeValue(ctx, uint8_array_ctor);
  JS_FreeValue(ctx, global);

  // Handle ArrayBuffer input
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue array_buffer_ctor = JS_GetPropertyStr(ctx, global_obj, "ArrayBuffer");
  if (JS_IsInstanceOf(ctx, arg, array_buffer_ctor) > 0) {
    JSValue uint8_array = create_uint8_array(ctx, JS_DupValue(ctx, arg));
    JS_FreeValue(ctx, array_buffer_ctor);
    JS_FreeValue(ctx, global_obj);
    return uint8_array;
  }
  JS_FreeValue(ctx, array_buffer_ctor);
  JS_FreeValue(ctx, global_obj);

  return JS_ThrowTypeError(ctx, "Buffer.from() argument must be a string, array, ArrayBuffer, or TypedArray");
}

// Buffer.isBuffer(obj)
static JSValue js_buffer_is_buffer(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }

  // Check if it's a Uint8Array (our Buffer implementation)
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  JS_FreeValue(ctx, global);

  if (!JS_IsException(uint8_array_ctor) && !JS_IsUndefined(uint8_array_ctor)) {
    int is_uint8_array = JS_IsInstanceOf(ctx, argv[0], uint8_array_ctor);
    JS_FreeValue(ctx, uint8_array_ctor);
    return JS_NewBool(ctx, is_uint8_array > 0);
  }

  JS_FreeValue(ctx, uint8_array_ctor);
  return JS_FALSE;
}

// Buffer constructor function
static JSValue js_buffer_constructor(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Buffer constructor requires at least 1 argument");
  }

  // For now, delegate to Buffer.from
  return js_buffer_from(ctx, this_val, argc, argv);
}

// Buffer.concat(list[, totalLength])
static JSValue js_buffer_concat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Buffer.concat() requires at least 1 argument");
  }

  if (!JS_IsArray(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "First argument must be an array");
  }

  JSValue array = argv[0];
  JSValue length_val = JS_GetPropertyStr(ctx, array, "length");
  int32_t array_length;
  if (JS_ToInt32(ctx, &array_length, length_val) < 0) {
    JS_FreeValue(ctx, length_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length_val);

  // Calculate total length if not provided
  size_t total_length = 0;
  bool length_provided = false;

  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    int32_t provided_length;
    if (JS_ToInt32(ctx, &provided_length, argv[1]) >= 0 && provided_length >= 0) {
      total_length = provided_length;
      length_provided = true;
    }
  }

  if (!length_provided) {
    for (int32_t i = 0; i < array_length; i++) {
      JSValue item = JS_GetPropertyUint32(ctx, array, i);
      size_t item_size;
      uint8_t* item_data = get_buffer_data(ctx, item, &item_size);
      if (item_data) {
        total_length += item_size;
      }
      JS_FreeValue(ctx, item);
    }
  }

  // Create result buffer
  uint8_t* result_data = malloc(total_length);
  if (total_length > 0 && !result_data) {
    return JS_ThrowOutOfMemory(ctx);
  }

  size_t offset = 0;
  for (int32_t i = 0; i < array_length && offset < total_length; i++) {
    JSValue item = JS_GetPropertyUint32(ctx, array, i);
    size_t item_size;
    uint8_t* item_data = get_buffer_data(ctx, item, &item_size);

    if (item_data) {
      size_t copy_size = (offset + item_size > total_length) ? (total_length - offset) : item_size;
      memcpy(result_data + offset, item_data, copy_size);
      offset += copy_size;
    }
    JS_FreeValue(ctx, item);
  }

  JSValue result_buffer = JS_NewArrayBuffer(ctx, result_data, total_length, NULL, NULL, false);
  if (JS_IsException(result_buffer)) {
    free(result_data);
    return JS_EXCEPTION;
  }

  JSValue uint8_array = create_uint8_array(ctx, result_buffer);
  JS_FreeValue(ctx, result_buffer);
  return uint8_array;
}

// CommonJS module export
JSValue JSRT_InitNodeBuffer(JSContext* ctx) {
  JSValue buffer_obj = JS_NewObject(ctx);

  // Create Buffer constructor function
  JSValue Buffer = JS_NewCFunction2(ctx, js_buffer_constructor, "Buffer", 1, JS_CFUNC_constructor, 0);

  // Static methods on Buffer
  JS_SetPropertyStr(ctx, Buffer, "alloc", JS_NewCFunction(ctx, js_buffer_alloc, "alloc", 3));
  JS_SetPropertyStr(ctx, Buffer, "allocUnsafe", JS_NewCFunction(ctx, js_buffer_alloc_unsafe, "allocUnsafe", 1));
  JS_SetPropertyStr(ctx, Buffer, "from", JS_NewCFunction(ctx, js_buffer_from, "from", 2));
  JS_SetPropertyStr(ctx, Buffer, "isBuffer", JS_NewCFunction(ctx, js_buffer_is_buffer, "isBuffer", 1));
  JS_SetPropertyStr(ctx, Buffer, "concat", JS_NewCFunction(ctx, js_buffer_concat, "concat", 2));

  // Export Buffer as both named and default export for CommonJS
  JS_SetPropertyStr(ctx, buffer_obj, "Buffer", JS_DupValue(ctx, Buffer));
  JS_SetPropertyStr(ctx, buffer_obj, "default", Buffer);

  return buffer_obj;
}

// ES Module initialization
int js_node_buffer_init(JSContext* ctx, JSModuleDef* m) {
  JSValue buffer_module = JSRT_InitNodeBuffer(ctx);

  // Get the Buffer constructor from the module
  JSValue Buffer = JS_GetPropertyStr(ctx, buffer_module, "Buffer");

  // Export Buffer as both named and default export for ES modules
  JS_SetModuleExport(ctx, m, "Buffer", JS_DupValue(ctx, Buffer));
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, Buffer));

  JS_FreeValue(ctx, Buffer);
  JS_FreeValue(ctx, buffer_module);
  return 0;
}