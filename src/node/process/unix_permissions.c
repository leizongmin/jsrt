#include <stdlib.h>
#include <string.h>
#include "process.h"

// Unix-specific user/group management functions
// These functions are only available on Unix-like systems

#ifndef _WIN32

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

// Helper function to resolve username to UID
static int resolve_username_to_uid(JSContext* ctx, JSValueConst username_val, uid_t* out_uid) {
  const char* username = JS_ToCString(ctx, username_val);
  if (!username) {
    return -1;
  }

  struct passwd* pwd = getpwnam(username);
  JS_FreeCString(ctx, username);

  if (!pwd) {
    JS_ThrowInternalError(ctx, "Failed to resolve username: %s", strerror(errno));
    return -1;
  }

  *out_uid = pwd->pw_uid;
  return 0;
}

// Helper function to resolve group name to GID
static int resolve_groupname_to_gid(JSContext* ctx, JSValueConst groupname_val, gid_t* out_gid) {
  const char* groupname = JS_ToCString(ctx, groupname_val);
  if (!groupname) {
    return -1;
  }

  struct group* grp = getgrnam(groupname);
  JS_FreeCString(ctx, groupname);

  if (!grp) {
    JS_ThrowInternalError(ctx, "Failed to resolve group name: %s", strerror(errno));
    return -1;
  }

  *out_gid = grp->gr_gid;
  return 0;
}

// process.getuid()
JSValue js_process_getuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  uid_t uid = getuid();
  return JS_NewInt32(ctx, (int32_t)uid);
}

// process.geteuid()
JSValue js_process_geteuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  uid_t euid = geteuid();
  return JS_NewInt32(ctx, (int32_t)euid);
}

// process.getgid()
JSValue js_process_getgid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  gid_t gid = getgid();
  return JS_NewInt32(ctx, (int32_t)gid);
}

// process.getegid()
JSValue js_process_getegid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  gid_t egid = getegid();
  return JS_NewInt32(ctx, (int32_t)egid);
}

// process.setuid(id)
JSValue js_process_setuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "setuid requires an argument");
  }

  uid_t uid;

  if (JS_IsString(argv[0])) {
    // Resolve username to UID
    if (resolve_username_to_uid(ctx, argv[0], &uid) < 0) {
      return JS_EXCEPTION;
    }
  } else if (JS_IsNumber(argv[0])) {
    int32_t uid_int;
    if (JS_ToInt32(ctx, &uid_int, argv[0]) < 0) {
      return JS_EXCEPTION;
    }
    uid = (uid_t)uid_int;
  } else {
    return JS_ThrowTypeError(ctx, "setuid argument must be a number or string");
  }

  if (setuid(uid) != 0) {
    return JS_ThrowInternalError(ctx, "setuid failed: %s", strerror(errno));
  }

  return JS_UNDEFINED;
}

// process.seteuid(id)
JSValue js_process_seteuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "seteuid requires an argument");
  }

  uid_t uid;

  if (JS_IsString(argv[0])) {
    if (resolve_username_to_uid(ctx, argv[0], &uid) < 0) {
      return JS_EXCEPTION;
    }
  } else if (JS_IsNumber(argv[0])) {
    int32_t uid_int;
    if (JS_ToInt32(ctx, &uid_int, argv[0]) < 0) {
      return JS_EXCEPTION;
    }
    uid = (uid_t)uid_int;
  } else {
    return JS_ThrowTypeError(ctx, "seteuid argument must be a number or string");
  }

  if (seteuid(uid) != 0) {
    return JS_ThrowInternalError(ctx, "seteuid failed: %s", strerror(errno));
  }

  return JS_UNDEFINED;
}

