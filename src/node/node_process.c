#include <stdlib.h>
#include <string.h>
#include "../util/debug.h"
#include "node_modules.h"

// Platform-specific includes and implementations
#ifdef _WIN32
// clang-format off
#include <windows.h>   // Must come first to define basic Windows types
#include <process.h>
#include <tlhelp32.h>
#include <winsock2.h>  // For struct timeval
// clang-format on

// Windows implementation of missing functions
static int jsrt_getppid(void) {
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE)
    return 0;

  PROCESSENTRY32 pe32 = {.dwSize = sizeof(PROCESSENTRY32)};
  DWORD currentPID = GetCurrentProcessId();
  DWORD parentPID = 0;

  if (Process32First(hSnapshot, &pe32)) {
    do {
      if (pe32.th32ProcessID == currentPID) {
        parentPID = pe32.th32ParentProcessID;
        break;
      }
    } while (Process32Next(hSnapshot, &pe32));
  }
  CloseHandle(hSnapshot);
  return (int)parentPID;
}

static int jsrt_gettimeofday(struct timeval* tv, void* tz) {
  if (!tv)
    return -1;

  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);

  unsigned __int64 time = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
  time /= 10;                    // Convert to microseconds
  time -= 11644473600000000ULL;  // Convert to Unix epoch

  tv->tv_sec = (long)(time / 1000000UL);
  tv->tv_usec = (long)(time % 1000000UL);
  return 0;
}

#define JSRT_GETPID() getpid()
#define JSRT_GETPPID() jsrt_getppid()
#define JSRT_GETTIMEOFDAY(tv, tz) jsrt_gettimeofday(tv, tz)

#else
#include <sys/time.h>
#include <unistd.h>

#define JSRT_GETPID() getpid()
#define JSRT_GETPPID() getppid()
#define JSRT_GETTIMEOFDAY(tv, tz) gettimeofday(tv, tz)

#endif

// Node.js process.hrtime() implementation
static JSValue js_process_hrtime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  struct timeval tv;
  if (JSRT_GETTIMEOFDAY(&tv, NULL) != 0) {
    return JS_ThrowInternalError(ctx, "Failed to get time");
  }

  uint64_t ns = (uint64_t)tv.tv_sec * 1000000000ULL + (uint64_t)tv.tv_usec * 1000ULL;

  if (argc > 0 && JS_IsArray(ctx, argv[0])) {
    // hrtime(time) - return diff from previous time
    JSValue sec_val = JS_GetPropertyUint32(ctx, argv[0], 0);
    JSValue nsec_val = JS_GetPropertyUint32(ctx, argv[0], 1);

    if (JS_IsException(sec_val) || JS_IsException(nsec_val)) {
      JS_FreeValue(ctx, sec_val);
      JS_FreeValue(ctx, nsec_val);
      return JS_ThrowTypeError(ctx, "Invalid time array");
    }

    uint32_t prev_sec, prev_nsec;
    if (JS_ToUint32(ctx, &prev_sec, sec_val) != 0 || JS_ToUint32(ctx, &prev_nsec, nsec_val) != 0) {
      JS_FreeValue(ctx, sec_val);
      JS_FreeValue(ctx, nsec_val);
      return JS_ThrowTypeError(ctx, "Invalid time values");
    }

    JS_FreeValue(ctx, sec_val);
    JS_FreeValue(ctx, nsec_val);

    uint64_t prev_total = (uint64_t)prev_sec * 1000000000ULL + (uint64_t)prev_nsec;
    uint64_t diff_ns = ns - prev_total;

    JSValue result = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, result, 0, JS_NewInt64(ctx, (int64_t)(diff_ns / 1000000000ULL)));
    JS_SetPropertyUint32(ctx, result, 1, JS_NewInt64(ctx, (int64_t)(diff_ns % 1000000000ULL)));
    return result;
  } else {
    // hrtime() - return current time
    JSValue result = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, result, 0, JS_NewInt64(ctx, (int64_t)(ns / 1000000000ULL)));
    JS_SetPropertyUint32(ctx, result, 1, JS_NewInt64(ctx, (int64_t)(ns % 1000000000ULL)));
    return result;
  }
}

