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

  // Enhanced Buffer integration - check for encoding options
  JSValue result;
  bool return_string = false;

  if (argc > 1) {
    if (JS_IsString(argv[1])) {
      // Second argument is a string encoding (e.g., 'utf8')
      return_string = true;
    } else if (JS_IsObject(argv[1])) {
      // Second argument is an options object
      JSValue encoding = JS_GetPropertyStr(ctx, argv[1], "encoding");
      if (JS_IsString(encoding)) {
        return_string = true;
      }
      JS_FreeValue(ctx, encoding);
    }
  }

  if (return_string) {
    // Return string for text data when encoding is specified
    result = JS_NewStringLen(ctx, buffer, size);
  } else {
    // Return Buffer for binary data when no encoding specified
    result = create_buffer_from_data(ctx, (const uint8_t*)buffer, size);
  }

  free(buffer);
  JS_FreeCString(ctx, path);

  return result;
}

// fs.writeFileSync(file, data[, options])
JSValue js_fs_write_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "file and data are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  const uint8_t* data;
  size_t data_len;
  bool is_binary = false;

  if (JS_IsString(argv[1])) {
    // String data
    const char* str_data = JS_ToCStringLen(ctx, &data_len, argv[1]);
    if (!str_data) {
      JS_FreeCString(ctx, path);
      return JS_EXCEPTION;
    }
    data = (const uint8_t*)str_data;
  } else {
    // Enhanced Buffer handling - check if it's a TypedArray (Buffer)
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");

    if (JS_IsInstanceOf(ctx, argv[1], uint8_array_ctor) > 0) {
      // It's a Buffer/Uint8Array - get binary data directly
      size_t byte_offset;
      JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[1], &byte_offset, &data_len, NULL);
      if (JS_IsException(array_buffer)) {
        JS_FreeValue(ctx, uint8_array_ctor);
        JS_FreeValue(ctx, global);
        JS_FreeCString(ctx, path);
        return JS_EXCEPTION;
      }

      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, array_buffer);
      JS_FreeValue(ctx, array_buffer);

      if (!buffer_data) {
        JS_FreeValue(ctx, uint8_array_ctor);
        JS_FreeValue(ctx, global);
        JS_FreeCString(ctx, path);
        return JS_ThrowTypeError(ctx, "Failed to get buffer data");
      }

      data = buffer_data + byte_offset;
      is_binary = true;
    } else {
      // For non-Buffer, non-TypedArray objects, reject
      JS_FreeValue(ctx, uint8_array_ctor);
      JS_FreeValue(ctx, global);
      JS_FreeCString(ctx, path);
      return JS_ThrowTypeError(ctx, "data must be string, Buffer, or TypedArray");
    }

    JS_FreeValue(ctx, uint8_array_ctor);
    JS_FreeValue(ctx, global);
  }

  FILE* file = fopen(path, "wb");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JS_FreeCString(ctx, path);
    if (!is_binary) {
      JS_FreeCString(ctx, (const char*)data);
    }
    return JS_Throw(ctx, error);
  }

  size_t written = fwrite(data, 1, data_len, file);
  fclose(file);

  if (written != data_len) {
    JSValue error = create_fs_error(ctx, errno, "write", path);
    JS_FreeCString(ctx, path);
    if (!is_binary) {
      JS_FreeCString(ctx, (const char*)data);
    }
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  if (!is_binary) {
    JS_FreeCString(ctx, (const char*)data);
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

  if (unlink(path) != 0) {
    JSValue error = create_fs_error(ctx, errno, "unlink", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
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

  const uint8_t* data;
  size_t data_len;
  bool is_binary = false;

  if (JS_IsString(argv[1])) {
    // String data
    const char* str_data = JS_ToCStringLen(ctx, &data_len, argv[1]);
    if (!str_data) {
      JS_FreeCString(ctx, path);
      return JS_EXCEPTION;
    }
    data = (const uint8_t*)str_data;
  } else {
    // Enhanced Buffer handling - check if it's a TypedArray (Buffer)
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");

    if (JS_IsInstanceOf(ctx, argv[1], uint8_array_ctor) > 0) {
      // It's a Buffer/Uint8Array - get binary data directly
      size_t byte_offset;
      JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[1], &byte_offset, &data_len, NULL);
      if (JS_IsException(array_buffer)) {
        JS_FreeValue(ctx, uint8_array_ctor);
        JS_FreeValue(ctx, global);
        JS_FreeCString(ctx, path);
        return JS_EXCEPTION;
      }

      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, array_buffer);
      JS_FreeValue(ctx, array_buffer);

      if (!buffer_data) {
        JS_FreeValue(ctx, uint8_array_ctor);
        JS_FreeValue(ctx, global);
        JS_FreeCString(ctx, path);
        return JS_ThrowTypeError(ctx, "Failed to get buffer data");
      }

      data = buffer_data + byte_offset;
      is_binary = true;
    } else {
      // For non-Buffer, non-TypedArray objects, reject
      JS_FreeValue(ctx, uint8_array_ctor);
      JS_FreeValue(ctx, global);
      JS_FreeCString(ctx, path);
      return JS_ThrowTypeError(ctx, "data must be string, Buffer, or TypedArray");
    }

    JS_FreeValue(ctx, uint8_array_ctor);
    JS_FreeValue(ctx, global);
  }

  // Open file in append mode
  FILE* file = fopen(path, "ab");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JS_FreeCString(ctx, path);
    if (!is_binary) {
      JS_FreeCString(ctx, (const char*)data);
    }
    return JS_Throw(ctx, error);
  }

  size_t written = fwrite(data, 1, data_len, file);
  fclose(file);

  if (written != data_len) {
    JSValue error = create_fs_error(ctx, errno, "write", path);
    JS_FreeCString(ctx, path);
    if (!is_binary) {
      JS_FreeCString(ctx, (const char*)data);
    }
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  if (!is_binary) {
    JS_FreeCString(ctx, (const char*)data);
  }

  return JS_UNDEFINED;
}

// fs.copyFileSync(src, dest[, mode])
JSValue js_fs_copy_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "src and dest are required");
  }

  const char* src = JS_ToCString(ctx, argv[0]);
  if (!src) {
    return JS_EXCEPTION;
  }

  const char* dest = JS_ToCString(ctx, argv[1]);
  if (!dest) {
    JS_FreeCString(ctx, src);
    return JS_EXCEPTION;
  }

  // Open source file for reading
  FILE* src_file = fopen(src, "rb");
  if (!src_file) {
    JSValue error = create_fs_error(ctx, errno, "open", src);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, error);
  }

  // Open destination file for writing
  FILE* dest_file = fopen(dest, "wb");
  if (!dest_file) {
    fclose(src_file);
    JSValue error = create_fs_error(ctx, errno, "open", dest);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, error);
  }

  // Copy file content in chunks
  char buffer[8192];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
    size_t bytes_written = fwrite(buffer, 1, bytes_read, dest_file);
    if (bytes_written != bytes_read) {
      fclose(src_file);
      fclose(dest_file);
      JSValue error = create_fs_error(ctx, errno, "write", dest);
      JS_FreeCString(ctx, src);
      JS_FreeCString(ctx, dest);
      return JS_Throw(ctx, error);
    }
  }

  // Check for read errors
  if (ferror(src_file)) {
    fclose(src_file);
    fclose(dest_file);
    JSValue error = create_fs_error(ctx, errno, "read", src);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, error);
  }

  fclose(src_file);
  fclose(dest_file);

  JS_FreeCString(ctx, src);
  JS_FreeCString(ctx, dest);

  return JS_UNDEFINED;
}

// fs.renameSync(oldPath, newPath)
JSValue js_fs_rename_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "oldPath and newPath are required");
  }

  const char* old_path = JS_ToCString(ctx, argv[0]);
  if (!old_path) {
    return JS_EXCEPTION;
  }

  const char* new_path = JS_ToCString(ctx, argv[1]);
  if (!new_path) {
    JS_FreeCString(ctx, old_path);
    return JS_EXCEPTION;
  }

  if (rename(old_path, new_path) != 0) {
    JSValue error = create_fs_error(ctx, errno, "rename", old_path);
    JS_FreeCString(ctx, old_path);
    JS_FreeCString(ctx, new_path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, old_path);
  JS_FreeCString(ctx, new_path);
  return JS_UNDEFINED;
}

// fs.accessSync(path[, mode])
JSValue js_fs_access_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int mode = F_OK;  // Default to checking existence
  if (argc > 1 && JS_IsNumber(argv[1])) {
    int32_t mode_int;
    JS_ToInt32(ctx, &mode_int, argv[1]);
    mode = mode_int;
  }

  if (access(path, mode) != 0) {
    JSValue error = create_fs_error(ctx, errno, "access", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}