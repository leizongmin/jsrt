#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../runtime.h"
#include "../std/assert.h"
#include "../util/debug.h"
#include "node_modules.h"

// Constants module - central consolidation point for OS-level constants
// Provides unified access to constants from os, fs, and crypto modules
// Phase 2: Enhanced with cross-module aggregation capabilities

// Forward declarations for module integration functions
JSValue JSRT_InitNodeOs(JSContext* ctx);
JSValue JSRT_InitNodeFs(JSContext* ctx);
JSValue create_crypto_constants(JSContext* ctx);

// Helper function: safely extract property from module object
static JSValue js_constants_extract_property(JSContext* ctx, JSValue module_obj, const char* property_name) {
  JSValue result = JS_GetPropertyStr(ctx, module_obj, property_name);
  if (JS_IsException(result)) {
    JSRT_Debug("Failed to extract property '%s' from module", property_name);
    JS_FreeValue(ctx, result);
    return JS_UNDEFINED;
  }
  return result;
}

// Helper function: create constant category with debug logging
static JSValue js_constants_create_category(JSContext* ctx, const char* category_name) {
  JSRT_Debug("Creating constants category: %s", category_name);
  JSValue category = JS_NewObject(ctx);
  if (JS_IsException(category)) {
    JSRT_Debug("Failed to create category object for: %s", category_name);
    return JS_UNDEFINED;
  }
  return category;
}

// Helper function: safely set integer constant
static bool js_constants_set_int32(JSContext* ctx, JSValue obj, const char* name, int32_t value) {
  JSValue int_val = JS_NewInt32(ctx, value);
  if (JS_IsException(int_val)) {
    JSRT_Debug("Failed to create integer constant: %s = %d", name, value);
    return false;
  }

  int set_result = JS_SetPropertyStr(ctx, obj, name, int_val);
  JS_FreeValue(ctx, int_val);

  if (set_result < 0) {
    JSRT_Debug("Failed to set property: %s = %d", name, value);
    return false;
  }

  return true;
}

