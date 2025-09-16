#include "fs_common.h"

// Helper to get buffer data from Buffer or TypedArray
static uint8_t* get_buffer_data_fs(JSContext* ctx, JSValue obj, size_t* size) {
  // First try to get as TypedArray (for Buffer objects)
  size_t byte_offset;
  JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, obj, &byte_offset, size, NULL);
  if (!JS_IsException(array_buffer)) {
    size_t buffer_size;
    uint8_t* buffer = JS_GetArrayBuffer(ctx, &buffer_size, array_buffer);
    JS_FreeValue(ctx, array_buffer);
    if (buffer) {
      return buffer + byte_offset;
    }
  }

  // Fallback: try to get as ArrayBuffer directly
  uint8_t* buffer = JS_GetArrayBuffer(ctx, size, obj);
  return buffer;
}

// fs.readFileSync(path[, options])
JSValue js_fs_read_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  FILE* file = fopen(path, "rb");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  // Get file size
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (size < 0) {
    fclose(file);
    JSValue error = create_fs_error(ctx, errno, "stat", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  // Read file content
  char* buffer = malloc(size + 1);
  if (!buffer) {
    fclose(file);
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  size_t read_size = fread(buffer, 1, size, file);
  fclose(file);

  if (read_size != (size_t)size) {
    free(buffer);
    JSValue error = create_fs_error(ctx, errno, "read", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  buffer[size] = '\0';

  // Check encoding option - default is to return Buffer
  bool return_string = false;
  if (argc >= 2 && JS_IsObject(argv[1]) && !JS_IsNull(argv[1])) {
    JSValue encoding = JS_GetPropertyStr(ctx, argv[1], "encoding");
    if (!JS_IsUndefined(encoding) && !JS_IsNull(encoding)) {
      const char* encoding_str = JS_ToCString(ctx, encoding);
      if (encoding_str && strcmp(encoding_str, "utf8") == 0) {
        return_string = true;
      }
      if (encoding_str) {
        JS_FreeCString(ctx, encoding_str);
      }
    }
    JS_FreeValue(ctx, encoding);
  } else if (argc >= 2 && JS_IsString(argv[1])) {
    // Second argument is encoding string directly
    const char* encoding_str = JS_ToCString(ctx, argv[1]);
    if (encoding_str && strcmp(encoding_str, "utf8") == 0) {
      return_string = true;
    }
    if (encoding_str) {
      JS_FreeCString(ctx, encoding_str);
    }
  }

  JS_FreeCString(ctx, path);

  JSValue result;
  if (return_string) {
    result = JS_NewString(ctx, buffer);
  } else {
    // Default: return Buffer
    result = create_buffer_from_data(ctx, (const uint8_t*)buffer, size);
  }

  free(buffer);
  return result;
}

// fs.writeFileSync(file, data[, options])
JSValue js_fs_write_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "file and data are required");
  }

  const char* file_path = JS_ToCString(ctx, argv[0]);
  if (!file_path) {
    return JS_EXCEPTION;
  }

  // Get data to write
  size_t data_size;
  const char* data;
  bool is_string = false;

  if (JS_IsString(argv[1])) {
    data = JS_ToCStringLen(ctx, &data_size, argv[1]);
    if (!data) {
      JS_FreeCString(ctx, file_path);
      return JS_EXCEPTION;
    }
    is_string = true;
  } else {
    // Try to get as Buffer or TypedArray
    uint8_t* buffer_data = get_buffer_data_fs(ctx, argv[1], &data_size);
    if (!buffer_data) {
      JS_FreeCString(ctx, file_path);
      return JS_ThrowTypeError(ctx, "data must be string, Buffer, or TypedArray");
    }
    data = (const char*)buffer_data;
  }

  FILE* file = fopen(file_path, "wb");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", file_path);
    JS_FreeCString(ctx, file_path);
    if (is_string) {
      JS_FreeCString(ctx, data);
    }
    return JS_Throw(ctx, error);
  }

  size_t written = fwrite(data, 1, data_size, file);
  fclose(file);

  if (written != data_size) {
    JSValue error = create_fs_error(ctx, errno, "write", file_path);
    JS_FreeCString(ctx, file_path);
    if (is_string) {
      JS_FreeCString(ctx, data);
    }
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, file_path);
  if (is_string) {
    JS_FreeCString(ctx, data);
  }

  return JS_UNDEFINED;
}

// fs.appendFileSync(path, data[, options])
JSValue js_fs_append_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "path and data are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Get data to append
  size_t data_size;
  const char* data;
  bool is_string = false;

  if (JS_IsString(argv[1])) {
    data = JS_ToCStringLen(ctx, &data_size, argv[1]);
    if (!data) {
      JS_FreeCString(ctx, path);
      return JS_EXCEPTION;
    }
    is_string = true;
  } else {
    // Try to get as Buffer or TypedArray
    uint8_t* buffer_data = get_buffer_data_fs(ctx, argv[1], &data_size);
    if (!buffer_data) {
      JS_FreeCString(ctx, path);
      return JS_ThrowTypeError(ctx, "data must be string, Buffer, or TypedArray");
    }
    data = (const char*)buffer_data;
  }

  FILE* file = fopen(path, "ab");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JS_FreeCString(ctx, path);
    if (is_string) {
      JS_FreeCString(ctx, data);
    }
    return JS_Throw(ctx, error);
  }

  size_t written = fwrite(data, 1, data_size, file);
  fclose(file);

  if (written != data_size) {
    JSValue error = create_fs_error(ctx, errno, "write", path);
    JS_FreeCString(ctx, path);
    if (is_string) {
      JS_FreeCString(ctx, data);
    }
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  if (is_string) {
    JS_FreeCString(ctx, data);
  }

  return JS_UNDEFINED;
}

// fs.existsSync(path)
JSValue js_fs_exists_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int result = access(path, F_OK);
  JS_FreeCString(ctx, path);

  return JS_NewBool(ctx, result == 0);
}

// fs.unlinkSync(path)
JSValue js_fs_unlink_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  if (unlink(path) < 0) {
    JSValue error = create_fs_error(ctx, errno, "unlink", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}