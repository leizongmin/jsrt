#include <limits.h>
#include "fs_common.h"

// Internal helper function to remove directory recursively with depth tracking
static int rmdir_recursive_internal(const char* path, int depth) {
  // Protect against excessively deep directory trees
  if (depth > 128) {
    errno = ELOOP;  // Too many levels of symbolic links
    return -1;
  }

  DIR* dir = opendir(path);
  if (!dir) {
    return -1;
  }

  struct dirent* entry;
  int result = 0;

  while ((entry = readdir(dir)) != NULL && result == 0) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Build full path with overflow check
    char full_path[PATH_MAX];
    int len = snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
    if (len >= sizeof(full_path)) {
      errno = ENAMETOOLONG;
      result = -1;
      break;
    }

    struct stat st;
    if (lstat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        // Recursively remove subdirectory with incremented depth
        result = rmdir_recursive_internal(full_path, depth + 1);
        // rmdir_recursive_internal already removes the directory itself
      } else {
        // Remove file
        result = unlink(full_path);
      }
    } else {
      result = -1;
    }
  }

  closedir(dir);

  // Remove the directory itself after all contents are removed
  if (result == 0) {
    result = rmdir(path);
  }

  return result;
}

// Public wrapper that starts at depth 0
static int rmdir_recursive(const char* path) {
  return rmdir_recursive_internal(path, 0);
}

// Internal helper function to copy directory recursively with depth tracking
static int copydir_recursive_internal(JSContext* ctx, const char* src, const char* dest, mode_t mode, int depth) {
  // Protect against excessively deep directory trees
  if (depth > 128) {
    errno = ELOOP;  // Too many levels of symbolic links
    return -1;
  }

  // Create destination directory
  if (JSRT_MKDIR(dest, mode) != 0) {
    if (errno != EEXIST) {
      return -1;
    }
  }

  DIR* dir = opendir(src);
  if (!dir) {
    return -1;
  }

  struct dirent* entry;
  int result = 0;

  while ((entry = readdir(dir)) != NULL && result == 0) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Build full paths with overflow checks
    char src_path[PATH_MAX];
    char dest_path[PATH_MAX];
    int src_len = snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
    if (src_len >= sizeof(src_path)) {
      errno = ENAMETOOLONG;
      result = -1;
      break;
    }
    int dest_len = snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);
    if (dest_len >= sizeof(dest_path)) {
      errno = ENAMETOOLONG;
      result = -1;
      break;
    }

    struct stat st;
    if (lstat(src_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        // Recursively copy subdirectory with incremented depth
        result = copydir_recursive_internal(ctx, src_path, dest_path, st.st_mode & 0777, depth + 1);
      } else if (S_ISREG(st.st_mode)) {
        // Copy regular file
        FILE* src_file = fopen(src_path, "rb");
        if (!src_file) {
          result = -1;
          continue;
        }

        FILE* dest_file = fopen(dest_path, "wb");
        if (!dest_file) {
          fclose(src_file);
          result = -1;
          continue;
        }

        // Copy file contents
        char buffer[8192];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
          if (fwrite(buffer, 1, bytes, dest_file) != bytes) {
            result = -1;
            break;
          }
        }

        fclose(src_file);
        fclose(dest_file);

        // Copy permissions
        chmod(dest_path, st.st_mode & 0777);
      } else if (S_ISLNK(st.st_mode)) {
        // Copy symlink
        char link_target[PATH_MAX];
        ssize_t len = readlink(src_path, link_target, sizeof(link_target) - 1);
        if (len >= 0) {
          link_target[len] = '\0';
          if (symlink(link_target, dest_path) != 0) {
            result = -1;
          }
        } else {
          result = -1;
        }
      }
    } else {
      result = -1;
    }
  }

  closedir(dir);
  return result;
}

// Public wrapper that starts at depth 0
static int copydir_recursive(JSContext* ctx, const char* src, const char* dest, mode_t mode) {
  return copydir_recursive_internal(ctx, src, dest, mode, 0);
}

