#include "event.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"
#include "../util/list.h"

// Forward declare class IDs
JSClassID JSRT_EventClassID;
JSClassID JSRT_EventTargetClassID;

// Event implementation
typedef struct {
  char *type;
  JSValue target;
  JSValue currentTarget;
  bool bubbles;
  bool cancelable;
  bool defaultPrevented;
  bool stopPropagationFlag;
  bool stopImmediatePropagationFlag;
} JSRT_Event;

static void JSRT_EventFinalize(JSRuntime *rt, JSValue val) {
  JSRT_Event *event = JS_GetOpaque(val, JSRT_EventClassID);
  if (event) {
    free(event->type);
    JS_FreeValueRT(rt, event->target);
    JS_FreeValueRT(rt, event->currentTarget);
    free(event);
  }
}

static JSClassDef JSRT_EventClass = {
    .class_name = "Event",
    .finalizer = JSRT_EventFinalize,
};

static JSValue JSRT_EventConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Event constructor requires at least 1 argument");
  }

  const char *type = JS_ToCString(ctx, argv[0]);
  if (!type) {
    return JS_EXCEPTION;
  }

  JSRT_Event *event = malloc(sizeof(JSRT_Event));
  event->type = strdup(type);
  event->target = JS_UNDEFINED;
  event->currentTarget = JS_UNDEFINED;
  event->bubbles = false;
  event->cancelable = false;
  event->defaultPrevented = false;
  event->stopPropagationFlag = false;
  event->stopImmediatePropagationFlag = false;

  // Handle eventInit parameter
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
    if (!JS_IsUndefined(bubbles)) {
      event->bubbles = JS_ToBool(ctx, bubbles);
      JS_FreeValue(ctx, bubbles);
    }

    JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
    if (!JS_IsUndefined(cancelable)) {
      event->cancelable = JS_ToBool(ctx, cancelable);
      JS_FreeValue(ctx, cancelable);
    }
  }

  JSValue obj = JS_NewObjectClass(ctx, JSRT_EventClassID);
  JS_SetOpaque(obj, event);

  JS_FreeCString(ctx, type);
  return obj;
}

static JSValue JSRT_EventGetType(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Event *event = JS_GetOpaque2(ctx, this_val, JSRT_EventClassID);
  if (!event) {
    return JS_EXCEPTION;
  }
  return JS_NewString(ctx, event->type);
}

static JSValue JSRT_EventGetTarget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Event *event = JS_GetOpaque2(ctx, this_val, JSRT_EventClassID);
  if (!event) {
    return JS_EXCEPTION;
  }
  return JS_DupValue(ctx, event->target);
}

static JSValue JSRT_EventGetCurrentTarget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Event *event = JS_GetOpaque2(ctx, this_val, JSRT_EventClassID);
  if (!event) {
    return JS_EXCEPTION;
  }
  return JS_DupValue(ctx, event->currentTarget);
}

static JSValue JSRT_EventGetBubbles(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Event *event = JS_GetOpaque2(ctx, this_val, JSRT_EventClassID);
  if (!event) {
    return JS_EXCEPTION;
  }
  return JS_NewBool(ctx, event->bubbles);
}

static JSValue JSRT_EventGetCancelable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Event *event = JS_GetOpaque2(ctx, this_val, JSRT_EventClassID);
  if (!event) {
    return JS_EXCEPTION;
  }
  return JS_NewBool(ctx, event->cancelable);
}

static JSValue JSRT_EventGetDefaultPrevented(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Event *event = JS_GetOpaque2(ctx, this_val, JSRT_EventClassID);
  if (!event) {
    return JS_EXCEPTION;
  }
  return JS_NewBool(ctx, event->defaultPrevented);
}

static JSValue JSRT_EventPreventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Event *event = JS_GetOpaque2(ctx, this_val, JSRT_EventClassID);
  if (!event) {
    return JS_EXCEPTION;
  }
  if (event->cancelable) {
    event->defaultPrevented = true;
  }
  return JS_UNDEFINED;
}

static JSValue JSRT_EventStopPropagation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Event *event = JS_GetOpaque2(ctx, this_val, JSRT_EventClassID);
  if (!event) {
    return JS_EXCEPTION;
  }
  event->stopPropagationFlag = true;
  return JS_UNDEFINED;
}

static JSValue JSRT_EventStopImmediatePropagation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Event *event = JS_GetOpaque2(ctx, this_val, JSRT_EventClassID);
  if (!event) {
    return JS_EXCEPTION;
  }
  event->stopPropagationFlag = true;
  event->stopImmediatePropagationFlag = true;
  return JS_UNDEFINED;
}

// EventTarget implementation
typedef struct JSRT_EventListener {
  char *type;
  JSValue callback;
  bool capture;
  bool once;
  bool passive;
  struct JSRT_EventListener *next;
} JSRT_EventListener;

typedef struct {
  JSRT_EventListener *listeners;
} JSRT_EventTarget;

