#ifndef URL_SAFE_STRING_H
#define URL_SAFE_STRING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Safe sprintf that prevents buffer overflows (for functions with cleanup)
#define SAFE_SNPRINTF(buf, size, format, ...)                                              \
  do {                                                                                     \
    int ret = snprintf(buf, size, format, __VA_ARGS__);                                    \
    if (ret < 0 || (size_t)ret >= size) {                                                  \
      JSRT_Debug("SAFE_SNPRINTF: Buffer overflow prevented in %s:%d", __FILE__, __LINE__); \
      goto cleanup_and_return_error;                                                       \
    }                                                                                      \
  } while (0)

// Safe sprintf for void functions that just return
#define SAFE_SNPRINTF_VOID(buf, size, format, ...)                                              \
  do {                                                                                          \
    int ret = snprintf(buf, size, format, __VA_ARGS__);                                         \
    if (ret < 0 || (size_t)ret >= size) {                                                       \
      JSRT_Debug("SAFE_SNPRINTF_VOID: Buffer overflow prevented in %s:%d", __FILE__, __LINE__); \
      return;                                                                                   \
    }                                                                                           \
  } while (0)

// Safe string duplication with NULL check
static inline char* safe_strdup(const char* str) {
  if (!str)
    return NULL;
  char* result = strdup(str);
  if (!result) {
    JSRT_Debug("safe_strdup: Memory allocation failed");
  }
  return result;
}

// Safe malloc with size validation
static inline void* safe_malloc(size_t size) {
  if (size == 0 || size > SIZE_MAX / 2) {
    JSRT_Debug("safe_malloc: Invalid size %zu", size);
    return NULL;
  }
  void* result = malloc(size);
  if (!result) {
    JSRT_Debug("safe_malloc: Memory allocation failed for size %zu", size);
  }
  return result;
}

// Safe string concatenation
static inline int safe_string_append(char** dest, size_t* dest_size, const char* src) {
  if (!dest || !src)
    return -1;

  size_t src_len = strlen(src);
  size_t current_len = *dest ? strlen(*dest) : 0;
  size_t new_size = current_len + src_len + 1;

  if (new_size < current_len)
    return -1;  // Overflow check

  char* new_dest = realloc(*dest, new_size);
  if (!new_dest)
    return -1;

  if (!*dest) {
    new_dest[0] = '\0';
  }
  strcat(new_dest, src);
  *dest = new_dest;
  *dest_size = new_size;
  return 0;
}

#endif  // URL_SAFE_STRING_H