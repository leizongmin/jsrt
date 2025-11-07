#include <string.h>
#include "../runtime.h"
#include "../std/assert.h"
#include "../util/debug.h"
#include "node_modules.h"

// Constants module - provides OS-level constants (errno, signals, etc.)
// This is a minimal implementation with essential constants for npm compatibility

// OS errno constants (subset)
static JSValue js_constants_create_errno(JSContext* ctx) {
  JSValue errno_obj = JS_NewObject(ctx);

  // Common errno values
  JS_SetPropertyStr(ctx, errno_obj, "E2BIG", JS_NewInt32(ctx, 7));
  JS_SetPropertyStr(ctx, errno_obj, "EACCES", JS_NewInt32(ctx, 13));
  JS_SetPropertyStr(ctx, errno_obj, "EADDRINUSE", JS_NewInt32(ctx, 98));
  JS_SetPropertyStr(ctx, errno_obj, "EADDRNOTAVAIL", JS_NewInt32(ctx, 99));
  JS_SetPropertyStr(ctx, errno_obj, "EAFNOSUPPORT", JS_NewInt32(ctx, 97));
  JS_SetPropertyStr(ctx, errno_obj, "EAGAIN", JS_NewInt32(ctx, 11));
  JS_SetPropertyStr(ctx, errno_obj, "EALREADY", JS_NewInt32(ctx, 114));
  JS_SetPropertyStr(ctx, errno_obj, "EBADF", JS_NewInt32(ctx, 9));
  JS_SetPropertyStr(ctx, errno_obj, "EBUSY", JS_NewInt32(ctx, 16));
  JS_SetPropertyStr(ctx, errno_obj, "ECHILD", JS_NewInt32(ctx, 10));
  JS_SetPropertyStr(ctx, errno_obj, "ECONNABORTED", JS_NewInt32(ctx, 103));
  JS_SetPropertyStr(ctx, errno_obj, "ECONNREFUSED", JS_NewInt32(ctx, 111));
  JS_SetPropertyStr(ctx, errno_obj, "ECONNRESET", JS_NewInt32(ctx, 104));
  JS_SetPropertyStr(ctx, errno_obj, "EDEADLK", JS_NewInt32(ctx, 35));
  JS_SetPropertyStr(ctx, errno_obj, "EDESTADDRREQ", JS_NewInt32(ctx, 89));
  JS_SetPropertyStr(ctx, errno_obj, "EDOM", JS_NewInt32(ctx, 33));
  JS_SetPropertyStr(ctx, errno_obj, "EEXIST", JS_NewInt32(ctx, 17));
  JS_SetPropertyStr(ctx, errno_obj, "EFAULT", JS_NewInt32(ctx, 14));
  JS_SetPropertyStr(ctx, errno_obj, "EFBIG", JS_NewInt32(ctx, 27));
  JS_SetPropertyStr(ctx, errno_obj, "EHOSTUNREACH", JS_NewInt32(ctx, 113));
  JS_SetPropertyStr(ctx, errno_obj, "EINTR", JS_NewInt32(ctx, 4));
  JS_SetPropertyStr(ctx, errno_obj, "EINVAL", JS_NewInt32(ctx, 22));
  JS_SetPropertyStr(ctx, errno_obj, "EIO", JS_NewInt32(ctx, 5));
  JS_SetPropertyStr(ctx, errno_obj, "EISCONN", JS_NewInt32(ctx, 106));
  JS_SetPropertyStr(ctx, errno_obj, "EISDIR", JS_NewInt32(ctx, 21));
  JS_SetPropertyStr(ctx, errno_obj, "ELOOP", JS_NewInt32(ctx, 40));
  JS_SetPropertyStr(ctx, errno_obj, "EMFILE", JS_NewInt32(ctx, 24));
  JS_SetPropertyStr(ctx, errno_obj, "EMLINK", JS_NewInt32(ctx, 31));
  JS_SetPropertyStr(ctx, errno_obj, "EMSGSIZE", JS_NewInt32(ctx, 90));
  JS_SetPropertyStr(ctx, errno_obj, "ENAMETOOLONG", JS_NewInt32(ctx, 36));
  JS_SetPropertyStr(ctx, errno_obj, "ENETDOWN", JS_NewInt32(ctx, 100));
  JS_SetPropertyStr(ctx, errno_obj, "ENETRESET", JS_NewInt32(ctx, 102));
  JS_SetPropertyStr(ctx, errno_obj, "ENETUNREACH", JS_NewInt32(ctx, 101));
  JS_SetPropertyStr(ctx, errno_obj, "ENFILE", JS_NewInt32(ctx, 23));
  JS_SetPropertyStr(ctx, errno_obj, "ENOBUFS", JS_NewInt32(ctx, 105));
  JS_SetPropertyStr(ctx, errno_obj, "ENODATA", JS_NewInt32(ctx, 61));
  JS_SetPropertyStr(ctx, errno_obj, "ENODEV", JS_NewInt32(ctx, 19));
  JS_SetPropertyStr(ctx, errno_obj, "ENOENT", JS_NewInt32(ctx, 2));
  JS_SetPropertyStr(ctx, errno_obj, "ENOEXEC", JS_NewInt32(ctx, 8));
  JS_SetPropertyStr(ctx, errno_obj, "ENOLINK", JS_NewInt32(ctx, 67));
  JS_SetPropertyStr(ctx, errno_obj, "ENOMEM", JS_NewInt32(ctx, 12));
  JS_SetPropertyStr(ctx, errno_obj, "ENOMSG", JS_NewInt32(ctx, 42));
  JS_SetPropertyStr(ctx, errno_obj, "ENOPROTOOPT", JS_NewInt32(ctx, 92));
  JS_SetPropertyStr(ctx, errno_obj, "ENOSPC", JS_NewInt32(ctx, 28));
  JS_SetPropertyStr(ctx, errno_obj, "ENOSR", JS_NewInt32(ctx, 63));
  JS_SetPropertyStr(ctx, errno_obj, "ENOSTR", JS_NewInt32(ctx, 60));
  JS_SetPropertyStr(ctx, errno_obj, "ENOSYS", JS_NewInt32(ctx, 38));
  JS_SetPropertyStr(ctx, errno_obj, "ENOTCONN", JS_NewInt32(ctx, 107));
  JS_SetPropertyStr(ctx, errno_obj, "ENOTDIR", JS_NewInt32(ctx, 20));
  JS_SetPropertyStr(ctx, errno_obj, "ENOTEMPTY", JS_NewInt32(ctx, 39));
  JS_SetPropertyStr(ctx, errno_obj, "ENOTSOCK", JS_NewInt32(ctx, 88));
  JS_SetPropertyStr(ctx, errno_obj, "ENOTSUP", JS_NewInt32(ctx, 95));
  JS_SetPropertyStr(ctx, errno_obj, "ENOTTY", JS_NewInt32(ctx, 25));
  JS_SetPropertyStr(ctx, errno_obj, "ENXIO", JS_NewInt32(ctx, 6));
  JS_SetPropertyStr(ctx, errno_obj, "EOPNOTSUPP", JS_NewInt32(ctx, 95));
  JS_SetPropertyStr(ctx, errno_obj, "EOVERFLOW", JS_NewInt32(ctx, 75));
  JS_SetPropertyStr(ctx, errno_obj, "EPERM", JS_NewInt32(ctx, 1));
  JS_SetPropertyStr(ctx, errno_obj, "EPIPE", JS_NewInt32(ctx, 32));
  JS_SetPropertyStr(ctx, errno_obj, "EPROTO", JS_NewInt32(ctx, 71));
  JS_SetPropertyStr(ctx, errno_obj, "EPROTONOSUPPORT", JS_NewInt32(ctx, 93));
  JS_SetPropertyStr(ctx, errno_obj, "EPROTOTYPE", JS_NewInt32(ctx, 91));
  JS_SetPropertyStr(ctx, errno_obj, "ERANGE", JS_NewInt32(ctx, 34));
  JS_SetPropertyStr(ctx, errno_obj, "EROFS", JS_NewInt32(ctx, 30));
  JS_SetPropertyStr(ctx, errno_obj, "ESPIPE", JS_NewInt32(ctx, 29));
  JS_SetPropertyStr(ctx, errno_obj, "ESRCH", JS_NewInt32(ctx, 3));
  JS_SetPropertyStr(ctx, errno_obj, "ETIME", JS_NewInt32(ctx, 62));
  JS_SetPropertyStr(ctx, errno_obj, "ETIMEDOUT", JS_NewInt32(ctx, 110));
  JS_SetPropertyStr(ctx, errno_obj, "ETXTBSY", JS_NewInt32(ctx, 26));
  JS_SetPropertyStr(ctx, errno_obj, "EWOULDBLOCK", JS_NewInt32(ctx, 11));
  JS_SetPropertyStr(ctx, errno_obj, "EXDEV", JS_NewInt32(ctx, 18));

  return errno_obj;
}

