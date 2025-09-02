#ifndef __JSRT_STD_FFI_H__
#define __JSRT_STD_FFI_H__

#include <quickjs.h>

#include "../runtime.h"

// Initialize FFI module
void JSRT_RuntimeSetupStdFFI(JSRT_Runtime *rt);

// Create FFI module for require("std:ffi")
JSValue JSRT_CreateFFIModule(JSContext *ctx);

#endif