#include "fs_common.h"

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

  // Check if options specify Buffer return
  bool return_buffer = false;
  if (argc >= 2 && JS_IsObject(argv[1]) && !JS_IsNull(argv[1])) {
    JSValue encoding = JS_GetPropertyStr(ctx, argv[1], "encoding");
    if (!JS_IsUndefined(encoding) && !JS_IsNull(encoding)) {
      const char* encoding_str = JS_ToCString(ctx, encoding);
      if (encoding_str && strcmp(encoding_str, "buffer") == 0) {
        return_buffer = true;
      }
      if (encoding_str) {
        JS_FreeCString(ctx, encoding_str);
      }
    }
    JS_FreeValue(ctx, encoding);
  }

  JS_FreeCString(ctx, path);

  JSValue result;
  if (return_buffer) {
    result = create_buffer_from_data(ctx, (const uint8_t*)buffer, size);
  } else {
    result = JS_NewString(ctx, buffer);
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
  JSValue buffer_val = JS_UNDEFINED;

  if (JS_IsString(argv[1])) {
    data = JS_ToCStringLen(ctx, &data_size, argv[1]);
    if (!data) {
      JS_FreeCString(ctx, file_path);
      return JS_EXCEPTION;
    }
  } else {
    // Assume it's a Buffer or ArrayBuffer
    buffer_val = argv[1];
    data = (const char*)JS_GetArrayBuffer(ctx, &data_size, buffer_val);
    if (!data) {
      JS_FreeCString(ctx, file_path);
      return JS_ThrowTypeError(ctx, "data must be a string or Buffer");
    }
  }

  FILE* file = fopen(file_path, "wb");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", file_path);
    JS_FreeCString(ctx, file_path);
    if (JS_IsString(argv[1])) {
      JS_FreeCString(ctx, data);
    }
    return JS_Throw(ctx, error);
  }

  size_t written = fwrite(data, 1, data_size, file);
  fclose(file);

  if (written != data_size) {
    JSValue error = create_fs_error(ctx, errno, "write", file_path);
    JS_FreeCString(ctx, file_path);
    if (JS_IsString(argv[1])) {
      JS_FreeCString(ctx, data);
    }
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, file_path);
  if (JS_IsString(argv[1])) {
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
  JSValue buffer_val = JS_UNDEFINED;

  if (JS_IsString(argv[1])) {
    data = JS_ToCStringLen(ctx, &data_size, argv[1]);
    if (!data) {
      JS_FreeCString(ctx, path);
      return JS_EXCEPTION;
    }
  } else {
    // Assume it's a Buffer or ArrayBuffer
    buffer_val = argv[1];
    data = (const char*)JS_GetArrayBuffer(ctx, &data_size, buffer_val);
    if (!data) {
      JS_FreeCString(ctx, path);
      return JS_ThrowTypeError(ctx, "data must be a string or Buffer");
    }
  }

  FILE* file = fopen(path, "ab");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JS_FreeCString(ctx, path);
    if (JS_IsString(argv[1])) {
      JS_FreeCString(ctx, data);
    }
    return JS_Throw(ctx, error);
  }

  size_t written = fwrite(data, 1, data_size, file);
  fclose(file);

  if (written != data_size) {
    JSValue error = create_fs_error(ctx, errno, "write", path);
    JS_FreeCString(ctx, path);
    if (JS_IsString(argv[1])) {
      JS_FreeCString(ctx, data);
    }
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);  
  if (JS_IsString(argv[1])) {
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