// OS errno constants (enhanced with platform-specific values)
static JSValue js_constants_create_errno(JSContext* ctx) {
  JSValue errno_obj = js_constants_create_category(ctx, "errno");
  if (JS_IsUndefined(errno_obj)) {
    return JS_UNDEFINED;
  }

// Platform-specific errno constants using system headers
#ifdef E2BIG
  js_constants_set_int32(ctx, errno_obj, "E2BIG", E2BIG);
#endif
#ifdef EACCES
  js_constants_set_int32(ctx, errno_obj, "EACCES", EACCES);
#endif
#ifdef EADDRINUSE
  js_constants_set_int32(ctx, errno_obj, "EADDRINUSE", EADDRINUSE);
#endif
#ifdef EADDRNOTAVAIL
  js_constants_set_int32(ctx, errno_obj, "EADDRNOTAVAIL", EADDRNOTAVAIL);
#endif
#ifdef EAFNOSUPPORT
  js_constants_set_int32(ctx, errno_obj, "EAFNOSUPPORT", EAFNOSUPPORT);
#endif
#ifdef EAGAIN
  js_constants_set_int32(ctx, errno_obj, "EAGAIN", EAGAIN);
#endif
#ifdef EALREADY
  js_constants_set_int32(ctx, errno_obj, "EALREADY", EALREADY);
#endif
#ifdef EBADF
  js_constants_set_int32(ctx, errno_obj, "EBADF", EBADF);
#endif
#ifdef EBUSY
  js_constants_set_int32(ctx, errno_obj, "EBUSY", EBUSY);
#endif
#ifdef ECHILD
  js_constants_set_int32(ctx, errno_obj, "ECHILD", ECHILD);
#endif
#ifdef ECONNABORTED
  js_constants_set_int32(ctx, errno_obj, "ECONNABORTED", ECONNABORTED);
#endif
#ifdef ECONNREFUSED
  js_constants_set_int32(ctx, errno_obj, "ECONNREFUSED", ECONNREFUSED);
#endif
#ifdef ECONNRESET
  js_constants_set_int32(ctx, errno_obj, "ECONNRESET", ECONNRESET);
#endif
#ifdef EDEADLK
  js_constants_set_int32(ctx, errno_obj, "EDEADLK", EDEADLK);
#endif
#ifdef EDESTADDRREQ
  js_constants_set_int32(ctx, errno_obj, "EDESTADDRREQ", EDESTADDRREQ);
#endif
#ifdef EDOM
  js_constants_set_int32(ctx, errno_obj, "EDOM", EDOM);
#endif
#ifdef EEXIST
  js_constants_set_int32(ctx, errno_obj, "EEXIST", EEXIST);
#endif
#ifdef EFAULT
  js_constants_set_int32(ctx, errno_obj, "EFAULT", EFAULT);
#endif
#ifdef EFBIG
  js_constants_set_int32(ctx, errno_obj, "EFBIG", EFBIG);
#endif
#ifdef EHOSTUNREACH
  js_constants_set_int32(ctx, errno_obj, "EHOSTUNREACH", EHOSTUNREACH);
#endif
#ifdef EINTR
  js_constants_set_int32(ctx, errno_obj, "EINTR", EINTR);
#endif
#ifdef EINVAL
  js_constants_set_int32(ctx, errno_obj, "EINVAL", EINVAL);
#endif
#ifdef EIO
  js_constants_set_int32(ctx, errno_obj, "EIO", EIO);
#endif
#ifdef EISCONN
  js_constants_set_int32(ctx, errno_obj, "EISCONN", EISCONN);
#endif
#ifdef EISDIR
  js_constants_set_int32(ctx, errno_obj, "EISDIR", EISDIR);
#endif
#ifdef ELOOP
  js_constants_set_int32(ctx, errno_obj, "ELOOP", ELOOP);
#endif
#ifdef EMFILE
  js_constants_set_int32(ctx, errno_obj, "EMFILE", EMFILE);
#endif
#ifdef EMLINK
  js_constants_set_int32(ctx, errno_obj, "EMLINK", EMLINK);
#endif
#ifdef EMSGSIZE
  js_constants_set_int32(ctx, errno_obj, "EMSGSIZE", EMSGSIZE);
#endif
#ifdef ENAMETOOLONG
  js_constants_set_int32(ctx, errno_obj, "ENAMETOOLONG", ENAMETOOLONG);
#endif
#ifdef ENETDOWN
  js_constants_set_int32(ctx, errno_obj, "ENETDOWN", ENETDOWN);
#endif
#ifdef ENETRESET
  js_constants_set_int32(ctx, errno_obj, "ENETRESET", ENETRESET);
#endif
#ifdef ENETUNREACH
  js_constants_set_int32(ctx, errno_obj, "ENETUNREACH", ENETUNREACH);
#endif
#ifdef ENFILE
  js_constants_set_int32(ctx, errno_obj, "ENFILE", ENFILE);
#endif
#ifdef ENOBUFS
  js_constants_set_int32(ctx, errno_obj, "ENOBUFS", ENOBUFS);
#endif
#ifdef ENODATA
  js_constants_set_int32(ctx, errno_obj, "ENODATA", ENODATA);
#endif
#ifdef ENODEV
  js_constants_set_int32(ctx, errno_obj, "ENODEV", ENODEV);
#endif
#ifdef ENOENT
  js_constants_set_int32(ctx, errno_obj, "ENOENT", ENOENT);
#endif
#ifdef ENOEXEC
  js_constants_set_int32(ctx, errno_obj, "ENOEXEC", ENOEXEC);
#endif
#ifdef ENOLINK
  js_constants_set_int32(ctx, errno_obj, "ENOLINK", ENOLINK);
#endif
#ifdef ENOMEM
  js_constants_set_int32(ctx, errno_obj, "ENOMEM", ENOMEM);
#endif
#ifdef ENOMSG
  js_constants_set_int32(ctx, errno_obj, "ENOMSG", ENOMSG);
#endif
#ifdef ENOPROTOOPT
  js_constants_set_int32(ctx, errno_obj, "ENOPROTOOPT", ENOPROTOOPT);
#endif
#ifdef ENOSPC
  js_constants_set_int32(ctx, errno_obj, "ENOSPC", ENOSPC);
#endif
#ifdef ENOSR
  js_constants_set_int32(ctx, errno_obj, "ENOSR", ENOSR);
#endif
#ifdef ENOSTR
  js_constants_set_int32(ctx, errno_obj, "ENOSTR", ENOSTR);
#endif
#ifdef ENOSYS
  js_constants_set_int32(ctx, errno_obj, "ENOSYS", ENOSYS);
#endif
#ifdef ENOTCONN
  js_constants_set_int32(ctx, errno_obj, "ENOTCONN", ENOTCONN);
#endif
#ifdef ENOTDIR
  js_constants_set_int32(ctx, errno_obj, "ENOTDIR", ENOTDIR);
#endif
#ifdef ENOTEMPTY
  js_constants_set_int32(ctx, errno_obj, "ENOTEMPTY", ENOTEMPTY);
#endif
#ifdef ENOTSOCK
  js_constants_set_int32(ctx, errno_obj, "ENOTSOCK", ENOTSOCK);
#endif
#ifdef ENOTSUP
  js_constants_set_int32(ctx, errno_obj, "ENOTSUP", ENOTSUP);
#endif
#ifdef ENOTTY
  js_constants_set_int32(ctx, errno_obj, "ENOTTY", ENOTTY);
#endif
#ifdef ENXIO
  js_constants_set_int32(ctx, errno_obj, "ENXIO", ENXIO);
#endif
#ifdef EOPNOTSUPP
  js_constants_set_int32(ctx, errno_obj, "EOPNOTSUPP", EOPNOTSUPP);
#endif
#ifdef EOVERFLOW
  js_constants_set_int32(ctx, errno_obj, "EOVERFLOW", EOVERFLOW);
#endif
#ifdef EPERM
  js_constants_set_int32(ctx, errno_obj, "EPERM", EPERM);
#endif
#ifdef EPIPE
  js_constants_set_int32(ctx, errno_obj, "EPIPE", EPIPE);
#endif
#ifdef EPROTO
  js_constants_set_int32(ctx, errno_obj, "EPROTO", EPROTO);
#endif
#ifdef EPROTONOSUPPORT
  js_constants_set_int32(ctx, errno_obj, "EPROTONOSUPPORT", EPROTONOSUPPORT);
#endif
#ifdef EPROTOTYPE
  js_constants_set_int32(ctx, errno_obj, "EPROTOTYPE", EPROTOTYPE);
#endif
#ifdef ERANGE
  js_constants_set_int32(ctx, errno_obj, "ERANGE", ERANGE);
#endif
#ifdef EROFS
  js_constants_set_int32(ctx, errno_obj, "EROFS", EROFS);
#endif
#ifdef ESPIPE
  js_constants_set_int32(ctx, errno_obj, "ESPIPE", ESPIPE);
#endif
#ifdef ESRCH
  js_constants_set_int32(ctx, errno_obj, "ESRCH", ESRCH);
#endif
#ifdef ETIME
  js_constants_set_int32(ctx, errno_obj, "ETIME", ETIME);
#endif
#ifdef ETIMEDOUT
  js_constants_set_int32(ctx, errno_obj, "ETIMEDOUT", ETIMEDOUT);
#endif
#ifdef ETXTBSY
  js_constants_set_int32(ctx, errno_obj, "ETXTBSY", ETXTBSY);
#endif
#ifdef EWOULDBLOCK
  js_constants_set_int32(ctx, errno_obj, "EWOULDBLOCK", EWOULDBLOCK);
#endif
#ifdef EXDEV
  js_constants_set_int32(ctx, errno_obj, "EXDEV", EXDEV);
#endif

  JSRT_Debug("Created %d errno constants", "errno");
  return errno_obj;
}

