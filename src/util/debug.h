#ifndef __JSRT_DEBUG_H__
#define __JSRT_DEBUG_H__

#ifdef DEBUG
#define JSRT_Debug(fmt, ...) \
  fprintf(stderr, "\033[32m[JSRT_Debug:%s:%d] " fmt "\033[0m\n", __FILE__, __LINE__, __VA_ARGS__)
#else
#define JSRT_Debug(...)
#endif

#endif
