#ifndef __JSRT_STD_PROCESS_H__
#define __JSRT_STD_PROCESS_H__

#include <quickjs.h>

#include "../runtime.h"

JSValue JSRT_CreateProcessModule(JSContext *ctx);
void JSRT_RuntimeSetupStdProcess(JSRT_Runtime *rt);

// Global variables for command line arguments
extern int g_jsrt_argc;
extern char **g_jsrt_argv;

#endif