// Signal constants (enhanced with platform-specific values)
static JSValue js_constants_create_signals(JSContext* ctx) {
  JSValue signals_obj = js_constants_create_category(ctx, "signals");
  if (JS_IsUndefined(signals_obj)) {
    return JS_UNDEFINED;
  }

#ifndef _WIN32
// Platform-specific signal constants using system headers
#ifdef SIGHUP
  js_constants_set_int32(ctx, signals_obj, "SIGHUP", SIGHUP);
#endif
#ifdef SIGINT
  js_constants_set_int32(ctx, signals_obj, "SIGINT", SIGINT);
#endif
#ifdef SIGQUIT
  js_constants_set_int32(ctx, signals_obj, "SIGQUIT", SIGQUIT);
#endif
#ifdef SIGILL
  js_constants_set_int32(ctx, signals_obj, "SIGILL", SIGILL);
#endif
#ifdef SIGTRAP
  js_constants_set_int32(ctx, signals_obj, "SIGTRAP", SIGTRAP);
#endif
#ifdef SIGABRT
  js_constants_set_int32(ctx, signals_obj, "SIGABRT", SIGABRT);
#endif
#ifdef SIGBUS
  js_constants_set_int32(ctx, signals_obj, "SIGBUS", SIGBUS);
#endif
#ifdef SIGFPE
  js_constants_set_int32(ctx, signals_obj, "SIGFPE", SIGFPE);
#endif
#ifdef SIGKILL
  js_constants_set_int32(ctx, signals_obj, "SIGKILL", SIGKILL);
#endif
#ifdef SIGUSR1
  js_constants_set_int32(ctx, signals_obj, "SIGUSR1", SIGUSR1);
#endif
#ifdef SIGSEGV
  js_constants_set_int32(ctx, signals_obj, "SIGSEGV", SIGSEGV);
#endif
#ifdef SIGUSR2
  js_constants_set_int32(ctx, signals_obj, "SIGUSR2", SIGUSR2);
#endif
#ifdef SIGPIPE
  js_constants_set_int32(ctx, signals_obj, "SIGPIPE", SIGPIPE);
#endif
#ifdef SIGALRM
  js_constants_set_int32(ctx, signals_obj, "SIGALRM", SIGALRM);
#endif
#ifdef SIGTERM
  js_constants_set_int32(ctx, signals_obj, "SIGTERM", SIGTERM);
#endif
#ifdef SIGSTKFLT
  js_constants_set_int32(ctx, signals_obj, "SIGSTKFLT", SIGSTKFLT);
#endif
#ifdef SIGCHLD
  js_constants_set_int32(ctx, signals_obj, "SIGCHLD", SIGCHLD);
#endif
#ifdef SIGCONT
  js_constants_set_int32(ctx, signals_obj, "SIGCONT", SIGCONT);
#endif
#ifdef SIGSTOP
  js_constants_set_int32(ctx, signals_obj, "SIGSTOP", SIGSTOP);
#endif
#ifdef SIGTSTP
  js_constants_set_int32(ctx, signals_obj, "SIGTSTP", SIGTSTP);
#endif
#ifdef SIGTTIN
  js_constants_set_int32(ctx, signals_obj, "SIGTTIN", SIGTTIN);
#endif
#ifdef SIGTTOU
  js_constants_set_int32(ctx, signals_obj, "SIGTTOU", SIGTTOU);
#endif
#ifdef SIGURG
  js_constants_set_int32(ctx, signals_obj, "SIGURG", SIGURG);
#endif
#ifdef SIGXCPU
  js_constants_set_int32(ctx, signals_obj, "SIGXCPU", SIGXCPU);
#endif
#ifdef SIGXFSZ
  js_constants_set_int32(ctx, signals_obj, "SIGXFSZ", SIGXFSZ);
#endif
#ifdef SIGVTALRM
  js_constants_set_int32(ctx, signals_obj, "SIGVTALRM", SIGVTALRM);
#endif
#ifdef SIGPROF
  js_constants_set_int32(ctx, signals_obj, "SIGPROF", SIGPROF);
#endif
#ifdef SIGWINCH
  js_constants_set_int32(ctx, signals_obj, "SIGWINCH", SIGWINCH);
#endif
#ifdef SIGIO
  js_constants_set_int32(ctx, signals_obj, "SIGIO", SIGIO);
#endif
#ifdef SIGSYS
  js_constants_set_int32(ctx, signals_obj, "SIGSYS", SIGSYS);
#endif
#endif  // !_WIN32

  JSRT_Debug("Created signal constants with platform-specific values");
  return signals_obj;
}

