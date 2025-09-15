#ifndef JSRT_PROCESS_NODE_H
#define JSRT_PROCESS_NODE_H

#include <quickjs.h>

// Initialize Node.js specific process functions on the process object
void jsrt_process_node_init(JSContext* ctx, JSValue process_obj);

// Execute all queued nextTick callbacks
void jsrt_process_execute_next_tick(JSContext* ctx);

// Cleanup function for nextTick callbacks
void jsrt_process_node_cleanup(void);

// External function declaration for error dumping
extern void js_std_dump_error(JSContext* ctx);

#endif  // JSRT_PROCESS_NODE_H