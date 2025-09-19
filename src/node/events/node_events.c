#include "events_internal.h"

// Initialize node:events module for CommonJS
JSValue JSRT_InitNodeEvents(JSContext* ctx) {
  JSValue events_obj = JS_NewObject(ctx);

  // Create EventEmitter constructor
  JSValue EventEmitter =
      JS_NewCFunction2(ctx, js_event_emitter_constructor, "EventEmitter", 0, JS_CFUNC_constructor, 0);

  // Create and set up EventEmitter prototype
  JSValue prototype = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, prototype, "on", JS_NewCFunction(ctx, js_event_emitter_on, "on", 2));
  JS_SetPropertyStr(ctx, prototype, "addListener",
                    JS_NewCFunction(ctx, js_event_emitter_add_listener, "addListener", 2));
  JS_SetPropertyStr(ctx, prototype, "once", JS_NewCFunction(ctx, js_event_emitter_once, "once", 2));
  JS_SetPropertyStr(ctx, prototype, "removeListener",
                    JS_NewCFunction(ctx, js_event_emitter_remove_listener, "removeListener", 2));
  JS_SetPropertyStr(ctx, prototype, "emit", JS_NewCFunction(ctx, js_event_emitter_emit, "emit", 1));
  JS_SetPropertyStr(ctx, prototype, "listenerCount",
                    JS_NewCFunction(ctx, js_event_emitter_listener_count, "listenerCount", 1));
  JS_SetPropertyStr(ctx, prototype, "removeAllListeners",
                    JS_NewCFunction(ctx, js_event_emitter_remove_all_listeners, "removeAllListeners", 0));

  // Add the new methods
  JS_SetPropertyStr(ctx, prototype, "prependListener",
                    JS_NewCFunction(ctx, js_event_emitter_prepend_listener, "prependListener", 2));
  JS_SetPropertyStr(ctx, prototype, "prependOnceListener",
                    JS_NewCFunction(ctx, js_event_emitter_prepend_once_listener, "prependOnceListener", 2));
  JS_SetPropertyStr(ctx, prototype, "eventNames", JS_NewCFunction(ctx, js_event_emitter_event_names, "eventNames", 0));
  JS_SetPropertyStr(ctx, prototype, "listeners", JS_NewCFunction(ctx, js_event_emitter_listeners, "listeners", 1));
  JS_SetPropertyStr(ctx, prototype, "rawListeners",
                    JS_NewCFunction(ctx, js_event_emitter_raw_listeners, "rawListeners", 1));
  JS_SetPropertyStr(ctx, prototype, "off", JS_NewCFunction(ctx, js_event_emitter_off, "off", 2));
  JS_SetPropertyStr(ctx, prototype, "setMaxListeners",
                    JS_NewCFunction(ctx, js_event_emitter_set_max_listeners, "setMaxListeners", 1));
  JS_SetPropertyStr(ctx, prototype, "getMaxListeners",
                    JS_NewCFunction(ctx, js_event_emitter_get_max_listeners, "getMaxListeners", 0));

  // Set the prototype on the constructor
  JS_SetPropertyStr(ctx, EventEmitter, "prototype", prototype);

  JS_SetPropertyStr(ctx, events_obj, "EventEmitter", EventEmitter);

  // Export EventEmitter as default export as well
  JS_SetPropertyStr(ctx, events_obj, "default", JS_DupValue(ctx, EventEmitter));

  return events_obj;
}

// Initialize node:events module for ES modules
int js_node_events_init(JSContext* ctx, JSModuleDef* m) {
  JSValue events_module = JSRT_InitNodeEvents(ctx);

  // Export EventEmitter
  JSValue EventEmitter = JS_GetPropertyStr(ctx, events_module, "EventEmitter");
  JS_SetModuleExport(ctx, m, "EventEmitter", JS_DupValue(ctx, EventEmitter));
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, EventEmitter));
  JS_FreeValue(ctx, EventEmitter);

  JS_FreeValue(ctx, events_module);
  return 0;
}