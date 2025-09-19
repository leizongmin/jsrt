#ifndef EVENTS_INTERNAL_H
#define EVENTS_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../util/debug.h"
#include "../node_modules.h"

// Helper function declarations
bool is_event_emitter(JSContext* ctx, JSValueConst this_val);
JSValue get_or_create_events(JSContext* ctx, JSValueConst this_val);
uint32_t get_array_length(JSContext* ctx, JSValueConst array);
JSValue get_or_create_max_listeners(JSContext* ctx, JSValueConst this_val);

// Core EventEmitter method declarations
JSValue js_event_emitter_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_add_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_once(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_remove_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_emit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_listener_count(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_remove_all_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Enhanced EventEmitter method declarations
JSValue js_event_emitter_prepend_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_prepend_once_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_event_names(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_raw_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_off(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_set_max_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_get_max_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Constructor declaration
JSValue js_event_emitter_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

#endif  // EVENTS_INTERNAL_H