// Signal constants (subset)
static JSValue js_constants_create_signals(JSContext* ctx) {
  JSValue signals_obj = JS_NewObject(ctx);

  // Common signals
  JS_SetPropertyStr(ctx, signals_obj, "SIGHUP", JS_NewInt32(ctx, 1));
  JS_SetPropertyStr(ctx, signals_obj, "SIGINT", JS_NewInt32(ctx, 2));
  JS_SetPropertyStr(ctx, signals_obj, "SIGQUIT", JS_NewInt32(ctx, 3));
  JS_SetPropertyStr(ctx, signals_obj, "SIGILL", JS_NewInt32(ctx, 4));
  JS_SetPropertyStr(ctx, signals_obj, "SIGTRAP", JS_NewInt32(ctx, 5));
  JS_SetPropertyStr(ctx, signals_obj, "SIGABRT", JS_NewInt32(ctx, 6));
  JS_SetPropertyStr(ctx, signals_obj, "SIGBUS", JS_NewInt32(ctx, 7));
  JS_SetPropertyStr(ctx, signals_obj, "SIGFPE", JS_NewInt32(ctx, 8));
  JS_SetPropertyStr(ctx, signals_obj, "SIGKILL", JS_NewInt32(ctx, 9));
  JS_SetPropertyStr(ctx, signals_obj, "SIGUSR1", JS_NewInt32(ctx, 10));
  JS_SetPropertyStr(ctx, signals_obj, "SIGSEGV", JS_NewInt32(ctx, 11));
  JS_SetPropertyStr(ctx, signals_obj, "SIGUSR2", JS_NewInt32(ctx, 12));
  JS_SetPropertyStr(ctx, signals_obj, "SIGPIPE", JS_NewInt32(ctx, 13));
  JS_SetPropertyStr(ctx, signals_obj, "SIGALRM", JS_NewInt32(ctx, 14));
  JS_SetPropertyStr(ctx, signals_obj, "SIGTERM", JS_NewInt32(ctx, 15));
  JS_SetPropertyStr(ctx, signals_obj, "SIGSTKFLT", JS_NewInt32(ctx, 16));
  JS_SetPropertyStr(ctx, signals_obj, "SIGCHLD", JS_NewInt32(ctx, 17));
  JS_SetPropertyStr(ctx, signals_obj, "SIGCONT", JS_NewInt32(ctx, 18));
  JS_SetPropertyStr(ctx, signals_obj, "SIGSTOP", JS_NewInt32(ctx, 19));
  JS_SetPropertyStr(ctx, signals_obj, "SIGTSTP", JS_NewInt32(ctx, 20));
  JS_SetPropertyStr(ctx, signals_obj, "SIGTTIN", JS_NewInt32(ctx, 21));
  JS_SetPropertyStr(ctx, signals_obj, "SIGTTOU", JS_NewInt32(ctx, 22));

  return signals_obj;
}

