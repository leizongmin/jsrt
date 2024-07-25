#include "console.h"

#include <quickjs.h>

#include "../util/dbuf.h"

#define UTF8_CHAR_LEN_MAX 6
static JSValue jsrt_internal_printf(JSContext *ctx, int argc, JSValueConst *argv, FILE *fp);

static JSValue jsrt_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

void JSRT_RuntimeSetupStdConsole(JSRT_Runtime *rt) {
  JSValueConst console = JS_NewObject(rt->ctx);

  JS_SetPropertyStr(rt->ctx, console, "log", JS_NewCFunction(rt->ctx, jsrt_console_log, "log", 1));
  JS_SetPropertyStr(rt->ctx, rt->global, "console", console);
}

static JSValue jsrt_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  int i;
  const char *str;
  size_t len;

  for (i = 0; i < argc; i++) {
    if (i != 0) putchar(' ');
    str = JS_ToCStringLen(ctx, &len, argv[i]);
    if (!str) return JS_EXCEPTION;
    fwrite(str, 1, len, stdout);
    JS_FreeCString(ctx, str);
  }
  putchar('\n');
  return JS_UNDEFINED;
}

static void jsrt_init_dbuf(JSContext *ctx, DynBuf *s) {
  dbuf_init2(s, JS_GetRuntime(ctx), (DynBufReallocFunc *)js_realloc_rt);
}

static bool jsrt_is_digit(int c) { return (c >= '0' && c <= '9'); }

