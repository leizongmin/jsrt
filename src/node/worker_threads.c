/**
 * @file worker_threads.c
 * @brief Node.js worker_threads module implementation
 *
 * This is a compatibility shim for the worker_threads module.
 * Since jsrt is a single-threaded runtime, we provide stub implementations
 * that allow packages requiring worker_threads to load without errors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../deps/quickjs/quickjs.h"
#include "util/debug.h"
#include "worker_threads.h"

// ============================================================================
// Worker Class Implementation (Stub)
// ============================================================================

// Worker constructor - creates a stub worker object
static JSValue js_worker_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSRT_Debug("Worker constructor called - creating stub worker");

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Worker requires at least 1 argument (script path)");
  }

  // Create worker object
  JSValue worker_obj = JS_NewObject(ctx);
  if (JS_IsException(worker_obj)) {
    return JS_EXCEPTION;
  }

  // Get the script path (first argument)
  const char* script_path = JS_ToCString(ctx, argv[0]);
  if (!script_path) {
    JS_FreeValue(ctx, worker_obj);
    return JS_ThrowTypeError(ctx, "Invalid script path");
  }

  // Store basic properties (stub implementation)
  JS_SetPropertyStr(ctx, worker_obj, "threadId", JS_NewInt32(ctx, 1));
  JS_SetPropertyStr(ctx, worker_obj, "resourceLimits", JS_NULL);
  JS_SetPropertyStr(ctx, worker_obj, "env", JS_NewObject(ctx));
  JS_SetPropertyStr(ctx, worker_obj, "eval", JS_NewBool(ctx, 0));
  JS_SetPropertyStr(ctx, worker_obj, "stdin", JS_NewBool(ctx, 1));
  JS_SetPropertyStr(ctx, worker_obj, "stdout", JS_NewBool(ctx, 1));
  JS_SetPropertyStr(ctx, worker_obj, "stderr", JS_NewBool(ctx, 1));

  // Create stub event emitter functionality
  JSValue listeners_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, worker_obj, "_listeners", listeners_obj);

  JS_FreeCString(ctx, script_path);
  JSRT_Debug("Stub worker created successfully");

  return worker_obj;
}

// Worker.postMessage() - stub implementation
static JSValue js_worker_post_message(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("Worker.postMessage called - stub implementation");

  // In a real implementation, this would serialize and send the message
  // For our stub, we just acknowledge the message

  return JS_UNDEFINED;
}

// Callback function for promise resolution
static JSValue js_promise_resolver(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc > 0 && JS_IsFunction(ctx, argv[0])) {
    JS_Call(ctx, argv[0], JS_UNDEFINED, 0, NULL);
  }
  return JS_UNDEFINED;
}

// Worker.terminate() - stub implementation
static JSValue js_worker_terminate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("Worker.terminate called - stub implementation");

  // In a real implementation, this would terminate the worker thread
  // For our stub, we just return a resolved Promise
  JSValue promise = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, promise, "then", JS_NewCFunction(ctx, js_promise_resolver, "then", 1));

  return promise;
}

// Worker.addEventListener() - stub implementation
static JSValue js_worker_add_event_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "addEventListener requires at least 2 arguments");
  }

  const char* event_type = JS_ToCString(ctx, argv[0]);
  if (!event_type) {
    return JS_ThrowTypeError(ctx, "Invalid event type");
  }

  JSValue listener = argv[1];
  if (!JS_IsFunction(ctx, listener)) {
    JS_FreeCString(ctx, event_type);
    return JS_ThrowTypeError(ctx, "Listener must be a function");
  }

  // Get or create listeners array for this event type
  JSValue listeners = JS_GetPropertyStr(ctx, this_val, "_listeners");
  JSValue event_listeners = JS_GetPropertyStr(ctx, listeners, event_type);

  if (!JS_IsArray(ctx, event_listeners)) {
    event_listeners = JS_NewArray(ctx);
    JS_SetPropertyStr(ctx, listeners, event_type, event_listeners);
  }

  // Add listener to array
  JSValue length_val = JS_GetPropertyStr(ctx, event_listeners, "length");
  int32_t length;
  JS_ToInt32(ctx, &length, length_val);
  JS_SetPropertyUint32(ctx, event_listeners, length, JS_DupValue(ctx, listener));
  JS_FreeValue(ctx, length_val);

  JS_FreeValue(ctx, event_listeners);
  JS_FreeValue(ctx, listeners);
  JS_FreeCString(ctx, event_type);

  return JS_UNDEFINED;
}

// ============================================================================
// MessagePort Helper Functions
// ============================================================================

// MessagePort.close() - stub implementation
static JSValue js_message_port_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("MessagePort.close called - stub implementation");
  return JS_UNDEFINED;
}

// ============================================================================
// MessageChannel Class Implementation (Stub)
// ============================================================================

// MessageChannel constructor
static JSValue js_message_channel_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSRT_Debug("MessageChannel constructor called - creating stub channel");

  JSValue channel_obj = JS_NewObject(ctx);
  if (JS_IsException(channel_obj)) {
    return JS_EXCEPTION;
  }

  // Create port1 and port2 (stub MessagePort objects)
  JSValue port1 = JS_NewObject(ctx);
  JSValue port2 = JS_NewObject(ctx);

  // Add basic MessagePort properties and methods
  JS_SetPropertyStr(ctx, port1, "started", JS_NewBool(ctx, 1));
  JS_SetPropertyStr(ctx, port1, "close", JS_NewCFunction(ctx, js_message_port_close, "close", 0));

  JS_SetPropertyStr(ctx, port2, "started", JS_NewBool(ctx, 1));
  JS_SetPropertyStr(ctx, port2, "close", JS_NewCFunction(ctx, js_message_port_close, "close", 0));

  JS_SetPropertyStr(ctx, channel_obj, "port1", port1);
  JS_SetPropertyStr(ctx, channel_obj, "port2", port2);

  return channel_obj;
}

// ============================================================================
// worker_threads Module Initialization
// ============================================================================

static JSValue js_worker_isMainThread(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Always return true since jsrt is single-threaded
  return JS_NewBool(ctx, 1);
}

static JSValue js_worker_parentPort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Return null since there's no parent thread in single-threaded mode
  return JS_NULL;
}

static JSValue js_worker_threadId(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Return 0 for main thread
  return JS_NewInt32(ctx, 0);
}

static JSValue js_worker_getEnvironmentData(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Return empty object for environment data
  return JS_NewObject(ctx);
}

static JSValue js_worker_setEnvironmentData(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "setEnvironmentData requires at least 2 arguments");
  }
  // Stub implementation - does nothing
  return JS_UNDEFINED;
}

// Initialize the worker_threads module namespace
JSValue JSRT_InitNodeWorkerThreads(JSContext* ctx) {
  JSRT_Debug("Initializing worker_threads module");

  JSValue worker_threads = JS_NewObject(ctx);

  // Create Worker class
  JSValue worker_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, worker_proto, "postMessage", JS_NewCFunction(ctx, js_worker_post_message, "postMessage", 1));
  JS_SetPropertyStr(ctx, worker_proto, "terminate", JS_NewCFunction(ctx, js_worker_terminate, "terminate", 0));
  JS_SetPropertyStr(ctx, worker_proto, "addEventListener",
                    JS_NewCFunction(ctx, js_worker_add_event_listener, "addEventListener", 2));
  JS_SetPropertyStr(ctx, worker_proto, "on", JS_NewCFunction(ctx, js_worker_add_event_listener, "on", 2));
  JS_SetPropertyStr(ctx, worker_proto, "removeEventListener",
                    JS_NewCFunction(ctx, js_worker_add_event_listener, "removeEventListener", 2));

  JSValue worker_class = JS_NewCFunction2(ctx, js_worker_constructor, "Worker", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, worker_class, "prototype", worker_proto);
  JS_SetPropertyStr(ctx, worker_threads, "Worker", worker_class);

  // Create MessageChannel class
  JSValue message_channel_proto = JS_NewObject(ctx);
  JSValue message_channel_class =
      JS_NewCFunction2(ctx, js_message_channel_constructor, "MessageChannel", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, message_channel_class, "prototype", message_channel_proto);
  JS_SetPropertyStr(ctx, worker_threads, "MessageChannel", message_channel_class);

  // Add worker-specific functions and properties
  JS_SetPropertyStr(ctx, worker_threads, "isMainThread",
                    JS_NewCFunction(ctx, js_worker_isMainThread, "isMainThread", 0));
  JS_SetPropertyStr(ctx, worker_threads, "parentPort", JS_NewCFunction(ctx, js_worker_parentPort, "parentPort", 0));
  JS_SetPropertyStr(ctx, worker_threads, "threadId", JS_NewCFunction(ctx, js_worker_threadId, "threadId", 0));
  JS_SetPropertyStr(ctx, worker_threads, "getEnvironmentData",
                    JS_NewCFunction(ctx, js_worker_getEnvironmentData, "getEnvironmentData", 0));
  JS_SetPropertyStr(ctx, worker_threads, "setEnvironmentData",
                    JS_NewCFunction(ctx, js_worker_setEnvironmentData, "setEnvironmentData", 2));

  // Add constants
  JS_SetPropertyStr(ctx, worker_threads, "SHARE_ENV", JS_NewInt32(ctx, 0));

  JSRT_Debug("worker_threads module initialized successfully");
  return worker_threads;
}

// ES module initialization
int js_node_worker_threads_init(JSContext* ctx, JSModuleDef* m) {
  JSValue worker_threads = JSRT_InitNodeWorkerThreads(ctx);

  // Export individual components
  JS_SetModuleExport(ctx, m, "Worker", JS_GetPropertyStr(ctx, worker_threads, "Worker"));
  JS_SetModuleExport(ctx, m, "MessageChannel", JS_GetPropertyStr(ctx, worker_threads, "MessageChannel"));
  JS_SetModuleExport(ctx, m, "isMainThread", JS_GetPropertyStr(ctx, worker_threads, "isMainThread"));
  JS_SetModuleExport(ctx, m, "parentPort", JS_GetPropertyStr(ctx, worker_threads, "parentPort"));
  JS_SetModuleExport(ctx, m, "threadId", JS_GetPropertyStr(ctx, worker_threads, "threadId"));
  JS_SetModuleExport(ctx, m, "getEnvironmentData", JS_GetPropertyStr(ctx, worker_threads, "getEnvironmentData"));
  JS_SetModuleExport(ctx, m, "setEnvironmentData", JS_GetPropertyStr(ctx, worker_threads, "setEnvironmentData"));
  JS_SetModuleExport(ctx, m, "SHARE_ENV", JS_GetPropertyStr(ctx, worker_threads, "SHARE_ENV"));

  // Export the whole module as default
  JS_SetModuleExport(ctx, m, "default", worker_threads);

  return 0;
}