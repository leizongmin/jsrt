#include "fs_common.h"

// Helper function to create Stats object from struct stat
// This is shared between fstatSync and lstatSync
static JSValue create_stats_object(JSContext* ctx, const struct stat* st) {
  JSValue stats = JS_NewObject(ctx);

  // Basic stat properties
  JS_SetPropertyStr(ctx, stats, "size", JS_NewInt64(ctx, st->st_size));
  JS_SetPropertyStr(ctx, stats, "mode", JS_NewInt32(ctx, st->st_mode));
  JS_SetPropertyStr(ctx, stats, "uid", JS_NewInt32(ctx, st->st_uid));
  JS_SetPropertyStr(ctx, stats, "gid", JS_NewInt32(ctx, st->st_gid));

  // Time properties (in milliseconds)
  JS_SetPropertyStr(ctx, stats, "atime", JS_NewDate(ctx, st->st_atime * 1000.0));
  JS_SetPropertyStr(ctx, stats, "mtime", JS_NewDate(ctx, st->st_mtime * 1000.0));
  JS_SetPropertyStr(ctx, stats, "ctime", JS_NewDate(ctx, st->st_ctime * 1000.0));

  // Store the mode for helper methods to access
  JS_SetPropertyStr(ctx, stats, "_mode", JS_NewInt32(ctx, st->st_mode));

  // Helper methods (reuse from existing code)
  extern JSValue js_fs_stat_is_file(JSContext * ctx, JSValueConst this_val, int argc, JSValueConst* argv);
  extern JSValue js_fs_stat_is_directory(JSContext * ctx, JSValueConst this_val, int argc, JSValueConst* argv);

  JSValue is_file_func = JS_NewCFunction(ctx, js_fs_stat_is_file, "isFile", 0);
  JSValue is_dir_func = JS_NewCFunction(ctx, js_fs_stat_is_directory, "isDirectory", 0);

  JS_SetPropertyStr(ctx, stats, "isFile", is_file_func);
  JS_SetPropertyStr(ctx, stats, "isDirectory", is_dir_func);

  return stats;
}

// fs.fstatSync(fd)
JSValue js_fs_fstat_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "fd is required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "fstat", NULL));
  }

  return create_stats_object(ctx, &st);
}

// fs.lstatSync(path)
JSValue js_fs_lstat_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  struct stat st;
  if (lstat(path, &st) != 0) {
    JSValue error = create_fs_error(ctx, errno, "lstat", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return create_stats_object(ctx, &st);
}