// File access mode constants
static JSValue js_constants_create_faccess(JSContext* ctx) {
  JSValue faccess_obj = JS_NewObject(ctx);

  // File access constants
  JS_SetPropertyStr(ctx, faccess_obj, "F_OK", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, faccess_obj, "R_OK", JS_NewInt32(ctx, 4));
  JS_SetPropertyStr(ctx, faccess_obj, "W_OK", JS_NewInt32(ctx, 2));
  JS_SetPropertyStr(ctx, faccess_obj, "X_OK", JS_NewInt32(ctx, 1));

  return faccess_obj;
}

// File open flags constants
static JSValue js_constants_create_fopen(JSContext* ctx) {
  JSValue fopen_obj = JS_NewObject(ctx);

  // File open flags
  JS_SetPropertyStr(ctx, fopen_obj, "O_RDONLY", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, fopen_obj, "O_WRONLY", JS_NewInt32(ctx, 1));
  JS_SetPropertyStr(ctx, fopen_obj, "O_RDWR", JS_NewInt32(ctx, 2));
  JS_SetPropertyStr(ctx, fopen_obj, "O_CREAT", JS_NewInt32(ctx, 64));
  JS_SetPropertyStr(ctx, fopen_obj, "O_EXCL", JS_NewInt32(ctx, 128));
  JS_SetPropertyStr(ctx, fopen_obj, "O_NOCTTY", JS_NewInt32(ctx, 256));
  JS_SetPropertyStr(ctx, fopen_obj, "O_TRUNC", JS_NewInt32(ctx, 512));
  JS_SetPropertyStr(ctx, fopen_obj, "O_APPEND", JS_NewInt32(ctx, 1024));
  JS_SetPropertyStr(ctx, fopen_obj, "O_NONBLOCK", JS_NewInt32(ctx, 2048));
  JS_SetPropertyStr(ctx, fopen_obj, "O_DSYNC", JS_NewInt32(ctx, 4096));
  JS_SetPropertyStr(ctx, fopen_obj, "O_SYNC", JS_NewInt32(ctx, 1052672));
  JS_SetPropertyStr(ctx, fopen_obj, "O_RSYNC", JS_NewInt32(ctx, 1052672));
  JS_SetPropertyStr(ctx, fopen_obj, "O_DIRECTORY", JS_NewInt32(ctx, 65536));
  JS_SetPropertyStr(ctx, fopen_obj, "O_NOFOLLOW", JS_NewInt32(ctx, 131072));
  JS_SetPropertyStr(ctx, fopen_obj, "O_SYMLINK", JS_NewInt32(ctx, 2097152));
  JS_SetPropertyStr(ctx, fopen_obj, "O_NOATIME", JS_NewInt32(ctx, 262144));
  JS_SetPropertyStr(ctx, fopen_obj, "O_CLOEXEC", JS_NewInt32(ctx, 524288));

  return fopen_obj;
}