// File access mode constants (platform-specific)
static JSValue js_constants_create_faccess(JSContext* ctx) {
  JSValue faccess_obj = js_constants_create_category(ctx, "faccess");
  if (JS_IsUndefined(faccess_obj)) {
    return JS_UNDEFINED;
  }

// Use platform-specific values from system headers
#ifdef F_OK
  js_constants_set_int32(ctx, faccess_obj, "F_OK", F_OK);
#else
  js_constants_set_int32(ctx, faccess_obj, "F_OK", 0);
#endif
#ifdef R_OK
  js_constants_set_int32(ctx, faccess_obj, "R_OK", R_OK);
#else
  js_constants_set_int32(ctx, faccess_obj, "R_OK", 4);
#endif
#ifdef W_OK
  js_constants_set_int32(ctx, faccess_obj, "W_OK", W_OK);
#else
  js_constants_set_int32(ctx, faccess_obj, "W_OK", 2);
#endif
#ifdef X_OK
  js_constants_set_int32(ctx, faccess_obj, "X_OK", X_OK);
#else
  js_constants_set_int32(ctx, faccess_obj, "X_OK", 1);
#endif

  return faccess_obj;
}

// File open flags constants (platform-specific)
static JSValue js_constants_create_fopen(JSContext* ctx) {
  JSValue fopen_obj = js_constants_create_category(ctx, "fopen");
  if (JS_IsUndefined(fopen_obj)) {
    return JS_UNDEFINED;
  }

// Use platform-specific open flags from system headers
#ifdef O_RDONLY
  js_constants_set_int32(ctx, fopen_obj, "O_RDONLY", O_RDONLY);
#else
  js_constants_set_int32(ctx, fopen_obj, "O_RDONLY", 0);
#endif
#ifdef O_WRONLY
  js_constants_set_int32(ctx, fopen_obj, "O_WRONLY", O_WRONLY);
#else
  js_constants_set_int32(ctx, fopen_obj, "O_WRONLY", 1);
#endif
#ifdef O_RDWR
  js_constants_set_int32(ctx, fopen_obj, "O_RDWR", O_RDWR);
#else
  js_constants_set_int32(ctx, fopen_obj, "O_RDWR", 2);
#endif
#ifdef O_CREAT
  js_constants_set_int32(ctx, fopen_obj, "O_CREAT", O_CREAT);
#endif
#ifdef O_EXCL
  js_constants_set_int32(ctx, fopen_obj, "O_EXCL", O_EXCL);
#endif
#ifdef O_NOCTTY
  js_constants_set_int32(ctx, fopen_obj, "O_NOCTTY", O_NOCTTY);
#endif
#ifdef O_TRUNC
  js_constants_set_int32(ctx, fopen_obj, "O_TRUNC", O_TRUNC);
#endif
#ifdef O_APPEND
  js_constants_set_int32(ctx, fopen_obj, "O_APPEND", O_APPEND);
#endif
#ifdef O_NONBLOCK
  js_constants_set_int32(ctx, fopen_obj, "O_NONBLOCK", O_NONBLOCK);
#endif
#ifdef O_DSYNC
  js_constants_set_int32(ctx, fopen_obj, "O_DSYNC", O_DSYNC);
#endif
#ifdef O_SYNC
  js_constants_set_int32(ctx, fopen_obj, "O_SYNC", O_SYNC);
#endif
#ifdef O_RSYNC
  js_constants_set_int32(ctx, fopen_obj, "O_RSYNC", O_RSYNC);
#endif
#ifdef O_DIRECTORY
  js_constants_set_int32(ctx, fopen_obj, "O_DIRECTORY", O_DIRECTORY);
#endif
#ifdef O_NOFOLLOW
  js_constants_set_int32(ctx, fopen_obj, "O_NOFOLLOW", O_NOFOLLOW);
#endif
#ifdef O_CLOEXEC
  js_constants_set_int32(ctx, fopen_obj, "O_CLOEXEC", O_CLOEXEC);
#endif

  return fopen_obj;
}

