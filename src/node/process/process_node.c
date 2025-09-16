#include "process_node.h"
#include <quickjs.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif
#include <psapi.h>
#include <windows.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

// Global array to store nextTick callbacks
static JSValue* next_tick_callbacks = NULL;
static size_t next_tick_count = 0;
static size_t next_tick_capacity = 0;

// Process nextTick function
static JSValue js_process_next_tick(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "nextTick requires a function argument");
  }

  // Expand callback array if needed
  if (next_tick_count >= next_tick_capacity) {
    size_t new_capacity = next_tick_capacity == 0 ? 8 : next_tick_capacity * 2;
    JSValue* new_callbacks = realloc(next_tick_callbacks, new_capacity * sizeof(JSValue));
    if (!new_callbacks) {
      return JS_ThrowOutOfMemory(ctx);
    }
    next_tick_callbacks = new_callbacks;
    next_tick_capacity = new_capacity;
  }

  // Store the callback (duplicate the value to prevent GC)
  next_tick_callbacks[next_tick_count] = JS_DupValue(ctx, argv[0]);
  next_tick_count++;

  return JS_UNDEFINED;
}

// Execute all nextTick callbacks
void jsrt_process_execute_next_tick(JSContext* ctx) {
  if (next_tick_count == 0) {
    return;
  }

  // Execute all callbacks
  for (size_t i = 0; i < next_tick_count; i++) {
    JSValue result = JS_Call(ctx, next_tick_callbacks[i], JS_UNDEFINED, 0, NULL);
    if (JS_IsException(result)) {
      // Log the exception but continue with other callbacks
      js_std_dump_error(ctx);
    }
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, next_tick_callbacks[i]);
  }

  // Reset the callback array
  next_tick_count = 0;
}

// Process memory usage function
static JSValue js_process_memory_usage(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue usage = JS_NewObject(ctx);

#ifdef _WIN32
  // Windows implementation
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    JS_SetPropertyStr(ctx, usage, "rss", JS_NewInt64(ctx, pmc.WorkingSetSize));
    JS_SetPropertyStr(ctx, usage, "heapTotal", JS_NewInt64(ctx, pmc.PagefileUsage));
    JS_SetPropertyStr(ctx, usage, "heapUsed", JS_NewInt64(ctx, pmc.PagefileUsage));
    JS_SetPropertyStr(ctx, usage, "external", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, usage, "arrayBuffers", JS_NewInt32(ctx, 0));
  } else {
    // Fallback values
    JS_SetPropertyStr(ctx, usage, "rss", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, usage, "heapTotal", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, usage, "heapUsed", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, usage, "external", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, usage, "arrayBuffers", JS_NewInt32(ctx, 0));
  }
#else
  // Unix-like systems implementation
  struct rusage rusage;
  if (getrusage(RUSAGE_SELF, &rusage) == 0) {
    // RSS is in kilobytes on most systems, convert to bytes
    long rss_bytes = rusage.ru_maxrss;
#ifdef __APPLE__
    // macOS reports RSS in bytes
    // No conversion needed
#else
    // Linux reports RSS in kilobytes
    rss_bytes *= 1024;
#endif

    JS_SetPropertyStr(ctx, usage, "rss", JS_NewInt64(ctx, rss_bytes));
    JS_SetPropertyStr(ctx, usage, "heapTotal", JS_NewInt64(ctx, rss_bytes));
    JS_SetPropertyStr(ctx, usage, "heapUsed", JS_NewInt64(ctx, rss_bytes));
    JS_SetPropertyStr(ctx, usage, "external", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, usage, "arrayBuffers", JS_NewInt32(ctx, 0));
  } else {
    // Fallback values
    JS_SetPropertyStr(ctx, usage, "rss", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, usage, "heapTotal", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, usage, "heapUsed", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, usage, "external", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, usage, "arrayBuffers", JS_NewInt32(ctx, 0));
  }
#endif

  return usage;
}

// Process abort function
static JSValue js_process_abort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  abort();
  return JS_UNDEFINED;  // Never reached
}

// Initialize Node.js specific process functions
void jsrt_process_node_init(JSContext* ctx, JSValue process_obj) {
  // Add nextTick method
  JS_SetPropertyStr(ctx, process_obj, "nextTick", JS_NewCFunction(ctx, js_process_next_tick, "nextTick", 1));

  // Add memoryUsage method
  JS_SetPropertyStr(ctx, process_obj, "memoryUsage", JS_NewCFunction(ctx, js_process_memory_usage, "memoryUsage", 0));

  // Add abort method
  JS_SetPropertyStr(ctx, process_obj, "abort", JS_NewCFunction(ctx, js_process_abort, "abort", 0));
}

// Cleanup function for nextTick callbacks
void jsrt_process_node_cleanup(void) {
  if (next_tick_callbacks) {
    free(next_tick_callbacks);
    next_tick_callbacks = NULL;
    next_tick_count = 0;
    next_tick_capacity = 0;
  }
}