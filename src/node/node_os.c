#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "node_modules.h"

#ifdef _WIN32
#include <lmcons.h>
#include <process.h>
#include <windows.h>
#include <winsock2.h>
#define JSRT_GETPID() _getpid()
#define JSRT_GETPPID() 0  // Not available on Windows
#define JSRT_GETHOSTNAME(buf, size) (GetComputerNameA(buf, &size) ? 0 : -1)
#else
#include <pwd.h>
#include <sys/time.h>
#define JSRT_GETPID() getpid()
#define JSRT_GETPPID() getppid()
#define JSRT_GETHOSTNAME(buf, size) gethostname(buf, size)
#endif

// node:os.arch implementation
static JSValue js_os_arch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
#ifdef _WIN64
  return JS_NewString(ctx, "x64");
#else
  return JS_NewString(ctx, "ia32");
#endif
#else
  struct utsname uts;
  if (uname(&uts) == 0) {
    if (strcmp(uts.machine, "x86_64") == 0 || strcmp(uts.machine, "amd64") == 0) {
      return JS_NewString(ctx, "x64");
    } else if (strncmp(uts.machine, "arm", 3) == 0) {
      if (strstr(uts.machine, "64")) {
        return JS_NewString(ctx, "arm64");
      } else {
        return JS_NewString(ctx, "arm");
      }
    } else if (strncmp(uts.machine, "aarch64", 7) == 0) {
      return JS_NewString(ctx, "arm64");
    } else if (strcmp(uts.machine, "i386") == 0 || strcmp(uts.machine, "i686") == 0) {
      return JS_NewString(ctx, "ia32");
    }
    return JS_NewString(ctx, uts.machine);
  }
  return JS_NewString(ctx, "unknown");
#endif
}

// node:os.platform implementation
static JSValue js_os_platform(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  return JS_NewString(ctx, "win32");
#elif defined(__APPLE__)
  return JS_NewString(ctx, "darwin");
#elif defined(__linux__)
  return JS_NewString(ctx, "linux");
#elif defined(__FreeBSD__)
  return JS_NewString(ctx, "freebsd");
#elif defined(__OpenBSD__)
  return JS_NewString(ctx, "openbsd");
#elif defined(__NetBSD__)
  return JS_NewString(ctx, "netbsd");
#elif defined(__sun)
  return JS_NewString(ctx, "sunos");
#else
  return JS_NewString(ctx, "unknown");
#endif
}

// node:os.type implementation
static JSValue js_os_type(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  return JS_NewString(ctx, "Windows_NT");
#else
  struct utsname uts;
  if (uname(&uts) == 0) {
    return JS_NewString(ctx, uts.sysname);
  }
  return JS_NewString(ctx, "Unknown");
#endif
}

// node:os.release implementation
static JSValue js_os_release(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  OSVERSIONINFOA osvi;
  ZeroMemory(&osvi, sizeof(OSVERSIONINFOA));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
  if (GetVersionExA(&osvi)) {
    char version[64];
    snprintf(version, sizeof(version), "%d.%d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    return JS_NewString(ctx, version);
  }
  return JS_NewString(ctx, "Unknown");
#else
  struct utsname uts;
  if (uname(&uts) == 0) {
    return JS_NewString(ctx, uts.release);
  }
  return JS_NewString(ctx, "Unknown");
#endif
}

// node:os.hostname implementation
static JSValue js_os_hostname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  char hostname[256];
#ifdef _WIN32
  DWORD size = sizeof(hostname);
  if (JSRT_GETHOSTNAME(hostname, size) == 0) {
#else
  if (JSRT_GETHOSTNAME(hostname, sizeof(hostname)) == 0) {
#endif
    return JS_NewString(ctx, hostname);
  }
  return JS_NewString(ctx, "localhost");
}

// node:os.tmpdir implementation
static JSValue js_os_tmpdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  char temp_path[MAX_PATH];
  DWORD length = GetTempPathA(MAX_PATH, temp_path);
  if (length > 0 && length < MAX_PATH) {
    // Remove trailing backslash if present
    if (temp_path[length - 1] == '\\') {
      temp_path[length - 1] = '\0';
    }
    return JS_NewString(ctx, temp_path);
  }
  return JS_NewString(ctx, "C:\\Windows\\Temp");
#else
  const char* tmpdir = getenv("TMPDIR");
  if (!tmpdir)
    tmpdir = getenv("TMP");
  if (!tmpdir)
    tmpdir = getenv("TEMP");
  if (!tmpdir)
    tmpdir = "/tmp";
  return JS_NewString(ctx, tmpdir);
#endif
}