// File type constants
static JSValue js_constants_create_filetype(JSContext* ctx) {
  JSValue filetype_obj = JS_NewObject(ctx);

  // File type constants for stat mode
  JS_SetPropertyStr(ctx, filetype_obj, "S_IFMT", JS_NewInt32(ctx, 61440));
  JS_SetPropertyStr(ctx, filetype_obj, "S_IFREG", JS_NewInt32(ctx, 32768));
  JS_SetPropertyStr(ctx, filetype_obj, "S_IFDIR", JS_NewInt32(ctx, 16384));
  JS_SetPropertyStr(ctx, filetype_obj, "S_IFCHR", JS_NewInt32(ctx, 8192));
  JS_SetPropertyStr(ctx, filetype_obj, "S_IFBLK", JS_NewInt32(ctx, 24576));
  JS_SetPropertyStr(ctx, filetype_obj, "S_IFIFO", JS_NewInt32(ctx, 4096));
  JS_SetPropertyStr(ctx, filetype_obj, "S_IFLNK", JS_NewInt32(ctx, 40960));
  JS_SetPropertyStr(ctx, filetype_obj, "S_IFSOCK", JS_NewInt32(ctx, 49152));

  return filetype_obj;
}

// Permissions constants
static JSValue js_constants_create_permissions(JSContext* ctx) {
  JSValue perm_obj = JS_NewObject(ctx);

  // Permission bits
  JS_SetPropertyStr(ctx, perm_obj, "S_IRWXU", JS_NewInt32(ctx, 448));
  JS_SetPropertyStr(ctx, perm_obj, "S_IRUSR", JS_NewInt32(ctx, 256));
  JS_SetPropertyStr(ctx, perm_obj, "S_IWUSR", JS_NewInt32(ctx, 128));
  JS_SetPropertyStr(ctx, perm_obj, "S_IXUSR", JS_NewInt32(ctx, 64));
  JS_SetPropertyStr(ctx, perm_obj, "S_IRWXG", JS_NewInt32(ctx, 56));
  JS_SetPropertyStr(ctx, perm_obj, "S_IRGRP", JS_NewInt32(ctx, 32));
  JS_SetPropertyStr(ctx, perm_obj, "S_IWGRP", JS_NewInt32(ctx, 16));
  JS_SetPropertyStr(ctx, perm_obj, "S_IXGRP", JS_NewInt32(ctx, 8));
  JS_SetPropertyStr(ctx, perm_obj, "S_IRWXO", JS_NewInt32(ctx, 7));
  JS_SetPropertyStr(ctx, perm_obj, "S_IROTH", JS_NewInt32(ctx, 4));
  JS_SetPropertyStr(ctx, perm_obj, "S_IWOTH", JS_NewInt32(ctx, 2));
  JS_SetPropertyStr(ctx, perm_obj, "S_IXOTH", JS_NewInt32(ctx, 1));

  return perm_obj;
}

