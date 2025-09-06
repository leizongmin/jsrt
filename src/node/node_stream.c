#include <stdlib.h>
#include <string.h>
#include "../util/debug.h"
#include "node_modules.h"

// Forward declare class IDs
static JSClassID js_readable_class_id;
static JSClassID js_writable_class_id;
static JSClassID js_transform_class_id;
static JSClassID js_passthrough_class_id;

// Stream base class
typedef struct {
  bool readable;
  bool writable;
  bool destroyed;
  bool ended;
  JSValue* buffered_data;
  size_t buffer_size;
  size_t buffer_capacity;
} JSStreamData;

static void js_stream_finalizer(JSRuntime* rt, JSValue obj) {
  JSStreamData* stream = JS_GetOpaque(obj, js_readable_class_id);
  if (!stream) {
    stream = JS_GetOpaque(obj, js_writable_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(obj, js_transform_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(obj, js_passthrough_class_id);
  }

  if (stream) {
    if (stream->buffered_data) {
      for (size_t i = 0; i < stream->buffer_size; i++) {
        JS_FreeValueRT(rt, stream->buffered_data[i]);
      }
      free(stream->buffered_data);
    }
    free(stream);
  }
}

static JSClassDef js_readable_class = {"Readable", .finalizer = js_stream_finalizer};
static JSClassDef js_writable_class = {"Writable", .finalizer = js_stream_finalizer};
static JSClassDef js_transform_class = {"Transform", .finalizer = js_stream_finalizer};
static JSClassDef js_passthrough_class = {"PassThrough", .finalizer = js_stream_finalizer};

// Readable stream implementation
static JSValue js_readable_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_readable_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSStreamData* stream = calloc(1, sizeof(JSStreamData));
  if (!stream) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  stream->readable = true;
  stream->writable = false;
  stream->destroyed = false;
  stream->ended = false;
  stream->buffer_capacity = 16;
  stream->buffered_data = malloc(sizeof(JSValue) * stream->buffer_capacity);

  JS_SetOpaque(obj, stream);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "readable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "destroyed", JS_NewBool(ctx, false), JS_PROP_WRITABLE);

  return obj;
}

// Readable.prototype.read
static JSValue js_readable_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
  }

  if (stream->buffer_size > 0) {
    JSValue data = stream->buffered_data[0];
    // Shift buffer
    for (size_t i = 1; i < stream->buffer_size; i++) {
      stream->buffered_data[i - 1] = stream->buffered_data[i];
    }
    stream->buffer_size--;
    return data;
  }

  return JS_NULL;
}

// Readable.prototype.push
static JSValue js_readable_push(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  JSValue chunk = argv[0];

  // If push(null), end the stream
  if (JS_IsNull(chunk)) {
    stream->ended = true;
    JS_SetPropertyStr(ctx, this_val, "readable", JS_NewBool(ctx, false));
    return JS_NewBool(ctx, false);
  }

  // Add to buffer
  if (stream->buffer_size >= stream->buffer_capacity) {
    stream->buffer_capacity *= 2;
    stream->buffered_data = realloc(stream->buffered_data, sizeof(JSValue) * stream->buffer_capacity);
  }

  stream->buffered_data[stream->buffer_size++] = JS_DupValue(ctx, chunk);

  return JS_NewBool(ctx, true);
}

// Writable stream implementation
static JSValue js_writable_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_writable_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSStreamData* stream = calloc(1, sizeof(JSStreamData));
  if (!stream) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  stream->readable = false;
  stream->writable = true;
  stream->destroyed = false;
  stream->ended = false;

  JS_SetOpaque(obj, stream);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "writable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "destroyed", JS_NewBool(ctx, false), JS_PROP_WRITABLE);

  return obj;
}

// Writable.prototype.write
static JSValue js_writable_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  if (!stream->writable) {
    return JS_ThrowTypeError(ctx, "Cannot write to stream");
  }

  return JS_NewBool(ctx, true);
}

