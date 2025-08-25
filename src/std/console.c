#include "console.h"

#include <quickjs.h>
#include <stdio.h>
#include <unistd.h>

#include "../util/colorize.h"
#include "../util/dbuf.h"

static JSValue jsrt_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

void JSRT_RuntimeSetupStdConsole(JSRT_Runtime *rt) {
  JSValueConst console = JS_NewObject(rt->ctx);

  JS_SetPropertyStr(rt->ctx, console, "log", JS_NewCFunction(rt->ctx, jsrt_console_log, "log", 1));
  JS_SetPropertyStr(rt->ctx, rt->global, "console", console);
}

static void jsrt_init_dbuf(JSContext *ctx, DynBuf *s) {
  dbuf_init2(s, JS_GetRuntime(ctx), (DynBufReallocFunc *)js_realloc_rt);
}

static JSValue jsrt_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  int i;
  const char *str;
  size_t len;

  DynBuf dbuf;
  jsrt_init_dbuf(ctx, &dbuf);
  bool colors = isatty(STDOUT_FILENO) && isatty(STDERR_FILENO);

  for (i = 0; i < argc; i++) {
    if (i != 0) {
      dbuf_putstr(&dbuf, " ");
    }
    JSRT_GetJSValuePrettyString(&dbuf, ctx, argv[i], NULL, colors);
  }

  fwrite(dbuf.buf, 1, dbuf.size, stdout);
  dbuf_free(&dbuf);

  putchar('\n');
  return JS_UNDEFINED;
}

JSValue JSRT_StringFormat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, bool colors) {
  JSRT_Runtime *rt = JS_GetContextOpaque(ctx);

  return JS_UNDEFINED;
}

void JSRT_GetJSValuePrettyString(DynBuf *s, JSContext *ctx, JSValueConst value, const char *name, bool colors) {
  uint32_t tag = JS_VALUE_GET_NORM_TAG(value);
  size_t len;
  const char *str;
  switch (tag) {
    case JS_TAG_UNDEFINED: {
      str = JS_ToCStringLen(ctx, &len, value);
      colors &&dbuf_putstr(s, JSRT_ColorizeFontBlack);
      dbuf_putstr(s, str);
      colors &&dbuf_putstr(s, JSRT_ColorizeClear);
      JS_FreeCString(ctx, str);
      break;
    }
    case JS_TAG_BIG_INT:
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
    case JS_TAG_BOOL:
    case JS_TAG_SYMBOL: {
      str = JS_ToCStringLen(ctx, &len, value);
      colors &&dbuf_putstr(s, JSRT_ColorizeFontYellow);
      dbuf_putstr(s, str);
      colors &&dbuf_putstr(s, JSRT_ColorizeClear);
      JS_FreeCString(ctx, str);
      break;
    }
    case JS_TAG_NULL: {
      str = JS_ToCStringLen(ctx, &len, value);
      colors &&dbuf_putstr(s, JSRT_ColorizeFontWhiteBold);
      dbuf_putstr(s, str);
      colors &&dbuf_putstr(s, JSRT_ColorizeClear);
      JS_FreeCString(ctx, str);
      break;
    }
    case JS_TAG_STRING: {
      str = JS_ToCStringLen(ctx, &len, value);
      colors &&dbuf_putstr(s, JSRT_ColorizeFontGreen);
      dbuf_putstr(s, str);
      colors &&dbuf_putstr(s, JSRT_ColorizeClear);
      JS_FreeCString(ctx, str);
      break;
    }
    case JS_TAG_OBJECT: {
      if (JS_IsFunction(ctx, value)) {
        colors &&dbuf_putstr(s, JSRT_ColorizeFontCyan);
        dbuf_putstr(s, "[Function: ");
        JSValue n = JS_GetPropertyStr(ctx, value, "name");
        str = JS_ToCStringLen(ctx, &len, n);
        dbuf_putstr(s, str);
        JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, n);
        dbuf_putstr(s, "]");
        colors &&dbuf_putstr(s, JSRT_ColorizeClear);
      } else {
        // is an array?
        JSValue n = JS_GetPropertyStr(ctx, value, "length");
        bool is_array = JS_VALUE_GET_NORM_TAG(n) == JS_TAG_INT;
        JS_FreeValue(ctx, n);

        JSPropertyEnum *tab;
        uint32_t keys_len;
        const char *k;
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
            dbuf_putstr(s, k);
            dbuf_putstr(s, ": ");
            v = JS_GetProperty(ctx, value, tab[i].atom);
            JSRT_GetJSValuePrettyString(s, ctx, v, k, colors);
            JS_FreeCString(ctx, k);
            JS_FreeValue(ctx, v);
            if (i < keys_len - 1) {
              dbuf_putstr(s, ", ");
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
