#include "fs_common.h"

// Helper function to copy file using buffer
static int copy_file_content(FILE* src, FILE* dest) {
  char buffer[8192];
  size_t bytes_read, bytes_written;

  while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
    bytes_written = fwrite(buffer, 1, bytes_read, dest);
    if (bytes_written != bytes_read) {
      return -1;
    }
  }

  return ferror(src) ? -1 : 0;
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

  // Check if source file exists
  FILE* src_file = fopen(src, "rb");
  if (!src_file) {
    JSValue error = create_fs_error(ctx, errno, "open", src);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, error);
  }

  // Create destination file
  FILE* dest_file = fopen(dest, "wb");
  if (!dest_file) {
    fclose(src_file);
    JSValue error = create_fs_error(ctx, errno, "open", dest);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, error);
  }

  // Copy file content
  if (copy_file_content(src_file, dest_file) < 0) {
    fclose(src_file);
    fclose(dest_file);
    unlink(dest);  // Remove partially copied file
    JSValue error = create_fs_error(ctx, errno, "write", dest);
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

  if (rename(old_path, new_path) < 0) {
    JSValue error = create_fs_error(ctx, errno, "rename", errno == ENOENT ? old_path : new_path);
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

  int mode = F_OK;  // Default: check existence
  if (argc >= 2) {
    if (JS_ToInt32(ctx, &mode, argv[1]) < 0) {
      JS_FreeCString(ctx, path);
      return JS_EXCEPTION;
    }
  }

  if (access(path, mode) < 0) {
    JSValue error = create_fs_error(ctx, errno, "access", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}