#include <stdlib.h>
#include <string.h>
#include "process.h"

// Global variable for process start time
double g_jsrt_start_time = 0.0;

// Initialize start time
static void init_process_start_time(void) {
  if (g_jsrt_start_time == 0.0) {
    struct timeval tv;
    if (jsrt_process_gettimeofday(&tv, NULL) == 0) {
      g_jsrt_start_time = tv.tv_sec + tv.tv_usec / 1000000.0;
    }
  }
}

// Process uptime function
JSValue js_process_uptime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  init_process_start_time();

  struct timeval current_time;
  if (jsrt_process_gettimeofday(&current_time, NULL) != 0) {
    return JS_ThrowInternalError(ctx, "Failed to get current time");
  }

  double current_seconds = current_time.tv_sec + current_time.tv_usec / 1000000.0;
  double uptime_seconds = current_seconds - g_jsrt_start_time;

  return JS_NewFloat64(ctx, uptime_seconds);
}

// Node.js process.hrtime() implementation
JSValue js_process_hrtime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  struct timeval tv;
  if (jsrt_process_gettimeofday(&tv, NULL) != 0) {
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
    if (JS_IsException(result)) {
      return JS_EXCEPTION;
    }

    JS_SetPropertyUint32(ctx, result, 0, JS_NewInt64(ctx, (int64_t)(diff_ns / 1000000000ULL)));
    JS_SetPropertyUint32(ctx, result, 1, JS_NewInt64(ctx, (int64_t)(diff_ns % 1000000000ULL)));
    return result;
  } else {
    // hrtime() - return current time
    JSValue result = JS_NewArray(ctx);
    if (JS_IsException(result)) {
      return JS_EXCEPTION;
    }

    JS_SetPropertyUint32(ctx, result, 0, JS_NewInt64(ctx, (int64_t)(ns / 1000000000ULL)));
    JS_SetPropertyUint32(ctx, result, 1, JS_NewInt64(ctx, (int64_t)(ns % 1000000000ULL)));
    return result;
  }
}

void jsrt_process_init_timing(void) {
  // Initialize process start time
  init_process_start_time();
}