// process.setgid(id)
JSValue js_process_setgid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "setgid requires an argument");
  }

  gid_t gid;

  if (JS_IsString(argv[0])) {
    // Resolve group name to GID
    if (resolve_groupname_to_gid(ctx, argv[0], &gid) < 0) {
      return JS_EXCEPTION;
    }
  } else if (JS_IsNumber(argv[0])) {
    int32_t gid_int;
    if (JS_ToInt32(ctx, &gid_int, argv[0]) < 0) {
      return JS_EXCEPTION;
    }
    gid = (gid_t)gid_int;
  } else {
    return JS_ThrowTypeError(ctx, "setgid argument must be a number or string");
  }

  if (setgid(gid) != 0) {
    return JS_ThrowInternalError(ctx, "setgid failed: %s", strerror(errno));
  }

  return JS_UNDEFINED;
}

// process.setegid(id)
JSValue js_process_setegid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "setegid requires an argument");
  }

  gid_t gid;

  if (JS_IsString(argv[0])) {
    if (resolve_groupname_to_gid(ctx, argv[0], &gid) < 0) {
      return JS_EXCEPTION;
    }
  } else if (JS_IsNumber(argv[0])) {
    int32_t gid_int;
    if (JS_ToInt32(ctx, &gid_int, argv[0]) < 0) {
      return JS_EXCEPTION;
    }
    gid = (gid_t)gid_int;
  } else {
    return JS_ThrowTypeError(ctx, "setegid argument must be a number or string");
  }

  if (setegid(gid) != 0) {
    return JS_ThrowInternalError(ctx, "setegid failed: %s", strerror(errno));
  }

  return JS_UNDEFINED;
}

// process.getgroups()
JSValue js_process_getgroups(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // First call to get the number of groups
  int ngroups = getgroups(0, NULL);
  if (ngroups < 0) {
    return JS_ThrowInternalError(ctx, "getgroups failed: %s", strerror(errno));
  }

  if (ngroups == 0) {
    return JS_NewArray(ctx);
  }

  // Allocate buffer for groups
  gid_t* groups = malloc(sizeof(gid_t) * ngroups);
  if (!groups) {
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for groups");
  }

  // Get the actual groups
  int result = getgroups(ngroups, groups);
  if (result < 0) {
    free(groups);
    return JS_ThrowInternalError(ctx, "getgroups failed: %s", strerror(errno));
  }

  // Create JavaScript array
  JSValue groups_array = JS_NewArray(ctx);
  if (JS_IsException(groups_array)) {
    free(groups);
    return JS_EXCEPTION;
  }

  for (int i = 0; i < result; i++) {
    JS_SetPropertyUint32(ctx, groups_array, i, JS_NewInt32(ctx, (int32_t)groups[i]));
  }

  free(groups);
  return groups_array;
}

// process.setgroups(groups)
JSValue js_process_setgroups(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "setgroups requires an argument");
  }

  if (!JS_IsArray(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "setgroups argument must be an array");
  }

  JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
  uint32_t length;
  if (JS_ToUint32(ctx, &length, length_val) < 0) {
    JS_FreeValue(ctx, length_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length_val);

  // Allocate buffer for groups
  gid_t* groups = malloc(sizeof(gid_t) * length);
  if (!groups) {
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for groups");
  }

  // Parse groups array
  for (uint32_t i = 0; i < length; i++) {
    JSValue item = JS_GetPropertyUint32(ctx, argv[0], i);

    if (JS_IsString(item)) {
      gid_t gid;
      if (resolve_groupname_to_gid(ctx, item, &gid) < 0) {
        JS_FreeValue(ctx, item);
        free(groups);
        return JS_EXCEPTION;
      }
      groups[i] = gid;
    } else if (JS_IsNumber(item)) {
      int32_t gid_int;
      if (JS_ToInt32(ctx, &gid_int, item) < 0) {
        JS_FreeValue(ctx, item);
        free(groups);
        return JS_EXCEPTION;
      }
      groups[i] = (gid_t)gid_int;
    } else {
      JS_FreeValue(ctx, item);
      free(groups);
      return JS_ThrowTypeError(ctx, "setgroups array elements must be numbers or strings");
    }

    JS_FreeValue(ctx, item);
  }

  // Set the groups
  if (setgroups(length, groups) != 0) {
    free(groups);
    return JS_ThrowInternalError(ctx, "setgroups failed: %s", strerror(errno));
  }

  free(groups);
  return JS_UNDEFINED;
}

