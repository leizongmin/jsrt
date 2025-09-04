#include "process.h"

// Platform-specific includes
#ifdef _WIN32
#include <process.h>
#include <tlhelp32.h>
#include <windows.h>
#include <winsock2.h>  // For struct timeval
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include <quickjs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <uv.h>

#include "../util/debug.h"
#include "crypto.h"

// Platform-specific function implementations
#ifdef _WIN32
// Windows equivalent of gettimeofday()
static int jsrt_gettimeofday(struct timeval* tv, void* tz) {
  FILETIME ft;
  unsigned __int64 tmpres = 0;

  if (NULL != tv) {
    GetSystemTimeAsFileTime(&ft);

    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    tmpres /= 10;  // convert into microseconds
    // converting file time to unix epoch
    tmpres -= 11644473600000000ULL;
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }

  return 0;
}

// Windows equivalent of getppid()
static int jsrt_getppid(void) {
  HANDLE hSnapshot;
  PROCESSENTRY32 pe32;
  DWORD dwParentPID = 0;
  DWORD dwCurrentPID = GetCurrentProcessId();

  hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE) {
    return 0;
  }

  pe32.dwSize = sizeof(PROCESSENTRY32);

  if (Process32First(hSnapshot, &pe32)) {
    do {
      if (pe32.th32ProcessID == dwCurrentPID) {
        dwParentPID = pe32.th32ParentProcessID;
        break;
      }
    } while (Process32Next(hSnapshot, &pe32));
  }

  CloseHandle(hSnapshot);
  return (int)dwParentPID;
}

// Cross-platform wrappers
#define JSRT_GETPID() getpid()
#define JSRT_GETPPID() jsrt_getppid()
#define JSRT_GETTIMEOFDAY(tv, tz) jsrt_gettimeofday(tv, tz)

#else
// Unix/Linux wrappers
#define JSRT_GETPID() getpid()
#define JSRT_GETPPID() getppid()
#define JSRT_GETTIMEOFDAY(tv, tz) gettimeofday(tv, tz)

#endif

// Global variables for command line arguments
int g_jsrt_argc = 0;
char** g_jsrt_argv = NULL;

// Start time for process.uptime()
static struct timeval g_process_start_time;
static bool g_process_start_time_initialized = false;

// Initialize start time
static void init_process_start_time() {
  if (!g_process_start_time_initialized) {
    JSRT_GETTIMEOFDAY(&g_process_start_time, NULL);
    g_process_start_time_initialized = true;
  }
}

// process.argv getter
static JSValue jsrt_process_get_argv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue argv_array = JS_NewArray(ctx);

  for (int i = 0; i < g_jsrt_argc; i++) {
    JS_SetPropertyUint32(ctx, argv_array, i, JS_NewString(ctx, g_jsrt_argv[i]));
  }

  return argv_array;
}

// process.uptime()
static JSValue jsrt_process_uptime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  init_process_start_time();

  struct timeval current_time;
  JSRT_GETTIMEOFDAY(&current_time, NULL);

  double uptime_seconds = (current_time.tv_sec - g_process_start_time.tv_sec) +
                          (current_time.tv_usec - g_process_start_time.tv_usec) / 1000000.0;

  return JS_NewFloat64(ctx, uptime_seconds);
}

// process.exit(code)
static JSValue jsrt_process_exit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  int exit_code = 0;

  // Get exit code from first argument, default to 0
  if (argc > 0) {
    if (JS_ToInt32(ctx, &exit_code, argv[0]) < 0) {
      // If conversion fails, use default exit code 0
      exit_code = 0;
    }
  }

  // Ensure exit code is in valid range (0-255 on most platforms)
  if (exit_code < 0) {
    exit_code = 1;  // Convert negative codes to 1
  } else if (exit_code > 255) {
    exit_code = exit_code & 0xFF;  // Truncate to 8 bits
  }

  // Exit the process immediately
  exit(exit_code);

  // This line should never be reached, but return undefined for completeness
  return JS_UNDEFINED;
}

// Helper function to get jsrt version from compile-time macro
static char* get_jsrt_version() {
  static char* cached_version = NULL;
  static bool version_loaded = false;

  if (version_loaded) {
    return cached_version;
  }

  version_loaded = true;

#ifdef JSRT_VERSION
  cached_version = strdup(JSRT_VERSION);
#else
  cached_version = strdup("1.0.0");  // Fallback
#endif

  return cached_version;
}

// process.pid getter
static JSValue jsrt_process_get_pid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewInt32(ctx, (int32_t)JSRT_GETPID());
}

// process.ppid getter
static JSValue jsrt_process_get_ppid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewInt32(ctx, (int32_t)JSRT_GETPPID());
}

// process.argv0 getter
static JSValue jsrt_process_get_argv0(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (g_jsrt_argc > 0 && g_jsrt_argv && g_jsrt_argv[0]) {
    return JS_NewString(ctx, g_jsrt_argv[0]);
  }
  return JS_NewString(ctx, "jsrt");
}

// process.version getter
static JSValue jsrt_process_get_version(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  char* version = get_jsrt_version();
  if (version != NULL) {
    // Add 'v' prefix to version
    char version_with_prefix[256];
    snprintf(version_with_prefix, sizeof(version_with_prefix), "v%s", version);
    return JS_NewString(ctx, version_with_prefix);
  }
  return JS_NewString(ctx, "v1.0.0");  // Fallback
}

// process.platform getter
static JSValue jsrt_process_get_platform(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef __APPLE__
  return JS_NewString(ctx, "darwin");
#elif __linux__
  return JS_NewString(ctx, "linux");
#elif _WIN32
  return JS_NewString(ctx, "win32");
#elif __FreeBSD__
  return JS_NewString(ctx, "freebsd");
#elif __OpenBSD__
  return JS_NewString(ctx, "openbsd");
#elif __NetBSD__
  return JS_NewString(ctx, "netbsd");
#else
  return JS_NewString(ctx, "unknown");
#endif
}

