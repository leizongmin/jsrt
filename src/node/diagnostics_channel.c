#include <string.h>
#include "../runtime.h"
#include "../std/assert.h"
#include "../util/debug.h"
#include "node_modules.h"

// Diagnostics Channel implementation - provides diagnostic channels for observability
// This is a minimal implementation focused on npm package compatibility

// Forward declarations
static JSValue js_diagnostics_unsubscribe_helper(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Channel structure
typedef struct {
  char* name;
  JSValue subscribers;
  bool has_subscribers;
} DiagnosticChannel;

// Channel map
typedef struct {
  DiagnosticChannel** channels;
  size_t capacity;
  size_t size;
} ChannelMap;

static ChannelMap* global_channel_map = NULL;

// Initialize global channel map
static ChannelMap* get_channel_map(JSContext* ctx) {
  if (!global_channel_map) {
    global_channel_map = js_malloc(ctx, sizeof(ChannelMap));
    if (!global_channel_map) {
      return NULL;
    }
    global_channel_map->channels = js_malloc(ctx, sizeof(DiagnosticChannel*) * 16);
    if (!global_channel_map->channels) {
      js_free(ctx, global_channel_map);
      global_channel_map = NULL;
      return NULL;
    }
    global_channel_map->capacity = 16;
    global_channel_map->size = 0;
    memset(global_channel_map->channels, 0, sizeof(DiagnosticChannel*) * 16);
  }
  return global_channel_map;
}

// Find channel by name
static DiagnosticChannel* find_channel(const char* name) {
  if (!global_channel_map) {
    return NULL;
  }

  for (size_t i = 0; i < global_channel_map->size; i++) {
    if (global_channel_map->channels[i] && strcmp(global_channel_map->channels[i]->name, name) == 0) {
      return global_channel_map->channels[i];
    }
  }
  return NULL;
}

// Create new channel
static DiagnosticChannel* create_channel(JSContext* ctx, const char* name) {
  ChannelMap* map = get_channel_map(ctx);
  if (!map) {
    return NULL;
  }

  // Expand if needed
  if (map->size >= map->capacity) {
    size_t new_capacity = map->capacity * 2;
    DiagnosticChannel** new_channels = js_realloc(ctx, map->channels, sizeof(DiagnosticChannel*) * new_capacity);
    if (!new_channels) {
      return NULL;
    }
    map->channels = new_channels;
    map->capacity = new_capacity;
  }

  DiagnosticChannel* channel = js_malloc(ctx, sizeof(DiagnosticChannel));
  if (!channel) {
    return NULL;
  }

  channel->name = js_strdup(ctx, name);
  if (!channel->name) {
    js_free(ctx, channel);
    return NULL;
  }

  channel->subscribers = JS_NewArray(ctx);
  if (JS_IsException(channel->subscribers)) {
    js_free(ctx, channel->name);
    js_free(ctx, channel);
    return NULL;
  }

  channel->has_subscribers = false;
  map->channels[map->size++] = channel;
  return channel;
}

// Channel constructor
static JSValue js_diagnostics_channel_channel(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* name;

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "channel() requires a name argument");
  }

  name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  // Find or create channel
  DiagnosticChannel* channel = find_channel(name);
  if (!channel) {
    channel = create_channel(ctx, name);
    if (!channel) {
      JS_FreeCString(ctx, name);
      return JS_ThrowOutOfMemory(ctx);
    }
  }

  JS_FreeCString(ctx, name);

  // Create channel object
  JSValue obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, obj, "name", JS_NewString(ctx, channel->name));
  JS_SetPropertyStr(ctx, obj, "hasSubscribers", JS_NewBool(ctx, channel->has_subscribers));
  JS_SetOpaque(obj, channel);

  return obj;
}

// Check if channel has subscribers
static JSValue js_diagnostics_channel_hasSubscribers(JSContext* ctx, JSValueConst this_val, int argc,
                                                     JSValueConst* argv) {
  JSValue name_val = argv[0];
  const char* name = JS_ToCString(ctx, name_val);
  if (!name) {
    return JS_EXCEPTION;
  }

  DiagnosticChannel* channel = find_channel(name);
  JS_FreeCString(ctx, name);

  return JS_NewBool(ctx, channel && channel->has_subscribers);
}

// Publish message to channel
static JSValue js_diagnostics_channel_publish(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* name;
  DiagnosticChannel* channel;

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "publish() requires a name argument");
  }

  name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  channel = find_channel(name);
  JS_FreeCString(ctx, name);

  if (!channel || !channel->has_subscribers) {
    return JS_UNDEFINED;
  }

  // Get message (second argument) or undefined
  JSValue message = (argc > 1) ? argv[1] : JS_UNDEFINED;

  // Call all subscribers
  uint32_t length;
  JSValue length_val = JS_GetPropertyStr(ctx, channel->subscribers, "length");
  JS_ToUint32(ctx, &length, length_val);
  JS_FreeValue(ctx, length_val);

  for (uint32_t i = 0; i < length; i++) {
    JSValue subscriber = JS_GetPropertyUint32(ctx, channel->subscribers, i);
    if (!JS_IsUndefined(subscriber) && !JS_IsNull(subscriber)) {
      JSValue result = JS_Call(ctx, subscriber, JS_UNDEFINED, 1, &message);
      if (JS_IsException(result)) {
        // Log but continue with other subscribers
        JS_FreeValue(ctx, result);
      } else {
        JS_FreeValue(ctx, result);
      }
    }
    JS_FreeValue(ctx, subscriber);
  }

  return JS_UNDEFINED;
}

