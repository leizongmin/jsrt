#ifndef __JSRT_MODULE_DEBUG_H__
#define __JSRT_MODULE_DEBUG_H__

/**
 * Module System Debug Logging
 *
 * Debug logging macros for the module loading system.
 * Only active when compiled with DEBUG flag (make jsrt_g).
 *
 * Usage:
 *   MODULE_DEBUG("Loading module: %s", module_name);
 *   MODULE_DEBUG_RESOLVER("Resolved path: %s -> %s", specifier, resolved);
 *   MODULE_DEBUG_CACHE("Cache hit for: %s", key);
 */

#ifdef DEBUG

#include <stdio.h>

// Main module debug macro - green color
#define MODULE_DEBUG(fmt, ...) \
  fprintf(stderr, "\033[32m[MODULE:%s:%d] " fmt "\033[0m\n", __FILE__, __LINE__, ##__VA_ARGS__)

// Resolver-specific debug - cyan color
#define MODULE_DEBUG_RESOLVER(fmt, ...) \
  fprintf(stderr, "\033[36m[RESOLVER:%s:%d] " fmt "\033[0m\n", __FILE__, __LINE__, ##__VA_ARGS__)

// Loader-specific debug - blue color
#define MODULE_DEBUG_LOADER(fmt, ...) \
  fprintf(stderr, "\033[34m[LOADER:%s:%d] " fmt "\033[0m\n", __FILE__, __LINE__, ##__VA_ARGS__)

// Cache-specific debug - yellow color
#define MODULE_DEBUG_CACHE(fmt, ...) \
  fprintf(stderr, "\033[33m[CACHE:%s:%d] " fmt "\033[0m\n", __FILE__, __LINE__, ##__VA_ARGS__)

// Detector-specific debug - magenta color
#define MODULE_DEBUG_DETECTOR(fmt, ...) \
  fprintf(stderr, "\033[35m[DETECTOR:%s:%d] " fmt "\033[0m\n", __FILE__, __LINE__, ##__VA_ARGS__)

// Protocol-specific debug - light blue color
#define MODULE_DEBUG_PROTOCOL(fmt, ...) \
  fprintf(stderr, "\033[96m[PROTOCOL:%s:%d] " fmt "\033[0m\n", __FILE__, __LINE__, ##__VA_ARGS__)

// Error/warning debug - red color
#define MODULE_DEBUG_ERROR(fmt, ...) \
  fprintf(stderr, "\033[31m[MODULE_ERROR:%s:%d] " fmt "\033[0m\n", __FILE__, __LINE__, ##__VA_ARGS__)

#else

// No-op macros when DEBUG is not defined
#define MODULE_DEBUG(...)
#define MODULE_DEBUG_RESOLVER(...)
#define MODULE_DEBUG_LOADER(...)
#define MODULE_DEBUG_CACHE(...)
#define MODULE_DEBUG_DETECTOR(...)
#define MODULE_DEBUG_PROTOCOL(...)
#define MODULE_DEBUG_ERROR(...)

#endif

#endif  // __JSRT_MODULE_DEBUG_H__