// Constants module initialization (CommonJS)
JSValue JSRT_InitNodeConstants(JSContext* ctx) {
  JSValue constants_obj = JS_NewObject(ctx);

  // Add all constant categories
  JS_SetPropertyStr(ctx, constants_obj, "errno", js_constants_create_errno(ctx));
  JS_SetPropertyStr(ctx, constants_obj, "signals", js_constants_create_signals(ctx));
  JS_SetPropertyStr(ctx, constants_obj, "F_OK", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, constants_obj, "R_OK", JS_NewInt32(ctx, 4));
  JS_SetPropertyStr(ctx, constants_obj, "W_OK", JS_NewInt32(ctx, 2));
  JS_SetPropertyStr(ctx, constants_obj, "X_OK", JS_NewInt32(ctx, 1));
  JS_SetPropertyStr(ctx, constants_obj, "faccess", js_constants_create_faccess(ctx));
  JS_SetPropertyStr(ctx, constants_obj, "fopen", js_constants_create_fopen(ctx));
  JS_SetPropertyStr(ctx, constants_obj, "filetype", js_constants_create_filetype(ctx));
  JS_SetPropertyStr(ctx, constants_obj, "permissions", js_constants_create_permissions(ctx));

  // Additional commonly used constants
  JS_SetPropertyStr(ctx, constants_obj, "UV_FS_O_APPEND", JS_NewInt32(ctx, 1024));
  JS_SetPropertyStr(ctx, constants_obj, "UV_FS_O_CREAT", JS_NewInt32(ctx, 64));
  JS_SetPropertyStr(ctx, constants_obj, "UV_FS_O_EXCL", JS_NewInt32(ctx, 128));
  JS_SetPropertyStr(ctx, constants_obj, "UV_FS_O_RDONLY", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, constants_obj, "UV_FS_O_RDWR", JS_NewInt32(ctx, 2));
  JS_SetPropertyStr(ctx, constants_obj, "UV_FS_O_TRUNC", JS_NewInt32(ctx, 512));
  JS_SetPropertyStr(ctx, constants_obj, "UV_FS_O_WRONLY", JS_NewInt32(ctx, 1));

  return constants_obj;
}

// Constants module initialization (ES Module)
int js_node_constants_init(JSContext* ctx, JSModuleDef* m) {
  JSValue constants_obj = JSRT_InitNodeConstants(ctx);

  // Add main exports
  JS_SetModuleExport(ctx, m, "errno", JS_GetPropertyStr(ctx, constants_obj, "errno"));
  JS_SetModuleExport(ctx, m, "signals", JS_GetPropertyStr(ctx, constants_obj, "signals"));
  JS_SetModuleExport(ctx, m, "F_OK", JS_GetPropertyStr(ctx, constants_obj, "F_OK"));
  JS_SetModuleExport(ctx, m, "R_OK", JS_GetPropertyStr(ctx, constants_obj, "R_OK"));
  JS_SetModuleExport(ctx, m, "W_OK", JS_GetPropertyStr(ctx, constants_obj, "W_OK"));
  JS_SetModuleExport(ctx, m, "X_OK", JS_GetPropertyStr(ctx, constants_obj, "X_OK"));
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, constants_obj));

  JS_FreeValue(ctx, constants_obj);
  return 0;
}