// Writable.prototype.end
static JSValue js_writable_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  stream->writable = false;
  stream->ended = true;
  JS_SetPropertyStr(ctx, this_val, "writable", JS_NewBool(ctx, false));

  return JS_UNDEFINED;
}

// PassThrough stream implementation (extends Transform)
static JSValue js_passthrough_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_passthrough_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSStreamData* stream = calloc(1, sizeof(JSStreamData));
  if (!stream) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  stream->readable = true;
  stream->writable = true;
  stream->destroyed = false;
  stream->ended = false;
  stream->buffer_capacity = 16;
  stream->buffered_data = malloc(sizeof(JSValue) * stream->buffer_capacity);

  JS_SetOpaque(obj, stream);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "readable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "writable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "destroyed", JS_NewBool(ctx, false), JS_PROP_WRITABLE);

  return obj;
}

// PassThrough.prototype.write (pass data through)
static JSValue js_passthrough_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_passthrough_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a passthrough stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  if (!stream->writable) {
    return JS_ThrowTypeError(ctx, "Cannot write to stream");
  }

  // In PassThrough, written data becomes readable
  JSValue chunk = argv[0];

  // Add to buffer
  if (stream->buffer_size >= stream->buffer_capacity) {
    stream->buffer_capacity *= 2;
    stream->buffered_data = realloc(stream->buffered_data, sizeof(JSValue) * stream->buffer_capacity);
  }

  stream->buffered_data[stream->buffer_size++] = JS_DupValue(ctx, chunk);

  return JS_NewBool(ctx, true);
}

// PassThrough.prototype.read (use same as readable)
static JSValue js_passthrough_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_passthrough_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a passthrough stream");
  }

  if (stream->buffer_size > 0) {
    JSValue data = stream->buffered_data[0];
    // Shift buffer
    for (size_t i = 1; i < stream->buffer_size; i++) {
      stream->buffered_data[i - 1] = stream->buffered_data[i];
    }
    stream->buffer_size--;
    return data;
  }

  return JS_NULL;
}

// PassThrough.prototype.push (use same as readable)
static JSValue js_passthrough_push(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_passthrough_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a passthrough stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  JSValue chunk = argv[0];

  // If push(null), end the stream
  if (JS_IsNull(chunk)) {
    stream->ended = true;
    JS_SetPropertyStr(ctx, this_val, "readable", JS_NewBool(ctx, false));
    return JS_NewBool(ctx, false);
  }

  // Add to buffer
  if (stream->buffer_size >= stream->buffer_capacity) {
    stream->buffer_capacity *= 2;
    stream->buffered_data = realloc(stream->buffered_data, sizeof(JSValue) * stream->buffer_capacity);
  }

  stream->buffered_data[stream->buffer_size++] = JS_DupValue(ctx, chunk);

  return JS_NewBool(ctx, true);
}

