/**
 * @file worker_threads.h
 * @brief Node.js worker_threads module interface
 */

#ifndef JSRT_WORKER_THREADS_H
#define JSRT_WORKER_THREADS_H

#include "../../deps/quickjs/quickjs.h"

// Initialize worker_threads module namespace
JSValue JSRT_InitNodeWorkerThreads(JSContext* ctx);

// Initialize worker_threads as a CommonJS module
int js_node_worker_threads_init(JSContext* ctx, JSModuleDef* m);

#endif  // JSRT_WORKER_THREADS_H