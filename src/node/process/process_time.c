#include "process_time.h"
#include <quickjs.h>
#include <time.h>
#include "process_platform.h"

// Global variable for process start time
extern struct timeval jsrt_start_time;

// Process uptime function
static JSValue js_process_uptime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  struct timeval current_time;
  if (JSRT_GETTIMEOFDAY(&current_time, NULL) != 0) {
    return JS_NewFloat64(ctx, 0.0);
  }

  double uptime =
      (current_time.tv_sec - jsrt_start_time.tv_sec) + (current_time.tv_usec - jsrt_start_time.tv_usec) / 1000000.0;

  return JS_NewFloat64(ctx, uptime);
}

// High resolution time function (Node.js compatible)
static JSValue js_process_hrtime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  struct timeval tv;
  if (JSRT_GETTIMEOFDAY(&tv, NULL) != 0) {
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt32(ctx, 0));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewInt32(ctx, 0));
    return arr;
  }

  // If a previous time is provided, calculate the difference
  if (argc > 0 && JS_IsArray(ctx, argv[0])) {
    JSValue prev_sec_val = JS_GetPropertyUint32(ctx, argv[0], 0);
    JSValue prev_nsec_val = JS_GetPropertyUint32(ctx, argv[0], 1);

    int32_t prev_sec, prev_nsec;
    if (JS_ToInt32(ctx, &prev_sec, prev_sec_val) == 0 && JS_ToInt32(ctx, &prev_nsec, prev_nsec_val) == 0) {
      long long current_nsec = (long long)tv.tv_sec * 1000000000LL + (long long)tv.tv_usec * 1000LL;
      long long prev_total_nsec = (long long)prev_sec * 1000000000LL + (long long)prev_nsec;
      long long diff_nsec = current_nsec - prev_total_nsec;

      int32_t diff_sec = (int32_t)(diff_nsec / 1000000000LL);
      int32_t diff_nsec_remainder = (int32_t)(diff_nsec % 1000000000LL);

      JSValue arr = JS_NewArray(ctx);
      JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt32(ctx, diff_sec));
      JS_SetPropertyUint32(ctx, arr, 1, JS_NewInt32(ctx, diff_nsec_remainder));

      JS_FreeValue(ctx, prev_sec_val);
      JS_FreeValue(ctx, prev_nsec_val);
      return arr;
    }

    JS_FreeValue(ctx, prev_sec_val);
    JS_FreeValue(ctx, prev_nsec_val);
  }

  // Return current time as [seconds, nanoseconds]
  JSValue arr = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt32(ctx, (int32_t)tv.tv_sec));
  JS_SetPropertyUint32(ctx, arr, 1, JS_NewInt32(ctx, (int32_t)(tv.tv_usec * 1000)));

  return arr;
}

// High resolution time in bigint format (Node.js 10.7.0+)
static JSValue js_process_hrtime_bigint(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  struct timeval tv;
  if (JSRT_GETTIMEOFDAY(&tv, NULL) != 0) {
    return JS_NewBigInt64(ctx, 0);
  }

  // Convert to nanoseconds
  long long nanoseconds = (long long)tv.tv_sec * 1000000000LL + (long long)tv.tv_usec * 1000LL;

  return JS_NewBigInt64(ctx, nanoseconds);
}

// Get current time in milliseconds since epoch
static JSValue js_process_now(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  struct timeval tv;
  if (JSRT_GETTIMEOFDAY(&tv, NULL) != 0) {
    return JS_NewFloat64(ctx, 0.0);
  }

  double milliseconds = (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
  return JS_NewFloat64(ctx, milliseconds);
}

// Initialize process time functions
void jsrt_process_time_init(JSContext* ctx, JSValue process_obj) {
  // Add uptime method
  JS_SetPropertyStr(ctx, process_obj, "uptime", JS_NewCFunction(ctx, js_process_uptime, "uptime", 0));

  // Add hrtime method
  JS_SetPropertyStr(ctx, process_obj, "hrtime", JS_NewCFunction(ctx, js_process_hrtime, "hrtime", 1));

  // Add hrtime.bigint method
  JSValue hrtime_func = JS_GetPropertyStr(ctx, process_obj, "hrtime");
  JS_SetPropertyStr(ctx, hrtime_func, "bigint", JS_NewCFunction(ctx, js_process_hrtime_bigint, "bigint", 0));
  JS_FreeValue(ctx, hrtime_func);

  // Add now method (non-standard but useful)
  JS_SetPropertyStr(ctx, process_obj, "now", JS_NewCFunction(ctx, js_process_now, "now", 0));
}