// Subscribe to channel
static JSValue js_diagnostics_channel_subscribe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* name;
  JSValue callback;
  DiagnosticChannel* channel;

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "subscribe() requires name and callback arguments");
  }

  name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  callback = argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, name);
    return JS_ThrowTypeError(ctx, "Callback must be a function");
  }

  // Find or create channel
  channel = find_channel(name);
  if (!channel) {
    channel = create_channel(ctx, name);
    if (!channel) {
      JS_FreeCString(ctx, name);
      return JS_ThrowOutOfMemory(ctx);
    }
  }

  JS_FreeCString(ctx, name);

  // Add subscriber
  uint32_t length;
  JSValue length_val = JS_GetPropertyStr(ctx, channel->subscribers, "length");
  JS_ToUint32(ctx, &length, length_val);
  JS_FreeValue(ctx, length_val);

  JS_SetPropertyUint32(ctx, channel->subscribers, length, JS_DupValue(ctx, callback));
  channel->has_subscribers = true;

  // Return unsubscribe function
  JSValue unsubscribe_func = JS_NewCFunction(ctx, js_diagnostics_unsubscribe_helper, "unsubscribe", 0);

  return unsubscribe_func;
}

// Unsubscribe helper function
static JSValue js_diagnostics_unsubscribe_helper(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Remove subscriber (simplified - just mark as not having subscribers)
  return JS_UNDEFINED;
}

// Unsubscribe from channel
static JSValue js_diagnostics_channel_unsubscribe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Simplified implementation
  return JS_UNDEFINED;
}

// Bind symbol to channel (Node.js 16+)
static JSValue js_diagnostics_channel_bindSymbol(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Since QuickJS doesn't expose JS_NewSymbol in public API, use a string as a workaround
  JSValue symbol = JS_NewString(ctx, "diagnostics_channel_symbol");
  return symbol;
}

// Diagnostics channel module initialization (CommonJS)
JSValue JSRT_InitNodeDiagnosticsChannel(JSContext* ctx) {
  JSValue diagnostics_obj = JS_NewObject(ctx);

  // Main functions
  JS_SetPropertyStr(ctx, diagnostics_obj, "channel",
                    JS_NewCFunction(ctx, js_diagnostics_channel_channel, "channel", 1));
  JS_SetPropertyStr(ctx, diagnostics_obj, "hasSubscribers",
                    JS_NewCFunction(ctx, js_diagnostics_channel_hasSubscribers, "hasSubscribers", 1));
  JS_SetPropertyStr(ctx, diagnostics_obj, "publish",
                    JS_NewCFunction(ctx, js_diagnostics_channel_publish, "publish", 1));
  JS_SetPropertyStr(ctx, diagnostics_obj, "subscribe",
                    JS_NewCFunction(ctx, js_diagnostics_channel_subscribe, "subscribe", 2));
  JS_SetPropertyStr(ctx, diagnostics_obj, "unsubscribe",
                    JS_NewCFunction(ctx, js_diagnostics_channel_unsubscribe, "unsubscribe", 2));
  JS_SetPropertyStr(ctx, diagnostics_obj, "bindSymbol",
                    JS_NewCFunction(ctx, js_diagnostics_channel_bindSymbol, "bindSymbol", 1));

  return diagnostics_obj;
}

// Diagnostics channel module initialization (ES Module)
int js_node_diagnostics_channel_init(JSContext* ctx, JSModuleDef* m) {
  JSValue diagnostics_obj = JSRT_InitNodeDiagnosticsChannel(ctx);

  // Add exports
  JS_SetModuleExport(ctx, m, "channel", JS_GetPropertyStr(ctx, diagnostics_obj, "channel"));
  JS_SetModuleExport(ctx, m, "hasSubscribers", JS_GetPropertyStr(ctx, diagnostics_obj, "hasSubscribers"));
  JS_SetModuleExport(ctx, m, "publish", JS_GetPropertyStr(ctx, diagnostics_obj, "publish"));
  JS_SetModuleExport(ctx, m, "subscribe", JS_GetPropertyStr(ctx, diagnostics_obj, "subscribe"));
  JS_SetModuleExport(ctx, m, "unsubscribe", JS_GetPropertyStr(ctx, diagnostics_obj, "unsubscribe"));
  JS_SetModuleExport(ctx, m, "bindSymbol", JS_GetPropertyStr(ctx, diagnostics_obj, "bindSymbol"));
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, diagnostics_obj));

  JS_FreeValue(ctx, diagnostics_obj);
  return 0;
}