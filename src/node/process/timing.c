#include <stdlib.h>
#include <string.h>
#include "process.h"

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

#ifdef __linux__
#include <time.h>
#endif

// Global variable for process start time (in nanoseconds)
static uint64_t g_process_start_time_ns = 0;

// Platform-specific high-resolution timer
static uint64_t get_high_resolution_time_ns(void) {
#ifdef _WIN32
  // Windows: QueryPerformanceCounter
  static LARGE_INTEGER frequency = {0};
  LARGE_INTEGER counter;

  if (frequency.QuadPart == 0) {
    QueryPerformanceFrequency(&frequency);
  }

  QueryPerformanceCounter(&counter);
  // Convert to nanoseconds
  return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);

#elif defined(__APPLE__)
  // macOS: mach_absolute_time
  static mach_timebase_info_data_t timebase = {0, 0};

  if (timebase.denom == 0) {
    mach_timebase_info(&timebase);
  }

  uint64_t time = mach_absolute_time();
  // Convert to nanoseconds
  return (time * timebase.numer) / timebase.denom;

#elif defined(__linux__)
  // Linux: clock_gettime with CLOCK_MONOTONIC
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
  }
  // Fallback to gettimeofday
  struct timeval tv;
  if (gettimeofday(&tv, NULL) == 0) {
    return (uint64_t)tv.tv_sec * 1000000000ULL + (uint64_t)tv.tv_usec * 1000ULL;
  }
  return 0;

#else
  // Other Unix: gettimeofday fallback
  struct timeval tv;
  if (jsrt_process_gettimeofday(&tv, NULL) == 0) {
    return (uint64_t)tv.tv_sec * 1000000000ULL + (uint64_t)tv.tv_usec * 1000ULL;
  }
  return 0;
#endif
}

// Initialize start time
static void init_process_start_time(void) {
  if (g_process_start_time_ns == 0) {
    g_process_start_time_ns = get_high_resolution_time_ns();
  }
}

// Process uptime function (with nanosecond precision)
JSValue js_process_uptime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  init_process_start_time();

  uint64_t current_ns = get_high_resolution_time_ns();
  if (current_ns == 0) {
    return JS_ThrowInternalError(ctx, "Failed to get current time");
  }

  uint64_t uptime_ns = current_ns - g_process_start_time_ns;
  double uptime_seconds = (double)uptime_ns / 1000000000.0;

  return JS_NewFloat64(ctx, uptime_seconds);
}

// Node.js process.hrtime() implementation (with improved precision and error handling)
JSValue js_process_hrtime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  uint64_t ns = get_high_resolution_time_ns();
  if (ns == 0) {
    return JS_ThrowInternalError(ctx, "Failed to get high-resolution time");
  }

  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    // hrtime(time) - return diff from previous time
    if (!JS_IsArray(ctx, argv[0])) {
      return JS_ThrowTypeError(ctx, "The \"time\" argument must be an instance of Array");
    }

    JSValue sec_val = JS_GetPropertyUint32(ctx, argv[0], 0);
    JSValue nsec_val = JS_GetPropertyUint32(ctx, argv[0], 1);

    if (JS_IsException(sec_val) || JS_IsException(nsec_val)) {
      JS_FreeValue(ctx, sec_val);
      JS_FreeValue(ctx, nsec_val);
      return JS_ThrowTypeError(ctx, "Invalid time array");
    }

    int64_t prev_sec, prev_nsec;
    if (JS_ToInt64(ctx, &prev_sec, sec_val) != 0 || JS_ToInt64(ctx, &prev_nsec, nsec_val) != 0) {
      JS_FreeValue(ctx, sec_val);
      JS_FreeValue(ctx, nsec_val);
      return JS_ThrowTypeError(ctx, "Invalid time values");
    }

    JS_FreeValue(ctx, sec_val);
    JS_FreeValue(ctx, nsec_val);

    // Validate nanosecond range
    if (prev_nsec < 0 || prev_nsec >= 1000000000LL) {
      return JS_ThrowRangeError(ctx, "Nanoseconds must be in range [0, 999999999]");
    }

    if (prev_sec < 0) {
      return JS_ThrowRangeError(ctx, "Seconds must be non-negative");
    }

    uint64_t prev_total = (uint64_t)prev_sec * 1000000000ULL + (uint64_t)prev_nsec;

    // Handle potential underflow (if clock goes backwards, though monotonic clocks shouldn't)
    int64_t diff_ns;
    if (ns >= prev_total) {
      diff_ns = (int64_t)(ns - prev_total);
    } else {
      // Clock went backwards (shouldn't happen with monotonic clock)
      diff_ns = -((int64_t)(prev_total - ns));
    }

    JSValue result = JS_NewArray(ctx);
    if (JS_IsException(result)) {
      return JS_EXCEPTION;
    }

    int64_t diff_sec = diff_ns / 1000000000LL;
    int64_t diff_nsec = diff_ns % 1000000000LL;

    // Ensure nanoseconds are always positive
    if (diff_nsec < 0) {
      diff_sec -= 1;
      diff_nsec += 1000000000LL;
    }

    JS_SetPropertyUint32(ctx, result, 0, JS_NewInt64(ctx, diff_sec));
    JS_SetPropertyUint32(ctx, result, 1, JS_NewInt64(ctx, diff_nsec));
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

// process.hrtime.bigint() implementation
JSValue js_process_hrtime_bigint(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  uint64_t ns = get_high_resolution_time_ns();
  if (ns == 0) {
    return JS_ThrowInternalError(ctx, "Failed to get high-resolution time");
  }

  // Return as BigInt in nanoseconds
  return JS_NewBigUint64(ctx, ns);
}

void jsrt_process_init_timing(void) {
  // Initialize process start time
  init_process_start_time();
}