// node:os.homedir implementation
static JSValue js_os_homedir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  const char* home = getenv("USERPROFILE");
  if (!home) {
    const char* drive = getenv("HOMEDRIVE");
    const char* path = getenv("HOMEPATH");
    if (drive && path) {
      char full_path[MAX_PATH];
      snprintf(full_path, sizeof(full_path), "%s%s", drive, path);
      return JS_NewString(ctx, full_path);
    }
    return JS_NewString(ctx, "C:\\");
  }
  return JS_NewString(ctx, home);
#else
  const char* home = getenv("HOME");
  if (!home) {
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
      home = pw->pw_dir;
    } else {
      home = "/";
    }
  }
  return JS_NewString(ctx, home);
#endif
}

// node:os.userInfo implementation
static JSValue js_os_userInfo(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue user_obj = JS_NewObject(ctx);

#ifdef _WIN32
  char username[UNLEN + 1];
  DWORD username_len = UNLEN + 1;
  if (GetUserNameA(username, &username_len)) {
    JS_SetPropertyStr(ctx, user_obj, "username", JS_NewString(ctx, username));
  } else {
    JS_SetPropertyStr(ctx, user_obj, "username", JS_NewString(ctx, "unknown"));
  }

  // Windows doesn't have uid/gid concepts like Unix
  JS_SetPropertyStr(ctx, user_obj, "uid", JS_NewInt32(ctx, -1));
  JS_SetPropertyStr(ctx, user_obj, "gid", JS_NewInt32(ctx, -1));
  JS_SetPropertyStr(ctx, user_obj, "shell", JS_NewString(ctx, getenv("COMSPEC") ? getenv("COMSPEC") : "cmd.exe"));
#else
  struct passwd* pw = getpwuid(getuid());
  if (pw) {
    JS_SetPropertyStr(ctx, user_obj, "username", JS_NewString(ctx, pw->pw_name));
    JS_SetPropertyStr(ctx, user_obj, "uid", JS_NewInt32(ctx, pw->pw_uid));
    JS_SetPropertyStr(ctx, user_obj, "gid", JS_NewInt32(ctx, pw->pw_gid));
    JS_SetPropertyStr(ctx, user_obj, "shell", JS_NewString(ctx, pw->pw_shell ? pw->pw_shell : "/bin/sh"));
    if (pw->pw_dir) {
      JS_SetPropertyStr(ctx, user_obj, "homedir", JS_NewString(ctx, pw->pw_dir));
    }
  } else {
    JS_SetPropertyStr(ctx, user_obj, "username", JS_NewString(ctx, "unknown"));
    JS_SetPropertyStr(ctx, user_obj, "uid", JS_NewInt32(ctx, getuid()));
    JS_SetPropertyStr(ctx, user_obj, "gid", JS_NewInt32(ctx, getgid()));
    JS_SetPropertyStr(ctx, user_obj, "shell", JS_NewString(ctx, "/bin/sh"));
  }
#endif

  // Add homedir using our homedir function
  JSValue homedir_val = js_os_homedir(ctx, JS_UNDEFINED, 0, NULL);
  JS_SetPropertyStr(ctx, user_obj, "homedir", homedir_val);

  return user_obj;
}

// node:os.endianness implementation
static JSValue js_os_endianness(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  union {
    uint32_t i;
    char c[4];
  } test = {0x01020304};

  if (test.c[0] == 1) {
    return JS_NewString(ctx, "BE");  // Big Endian
  } else {
    return JS_NewString(ctx, "LE");  // Little Endian
  }
}

