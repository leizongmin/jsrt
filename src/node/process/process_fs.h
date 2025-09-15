#ifndef JSRT_PROCESS_FS_H
#define JSRT_PROCESS_FS_H

#include <quickjs.h>

// Initialize process filesystem functions on the process object
void jsrt_process_fs_init(JSContext* ctx, JSValue process_obj);

#endif  // JSRT_PROCESS_FS_H