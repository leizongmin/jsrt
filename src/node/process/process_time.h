#ifndef JSRT_PROCESS_TIME_H
#define JSRT_PROCESS_TIME_H

#include <quickjs.h>

// Initialize process time functions on the process object
void jsrt_process_time_init(JSContext* ctx, JSValue process_obj);

#endif  // JSRT_PROCESS_TIME_H