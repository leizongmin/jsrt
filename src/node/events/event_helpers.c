#include "events_internal.h"

// Helper: Check if this is an EventEmitter (has _events property)
bool is_event_emitter(JSContext* ctx, JSValueConst this_val) {
  JSValue events_prop = JS_GetPropertyStr(ctx, this_val, "_events");
  bool result = !JS_IsUndefined(events_prop);
  JS_FreeValue(ctx, events_prop);
  return result;
}

// Helper: Get or create events object
JSValue get_or_create_events(JSContext* ctx, JSValueConst this_val) {
  JSValue events_obj = JS_GetPropertyStr(ctx, this_val, "_events");
  if (JS_IsUndefined(events_obj)) {
    events_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, this_val, "_events", events_obj);
  }
  return events_obj;
}

// Helper: Get array length
uint32_t get_array_length(JSContext* ctx, JSValueConst array) {
  JSValue length_val = JS_GetPropertyStr(ctx, array, "length");
  uint32_t length = 0;
  if (JS_IsNumber(length_val)) {
    JS_ToUint32(ctx, &length, length_val);
  }
  JS_FreeValue(ctx, length_val);
  return length;
}

// Helper: Get or create _maxListeners property
JSValue get_or_create_max_listeners(JSContext* ctx, JSValueConst this_val) {
  JSValue max_listeners = JS_GetPropertyStr(ctx, this_val, "_maxListeners");
  if (JS_IsUndefined(max_listeners)) {
    // Default max listeners is 10
    max_listeners = JS_NewInt32(ctx, 10);
    JS_SetPropertyStr(ctx, this_val, "_maxListeners", JS_DupValue(ctx, max_listeners));
  }
  return max_listeners;
}