// process.initgroups(user, extraGroup)
JSValue js_process_initgroups(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "initgroups requires 2 arguments (user, extraGroup)");
  }

  const char* username;
  gid_t extra_gid;

  // Parse user argument
  if (JS_IsString(argv[0])) {
    username = JS_ToCString(ctx, argv[0]);
    if (!username) {
      return JS_EXCEPTION;
    }
  } else if (JS_IsNumber(argv[0])) {
    int32_t uid_int;
    if (JS_ToInt32(ctx, &uid_int, argv[0]) < 0) {
      return JS_EXCEPTION;
    }
    struct passwd* pwd = getpwuid((uid_t)uid_int);
    if (!pwd) {
      return JS_ThrowInternalError(ctx, "Failed to resolve UID to username: %s", strerror(errno));
    }
    username = pwd->pw_name;
  } else {
    return JS_ThrowTypeError(ctx, "initgroups user argument must be a string or number");
  }

  // Parse extraGroup argument
  if (JS_IsString(argv[1])) {
    if (resolve_groupname_to_gid(ctx, argv[1], &extra_gid) < 0) {
      if (JS_IsString(argv[0])) {
        JS_FreeCString(ctx, username);
      }
      return JS_EXCEPTION;
    }
  } else if (JS_IsNumber(argv[1])) {
    int32_t gid_int;
    if (JS_ToInt32(ctx, &gid_int, argv[1]) < 0) {
      if (JS_IsString(argv[0])) {
        JS_FreeCString(ctx, username);
      }
      return JS_EXCEPTION;
    }
    extra_gid = (gid_t)gid_int;
  } else {
    if (JS_IsString(argv[0])) {
      JS_FreeCString(ctx, username);
    }
    return JS_ThrowTypeError(ctx, "initgroups extraGroup argument must be a string or number");
  }

  // Call initgroups
  if (initgroups(username, extra_gid) != 0) {
    if (JS_IsString(argv[0])) {
      JS_FreeCString(ctx, username);
    }
    return JS_ThrowInternalError(ctx, "initgroups failed: %s", strerror(errno));
  }

  if (JS_IsString(argv[0])) {
    JS_FreeCString(ctx, username);
  }

  return JS_UNDEFINED;
}

// process.umask([mask])
JSValue js_process_umask(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  mode_t old_mask;

  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    // Set new mask and return old
    int32_t new_mask_int;
    if (JS_ToInt32(ctx, &new_mask_int, argv[0]) < 0) {
      return JS_EXCEPTION;
    }

    mode_t new_mask = (mode_t)new_mask_int;
    old_mask = umask(new_mask);
  } else {
    // Get current mask without changing it
    // Note: There's no direct way to get umask without setting it
    // We have to set it twice
    old_mask = umask(0);
    umask(old_mask);
  }

  return JS_NewInt32(ctx, (int32_t)old_mask);
}

void jsrt_process_init_unix_permissions(void) {
  // Initialization (if needed)
}

#else  // _WIN32

// Windows stubs - these functions don't exist on Windows
JSValue js_process_getuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.getuid is not available on Windows");
}

JSValue js_process_geteuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.geteuid is not available on Windows");
}

JSValue js_process_getgid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.getgid is not available on Windows");
}

JSValue js_process_getegid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.getegid is not available on Windows");
}

JSValue js_process_setuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.setuid is not available on Windows");
}

JSValue js_process_seteuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.seteuid is not available on Windows");
}

JSValue js_process_setgid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.setgid is not available on Windows");
}

JSValue js_process_setegid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.setegid is not available on Windows");
}

JSValue js_process_getgroups(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.getgroups is not available on Windows");
}

JSValue js_process_setgroups(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.setgroups is not available on Windows");
}

JSValue js_process_initgroups(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.initgroups is not available on Windows");
}

JSValue js_process_umask(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "process.umask is not available on Windows");
}

void jsrt_process_init_unix_permissions(void) {
  // No-op on Windows
}

#endif  // _WIN32