static JSValue jsrt_internal_printf(JSContext *ctx, int argc, JSValueConst *argv, FILE *fp) {
  char fmtbuf[32];
  uint8_t cbuf[UTF8_CHAR_LEN_MAX + 1];
  JSValue res;
  DynBuf dbuf;
  const char *fmt_str = NULL;
  const uint8_t *fmt, *fmt_end;
  const uint8_t *p;
  char *q;
  int i, c, len, mod;
  size_t fmt_len;
  int32_t int32_arg;
  int64_t int64_arg;
  double double_arg;
  const char *string_arg;
  /* Use indirect call to dbuf_printf to prevent gcc warning */
  int (*dbuf_printf_fun)(DynBuf *s, const char *fmt, ...) = (void *)dbuf_printf;

  jsrt_init_dbuf(ctx, &dbuf);

  if (argc > 0) {
    fmt_str = JS_ToCStringLen(ctx, &fmt_len, argv[0]);
    if (!fmt_str) goto fail;

    i = 1;
    fmt = (const uint8_t *)fmt_str;
    fmt_end = fmt + fmt_len;
    while (fmt < fmt_end) {
      for (p = fmt; fmt < fmt_end && *fmt != '%'; fmt++) continue;
      dbuf_put(&dbuf, p, fmt - p);
      if (fmt >= fmt_end) break;
      q = fmtbuf;
      *q++ = *fmt++; /* copy '%' */

      /* flags */
      for (;;) {
        c = *fmt;
        if (c == '0' || c == '#' || c == '+' || c == '-' || c == ' ' || c == '\'') {
          if (q >= fmtbuf + sizeof(fmtbuf) - 1) goto invalid;
          *q++ = c;
          fmt++;
        } else {
          break;
        }
      }
      /* width */
      if (*fmt == '*') {
        if (i >= argc) goto missing;
        if (JS_ToInt32(ctx, &int32_arg, argv[i++])) goto fail;
        q += snprintf(q, fmtbuf + sizeof(fmtbuf) - q, "%d", int32_arg);
        fmt++;
      } else {
        while (jsrt_is_digit(*fmt)) {
          if (q >= fmtbuf + sizeof(fmtbuf) - 1) goto invalid;
          *q++ = *fmt++;
        }
      }
      if (*fmt == '.') {
        if (q >= fmtbuf + sizeof(fmtbuf) - 1) goto invalid;
        *q++ = *fmt++;
        if (*fmt == '*') {
          if (i >= argc) goto missing;
          if (JS_ToInt32(ctx, &int32_arg, argv[i++])) goto fail;
          q += snprintf(q, fmtbuf + sizeof(fmtbuf) - q, "%d", int32_arg);
          fmt++;
        } else {
          while (jsrt_is_digit(*fmt)) {
            if (q >= fmtbuf + sizeof(fmtbuf) - 1) goto invalid;
            *q++ = *fmt++;
          }
        }
      }

      /* we only support the "l" modifier for 64 bit numbers */
      mod = ' ';
      if (*fmt == 'l') {
        mod = *fmt++;
      }

      /* type */
      c = *fmt++;
      if (q >= fmtbuf + sizeof(fmtbuf) - 1) goto invalid;
      *q++ = c;
      *q = '\0';

      switch (c) {
        case 'c':
          if (i >= argc) goto missing;
          if (JS_IsString(argv[i])) {
            string_arg = JS_ToCString(ctx, argv[i++]);
            if (!string_arg) goto fail;
            int32_arg = unicode_from_utf8((const uint8_t *)string_arg, UTF8_CHAR_LEN_MAX, &p);
            JS_FreeCString(ctx, string_arg);
          } else {
            if (JS_ToInt32(ctx, &int32_arg, argv[i++])) goto fail;
          }
          /* handle utf-8 encoding explicitly */
          if ((unsigned)int32_arg > 0x10FFFF) int32_arg = 0xFFFD;
          /* ignore conversion flags, width and precision */
          len = unicode_to_utf8(cbuf, int32_arg);
          dbuf_put(&dbuf, cbuf, len);
          break;

        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
        case 'X':
          if (i >= argc) goto missing;
          if (JS_ToInt64Ext(ctx, &int64_arg, argv[i++])) goto fail;
          if (mod == 'l') {
            /* 64 bit number */
#if defined(_WIN32)
            if (q >= fmtbuf + sizeof(fmtbuf) - 3) goto invalid;
            q[2] = q[-1];
            q[-1] = 'I';
            q[0] = '6';
            q[1] = '4';
            q[3] = '\0';
            dbuf_printf_fun(&dbuf, fmtbuf, (int64_t)int64_arg);
#else
            if (q >= fmtbuf + sizeof(fmtbuf) - 2) goto invalid;
            q[1] = q[-1];
            q[-1] = q[0] = 'l';
            q[2] = '\0';
            dbuf_printf_fun(&dbuf, fmtbuf, (long long)int64_arg);
#endif
          } else {
            dbuf_printf_fun(&dbuf, fmtbuf, (int)int64_arg);
          }
          break;

        case 's':
          if (i >= argc) goto missing;
          /* XXX: handle strings containing null characters */
          string_arg = JS_ToCString(ctx, argv[i++]);
          if (!string_arg) goto fail;
          dbuf_printf_fun(&dbuf, fmtbuf, string_arg);
          JS_FreeCString(ctx, string_arg);
          break;

        case 'e':
        case 'f':
        case 'g':
        case 'a':
        case 'E':
        case 'F':
        case 'G':
        case 'A':
          if (i >= argc) goto missing;
          if (JS_ToFloat64(ctx, &double_arg, argv[i++])) goto fail;
          dbuf_printf_fun(&dbuf, fmtbuf, double_arg);
          break;

        case '%':
          dbuf_putc(&dbuf, '%');
          break;

        default:
          /* XXX: should support an extension mechanism */
        invalid:
          JS_ThrowTypeError(ctx, "invalid conversion specifier in format string");
          goto fail;
        missing:
          JS_ThrowReferenceError(ctx, "missing argument for conversion specifier");
          goto fail;
      }
    }
    JS_FreeCString(ctx, fmt_str);
  }
  if (dbuf.error) {
    res = JS_ThrowOutOfMemory(ctx);
  } else {
    if (fp) {
      len = fwrite(dbuf.buf, 1, dbuf.size, fp);
      res = JS_NewInt32(ctx, len);
    } else {
      res = JS_NewStringLen(ctx, (char *)dbuf.buf, dbuf.size);
    }
  }
  dbuf_free(&dbuf);
  return res;

fail:
  JS_FreeCString(ctx, fmt_str);
  dbuf_free(&dbuf);
  return JS_EXCEPTION;
}
