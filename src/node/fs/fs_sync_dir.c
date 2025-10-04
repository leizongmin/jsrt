#include "fs_common.h"

// fs.Stats.isFile() method
JSValue js_fs_stat_is_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue mode_val = JS_GetPropertyStr(ctx, this_val, "_mode");
  if (JS_IsException(mode_val)) {
    return JS_EXCEPTION;
  }

  int32_t mode;
  if (JS_ToInt32(ctx, &mode, mode_val) < 0) {
    JS_FreeValue(ctx, mode_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, mode_val);

  return JS_NewBool(ctx, S_ISREG(mode));
}

// fs.Stats.isDirectory() method
JSValue js_fs_stat_is_directory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue mode_val = JS_GetPropertyStr(ctx, this_val, "_mode");
  if (JS_IsException(mode_val)) {
    return JS_EXCEPTION;
  }

  int32_t mode;
  if (JS_ToInt32(ctx, &mode, mode_val) < 0) {
    JS_FreeValue(ctx, mode_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, mode_val);

  return JS_NewBool(ctx, S_ISDIR(mode));
}

// fs.statSync(path)
JSValue js_fs_stat_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  struct stat st;
  if (stat(path, &st) != 0) {
    JSValue error = create_fs_error(ctx, errno, "stat", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JSValue stats = JS_NewObject(ctx);

  // Basic stat properties
  JS_SetPropertyStr(ctx, stats, "size", JS_NewInt64(ctx, st.st_size));
  JS_SetPropertyStr(ctx, stats, "mode", JS_NewInt32(ctx, st.st_mode));
  JS_SetPropertyStr(ctx, stats, "uid", JS_NewInt32(ctx, st.st_uid));
  JS_SetPropertyStr(ctx, stats, "gid", JS_NewInt32(ctx, st.st_gid));

  // Time properties (in milliseconds)
  JS_SetPropertyStr(ctx, stats, "atime", JS_NewDate(ctx, st.st_atime * 1000.0));
  JS_SetPropertyStr(ctx, stats, "mtime", JS_NewDate(ctx, st.st_mtime * 1000.0));
  JS_SetPropertyStr(ctx, stats, "ctime", JS_NewDate(ctx, st.st_ctime * 1000.0));

  // Helper methods
  JSValue is_file_func = JS_NewCFunction(ctx, js_fs_stat_is_file, "isFile", 0);
  JSValue is_dir_func = JS_NewCFunction(ctx, js_fs_stat_is_directory, "isDirectory", 0);

  // Store the mode for the methods to access
  JS_SetPropertyStr(ctx, stats, "_mode", JS_NewInt32(ctx, st.st_mode));

  JS_SetPropertyStr(ctx, stats, "isFile", is_file_func);
  JS_SetPropertyStr(ctx, stats, "isDirectory", is_dir_func);

  JS_FreeCString(ctx, path);
  return stats;
}

// fs.readdirSync(path)
JSValue js_fs_readdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  DIR* dir = opendir(path);
  if (!dir) {
    JSValue error = create_fs_error(ctx, errno, "scandir", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JSValue files = JS_NewArray(ctx);
  struct dirent* entry;
  int index = 0;

  while ((entry = readdir(dir)) != NULL) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    JS_SetPropertyUint32(ctx, files, index++, JS_NewString(ctx, entry->d_name));
  }

  closedir(dir);
  JS_FreeCString(ctx, path);

  return files;
}

// fs.mkdirSync(path[, options])
JSValue js_fs_mkdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  mode_t mode = 0755;  // Default permissions
  bool recursive = false;

  // Check for options
  if (argc > 1 && JS_IsObject(argv[1])) {
    // Check for mode
    JSValue mode_val = JS_GetPropertyStr(ctx, argv[1], "mode");
    if (JS_IsNumber(mode_val)) {
      int32_t mode_int;
      JS_ToInt32(ctx, &mode_int, mode_val);
      mode = (mode_t)mode_int;
    }
    JS_FreeValue(ctx, mode_val);

    // Check for recursive option
    JSValue recursive_val = JS_GetPropertyStr(ctx, argv[1], "recursive");
    if (JS_IsBool(recursive_val)) {
      recursive = JS_ToBool(ctx, recursive_val);
    }
    JS_FreeValue(ctx, recursive_val);
  }

  int result;
  if (recursive) {
    result = mkdir_recursive(path, mode);
  } else {
    result = JSRT_MKDIR(path, mode);
  }

  if (result != 0) {
    JSValue error = create_fs_error(ctx, errno, "mkdir", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.rmdirSync(path[, options])
JSValue js_fs_rmdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  if (rmdir(path) != 0) {
    JSValue error = create_fs_error(ctx, errno, "rmdir", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// Dir class structure for opendirSync
typedef struct {
  DIR* dir;
  char* path;
} FsDir;

// Dir class ID and definition
static JSClassID js_dir_class_id = 0;

// Dir class finalizer
static void fs_dir_finalizer(JSRuntime* rt, JSValue val) {
  FsDir* dir = JS_GetOpaque(val, js_dir_class_id);
  if (dir) {
    if (dir->dir) {
      closedir(dir->dir);
    }
    if (dir->path) {
      js_free_rt(rt, dir->path);
    }
    js_free_rt(rt, dir);
  }
}

static JSClassDef js_dir_class = {
    "Dir",
    .finalizer = fs_dir_finalizer,
};

// Dir.readSync() method
static JSValue js_fs_dir_read_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FsDir* dir = JS_GetOpaque(this_val, js_dir_class_id);
  if (!dir || !dir->dir) {
    return JS_ThrowTypeError(ctx, "Invalid Dir object");
  }

  struct dirent* entry = readdir(dir->dir);
  if (!entry) {
    return JS_NULL;  // End of directory
  }

  // Skip . and ..
  while (entry && (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
    entry = readdir(dir->dir);
  }

  if (!entry) {
    return JS_NULL;
  }

  // Create Dirent object
  JSValue dirent = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, dirent, "name", JS_NewString(ctx, entry->d_name));

  return dirent;
}

// Dir.closeSync() method
static JSValue js_fs_dir_close_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FsDir* dir = JS_GetOpaque(this_val, js_dir_class_id);
  if (!dir) {
    return JS_ThrowTypeError(ctx, "Invalid Dir object");
  }

  if (dir->dir) {
    closedir(dir->dir);
    dir->dir = NULL;
  }

  return JS_UNDEFINED;
}

// Initialize Dir class (called once)
static void ensure_dir_class_initialized(JSContext* ctx) {
  if (js_dir_class_id == 0) {
    JS_NewClassID(&js_dir_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_dir_class_id, &js_dir_class);
  }
}

// fs.opendirSync(path)
JSValue js_fs_opendir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Ensure Dir class is initialized
  ensure_dir_class_initialized(ctx);

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  DIR* dir = opendir(path);
  if (!dir) {
    JSValue error = create_fs_error(ctx, errno, "opendir", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  // Create Dir object
  FsDir* fs_dir = js_mallocz(ctx, sizeof(FsDir));
  if (!fs_dir) {
    closedir(dir);
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  fs_dir->dir = dir;
  fs_dir->path = js_strdup(ctx, path);
  JS_FreeCString(ctx, path);

  JSValue dir_obj = JS_NewObjectClass(ctx, js_dir_class_id);
  if (JS_IsException(dir_obj)) {
    js_free(ctx, fs_dir->path);
    js_free(ctx, fs_dir);
    closedir(dir);
    return JS_EXCEPTION;
  }

  JS_SetOpaque(dir_obj, fs_dir);

  // Add methods
  JSValue read_func = JS_NewCFunction(ctx, js_fs_dir_read_sync, "readSync", 0);
  JSValue close_func = JS_NewCFunction(ctx, js_fs_dir_close_sync, "closeSync", 0);

  JS_SetPropertyStr(ctx, dir_obj, "readSync", read_func);
  JS_SetPropertyStr(ctx, dir_obj, "closeSync", close_func);

  return dir_obj;
}