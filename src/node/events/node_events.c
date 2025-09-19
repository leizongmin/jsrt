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

  // Create EventTarget constructor
  JSValue EventTarget = JS_NewCFunction2(ctx, js_event_target_constructor, "EventTarget", 0, JS_CFUNC_constructor, 0);

  // Create and set up EventTarget prototype
  JSValue et_prototype = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, et_prototype, "addEventListener",
                    JS_NewCFunction(ctx, js_event_target_add_event_listener, "addEventListener", 2));
  JS_SetPropertyStr(ctx, et_prototype, "removeEventListener",
                    JS_NewCFunction(ctx, js_event_target_remove_event_listener, "removeEventListener", 2));
  JS_SetPropertyStr(ctx, et_prototype, "dispatchEvent",
                    JS_NewCFunction(ctx, js_event_target_dispatch_event, "dispatchEvent", 1));

  // Set the prototype on the EventTarget constructor
  JS_SetPropertyStr(ctx, EventTarget, "prototype", et_prototype);

  JS_SetPropertyStr(ctx, events_obj, "EventTarget", EventTarget);

  // Create Event constructor
  JSValue Event = JS_NewCFunction2(ctx, js_event_constructor, "Event", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, events_obj, "Event", Event);

  // Create CustomEvent constructor
  JSValue CustomEvent = JS_NewCFunction2(ctx, js_custom_event_constructor, "CustomEvent", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, events_obj, "CustomEvent", CustomEvent);

  // Add static utility methods
  JS_SetPropertyStr(ctx, events_obj, "getEventListeners",
                    JS_NewCFunction(ctx, js_events_get_event_listeners, "getEventListeners", 2));
  JS_SetPropertyStr(ctx, events_obj, "once", JS_NewCFunction(ctx, js_events_once, "once", 2));
  JS_SetPropertyStr(ctx, events_obj, "setMaxListeners",
                    JS_NewCFunction(ctx, js_events_set_max_listeners, "setMaxListeners", 1));
  JS_SetPropertyStr(ctx, events_obj, "getMaxListeners",
                    JS_NewCFunction(ctx, js_events_get_max_listeners, "getMaxListeners", 1));
  JS_SetPropertyStr(ctx, events_obj, "addAbortListener",
                    JS_NewCFunction(ctx, js_events_add_abort_listener, "addAbortListener", 2));

  // Add error handling symbols and features
  JSValue error_monitor = js_events_get_error_monitor(ctx, JS_UNDEFINED, 0, NULL);
  JS_SetPropertyStr(ctx, events_obj, "errorMonitor", error_monitor);

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

  // Export EventTarget
  JSValue EventTarget = JS_GetPropertyStr(ctx, events_module, "EventTarget");
  JS_SetModuleExport(ctx, m, "EventTarget", JS_DupValue(ctx, EventTarget));
  JS_FreeValue(ctx, EventTarget);

  // Export Event
  JSValue Event = JS_GetPropertyStr(ctx, events_module, "Event");
  JS_SetModuleExport(ctx, m, "Event", JS_DupValue(ctx, Event));
  JS_FreeValue(ctx, Event);

  // Export CustomEvent
  JSValue CustomEvent = JS_GetPropertyStr(ctx, events_module, "CustomEvent");
  JS_SetModuleExport(ctx, m, "CustomEvent", JS_DupValue(ctx, CustomEvent));
  JS_FreeValue(ctx, CustomEvent);

  // Export static utility methods
  JSValue getEventListeners = JS_GetPropertyStr(ctx, events_module, "getEventListeners");
  JS_SetModuleExport(ctx, m, "getEventListeners", JS_DupValue(ctx, getEventListeners));
  JS_FreeValue(ctx, getEventListeners);

  JSValue once = JS_GetPropertyStr(ctx, events_module, "once");
  JS_SetModuleExport(ctx, m, "once", JS_DupValue(ctx, once));
  JS_FreeValue(ctx, once);

  JSValue setMaxListeners = JS_GetPropertyStr(ctx, events_module, "setMaxListeners");
  JS_SetModuleExport(ctx, m, "setMaxListeners", JS_DupValue(ctx, setMaxListeners));
  JS_FreeValue(ctx, setMaxListeners);

  JSValue getMaxListeners = JS_GetPropertyStr(ctx, events_module, "getMaxListeners");
  JS_SetModuleExport(ctx, m, "getMaxListeners", JS_DupValue(ctx, getMaxListeners));
  JS_FreeValue(ctx, getMaxListeners);

  JSValue addAbortListener = JS_GetPropertyStr(ctx, events_module, "addAbortListener");
  JS_SetModuleExport(ctx, m, "addAbortListener", JS_DupValue(ctx, addAbortListener));
  JS_FreeValue(ctx, addAbortListener);

  // Export errorMonitor symbol
  JSValue errorMonitor = JS_GetPropertyStr(ctx, events_module, "errorMonitor");
  JS_SetModuleExport(ctx, m, "errorMonitor", JS_DupValue(ctx, errorMonitor));
  JS_FreeValue(ctx, errorMonitor);

  JS_FreeValue(ctx, events_module);
  return 0;
}