// Initialize node:os module for CommonJS
JSValue JSRT_InitNodeOs(JSContext* ctx) {
  JSValue os_obj = JS_NewObject(ctx);

  // Add methods
  JS_SetPropertyStr(ctx, os_obj, "arch", JS_NewCFunction(ctx, js_os_arch, "arch", 0));
  JS_SetPropertyStr(ctx, os_obj, "platform", JS_NewCFunction(ctx, js_os_platform, "platform", 0));
  JS_SetPropertyStr(ctx, os_obj, "type", JS_NewCFunction(ctx, js_os_type, "type", 0));
  JS_SetPropertyStr(ctx, os_obj, "release", JS_NewCFunction(ctx, js_os_release, "release", 0));
  JS_SetPropertyStr(ctx, os_obj, "hostname", JS_NewCFunction(ctx, js_os_hostname, "hostname", 0));
  JS_SetPropertyStr(ctx, os_obj, "tmpdir", JS_NewCFunction(ctx, js_os_tmpdir, "tmpdir", 0));
  JS_SetPropertyStr(ctx, os_obj, "homedir", JS_NewCFunction(ctx, js_os_homedir, "homedir", 0));
  JS_SetPropertyStr(ctx, os_obj, "userInfo", JS_NewCFunction(ctx, js_os_userInfo, "userInfo", 0));
  JS_SetPropertyStr(ctx, os_obj, "endianness", JS_NewCFunction(ctx, js_os_endianness, "endianness", 0));

  // Add constants
  JS_SetPropertyStr(ctx, os_obj, "EOL",
                    JS_NewString(ctx,
#ifdef _WIN32
                                 "\r\n"
#else
                                 "\n"
#endif
                                 ));

  return os_obj;
}

// Initialize node:os module for ES modules
int js_node_os_init(JSContext* ctx, JSModuleDef* m) {
  JSValue os_module = JSRT_InitNodeOs(ctx);

  // Export as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, os_module));

  // Export individual functions
  JSValue arch = JS_GetPropertyStr(ctx, os_module, "arch");
  JS_SetModuleExport(ctx, m, "arch", JS_DupValue(ctx, arch));
  JS_FreeValue(ctx, arch);

  JSValue platform = JS_GetPropertyStr(ctx, os_module, "platform");
  JS_SetModuleExport(ctx, m, "platform", JS_DupValue(ctx, platform));
  JS_FreeValue(ctx, platform);

  JSValue type = JS_GetPropertyStr(ctx, os_module, "type");
  JS_SetModuleExport(ctx, m, "type", JS_DupValue(ctx, type));
  JS_FreeValue(ctx, type);

  JSValue release = JS_GetPropertyStr(ctx, os_module, "release");
  JS_SetModuleExport(ctx, m, "release", JS_DupValue(ctx, release));
  JS_FreeValue(ctx, release);

  JSValue hostname = JS_GetPropertyStr(ctx, os_module, "hostname");
  JS_SetModuleExport(ctx, m, "hostname", JS_DupValue(ctx, hostname));
  JS_FreeValue(ctx, hostname);

  JSValue tmpdir = JS_GetPropertyStr(ctx, os_module, "tmpdir");
  JS_SetModuleExport(ctx, m, "tmpdir", JS_DupValue(ctx, tmpdir));
  JS_FreeValue(ctx, tmpdir);

  JSValue homedir = JS_GetPropertyStr(ctx, os_module, "homedir");
  JS_SetModuleExport(ctx, m, "homedir", JS_DupValue(ctx, homedir));
  JS_FreeValue(ctx, homedir);

  JSValue userInfo = JS_GetPropertyStr(ctx, os_module, "userInfo");
  JS_SetModuleExport(ctx, m, "userInfo", JS_DupValue(ctx, userInfo));
  JS_FreeValue(ctx, userInfo);

  JSValue endianness = JS_GetPropertyStr(ctx, os_module, "endianness");
  JS_SetModuleExport(ctx, m, "endianness", JS_DupValue(ctx, endianness));
  JS_FreeValue(ctx, endianness);

  JSValue eol = JS_GetPropertyStr(ctx, os_module, "EOL");
  JS_SetModuleExport(ctx, m, "EOL", JS_DupValue(ctx, eol));
  JS_FreeValue(ctx, eol);

  JS_FreeValue(ctx, os_module);
  return 0;
}