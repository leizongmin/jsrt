#ifndef __JSRT_STD_CONSOLE_H__
#define __JSRT_STD_CONSOLE_H__

#include "../runtime.h"
#include "../util/dbuf.h"

typedef enum {
  JSRT_VALUE_COLOR_NONE = 0,
  JSRT_VALUE_COLOR_FULL,
  JSRT_VALUE_COLOR_NO_STRINGS,
} JSRT_ValueColorMode;

void JSRT_RuntimeSetupStdConsole(JSRT_Runtime* rt);
JSValue JSRT_StringFormat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, bool colors);
void JSRT_GetJSValuePrettyString(DynBuf* s, JSContext* ctx, JSValueConst value, const char* name,
                                 JSRT_ValueColorMode color_mode);

#endif