// File type constants (platform-specific)
static JSValue js_constants_create_filetype(JSContext* ctx) {
  JSValue filetype_obj = js_constants_create_category(ctx, "filetype");
  if (JS_IsUndefined(filetype_obj)) {
    return JS_UNDEFINED;
  }

// Use platform-specific file type constants from system headers
#ifdef S_IFMT
  js_constants_set_int32(ctx, filetype_obj, "S_IFMT", S_IFMT);
#else
  js_constants_set_int32(ctx, filetype_obj, "S_IFMT", 0170000);
#endif
#ifdef S_IFREG
  js_constants_set_int32(ctx, filetype_obj, "S_IFREG", S_IFREG);
#else
  js_constants_set_int32(ctx, filetype_obj, "S_IFREG", 0100000);
#endif
#ifdef S_IFDIR
  js_constants_set_int32(ctx, filetype_obj, "S_IFDIR", S_IFDIR);
#else
  js_constants_set_int32(ctx, filetype_obj, "S_IFDIR", 0040000);
#endif
#ifdef S_IFCHR
  js_constants_set_int32(ctx, filetype_obj, "S_IFCHR", S_IFCHR);
#else
  js_constants_set_int32(ctx, filetype_obj, "S_IFCHR", 0020000);
#endif
#ifdef S_IFBLK
  js_constants_set_int32(ctx, filetype_obj, "S_IFBLK", S_IFBLK);
#else
  js_constants_set_int32(ctx, filetype_obj, "S_IFBLK", 0060000);
#endif
#ifdef S_IFIFO
  js_constants_set_int32(ctx, filetype_obj, "S_IFIFO", S_IFIFO);
#else
  js_constants_set_int32(ctx, filetype_obj, "S_IFIFO", 0010000);
#endif
#ifdef S_IFLNK
  js_constants_set_int32(ctx, filetype_obj, "S_IFLNK", S_IFLNK);
#else
  js_constants_set_int32(ctx, filetype_obj, "S_IFLNK", 0120000);
#endif
#ifdef S_IFSOCK
  js_constants_set_int32(ctx, filetype_obj, "S_IFSOCK", S_IFSOCK);
#else
  js_constants_set_int32(ctx, filetype_obj, "S_IFSOCK", 0140000);
#endif

  return filetype_obj;
}

// Permissions constants (platform-specific)
static JSValue js_constants_create_permissions(JSContext* ctx) {
  JSValue perm_obj = js_constants_create_category(ctx, "permissions");
  if (JS_IsUndefined(perm_obj)) {
    return JS_UNDEFINED;
  }

// Use platform-specific permission constants from system headers
#ifdef S_IRWXU
  js_constants_set_int32(ctx, perm_obj, "S_IRWXU", S_IRWXU);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IRWXU", 00700);
#endif
#ifdef S_IRUSR
  js_constants_set_int32(ctx, perm_obj, "S_IRUSR", S_IRUSR);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IRUSR", 00400);
#endif
#ifdef S_IWUSR
  js_constants_set_int32(ctx, perm_obj, "S_IWUSR", S_IWUSR);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IWUSR", 00200);
#endif
#ifdef S_IXUSR
  js_constants_set_int32(ctx, perm_obj, "S_IXUSR", S_IXUSR);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IXUSR", 00100);
#endif
#ifdef S_IRWXG
  js_constants_set_int32(ctx, perm_obj, "S_IRWXG", S_IRWXG);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IRWXG", 00070);
#endif
#ifdef S_IRGRP
  js_constants_set_int32(ctx, perm_obj, "S_IRGRP", S_IRGRP);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IRGRP", 00040);
#endif
#ifdef S_IWGRP
  js_constants_set_int32(ctx, perm_obj, "S_IWGRP", S_IWGRP);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IWGRP", 00020);
#endif
#ifdef S_IXGRP
  js_constants_set_int32(ctx, perm_obj, "S_IXGRP", S_IXGRP);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IXGRP", 00010);
#endif
#ifdef S_IRWXO
  js_constants_set_int32(ctx, perm_obj, "S_IRWXO", S_IRWXO);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IRWXO", 00007);
#endif
#ifdef S_IROTH
  js_constants_set_int32(ctx, perm_obj, "S_IROTH", S_IROTH);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IROTH", 00004);
#endif
#ifdef S_IWOTH
  js_constants_set_int32(ctx, perm_obj, "S_IWOTH", S_IWOTH);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IWOTH", 00002);
#endif
#ifdef S_IXOTH
  js_constants_set_int32(ctx, perm_obj, "S_IXOTH", S_IXOTH);
#else
  js_constants_set_int32(ctx, perm_obj, "S_IXOTH", 00001);
#endif

  return perm_obj;
}

