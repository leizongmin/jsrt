#ifndef JSRT_HTTP_FETCH_H
#define JSRT_HTTP_FETCH_H

#include "../runtime.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif

JSValue jsrt_fetch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void JSRT_RuntimeSetupHttpFetch(JSRT_Runtime* rt);

#ifdef __cplusplus
}
#endif

#endif