static void JSRT_EventTargetFinalize(JSRuntime *rt, JSValue val) {
  JSRT_EventTarget *target = JS_GetOpaque(val, JSRT_EventTargetClassID);
  if (target) {
    JSRT_EventListener *listener = target->listeners;
    while (listener) {
      JSRT_EventListener *next = listener->next;
      free(listener->type);
      JS_FreeValueRT(rt, listener->callback);
      free(listener);
      listener = next;
    }
    free(target);
  }
}

static JSClassDef JSRT_EventTargetClass = {
    .class_name = "EventTarget",
    .finalizer = JSRT_EventTargetFinalize,
};

static JSValue JSRT_EventTargetConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  JSRT_EventTarget *target = malloc(sizeof(JSRT_EventTarget));
  target->listeners = NULL;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_EventTargetClassID);
  JS_SetOpaque(obj, target);
  return obj;
}

static JSValue JSRT_EventTargetAddEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "addEventListener requires at least 2 arguments");
  }

  JSRT_EventTarget *target = JS_GetOpaque2(ctx, this_val, JSRT_EventTargetClassID);
  if (!target) {
    return JS_EXCEPTION;
  }

  const char *type = JS_ToCString(ctx, argv[0]);
  if (!type) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[1])) {
    JS_FreeCString(ctx, type);
    return JS_ThrowTypeError(ctx, "Listener must be a function");
  }

  // Check if this exact listener already exists
  JSRT_EventListener *existing = target->listeners;
  while (existing) {
    if (strcmp(existing->type, type) == 0 && JS_VALUE_GET_PTR(existing->callback) == JS_VALUE_GET_PTR(argv[1])) {
      JS_FreeCString(ctx, type);
      return JS_UNDEFINED; // Already exists, don't add duplicate
    }
    existing = existing->next;
  }

  JSRT_EventListener *listener = malloc(sizeof(JSRT_EventListener));
  listener->type = strdup(type);
  listener->callback = JS_DupValue(ctx, argv[1]);
  listener->capture = false;
  listener->once = false;
  listener->passive = false;

  // Handle options parameter
  if (argc >= 3) {
    if (JS_IsBool(argv[2])) {
      // Boolean useCapture (legacy)
      listener->capture = JS_ToBool(ctx, argv[2]);
    } else if (JS_IsObject(argv[2])) {
      JSValue capture = JS_GetPropertyStr(ctx, argv[2], "capture");
      if (!JS_IsUndefined(capture)) {
        listener->capture = JS_ToBool(ctx, capture);
        JS_FreeValue(ctx, capture);
      }

      JSValue once = JS_GetPropertyStr(ctx, argv[2], "once");
      if (!JS_IsUndefined(once)) {
        listener->once = JS_ToBool(ctx, once);
        JS_FreeValue(ctx, once);
      }

      JSValue passive = JS_GetPropertyStr(ctx, argv[2], "passive");
      if (!JS_IsUndefined(passive)) {
        listener->passive = JS_ToBool(ctx, passive);
        JS_FreeValue(ctx, passive);
      }
    }
  }

  // Add to beginning of list
  listener->next = target->listeners;
  target->listeners = listener;

  JS_FreeCString(ctx, type);
  return JS_UNDEFINED;
}

static JSValue JSRT_EventTargetRemoveEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "removeEventListener requires at least 2 arguments");
  }

  JSRT_EventTarget *target = JS_GetOpaque2(ctx, this_val, JSRT_EventTargetClassID);
  if (!target) {
    return JS_EXCEPTION;
  }

  const char *type = JS_ToCString(ctx, argv[0]);
  if (!type) {
    return JS_EXCEPTION;
  }

  JSRT_EventListener *prev = NULL;
  JSRT_EventListener *current = target->listeners;

  while (current) {
    if (strcmp(current->type, type) == 0 && JS_VALUE_GET_PTR(current->callback) == JS_VALUE_GET_PTR(argv[1])) {
      // Remove this listener
      if (prev) {
        prev->next = current->next;
      } else {
        target->listeners = current->next;
      }

      free(current->type);
      JS_FreeValue(ctx, current->callback);
      free(current);
      break;
    }
    prev = current;
    current = current->next;
  }

  JS_FreeCString(ctx, type);
  return JS_UNDEFINED;
}

