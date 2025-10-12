#ifndef __JSRT_DEBUG_H__
#define __JSRT_DEBUG_H__

#include <stdio.h>

#ifdef DEBUG
#define JSRT_Debug(fmt, ...) \
  fprintf(stderr, "\033[32m[JSRT_Debug:%s:%d] " fmt "\033[0m\n", __FILE__, __LINE__, ##__VA_ARGS__)

// Truncated debug output - limits output to avoid performance issues
// Maximum 200 characters before truncation
#define JSRT_Debug_Truncated(fmt, ...)                          \
  do {                                                          \
    char _dbg_buf[250];                                         \
    int _written = snprintf(_dbg_buf, 200, fmt, ##__VA_ARGS__); \
    if (_written >= 200) {                                      \
      snprintf(_dbg_buf + 196, 5, "...");                       \
      fprintf(stderr, "%s [truncated]\n", _dbg_buf);            \
    } else if (_written > 0) {                                  \
      fprintf(stderr, "%s", _dbg_buf);                          \
    }                                                           \
  } while (0)
#else
#define JSRT_Debug(...)
#define JSRT_Debug_Truncated(...)
#endif

#endif