// Cross-module integration functions

// Import os.constants safely with error handling
static JSValue js_constants_import_os_constants(JSContext* ctx) {
  JSRT_Debug("Attempting to import os.constants");

  JSValue os_module = JSRT_InitNodeOs(ctx);
  if (JS_IsException(os_module)) {
    JSRT_Debug("Failed to initialize os module");
    JS_FreeValue(ctx, os_module);
    return JS_UNDEFINED;
  }

  JSValue os_constants = js_constants_extract_property(ctx, os_module, "constants");
  JS_FreeValue(ctx, os_module);

  if (JS_IsUndefined(os_constants)) {
    JSRT_Debug("os.constants not available, will use built-in implementation");
  } else {
    JSRT_Debug("Successfully imported os.constants");
  }

  return os_constants;
}

// Import fs.constants safely with error handling
static JSValue js_constants_import_fs_constants(JSContext* ctx) {
  JSRT_Debug("Attempting to import fs.constants");

  JSValue fs_module = JSRT_InitNodeFs(ctx);
  if (JS_IsException(fs_module)) {
    JSRT_Debug("Failed to initialize fs module");
    JS_FreeValue(ctx, fs_module);
    return JS_UNDEFINED;
  }

  JSValue fs_constants = js_constants_extract_property(ctx, fs_module, "constants");
  JS_FreeValue(ctx, fs_module);

  if (JS_IsUndefined(fs_constants)) {
    JSRT_Debug("fs.constants not available, will use built-in implementation");
  } else {
    JSRT_Debug("Successfully imported fs.constants");
  }

  return fs_constants;
}

// Import crypto.constants safely with error handling
static JSValue js_constants_import_crypto_constants(JSContext* ctx) {
  JSRT_Debug("Attempting to import crypto.constants");

  JSValue crypto_constants = create_crypto_constants(ctx);
  if (JS_IsException(crypto_constants)) {
    JSRT_Debug("Failed to initialize crypto constants");
    JS_FreeValue(ctx, crypto_constants);
    return JS_UNDEFINED;
  }

  if (JS_IsUndefined(crypto_constants)) {
    JSRT_Debug("crypto.constants not available");
  } else {
    JSRT_Debug("Successfully imported crypto.constants");
  }

  return crypto_constants;
}

// Merge priority constants from os.constants
static void js_constants_merge_priority_constants(JSContext* ctx, JSValue constants_obj, JSValue os_constants) {
  if (JS_IsUndefined(os_constants)) {
    // Add fallback priority constants
    JSValue priority_obj = js_constants_create_category(ctx, "priority");
    if (!JS_IsUndefined(priority_obj)) {
      js_constants_set_int32(ctx, priority_obj, "PRIORITY_LOW", 19);
      js_constants_set_int32(ctx, priority_obj, "PRIORITY_BELOW_NORMAL", 10);
      js_constants_set_int32(ctx, priority_obj, "PRIORITY_NORMAL", 0);
      js_constants_set_int32(ctx, priority_obj, "PRIORITY_ABOVE_NORMAL", -7);
      js_constants_set_int32(ctx, priority_obj, "PRIORITY_HIGH", -14);
      js_constants_set_int32(ctx, priority_obj, "PRIORITY_HIGHEST", -20);

      JS_SetPropertyStr(ctx, constants_obj, "priority", priority_obj);
      JSRT_Debug("Added fallback priority constants");
    }
    return;
  }

  JSValue priority = js_constants_extract_property(ctx, os_constants, "priority");
  if (!JS_IsUndefined(priority)) {
    JS_SetPropertyStr(ctx, constants_obj, "priority", priority);
    JSRT_Debug("Merged priority constants from os.constants");
  } else {
    JS_FreeValue(ctx, priority);
  }
}

