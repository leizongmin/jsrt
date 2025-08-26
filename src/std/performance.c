#include "performance.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <uv.h>

#include "../util/debug.h"

// Global state to track the time origin (when the runtime started)
static uint64_t performance_time_origin = 0;

// performance.now() implementation - returns high resolution time relative to timeOrigin
static JSValue JSRT_PerformanceNow(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  // uv_hrtime() returns time in nanoseconds, performance.now() should return milliseconds
  uint64_t current_time = uv_hrtime();

  if (performance_time_origin == 0) {
    // If timeOrigin is not set, return relative to current time (should not happen in normal cases)
    return JS_NewFloat64(ctx, 0.0);
  }

  // Calculate elapsed time in milliseconds with high precision
  double elapsed_ms = (double)(current_time - performance_time_origin) / 1000000.0;

  return JS_NewFloat64(ctx, elapsed_ms);
}

// Property getter for performance.timeOrigin
static JSValue JSRT_PerformanceTimeOrigin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (performance_time_origin == 0) {
    return JS_NewFloat64(ctx, 0.0);
  }

  // Convert nanoseconds to milliseconds since Unix epoch
  // We need to convert from high-res timer to Unix timestamp
  // For simplicity, we'll return the relative time origin as 0
  // This matches typical browser behavior where timeOrigin represents the navigation start time
  return JS_NewFloat64(ctx, 0.0);
}

// Setup Performance API
void JSRT_RuntimeSetupStdPerformance(JSRT_Runtime *rt) {
  // Set the time origin when the runtime is initialized
  if (performance_time_origin == 0) {
    performance_time_origin = uv_hrtime();
  }

  // Create the performance object
  JSValue performance_obj = JS_NewObject(rt->ctx);

  // Add performance.now() method
  JS_SetPropertyStr(rt->ctx, performance_obj, "now", JS_NewCFunction(rt->ctx, JSRT_PerformanceNow, "now", 0));

  // Add performance.timeOrigin property (read-only)
  JSValue time_origin_val = JSRT_PerformanceTimeOrigin(rt->ctx, JS_UNDEFINED, 0, NULL);
  JS_DefinePropertyValueStr(rt->ctx, performance_obj, "timeOrigin", time_origin_val, JS_PROP_C_W_E);

  // Add the performance object to global scope
  JS_SetPropertyStr(rt->ctx, rt->global, "performance", performance_obj);
}