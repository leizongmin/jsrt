#ifndef __JSRT_STD_CONSOLE_H__
#define __JSRT_STD_CONSOLE_H__

#include "../runtime.h"
#include "../util/dbuf.h"

void JSRT_RuntimeSetupStdConsole(JSRT_Runtime *rt);
JSValue JSRT_StringFormat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
void JSRT_GetJSValuePrettyString(DynBuf *s, JSContext *ctx, JSValueConst value, const char *name);

#endif
