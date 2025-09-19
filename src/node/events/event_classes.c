#include <uv.h>
#include "events_internal.h"

// Event constructor - new Event(type[, options])
JSValue js_event_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Event constructor requires at least 1 argument");
  }

  // Get event type (required)
  const char* event_type = JS_ToCString(ctx, argv[0]);
  if (!event_type) {
    return JS_ThrowTypeError(ctx, "Event type must be a string");
  }

  // Create new Event object
  JSValue obj, proto;

  if (JS_IsUndefined(new_target)) {
    JS_FreeCString(ctx, event_type);
    return JS_ThrowTypeError(ctx, "Event constructor must be called with 'new'");
  }

  obj = JS_NewObject(ctx);
  if (JS_IsException(obj)) {
    JS_FreeCString(ctx, event_type);
    return obj;
  }

  // Get the prototype from new_target (the constructor)
  proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  if (!JS_IsException(proto)) {
    // Set the prototype of the new object
    JS_SetPrototype(ctx, obj, proto);
    JS_FreeValue(ctx, proto);
  }

  // Set required properties
  JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, event_type));
  JS_SetPropertyStr(ctx, obj, "bubbles", JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "cancelable", JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "composed", JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "defaultPrevented", JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "eventPhase", JS_NewInt32(ctx, 0));  // NONE
  JS_SetPropertyStr(ctx, obj, "isTrusted", JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "timeStamp", JS_NewFloat64(ctx, (double)uv_hrtime() / 1000000.0));
  JS_SetPropertyStr(ctx, obj, "target", JS_NULL);
  JS_SetPropertyStr(ctx, obj, "currentTarget", JS_NULL);

  // Parse options (optional second argument)
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue options = argv[1];

    // Get 'bubbles' option
    JSValue bubbles_val = JS_GetPropertyStr(ctx, options, "bubbles");
    if (!JS_IsUndefined(bubbles_val)) {
      JS_SetPropertyStr(ctx, obj, "bubbles", JS_DupValue(ctx, bubbles_val));
    }
    JS_FreeValue(ctx, bubbles_val);

    // Get 'cancelable' option
    JSValue cancelable_val = JS_GetPropertyStr(ctx, options, "cancelable");
    if (!JS_IsUndefined(cancelable_val)) {
      JS_SetPropertyStr(ctx, obj, "cancelable", JS_DupValue(ctx, cancelable_val));
    }
    JS_FreeValue(ctx, cancelable_val);

    // Get 'composed' option
    JSValue composed_val = JS_GetPropertyStr(ctx, options, "composed");
    if (!JS_IsUndefined(composed_val)) {
      JS_SetPropertyStr(ctx, obj, "composed", JS_DupValue(ctx, composed_val));
    }
    JS_FreeValue(ctx, composed_val);
  }

  // Add preventDefault method
  JSValue prevent_default = JS_NewCFunction(ctx, js_event_prevent_default, "preventDefault", 0);
  JS_SetPropertyStr(ctx, obj, "preventDefault", prevent_default);

  // Add stopPropagation method (no-op in Node.js)
  JSValue stop_propagation = JS_NewCFunction(ctx, js_event_stop_propagation, "stopPropagation", 0);
  JS_SetPropertyStr(ctx, obj, "stopPropagation", stop_propagation);

  // Add stopImmediatePropagation method (no-op in Node.js)
  JSValue stop_immediate_propagation =
      JS_NewCFunction(ctx, js_event_stop_immediate_propagation, "stopImmediatePropagation", 0);
  JS_SetPropertyStr(ctx, obj, "stopImmediatePropagation", stop_immediate_propagation);

  JS_FreeCString(ctx, event_type);
  return obj;
}

// CustomEvent constructor - new CustomEvent(type[, options])
JSValue js_custom_event_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "CustomEvent constructor requires at least 1 argument");
  }

  // Create base Event object first
  JSValue event_obj = js_event_constructor(ctx, new_target, argc, argv);
  if (JS_IsException(event_obj)) {
    return event_obj;
  }

  // Add detail property (defaults to null)
  JSValue detail = JS_NULL;
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue options = argv[1];
    JSValue detail_val = JS_GetPropertyStr(ctx, options, "detail");
    if (!JS_IsUndefined(detail_val)) {
      detail = detail_val;
    } else {
      JS_FreeValue(ctx, detail_val);
    }
  }

  JS_SetPropertyStr(ctx, event_obj, "detail", detail);

  return event_obj;
}

// Event.prototype.preventDefault()
JSValue js_event_prevent_default(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!JS_IsObject(this_val)) {
    return JS_ThrowTypeError(ctx, "preventDefault can only be called on Event objects");
  }

  // Check if event is cancelable
  JSValue cancelable = JS_GetPropertyStr(ctx, this_val, "cancelable");
  bool is_cancelable = JS_ToBool(ctx, cancelable);
  JS_FreeValue(ctx, cancelable);

  if (is_cancelable) {
    JS_SetPropertyStr(ctx, this_val, "defaultPrevented", JS_NewBool(ctx, true));
  }

  return JS_UNDEFINED;
}

// Event.prototype.stopPropagation() - no-op in Node.js
JSValue js_event_stop_propagation(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // No-op in Node.js EventTarget implementation
  return JS_UNDEFINED;
}

// Event.prototype.stopImmediatePropagation() - no-op in Node.js
JSValue js_event_stop_immediate_propagation(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // No-op in Node.js EventTarget implementation
  return JS_UNDEFINED;
}