// Merge fs access constants for backward compatibility
static void js_constants_merge_fs_access_constants(JSContext* ctx, JSValue constants_obj, JSValue fs_constants) {
  if (JS_IsUndefined(fs_constants)) {
    // Use our built-in access constants
    JS_SetPropertyStr(ctx, constants_obj, "F_OK", JS_NewInt32(ctx, F_OK));
    JS_SetPropertyStr(ctx, constants_obj, "R_OK", JS_NewInt32(ctx, R_OK));
    JS_SetPropertyStr(ctx, constants_obj, "W_OK", JS_NewInt32(ctx, W_OK));
    JS_SetPropertyStr(ctx, constants_obj, "X_OK", JS_NewInt32(ctx, X_OK));
    JSRT_Debug("Added built-in fs access constants");
    return;
  }

  // Merge access constants from fs.constants
  JSValue f_ok = js_constants_extract_property(ctx, fs_constants, "F_OK");
  JSValue r_ok = js_constants_extract_property(ctx, fs_constants, "R_OK");
  JSValue w_ok = js_constants_extract_property(ctx, fs_constants, "W_OK");
  JSValue x_ok = js_constants_extract_property(ctx, fs_constants, "X_OK");

  if (!JS_IsUndefined(f_ok)) {
    JS_SetPropertyStr(ctx, constants_obj, "F_OK", f_ok);
  } else {
    JS_FreeValue(ctx, f_ok);
    JS_SetPropertyStr(ctx, constants_obj, "F_OK", JS_NewInt32(ctx, F_OK));
  }

  if (!JS_IsUndefined(r_ok)) {
    JS_SetPropertyStr(ctx, constants_obj, "R_OK", r_ok);
  } else {
    JS_FreeValue(ctx, r_ok);
    JS_SetPropertyStr(ctx, constants_obj, "R_OK", JS_NewInt32(ctx, R_OK));
  }

  if (!JS_IsUndefined(w_ok)) {
    JS_SetPropertyStr(ctx, constants_obj, "W_OK", w_ok);
  } else {
    JS_FreeValue(ctx, w_ok);
    JS_SetPropertyStr(ctx, constants_obj, "W_OK", JS_NewInt32(ctx, W_OK));
  }

  if (!JS_IsUndefined(x_ok)) {
    JS_SetPropertyStr(ctx, constants_obj, "X_OK", x_ok);
  } else {
    JS_FreeValue(ctx, x_ok);
    JS_SetPropertyStr(ctx, constants_obj, "X_OK", JS_NewInt32(ctx, X_OK));
  }

  JSRT_Debug("Merged fs access constants from fs.constants");
}

// Constants module initialization (CommonJS) - Enhanced with cross-module consolidation
JSValue JSRT_InitNodeConstants(JSContext* ctx) {
  JSRT_Debug("Initializing constants module with cross-module consolidation");

  JSValue constants_obj = JS_NewObject(ctx);
  if (JS_IsException(constants_obj)) {
    JSRT_Debug("Failed to create constants module object");
    return JS_UNDEFINED;
  }

  // Import constants from other modules for consolidation
  JSValue os_constants = js_constants_import_os_constants(ctx);
  JSValue fs_constants = js_constants_import_fs_constants(ctx);
  JSValue crypto_constants = js_constants_import_crypto_constants(ctx);

  // Core OS constants (our enhanced implementations)
  JSValue errno_obj = js_constants_create_errno(ctx);
  JSValue signals_obj = js_constants_create_signals(ctx);

  if (!JS_IsUndefined(errno_obj)) {
    JS_SetPropertyStr(ctx, constants_obj, "errno", errno_obj);
  }
  if (!JS_IsUndefined(signals_obj)) {
    JS_SetPropertyStr(ctx, constants_obj, "signals", signals_obj);
  }

  // Merge priority constants from os module or use fallback
  js_constants_merge_priority_constants(ctx, constants_obj, os_constants);

  // Merge fs access constants for backward compatibility
  js_constants_merge_fs_access_constants(ctx, constants_obj, fs_constants);

  // File system constants (our enhanced implementations)
  JSValue faccess_obj = js_constants_create_faccess(ctx);
  JSValue fopen_obj = js_constants_create_fopen(ctx);
  JSValue filetype_obj = js_constants_create_filetype(ctx);
  JSValue permissions_obj = js_constants_create_permissions(ctx);

  if (!JS_IsUndefined(faccess_obj)) {
    JS_SetPropertyStr(ctx, constants_obj, "faccess", faccess_obj);
  }
  if (!JS_IsUndefined(fopen_obj)) {
    JS_SetPropertyStr(ctx, constants_obj, "fopen", fopen_obj);
  }
  if (!JS_IsUndefined(filetype_obj)) {
    JS_SetPropertyStr(ctx, constants_obj, "filetype", filetype_obj);
  }
  if (!JS_IsUndefined(permissions_obj)) {
    JS_SetPropertyStr(ctx, constants_obj, "permissions", permissions_obj);
  }

  // Add crypto constants if available
  if (!JS_IsUndefined(crypto_constants)) {
    JS_SetPropertyStr(ctx, constants_obj, "crypto", crypto_constants);
    JSRT_Debug("Added crypto constants to consolidated module");
  }

  // Additional commonly used constants (libuv-specific)
  js_constants_set_int32(ctx, constants_obj, "UV_FS_O_APPEND", 1024);
  js_constants_set_int32(ctx, constants_obj, "UV_FS_O_CREAT", 64);
  js_constants_set_int32(ctx, constants_obj, "UV_FS_O_EXCL", 128);
  js_constants_set_int32(ctx, constants_obj, "UV_FS_O_RDONLY", 0);
  js_constants_set_int32(ctx, constants_obj, "UV_FS_O_RDWR", 2);
  js_constants_set_int32(ctx, constants_obj, "UV_FS_O_TRUNC", 512);
  js_constants_set_int32(ctx, constants_obj, "UV_FS_O_WRONLY", 1);

  // Cleanup imported module references
  if (!JS_IsUndefined(os_constants)) {
    JS_FreeValue(ctx, os_constants);
  }
  if (!JS_IsUndefined(fs_constants)) {
    JS_FreeValue(ctx, fs_constants);
  }
  // crypto_constants is now owned by constants_obj, no need to free

  JSRT_Debug("Constants module initialization completed with cross-module consolidation");
  return constants_obj;
}

