#include "console.h"

#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>  // For struct timeval
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include "../util/colorize.h"
#include "../util/dbuf.h"

// Platform-specific function implementations
#ifdef _WIN32
// Windows equivalent of gettimeofday()
static int jsrt_console_gettimeofday(struct timeval* tv, void* tz) {
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
#define JSRT_GETTIMEOFDAY(tv, tz) jsrt_console_gettimeofday(tv, tz)
#else
#define JSRT_GETTIMEOFDAY(tv, tz) gettimeofday(tv, tz)
#endif

// Forward declarations
static JSValue jsrt_console_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_error(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_warn(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_info(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_debug(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_trace(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_assert(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_time(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_timeEnd(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_count(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_countReset(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_group(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_groupEnd(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_groupCollapsed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_clear(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_dir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_console_table(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Global state for console functionality
typedef struct {
  double start_time;
} ConsoleTimer;

typedef struct {
  int count;
} ConsoleCounter;

static ConsoleTimer* timers = NULL;
static char** timer_labels = NULL;
static int timer_count = 0;
static int timer_capacity = 0;

static ConsoleCounter* counters = NULL;
static char** counter_labels = NULL;
static int counter_count = 0;
static int counter_capacity = 0;

static int group_level = 0;

void JSRT_RuntimeSetupStdConsole(JSRT_Runtime* rt) {
  JSValueConst console = JS_NewObject(rt->ctx);

  JS_SetPropertyStr(rt->ctx, console, "log", JS_NewCFunction(rt->ctx, jsrt_console_log, "log", 1));
  JS_SetPropertyStr(rt->ctx, console, "error", JS_NewCFunction(rt->ctx, jsrt_console_error, "error", 1));
  JS_SetPropertyStr(rt->ctx, console, "warn", JS_NewCFunction(rt->ctx, jsrt_console_warn, "warn", 1));
  JS_SetPropertyStr(rt->ctx, console, "info", JS_NewCFunction(rt->ctx, jsrt_console_info, "info", 1));
  JS_SetPropertyStr(rt->ctx, console, "debug", JS_NewCFunction(rt->ctx, jsrt_console_debug, "debug", 1));
  JS_SetPropertyStr(rt->ctx, console, "trace", JS_NewCFunction(rt->ctx, jsrt_console_trace, "trace", 1));
  JS_SetPropertyStr(rt->ctx, console, "assert", JS_NewCFunction(rt->ctx, jsrt_console_assert, "assert", 2));
  JS_SetPropertyStr(rt->ctx, console, "time", JS_NewCFunction(rt->ctx, jsrt_console_time, "time", 1));
  JS_SetPropertyStr(rt->ctx, console, "timeEnd", JS_NewCFunction(rt->ctx, jsrt_console_timeEnd, "timeEnd", 1));
  JS_SetPropertyStr(rt->ctx, console, "count", JS_NewCFunction(rt->ctx, jsrt_console_count, "count", 1));
  JS_SetPropertyStr(rt->ctx, console, "countReset", JS_NewCFunction(rt->ctx, jsrt_console_countReset, "countReset", 1));
  JS_SetPropertyStr(rt->ctx, console, "group", JS_NewCFunction(rt->ctx, jsrt_console_group, "group", 1));
  JS_SetPropertyStr(rt->ctx, console, "groupEnd", JS_NewCFunction(rt->ctx, jsrt_console_groupEnd, "groupEnd", 0));
  JS_SetPropertyStr(rt->ctx, console, "groupCollapsed",
                    JS_NewCFunction(rt->ctx, jsrt_console_groupCollapsed, "groupCollapsed", 1));
  JS_SetPropertyStr(rt->ctx, console, "clear", JS_NewCFunction(rt->ctx, jsrt_console_clear, "clear", 0));
  JS_SetPropertyStr(rt->ctx, console, "dir", JS_NewCFunction(rt->ctx, jsrt_console_dir, "dir", 1));
  JS_SetPropertyStr(rt->ctx, console, "table", JS_NewCFunction(rt->ctx, jsrt_console_table, "table", 1));
  JS_SetPropertyStr(rt->ctx, rt->global, "console", console);
}

static void jsrt_init_dbuf(JSContext* ctx, DynBuf* s) {
  dbuf_init2(s, JS_GetRuntime(ctx), (DynBufReallocFunc*)js_realloc_rt);
}

// Get current time in milliseconds
static double jsrt_get_time_ms() {
  struct timeval tv;
  JSRT_GETTIMEOFDAY(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// Common function for console output with different streams and colors
static void jsrt_console_output(JSContext* ctx, int argc, JSValueConst* argv, FILE* stream, const char* color_start,
                                const char* color_end, const char* prefix) {
  int i;
  DynBuf dbuf;
  jsrt_init_dbuf(ctx, &dbuf);
  bool colors = isatty(fileno(stream));

  // Add indentation for groups
  for (int g = 0; g < group_level; g++) {
    dbuf_putstr(&dbuf, "  ");
  }

  // Add prefix if provided
  if (prefix) {
    if (colors && color_start)
      dbuf_putstr(&dbuf, color_start);
    dbuf_putstr(&dbuf, prefix);
    if (colors && color_end)
      dbuf_putstr(&dbuf, color_end);
    if (argc > 0)
      dbuf_putstr(&dbuf, " ");
  }

  for (i = 0; i < argc; i++) {
    if (i != 0) {
      dbuf_putstr(&dbuf, " ");
    }
    JSRT_GetJSValuePrettyString(&dbuf, ctx, argv[i], NULL, colors);
  }

  fwrite(dbuf.buf, 1, dbuf.size, stream);
  dbuf_free(&dbuf);
  putc('\n', stream);
}

static JSValue jsrt_console_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  jsrt_console_output(ctx, argc, argv, stdout, NULL, NULL, NULL);
  return JS_UNDEFINED;
}

static JSValue jsrt_console_error(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  jsrt_console_output(ctx, argc, argv, stderr, JSRT_ColorizeFontRed, JSRT_ColorizeClear, NULL);
  return JS_UNDEFINED;
}

static JSValue jsrt_console_warn(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  jsrt_console_output(ctx, argc, argv, stderr, JSRT_ColorizeFontYellow, JSRT_ColorizeClear, NULL);
  return JS_UNDEFINED;
}

static JSValue jsrt_console_info(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  jsrt_console_output(ctx, argc, argv, stdout, JSRT_ColorizeFontBlue, JSRT_ColorizeClear, NULL);
  return JS_UNDEFINED;
}

static JSValue jsrt_console_debug(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  jsrt_console_output(ctx, argc, argv, stdout, JSRT_ColorizeFontBlack, JSRT_ColorizeClear, NULL);
  return JS_UNDEFINED;
}

static JSValue jsrt_console_trace(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Output the trace message first
  jsrt_console_output(ctx, argc, argv, stderr, JSRT_ColorizeFontCyan, JSRT_ColorizeClear, "Trace:");

  // Output a simple stack trace indication
  for (int g = 0; g <= group_level; g++) {
    fprintf(stderr, "  ");
  }
  fprintf(stderr, "%sat <anonymous>%s\n", isatty(STDERR_FILENO) ? JSRT_ColorizeFontBlack : "",
          isatty(STDERR_FILENO) ? JSRT_ColorizeClear : "");
  return JS_UNDEFINED;
}

static JSValue jsrt_console_assert(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc == 0) {
    // No assertion, always fails
    jsrt_console_output(ctx, 0, NULL, stderr, JSRT_ColorizeFontRed, JSRT_ColorizeClear, "Assertion failed:");
    return JS_UNDEFINED;
  }

  // Check if first argument is truthy
  int32_t assertion = JS_ToBool(ctx, argv[0]);
  if (assertion <= 0) {  // false, 0, or error
    // Print assertion failed message with remaining arguments
    if (argc > 1) {
      jsrt_console_output(ctx, argc - 1, argv + 1, stderr, JSRT_ColorizeFontRed, JSRT_ColorizeClear,
                          "Assertion failed:");
    } else {
      jsrt_console_output(ctx, 0, NULL, stderr, JSRT_ColorizeFontRed, JSRT_ColorizeClear, "Assertion failed");
    }
  }
  return JS_UNDEFINED;
}

// Timer management functions
static int jsrt_find_timer(const char* label) {
  for (int i = 0; i < timer_count; i++) {
    if (timer_labels[i] && strcmp(timer_labels[i], label) == 0) {
      return i;
    }
  }
  return -1;
}

static int jsrt_add_timer(const char* label, double start_time) {
  if (timer_count >= timer_capacity) {
    timer_capacity = timer_capacity == 0 ? 4 : timer_capacity * 2;
    timers = realloc(timers, timer_capacity * sizeof(ConsoleTimer));
    timer_labels = realloc(timer_labels, timer_capacity * sizeof(char*));
  }

  int index = timer_count++;
  timer_labels[index] = strdup(label);
  timers[index].start_time = start_time;
  return index;
}

static void jsrt_remove_timer(int index) {
  if (index >= 0 && index < timer_count) {
    free(timer_labels[index]);
    // Shift remaining timers down
    for (int i = index; i < timer_count - 1; i++) {
      timer_labels[i] = timer_labels[i + 1];
      timers[i] = timers[i + 1];
    }
    timer_count--;
  }
}

static JSValue jsrt_console_time(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* label = "default";
  if (argc > 0) {
    label = JS_ToCString(ctx, argv[0]);
    if (!label)
      label = "default";
  }

  // Check if timer already exists
  if (jsrt_find_timer(label) >= 0) {
    char message[256];
    snprintf(message, sizeof(message), "Timer '%s' already exists", label);
    jsrt_console_output(ctx, 0, NULL, stdout, JSRT_ColorizeFontYellow, JSRT_ColorizeClear, message);
  } else {
    jsrt_add_timer(label, jsrt_get_time_ms());
  }

  if (argc > 0 && label != JS_ToCString(ctx, argv[0])) {
    JS_FreeCString(ctx, label);
  }
  return JS_UNDEFINED;
}

static JSValue jsrt_console_timeEnd(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* label = "default";
  if (argc > 0) {
    label = JS_ToCString(ctx, argv[0]);
    if (!label)
      label = "default";
  }

  int timer_index = jsrt_find_timer(label);
  if (timer_index >= 0) {
    double elapsed = jsrt_get_time_ms() - timers[timer_index].start_time;

    // Format and output the timing result
    char message[256];
    snprintf(message, sizeof(message), "%s: %.3fms", label, elapsed);

    DynBuf dbuf;
    jsrt_init_dbuf(ctx, &dbuf);
    for (int g = 0; g < group_level; g++) {
      dbuf_putstr(&dbuf, "  ");
    }
    dbuf_putstr(&dbuf, message);
    fwrite(dbuf.buf, 1, dbuf.size, stdout);
    dbuf_free(&dbuf);
    putchar('\n');

    jsrt_remove_timer(timer_index);
  } else {
    char message[256];
    snprintf(message, sizeof(message), "Timer '%s' does not exist", label);
    jsrt_console_output(ctx, 0, NULL, stdout, JSRT_ColorizeFontYellow, JSRT_ColorizeClear, message);
  }

  if (argc > 0 && label != JS_ToCString(ctx, argv[0])) {
    JS_FreeCString(ctx, label);
  }
  return JS_UNDEFINED;
}

// Counter management functions
static int jsrt_find_counter(const char* label) {
  for (int i = 0; i < counter_count; i++) {
    if (counter_labels[i] && strcmp(counter_labels[i], label) == 0) {
      return i;
    }
  }
  return -1;
}

static int jsrt_add_counter(const char* label) {
  if (counter_count >= counter_capacity) {
    counter_capacity = counter_capacity == 0 ? 4 : counter_capacity * 2;
    counters = realloc(counters, counter_capacity * sizeof(ConsoleCounter));
    counter_labels = realloc(counter_labels, counter_capacity * sizeof(char*));
  }

  int index = counter_count++;
  counter_labels[index] = strdup(label);
  counters[index].count = 0;
  return index;
}

static JSValue jsrt_console_count(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* label = "default";
  if (argc > 0) {
    label = JS_ToCString(ctx, argv[0]);
    if (!label)
      label = "default";
  }

  int counter_index = jsrt_find_counter(label);
  if (counter_index < 0) {
    counter_index = jsrt_add_counter(label);
  }

  counters[counter_index].count++;

  // Format and output the count
  char message[256];
  snprintf(message, sizeof(message), "%s: %d", label, counters[counter_index].count);

  DynBuf dbuf;
  jsrt_init_dbuf(ctx, &dbuf);
  for (int g = 0; g < group_level; g++) {
    dbuf_putstr(&dbuf, "  ");
  }
  dbuf_putstr(&dbuf, message);
  fwrite(dbuf.buf, 1, dbuf.size, stdout);
  dbuf_free(&dbuf);
  putchar('\n');

  if (argc > 0 && label != JS_ToCString(ctx, argv[0])) {
    JS_FreeCString(ctx, label);
  }
  return JS_UNDEFINED;
}

static JSValue jsrt_console_countReset(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* label = "default";
  if (argc > 0) {
    label = JS_ToCString(ctx, argv[0]);
    if (!label)
      label = "default";
  }

  int counter_index = jsrt_find_counter(label);
  if (counter_index >= 0) {
    counters[counter_index].count = 0;
  } else {
    char message[256];
    snprintf(message, sizeof(message), "Count for '%s' does not exist", label);
    jsrt_console_output(ctx, 0, NULL, stdout, JSRT_ColorizeFontYellow, JSRT_ColorizeClear, message);
  }

  if (argc > 0 && label != JS_ToCString(ctx, argv[0])) {
    JS_FreeCString(ctx, label);
  }
  return JS_UNDEFINED;
}

static JSValue jsrt_console_group(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  jsrt_console_output(ctx, argc, argv, stdout, NULL, NULL, NULL);
  group_level++;
  return JS_UNDEFINED;
}

static JSValue jsrt_console_groupEnd(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (group_level > 0) {
    group_level--;
  }
  return JS_UNDEFINED;
}

static JSValue jsrt_console_groupCollapsed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Same as group for now - could be enhanced to show collapsed state
  return jsrt_console_group(ctx, this_val, argc, argv);
}

static JSValue jsrt_console_clear(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Clear terminal using ANSI escape sequences
  printf("\033[2J\033[H");
  fflush(stdout);
  return JS_UNDEFINED;
}

static JSValue jsrt_console_dir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Same as log for now - could be enhanced with more detailed object inspection
  jsrt_console_output(ctx, argc, argv, stdout, NULL, NULL, NULL);
  return JS_UNDEFINED;
}

static JSValue jsrt_console_table(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc == 0) {
    return JS_UNDEFINED;
  }

  JSValueConst data = argv[0];
  uint32_t tag = JS_VALUE_GET_NORM_TAG(data);

  if (tag == JS_TAG_OBJECT) {
    // Check if it's an array
    JSValue length_val = JS_GetPropertyStr(ctx, data, "length");
    bool is_array = JS_VALUE_GET_NORM_TAG(length_val) == JS_TAG_INT;
    JS_FreeValue(ctx, length_val);

    DynBuf dbuf;
    jsrt_init_dbuf(ctx, &dbuf);

    // Add group indentation
    for (int g = 0; g < group_level; g++) {
      dbuf_putstr(&dbuf, "  ");
    }

    if (is_array) {
      dbuf_putstr(&dbuf, "┌─────────┬─────────┐\n");
      for (int g = 0; g < group_level; g++) {
        dbuf_putstr(&dbuf, "  ");
      }
      dbuf_putstr(&dbuf, "│ (index) │ Values  │\n");
      for (int g = 0; g < group_level; g++) {
        dbuf_putstr(&dbuf, "  ");
      }
      dbuf_putstr(&dbuf, "├─────────┼─────────┤\n");

      // Get array properties
      JSPropertyEnum* tab;
      uint32_t keys_len;
      if (JS_GetOwnPropertyNames(ctx, &tab, &keys_len, data, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) >= 0) {
        for (uint32_t i = 0; i < keys_len; i++) {
          const char* key = JS_AtomToCString(ctx, tab[i].atom);
          JSValue value = JS_GetProperty(ctx, data, tab[i].atom);

          for (int g = 0; g < group_level; g++) {
            dbuf_putstr(&dbuf, "  ");
          }
          dbuf_putstr(&dbuf, "│    ");
          dbuf_putstr(&dbuf, key);
          dbuf_putstr(&dbuf, "    │   ");

          // Format the value simply
          const char* value_str = JS_ToCString(ctx, value);
          dbuf_putstr(&dbuf, value_str ? value_str : "undefined");
          JS_FreeCString(ctx, value_str);

          dbuf_putstr(&dbuf, "   │\n");

          JS_FreeCString(ctx, key);
          JS_FreeValue(ctx, value);
        }
        js_free(ctx, tab);
      }

      for (int g = 0; g < group_level; g++) {
        dbuf_putstr(&dbuf, "  ");
      }
      dbuf_putstr(&dbuf, "└─────────┴─────────┘");
    } else {
      // Object table
      dbuf_putstr(&dbuf, "(object table - same as console.log for now)");
    }

    fwrite(dbuf.buf, 1, dbuf.size, stdout);
    dbuf_free(&dbuf);
    putchar('\n');
  } else {
    // Fall back to regular log
    jsrt_console_output(ctx, argc, argv, stdout, NULL, NULL, NULL);
  }

  return JS_UNDEFINED;
}

JSValue JSRT_StringFormat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, bool colors) {
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);

  return JS_UNDEFINED;
}

void JSRT_GetJSValuePrettyString(DynBuf* s, JSContext* ctx, JSValueConst value, const char* name, bool colors) {
  uint32_t tag = JS_VALUE_GET_NORM_TAG(value);
  size_t len;
  const char* str;
  switch (tag) {
    case JS_TAG_UNDEFINED: {
      str = JS_ToCStringLen(ctx, &len, value);
      colors&& dbuf_putstr(s, JSRT_ColorizeFontBlack);
      if (str) {
        dbuf_putstr(s, str);
        JS_FreeCString(ctx, str);
      } else {
        dbuf_putstr(s, "undefined");
      }
      colors&& dbuf_putstr(s, JSRT_ColorizeClear);
      break;
    }
    case JS_TAG_BIG_INT:
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
    case JS_TAG_BOOL:
    case JS_TAG_SYMBOL: {
      str = JS_ToCStringLen(ctx, &len, value);
      colors&& dbuf_putstr(s, JSRT_ColorizeFontYellow);
      if (str) {
        dbuf_putstr(s, str);
        JS_FreeCString(ctx, str);
      } else {
        dbuf_putstr(s, "[invalid value]");
      }
      colors&& dbuf_putstr(s, JSRT_ColorizeClear);
      break;
    }
    case JS_TAG_NULL: {
      str = JS_ToCStringLen(ctx, &len, value);
      colors&& dbuf_putstr(s, JSRT_ColorizeFontWhiteBold);
      if (str) {
        dbuf_putstr(s, str);
        JS_FreeCString(ctx, str);
      } else {
        dbuf_putstr(s, "null");
      }
      colors&& dbuf_putstr(s, JSRT_ColorizeClear);
      break;
    }
    case JS_TAG_STRING: {
      str = JS_ToCStringLen(ctx, &len, value);
      colors&& dbuf_putstr(s, JSRT_ColorizeFontGreen);
      if (str) {
        dbuf_putstr(s, str);
        JS_FreeCString(ctx, str);
      } else {
        dbuf_putstr(s, "[invalid string]");
      }
      colors&& dbuf_putstr(s, JSRT_ColorizeClear);
      break;
    }
    case JS_TAG_OBJECT: {
      if (JS_IsFunction(ctx, value)) {
        colors&& dbuf_putstr(s, JSRT_ColorizeFontCyan);
        dbuf_putstr(s, "[Function: ");
        JSValue n = JS_GetPropertyStr(ctx, value, "name");
        str = JS_ToCStringLen(ctx, &len, n);
        if (str) {
          dbuf_putstr(s, str);
          JS_FreeCString(ctx, str);
        } else {
          dbuf_putstr(s, "anonymous");
        }
        JS_FreeValue(ctx, n);
        dbuf_putstr(s, "]");
        colors&& dbuf_putstr(s, JSRT_ColorizeClear);
      } else {
        // is an array?
        JSValue n = JS_GetPropertyStr(ctx, value, "length");
        bool is_array = JS_VALUE_GET_NORM_TAG(n) == JS_TAG_INT;
        JS_FreeValue(ctx, n);

        JSPropertyEnum* tab;
        uint32_t keys_len;
        const char* k;
        JSValue v;

        if (is_array) {
          dbuf_putstr(s, "Array [ ");
        } else {
          if (name == NULL) {
            dbuf_putstr(s, "Object { ");
          } else {
            dbuf_putstr(s, "Object [");
            dbuf_putstr(s, name);
            dbuf_putstr(s, "] { ");
          }
        }

        if (JS_GetOwnPropertyNames(ctx, &tab, &keys_len, value, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) >= 0) {
          for (uint32_t i = 0; i < keys_len; i++) {
            // k is an number?
            k = JS_AtomToCString(ctx, tab[i].atom);
            if (k) {
              dbuf_putstr(s, k);
              dbuf_putstr(s, ": ");
              v = JS_GetProperty(ctx, value, tab[i].atom);
              JSRT_GetJSValuePrettyString(s, ctx, v, k, colors);
              JS_FreeCString(ctx, k);
              JS_FreeValue(ctx, v);
              if (i < keys_len - 1) {
                dbuf_putstr(s, ", ");
              }
            } else {
              dbuf_putstr(s, "[invalid key]: ");
              v = JS_GetProperty(ctx, value, tab[i].atom);
              JSRT_GetJSValuePrettyString(s, ctx, v, NULL, colors);
              JS_FreeValue(ctx, v);
              if (i < keys_len - 1) {
                dbuf_putstr(s, ", ");
              }
            }
          }
        }

        if (is_array) {
          dbuf_putstr(s, " ]");
        } else {
          dbuf_putstr(s, " }");
        }
      }
    } break;
    default:
      dbuf_putstr(s, "<unknown>");
      break;
  }
}