// fs.rmSync(path, options)
JSValue js_fs_rm_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  bool recursive = false;
  bool force = false;

  // Parse options
  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue recursive_val = JS_GetPropertyStr(ctx, argv[1], "recursive");
    if (JS_IsBool(recursive_val)) {
      recursive = JS_ToBool(ctx, recursive_val);
    }
    JS_FreeValue(ctx, recursive_val);

    JSValue force_val = JS_GetPropertyStr(ctx, argv[1], "force");
    if (JS_IsBool(force_val)) {
      force = JS_ToBool(ctx, force_val);
    }
    JS_FreeValue(ctx, force_val);
  }

  // Check if path exists
  struct stat st;
  if (lstat(path, &st) != 0) {
    if (force && errno == ENOENT) {
      // force option ignores ENOENT
      JS_FreeCString(ctx, path);
      return JS_UNDEFINED;
    }
    JSValue error = create_fs_error(ctx, errno, "rm", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  int result;
  if (S_ISDIR(st.st_mode)) {
    if (recursive) {
      result = rmdir_recursive(path);
    } else {
      // Non-recursive rm on directory should fail
      JS_FreeCString(ctx, path);
      return JS_Throw(ctx, create_fs_error(ctx, EISDIR, "rm", path));
    }
  } else {
    result = unlink(path);
  }

  if (result != 0) {
    JSValue error = create_fs_error(ctx, errno, "rm", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.cpSync(src, dest, options)
JSValue js_fs_cp_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

  bool recursive = false;
  bool force = false;

  // Parse options
  if (argc > 2 && JS_IsObject(argv[2])) {
    JSValue recursive_val = JS_GetPropertyStr(ctx, argv[2], "recursive");
    if (JS_IsBool(recursive_val)) {
      recursive = JS_ToBool(ctx, recursive_val);
    }
    JS_FreeValue(ctx, recursive_val);

    JSValue force_val = JS_GetPropertyStr(ctx, argv[2], "force");
    if (JS_IsBool(force_val)) {
      force = JS_ToBool(ctx, force_val);
    }
    JS_FreeValue(ctx, force_val);
  }

  // Check if source exists
  struct stat src_st;
  if (lstat(src, &src_st) != 0) {
    JSValue error = create_fs_error(ctx, errno, "cp", src);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, error);
  }

  // Check if destination exists
  struct stat dest_st;
  bool dest_exists = (lstat(dest, &dest_st) == 0);

  if (dest_exists && !force) {
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, create_fs_error(ctx, EEXIST, "cp", dest));
  }

  int result = 0;

  if (S_ISDIR(src_st.st_mode)) {
    if (!recursive) {
      JS_FreeCString(ctx, src);
      JS_FreeCString(ctx, dest);
      return JS_Throw(ctx, create_fs_error(ctx, EISDIR, "cp", src));
    }

    result = copydir_recursive(ctx, src, dest, src_st.st_mode & 0777);
  } else if (S_ISREG(src_st.st_mode)) {
    // Copy regular file
    FILE* src_file = fopen(src, "rb");
    if (!src_file) {
      JSValue error = create_fs_error(ctx, errno, "cp", src);
      JS_FreeCString(ctx, src);
      JS_FreeCString(ctx, dest);
      return JS_Throw(ctx, error);
    }

    FILE* dest_file = fopen(dest, "wb");
    if (!dest_file) {
      fclose(src_file);
      JSValue error = create_fs_error(ctx, errno, "cp", dest);
      JS_FreeCString(ctx, src);
      JS_FreeCString(ctx, dest);
      return JS_Throw(ctx, error);
    }

    // Copy file contents
    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
      if (fwrite(buffer, 1, bytes, dest_file) != bytes) {
        result = -1;
        break;
      }
    }

    fclose(src_file);
    fclose(dest_file);

    // Copy permissions
    if (result == 0) {
      chmod(dest, src_st.st_mode & 0777);
    }
  } else if (S_ISLNK(src_st.st_mode)) {
    // Copy symlink
    char link_target[PATH_MAX];
    ssize_t len = readlink(src, link_target, sizeof(link_target) - 1);
    if (len >= 0) {
      link_target[len] = '\0';
      if (symlink(link_target, dest) != 0) {
        result = -1;
      }
    } else {
      result = -1;
    }
  }

  if (result != 0) {
    JSValue error = create_fs_error(ctx, errno, "cp", dest);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, src);
  JS_FreeCString(ctx, dest);
  return JS_UNDEFINED;
}
