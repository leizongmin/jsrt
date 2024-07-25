#ifndef __JSRT_UTIL_DBUF_H__
#define __JSRT_UTIL_DBUF_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *DynBufReallocFunc(void *opaque, void *ptr, size_t size);

typedef struct DynBuf {
  uint8_t *buf;
  size_t size;
  size_t allocated_size;
  bool error; /* true if a memory allocation error occurred */
  DynBufReallocFunc *realloc_func;
  void *opaque; /* for realloc_func */
} DynBuf;

void *dbuf_default_realloc(void *opaque, void *ptr, size_t size);

void dbuf_init2(DynBuf *s, void *opaque, DynBufReallocFunc *realloc_func);

void dbuf_init(DynBuf *s);

/* return < 0 if error */
int dbuf_realloc(DynBuf *s, size_t new_size);

int dbuf_write(DynBuf *s, size_t offset, const uint8_t *data, size_t len);

int dbuf_put(DynBuf *s, const uint8_t *data, size_t len);

int dbuf_put_self(DynBuf *s, size_t offset, size_t len);

int dbuf_putc(DynBuf *s, uint8_t c);

int dbuf_putstr(DynBuf *s, const char *str);

int __attribute__((format(printf, 2, 3))) dbuf_printf(DynBuf *s, const char *fmt, ...);

void dbuf_free(DynBuf *s);

int unicode_from_utf8(const uint8_t *p, int max_len, const uint8_t **pp);
int unicode_to_utf8(uint8_t *buf, unsigned int c);

#endif