static JSValue JSRT_EventTargetDispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "dispatchEvent requires 1 argument");
  }

  JSRT_EventTarget *target = JS_GetOpaque2(ctx, this_val, JSRT_EventTargetClassID);
  if (!target) {
    return JS_EXCEPTION;
  }

  JSRT_Event *event = JS_GetOpaque2(ctx, argv[0], JSRT_EventClassID);
  if (!event) {
    return JS_ThrowTypeError(ctx, "Argument must be an Event");
  }

  // Set event target if not already set
  if (JS_IsUndefined(event->target)) {
    event->target = JS_DupValue(ctx, this_val);
  }
  event->currentTarget = JS_DupValue(ctx, this_val);

  // Find matching listeners
  JSRT_EventListener *listener = target->listeners;
  JSRT_EventListener *toRemove = NULL;

  while (listener) {
    if (strcmp(listener->type, event->type) == 0) {
      // Call the listener
      JSValue result = JS_Call(ctx, listener->callback, this_val, 1, argv);
      if (JS_IsException(result)) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
      }
      JS_FreeValue(ctx, result);

      // Remove once listeners
      if (listener->once) {
        toRemove = listener;
      }

      // Check if stopImmediatePropagation was called
      if (event->stopImmediatePropagationFlag) {
        break;
      }
    }
    listener = listener->next;

    // Remove once listeners after iteration
    if (toRemove) {
      JSRT_EventListener *prev = NULL;
      JSRT_EventListener *current = target->listeners;
      
      while (current) {
        if (current == toRemove) {
          if (prev) {
            prev->next = current->next;
          } else {
            target->listeners = current->next;
          }
          free(current->type);
          JS_FreeValue(ctx, current->callback);
          free(current);
          break;
        }
        prev = current;
        current = current->next;
      }
      toRemove = NULL;
    }
  }

  return JS_NewBool(ctx, !event->defaultPrevented);
}

// Setup function
void JSRT_RuntimeSetupStdEvent(JSRT_Runtime *rt) {
  JSContext *ctx = rt->ctx;
  
  JSRT_Debug("JSRT_RuntimeSetupStdEvent: initializing Event/EventTarget API");

  // Register Event class
  JS_NewClassID(&JSRT_EventClassID);
  JS_NewClass(rt->rt, JSRT_EventClassID, &JSRT_EventClass);

  JSValue event_proto = JS_NewObject(ctx);
  
  // Use property descriptors for getters
  JSValue get_type = JS_NewCFunction(ctx, JSRT_EventGetType, "get type", 0);
  JSValue get_target = JS_NewCFunction(ctx, JSRT_EventGetTarget, "get target", 0);
  JSValue get_currentTarget = JS_NewCFunction(ctx, JSRT_EventGetCurrentTarget, "get currentTarget", 0);
  JSValue get_bubbles = JS_NewCFunction(ctx, JSRT_EventGetBubbles, "get bubbles", 0);
  JSValue get_cancelable = JS_NewCFunction(ctx, JSRT_EventGetCancelable, "get cancelable", 0);
  JSValue get_defaultPrevented = JS_NewCFunction(ctx, JSRT_EventGetDefaultPrevented, "get defaultPrevented", 0);
  
  JS_DefinePropertyGetSet(ctx, event_proto, JS_NewAtom(ctx, "type"), get_type, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, event_proto, JS_NewAtom(ctx, "target"), get_target, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, event_proto, JS_NewAtom(ctx, "currentTarget"), get_currentTarget, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, event_proto, JS_NewAtom(ctx, "bubbles"), get_bubbles, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, event_proto, JS_NewAtom(ctx, "cancelable"), get_cancelable, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, event_proto, JS_NewAtom(ctx, "defaultPrevented"), get_defaultPrevented, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  
  JS_SetPropertyStr(ctx, event_proto, "preventDefault", JS_NewCFunction(ctx, JSRT_EventPreventDefault, "preventDefault", 0));
  JS_SetPropertyStr(ctx, event_proto, "stopPropagation", JS_NewCFunction(ctx, JSRT_EventStopPropagation, "stopPropagation", 0));
  JS_SetPropertyStr(ctx, event_proto, "stopImmediatePropagation", JS_NewCFunction(ctx, JSRT_EventStopImmediatePropagation, "stopImmediatePropagation", 0));
  
  JS_SetClassProto(ctx, JSRT_EventClassID, event_proto);

  JSValue event_ctor = JS_NewCFunction2(ctx, JSRT_EventConstructor, "Event", 2, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "Event", event_ctor);

  // Register EventTarget class
  JS_NewClassID(&JSRT_EventTargetClassID);
  JS_NewClass(rt->rt, JSRT_EventTargetClassID, &JSRT_EventTargetClass);

  JSValue event_target_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, event_target_proto, "addEventListener", JS_NewCFunction(ctx, JSRT_EventTargetAddEventListener, "addEventListener", 3));
  JS_SetPropertyStr(ctx, event_target_proto, "removeEventListener", JS_NewCFunction(ctx, JSRT_EventTargetRemoveEventListener, "removeEventListener", 3));
  JS_SetPropertyStr(ctx, event_target_proto, "dispatchEvent", JS_NewCFunction(ctx, JSRT_EventTargetDispatchEvent, "dispatchEvent", 1));
  
  JS_SetClassProto(ctx, JSRT_EventTargetClassID, event_target_proto);

  JSValue event_target_ctor = JS_NewCFunction2(ctx, JSRT_EventTargetConstructor, "EventTarget", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "EventTarget", event_target_ctor);

  JSRT_Debug("Event/EventTarget API setup completed");
}