// process.arch getter
static JSValue jsrt_process_get_arch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#if defined(__x86_64__) || defined(_M_X64)
  return JS_NewString(ctx, "x64");
#elif defined(__i386__) || defined(_M_IX86)
  return JS_NewString(ctx, "x32");
#elif defined(__aarch64__) || defined(_M_ARM64)
  return JS_NewString(ctx, "arm64");
#elif defined(__arm__) || defined(_M_ARM)
  return JS_NewString(ctx, "arm");
#else
  return JS_NewString(ctx, "unknown");
#endif
}

// process.versions getter
static JSValue jsrt_process_get_versions(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue versions_obj = JS_NewObject(ctx);

  // Add jsrt version from VERSION file
  char* version = get_jsrt_version();
  if (version != NULL) {
    JS_SetPropertyStr(ctx, versions_obj, "jsrt", JS_NewString(ctx, version));
  } else {
    JS_SetPropertyStr(ctx, versions_obj, "jsrt", JS_NewString(ctx, "1.0.0"));  // Fallback
  }

  // Add libuv version
  const char* uv_version = uv_version_string();
  if (uv_version != NULL) {
    JS_SetPropertyStr(ctx, versions_obj, "uv", JS_NewString(ctx, uv_version));
  }

  // Add OpenSSL version if available
  const char* openssl_version = JSRT_GetOpenSSLVersion();
  if (openssl_version != NULL) {
    JS_SetPropertyStr(ctx, versions_obj, "openssl", JS_NewString(ctx, openssl_version));
  }

  return versions_obj;
}

// process.env getter
static JSValue jsrt_process_get_env(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue env_obj = JS_NewObject(ctx);

#ifdef _WIN32
  // Windows: Use GetEnvironmentStrings()
  LPCH env_strings = GetEnvironmentStrings();
  if (env_strings != NULL) {
    LPCH current = env_strings;
    while (*current) {
      char* env_var = current;
      char* equals = strchr(env_var, '=');
      if (equals != NULL) {
        *equals = '\0';  // Split key and value
        char* key = env_var;
        char* value = equals + 1;
        JS_SetPropertyStr(ctx, env_obj, key, JS_NewString(ctx, value));
        *equals = '=';  // Restore original string
      }
      current += strlen(current) + 1;
    }
    FreeEnvironmentStrings(env_strings);
  }
#else
  // Unix/Linux: Use extern char **environ
  extern char** environ;
  if (environ != NULL) {
    for (char** env = environ; *env != NULL; env++) {
      char* env_var = strdup(*env);
      if (env_var != NULL) {
        char* equals = strchr(env_var, '=');
        if (equals != NULL) {
          *equals = '\0';  // Split key and value
          char* key = env_var;
          char* value = equals + 1;
          JS_SetPropertyStr(ctx, env_obj, key, JS_NewString(ctx, value));
        }
        free(env_var);
      }
    }
  }
#endif

  return env_obj;
}

// Create process module for jsrt:process
JSValue JSRT_CreateProcessModule(JSContext* ctx) {
  JSValue process_obj = JS_NewObject(ctx);

  // process.argv - define as a getter property
  JSValue argv_prop = JS_NewCFunction(ctx, jsrt_process_get_argv, "get argv", 0);
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "argv"), argv_prop, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  // process.pid - define as a getter property
  JSValue pid_prop = JS_NewCFunction(ctx, jsrt_process_get_pid, "get pid", 0);
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "pid"), pid_prop, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  // process.ppid - define as a getter property
  JSValue ppid_prop = JS_NewCFunction(ctx, jsrt_process_get_ppid, "get ppid", 0);
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "ppid"), ppid_prop, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  // process.argv0 - define as a getter property
  JSValue argv0_prop = JS_NewCFunction(ctx, jsrt_process_get_argv0, "get argv0", 0);
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "argv0"), argv0_prop, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  // process.version - define as a getter property
  JSValue version_prop = JS_NewCFunction(ctx, jsrt_process_get_version, "get version", 0);
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "version"), version_prop, JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  // process.platform - define as a getter property
  JSValue platform_prop = JS_NewCFunction(ctx, jsrt_process_get_platform, "get platform", 0);
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "platform"), platform_prop, JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  // process.arch - define as a getter property
  JSValue arch_prop = JS_NewCFunction(ctx, jsrt_process_get_arch, "get arch", 0);
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "arch"), arch_prop, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  // process.versions - define as a getter property
  JSValue versions_prop = JS_NewCFunction(ctx, jsrt_process_get_versions, "get versions", 0);
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "versions"), versions_prop, JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  // process.env - define as a getter property
  JSValue env_prop = JS_NewCFunction(ctx, jsrt_process_get_env, "get env", 0);
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "env"), env_prop, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  // process.uptime()
  JS_SetPropertyStr(ctx, process_obj, "uptime", JS_NewCFunction(ctx, jsrt_process_uptime, "uptime", 0));

  // process.exit()
  JS_SetPropertyStr(ctx, process_obj, "exit", JS_NewCFunction(ctx, jsrt_process_exit, "exit", 1));

  return process_obj;
}

// Setup process module in runtime
void JSRT_RuntimeSetupStdProcess(JSRT_Runtime* rt) {
  init_process_start_time();

  // Create process object and set it as global
  JSValue process_obj = JSRT_CreateProcessModule(rt->ctx);
  JS_SetPropertyStr(rt->ctx, rt->global, "process", process_obj);

  JSRT_Debug("JSRT_RuntimeSetupStdProcess: initialized process module");
}