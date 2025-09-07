#pragma once
#include <quickjs.h>
#include "../runtime.h"

JSValue jsrt_init_module_navigator(JSContext* ctx);
void JSRT_RuntimeSetupNavigator(JSRT_Runtime* rt);