// Node.js process.nextTick() implementation
static JSValue js_process_nexttick(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "Callback must be a function");
  }

  // Queue the callback to run on the next iteration of the event loop
  // For now, use setTimeout with 0 delay as a simple implementation
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue setTimeout = JS_GetPropertyStr(ctx, global, "setTimeout");

  if (JS_IsFunction(ctx, setTimeout)) {
    JSValue args[2] = {argv[0], JS_NewInt32(ctx, 0)};
    JSValue result = JS_Call(ctx, setTimeout, global, 2, args);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, setTimeout);
    JS_FreeValue(ctx, global);
    return result;
  }

  JS_FreeValue(ctx, setTimeout);
  JS_FreeValue(ctx, global);
  return JS_ThrowInternalError(ctx, "setTimeout not available");
}

// Node.js process.uptime() implementation
static JSValue js_process_uptime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Simple implementation - return time since program start
  // In a real implementation, this would track actual process start time
  static struct timeval start_time = {0};

  if (start_time.tv_sec == 0) {
    JSRT_GETTIMEOFDAY(&start_time, NULL);
  }

  struct timeval current_time;
  if (JSRT_GETTIMEOFDAY(&current_time, NULL) != 0) {
    return JS_ThrowInternalError(ctx, "Failed to get time");
  }

  double uptime = (current_time.tv_sec - start_time.tv_sec) + (current_time.tv_usec - start_time.tv_usec) / 1000000.0;

  return JS_NewFloat64(ctx, uptime);
}

// Node.js process.memoryUsage() implementation
static JSValue js_process_memory_usage(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObject(ctx);

  // Simplified memory usage - in a real implementation this would
  // use platform-specific APIs to get actual memory usage
  JS_SetPropertyStr(ctx, obj, "rss", JS_NewInt64(ctx, 1024 * 1024));       // 1MB placeholder
  JS_SetPropertyStr(ctx, obj, "heapTotal", JS_NewInt64(ctx, 512 * 1024));  // 512KB placeholder
  JS_SetPropertyStr(ctx, obj, "heapUsed", JS_NewInt64(ctx, 256 * 1024));   // 256KB placeholder
  JS_SetPropertyStr(ctx, obj, "external", JS_NewInt64(ctx, 0));
  JS_SetPropertyStr(ctx, obj, "arrayBuffers", JS_NewInt64(ctx, 0));

  return obj;
}

// Extend existing jsrt:process module with Node.js-specific functionality
JSValue JSRT_InitNodeProcess(JSContext* ctx) {
  // Get the existing jsrt:process module
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue jsrt_process = JS_GetPropertyStr(ctx, global, "process");

  if (JS_IsUndefined(jsrt_process)) {
    // If jsrt:process doesn't exist, create a new process object
    jsrt_process = JS_NewObject(ctx);

    // Add basic process properties
    JS_SetPropertyStr(ctx, jsrt_process, "pid", JS_NewInt32(ctx, JSRT_GETPID()));
    JS_SetPropertyStr(ctx, jsrt_process, "ppid", JS_NewInt32(ctx, JSRT_GETPPID()));

    // Add versions object
    JSValue versions = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, versions, "jsrt", JS_NewString(ctx, "1.0.0"));
    JS_SetPropertyStr(ctx, versions, "node", JS_NewString(ctx, "20.0.0"));  // Node.js API compatibility version
    JS_SetPropertyStr(ctx, versions, "quickjs", JS_NewString(ctx, "2024-01-13"));
    JS_SetPropertyStr(ctx, jsrt_process, "versions", versions);
  } else {
    // Add Node.js compatibility version to the existing process object
    JS_SetPropertyStr(ctx, jsrt_process, "nodeVersion", JS_NewString(ctx, "20.0.0"));
  }

  JS_FreeValue(ctx, global);

  // Add Node.js-specific process methods
  JS_SetPropertyStr(ctx, jsrt_process, "hrtime", JS_NewCFunction(ctx, js_process_hrtime, "hrtime", 1));
  JS_SetPropertyStr(ctx, jsrt_process, "nextTick", JS_NewCFunction(ctx, js_process_nexttick, "nextTick", 1));
  JS_SetPropertyStr(ctx, jsrt_process, "uptime", JS_NewCFunction(ctx, js_process_uptime, "uptime", 0));
  JS_SetPropertyStr(ctx, jsrt_process, "memoryUsage", JS_NewCFunction(ctx, js_process_memory_usage, "memoryUsage", 0));

  return jsrt_process;
}

// ES module initialization
int js_node_process_init(JSContext* ctx, JSModuleDef* m) {
  JSValue process = JSRT_InitNodeProcess(ctx);
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, process));
  JS_FreeValue(ctx, process);
  return 0;
}