// Constants module initialization (ES Module) - Enhanced with comprehensive exports
int js_node_constants_init(JSContext* ctx, JSModuleDef* m) {
  JSValue constants_obj = JSRT_InitNodeConstants(ctx);
  if (JS_IsException(constants_obj)) {
    JSRT_Debug("Failed to initialize constants for ES module");
    return -1;
  }

  // Core exports - errno and signals
  JSValue errno_val = JS_GetPropertyStr(ctx, constants_obj, "errno");
  JSValue signals_val = JS_GetPropertyStr(ctx, constants_obj, "signals");
  JSValue priority_val = JS_GetPropertyStr(ctx, constants_obj, "priority");

  if (!JS_IsUndefined(errno_val)) {
    JS_SetModuleExport(ctx, m, "errno", errno_val);
  }
  if (!JS_IsUndefined(signals_val)) {
    JS_SetModuleExport(ctx, m, "signals", signals_val);
  }
  if (!JS_IsUndefined(priority_val)) {
    JS_SetModuleExport(ctx, m, "priority", priority_val);
  }

  // File access constants (for backward compatibility)
  JSValue f_ok_val = JS_GetPropertyStr(ctx, constants_obj, "F_OK");
  JSValue r_ok_val = JS_GetPropertyStr(ctx, constants_obj, "R_OK");
  JSValue w_ok_val = JS_GetPropertyStr(ctx, constants_obj, "W_OK");
  JSValue x_ok_val = JS_GetPropertyStr(ctx, constants_obj, "X_OK");

  if (!JS_IsUndefined(f_ok_val)) {
    JS_SetModuleExport(ctx, m, "F_OK", f_ok_val);
  }
  if (!JS_IsUndefined(r_ok_val)) {
    JS_SetModuleExport(ctx, m, "R_OK", r_ok_val);
  }
  if (!JS_IsUndefined(w_ok_val)) {
    JS_SetModuleExport(ctx, m, "W_OK", w_ok_val);
  }
  if (!JS_IsUndefined(x_ok_val)) {
    JS_SetModuleExport(ctx, m, "X_OK", x_ok_val);
  }

  // File system category exports
  JSValue faccess_val = JS_GetPropertyStr(ctx, constants_obj, "faccess");
  JSValue fopen_val = JS_GetPropertyStr(ctx, constants_obj, "fopen");
  JSValue filetype_val = JS_GetPropertyStr(ctx, constants_obj, "filetype");
  JSValue permissions_val = JS_GetPropertyStr(ctx, constants_obj, "permissions");

  if (!JS_IsUndefined(faccess_val)) {
    JS_SetModuleExport(ctx, m, "faccess", faccess_val);
  }
  if (!JS_IsUndefined(fopen_val)) {
    JS_SetModuleExport(ctx, m, "fopen", fopen_val);
  }
  if (!JS_IsUndefined(filetype_val)) {
    JS_SetModuleExport(ctx, m, "filetype", filetype_val);
  }
  if (!JS_IsUndefined(permissions_val)) {
    JS_SetModuleExport(ctx, m, "permissions", permissions_val);
  }

  // Crypto constants export (if available)
  JSValue crypto_val = JS_GetPropertyStr(ctx, constants_obj, "crypto");
  if (!JS_IsUndefined(crypto_val)) {
    JS_SetModuleExport(ctx, m, "crypto", crypto_val);
  }

  // Export the entire module as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, constants_obj));

  // Cleanup temporary values (except those now owned by module exports)
  JS_FreeValue(ctx, errno_val);
  JS_FreeValue(ctx, signals_val);
  JS_FreeValue(ctx, priority_val);
  JS_FreeValue(ctx, faccess_val);
  JS_FreeValue(ctx, fopen_val);
  JS_FreeValue(ctx, filetype_val);
  JS_FreeValue(ctx, permissions_val);
  JS_FreeValue(ctx, crypto_val);
  // Note: F_OK, R_OK, W_OK, X_OK are now owned by module exports, no need to free

  JS_FreeValue(ctx, constants_obj);

  JSRT_Debug("Constants ES module initialization completed with comprehensive exports");
  return 0;
}