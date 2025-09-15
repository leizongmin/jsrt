#include <stdlib.h>
#include <string.h>
#include "process.h"

#ifdef _WIN32
#include <psapi.h>
#include <windows.h>
#else
#include <sys/resource.h>
#endif

// Node.js process.nextTick() implementation
JSValue js_process_nextTick(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

// Node.js process.memoryUsage() implementation
JSValue js_process_memoryUsage(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObject(ctx);

  if (JS_IsException(obj)) {
    return JS_EXCEPTION;
  }

#ifdef _WIN32
  // Windows memory usage implementation
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    // RSS (Resident Set Size) - physical memory currently used
    JS_SetPropertyStr(ctx, obj, "rss", JS_NewInt64(ctx, (int64_t)pmc.WorkingSetSize));

    // Heap total and used - simplified implementation
    JS_SetPropertyStr(ctx, obj, "heapTotal", JS_NewInt64(ctx, (int64_t)pmc.PagefileUsage));
    JS_SetPropertyStr(ctx, obj, "heapUsed", JS_NewInt64(ctx, (int64_t)pmc.PagefileUsage / 2));
  } else {
    // Fallback values if GetProcessMemoryInfo fails
    JS_SetPropertyStr(ctx, obj, "rss", JS_NewInt64(ctx, 1024 * 1024));       // 1MB
    JS_SetPropertyStr(ctx, obj, "heapTotal", JS_NewInt64(ctx, 512 * 1024));  // 512KB
    JS_SetPropertyStr(ctx, obj, "heapUsed", JS_NewInt64(ctx, 256 * 1024));   // 256KB
  }
#else
  // Unix/Linux memory usage implementation
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
// RSS (Resident Set Size) - convert from KB to bytes on Linux
#ifdef __linux__
    JS_SetPropertyStr(ctx, obj, "rss", JS_NewInt64(ctx, (int64_t)usage.ru_maxrss * 1024));
#else
    // On macOS, ru_maxrss is already in bytes
    JS_SetPropertyStr(ctx, obj, "rss", JS_NewInt64(ctx, (int64_t)usage.ru_maxrss));
#endif

    // Simplified heap implementation - use a fraction of RSS
    int64_t rss = usage.ru_maxrss;
#ifdef __linux__
    rss *= 1024;  // Convert to bytes on Linux
#endif
    JS_SetPropertyStr(ctx, obj, "heapTotal", JS_NewInt64(ctx, rss / 2));
    JS_SetPropertyStr(ctx, obj, "heapUsed", JS_NewInt64(ctx, rss / 4));
  } else {
    // Fallback values if getrusage fails
    JS_SetPropertyStr(ctx, obj, "rss", JS_NewInt64(ctx, 1024 * 1024));       // 1MB
    JS_SetPropertyStr(ctx, obj, "heapTotal", JS_NewInt64(ctx, 512 * 1024));  // 512KB
    JS_SetPropertyStr(ctx, obj, "heapUsed", JS_NewInt64(ctx, 256 * 1024));   // 256KB
  }
#endif

  // External and arrayBuffers - simplified implementation
  JS_SetPropertyStr(ctx, obj, "external", JS_NewInt64(ctx, 0));
  JS_SetPropertyStr(ctx, obj, "arrayBuffers", JS_NewInt64(ctx, 0));

  return obj;
}

void jsrt_process_init_nodejs(void) {
  // Node.js specific initialization if needed
  // Currently no initialization required
}