// Module initialization
JSValue JSRT_InitNodeStream(JSContext* ctx) {
  JSValue stream_module = JS_NewObject(ctx);

  // Register class IDs
  JS_NewClassID(&js_readable_class_id);
  JS_NewClassID(&js_writable_class_id);
  JS_NewClassID(&js_transform_class_id);
  JS_NewClassID(&js_passthrough_class_id);

  // Create class definitions
  JS_NewClass(JS_GetRuntime(ctx), js_readable_class_id, &js_readable_class);
  JS_NewClass(JS_GetRuntime(ctx), js_writable_class_id, &js_writable_class);
  JS_NewClass(JS_GetRuntime(ctx), js_transform_class_id, &js_transform_class);
  JS_NewClass(JS_GetRuntime(ctx), js_passthrough_class_id, &js_passthrough_class);

  // Create constructors
  JSValue readable_ctor = JS_NewCFunction2(ctx, js_readable_constructor, "Readable", 1, JS_CFUNC_constructor, 0);
  JSValue writable_ctor = JS_NewCFunction2(ctx, js_writable_constructor, "Writable", 1, JS_CFUNC_constructor, 0);
  JSValue passthrough_ctor =
      JS_NewCFunction2(ctx, js_passthrough_constructor, "PassThrough", 0, JS_CFUNC_constructor, 0);

  // Create prototypes
  JSValue readable_proto = JS_NewObject(ctx);
  JSValue writable_proto = JS_NewObject(ctx);
  JSValue passthrough_proto = JS_NewObject(ctx);

  // Add methods to Readable prototype
  JS_SetPropertyStr(ctx, readable_proto, "read", JS_NewCFunction(ctx, js_readable_read, "read", 0));
  JS_SetPropertyStr(ctx, readable_proto, "push", JS_NewCFunction(ctx, js_readable_push, "push", 1));

  // Add methods to Writable prototype
  JS_SetPropertyStr(ctx, writable_proto, "write", JS_NewCFunction(ctx, js_writable_write, "write", 1));
  JS_SetPropertyStr(ctx, writable_proto, "end", JS_NewCFunction(ctx, js_writable_end, "end", 0));

  // Add methods to PassThrough prototype (inherits from both Readable and Writable)
  JS_SetPropertyStr(ctx, passthrough_proto, "read", JS_NewCFunction(ctx, js_passthrough_read, "read", 0));
  JS_SetPropertyStr(ctx, passthrough_proto, "push", JS_NewCFunction(ctx, js_passthrough_push, "push", 1));
  JS_SetPropertyStr(ctx, passthrough_proto, "write", JS_NewCFunction(ctx, js_passthrough_write, "write", 1));
  JS_SetPropertyStr(ctx, passthrough_proto, "end", JS_NewCFunction(ctx, js_writable_end, "end", 0));

  // Set prototypes
  JS_SetPropertyStr(ctx, readable_ctor, "prototype", readable_proto);
  JS_SetPropertyStr(ctx, writable_ctor, "prototype", writable_proto);
  JS_SetPropertyStr(ctx, passthrough_ctor, "prototype", passthrough_proto);

  // Set constructor property on prototypes
  JS_SetPropertyStr(ctx, readable_proto, "constructor", JS_DupValue(ctx, readable_ctor));
  JS_SetPropertyStr(ctx, writable_proto, "constructor", JS_DupValue(ctx, writable_ctor));
  JS_SetPropertyStr(ctx, passthrough_proto, "constructor", JS_DupValue(ctx, passthrough_ctor));

  // Set class prototypes
  JS_SetClassProto(ctx, js_readable_class_id, JS_DupValue(ctx, readable_proto));
  JS_SetClassProto(ctx, js_writable_class_id, JS_DupValue(ctx, writable_proto));
  JS_SetClassProto(ctx, js_passthrough_class_id, JS_DupValue(ctx, passthrough_proto));
  JS_SetClassProto(ctx, js_transform_class_id, JS_DupValue(ctx, passthrough_proto));

  // Add to module
  JS_SetPropertyStr(ctx, stream_module, "Readable", readable_ctor);
  JS_SetPropertyStr(ctx, stream_module, "Writable", writable_ctor);
  JS_SetPropertyStr(ctx, stream_module, "PassThrough", passthrough_ctor);

  // Transform is an alias for PassThrough in this basic implementation
  JS_SetPropertyStr(ctx, stream_module, "Transform", JS_DupValue(ctx, passthrough_ctor));

  return stream_module;
}

// ES Module support
int js_node_stream_init(JSContext* ctx, JSModuleDef* m) {
  JSValue stream_module = JSRT_InitNodeStream(ctx);

  // Export individual classes
  JS_SetModuleExport(ctx, m, "Readable", JS_GetPropertyStr(ctx, stream_module, "Readable"));
  JS_SetModuleExport(ctx, m, "Writable", JS_GetPropertyStr(ctx, stream_module, "Writable"));
  JS_SetModuleExport(ctx, m, "Transform", JS_GetPropertyStr(ctx, stream_module, "Transform"));
  JS_SetModuleExport(ctx, m, "PassThrough", JS_GetPropertyStr(ctx, stream_module, "PassThrough"));

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", stream_module);

  return 0;
}