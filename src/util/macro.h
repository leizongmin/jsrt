#ifndef __JSRT_UTIL_MACRO_H__
#define __JSRT_UTIL_MACRO_H__

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#endif
