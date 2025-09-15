#ifndef JSRT_PROCESS_INFO_H
#define JSRT_PROCESS_INFO_H

#include <quickjs.h>

// Initialize process information properties on the process object
void jsrt_process_info_init(JSContext* ctx, JSValue process_obj);

#endif  // JSRT_PROCESS_INFO_H