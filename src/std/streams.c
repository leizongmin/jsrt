#include "streams.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"

// Forward declare class IDs
JSClassID JSRT_ReadableStreamClassID;
JSClassID JSRT_WritableStreamClassID;
JSClassID JSRT_TransformStreamClassID;

// ReadableStream implementation
typedef struct {
  JSValue controller;
  bool locked;
} JSRT_ReadableStream;

static void JSRT_ReadableStreamFinalize(JSRuntime* rt, JSValue val) {
  JSRT_ReadableStream* stream = JS_GetOpaque(val, JSRT_ReadableStreamClassID);
  if (stream) {
    if (!JS_IsUndefined(stream->controller)) {
      JS_FreeValueRT(rt, stream->controller);
    }
    free(stream);
  }
}

static JSClassDef JSRT_ReadableStreamClass = {
    .class_name = "ReadableStream",
    .finalizer = JSRT_ReadableStreamFinalize,
};

static JSValue JSRT_ReadableStreamConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSRT_ReadableStream* stream = malloc(sizeof(JSRT_ReadableStream));
  stream->controller = JS_UNDEFINED;
  stream->locked = false;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_ReadableStreamClassID);
  JS_SetOpaque(obj, stream);
  return obj;
}

static JSValue JSRT_ReadableStreamGetLocked(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_ReadableStream* stream = JS_GetOpaque(this_val, JSRT_ReadableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }
  return JS_NewBool(ctx, stream->locked);
}

static JSValue JSRT_ReadableStreamGetReader(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_ReadableStream* stream = JS_GetOpaque(this_val, JSRT_ReadableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  if (stream->locked) {
    return JS_ThrowTypeError(ctx, "ReadableStream is already locked to a reader");
  }

  stream->locked = true;

  // Create a basic reader object
  JSValue reader = JS_NewObject(ctx);

  // For now, just set closed to undefined to avoid segfault
  JS_SetPropertyStr(ctx, reader, "closed", JS_UNDEFINED);

  return reader;
}

// WritableStream implementation
typedef struct {
  JSValue controller;
  bool locked;
} JSRT_WritableStream;

static void JSRT_WritableStreamFinalize(JSRuntime* rt, JSValue val) {
  JSRT_WritableStream* stream = JS_GetOpaque(val, JSRT_WritableStreamClassID);
  if (stream) {
    if (!JS_IsUndefined(stream->controller)) {
      JS_FreeValueRT(rt, stream->controller);
    }
    free(stream);
  }
}

static JSClassDef JSRT_WritableStreamClass = {
    .class_name = "WritableStream",
    .finalizer = JSRT_WritableStreamFinalize,
};

static JSValue JSRT_WritableStreamConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSRT_WritableStream* stream = malloc(sizeof(JSRT_WritableStream));
  stream->controller = JS_UNDEFINED;
  stream->locked = false;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_WritableStreamClassID);
  JS_SetOpaque(obj, stream);
  return obj;
}

static JSValue JSRT_WritableStreamGetLocked(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_WritableStream* stream = JS_GetOpaque(this_val, JSRT_WritableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }
  return JS_NewBool(ctx, stream->locked);
}

// TransformStream implementation
typedef struct {
  JSValue readable;
  JSValue writable;
} JSRT_TransformStream;

static void JSRT_TransformStreamFinalize(JSRuntime* rt, JSValue val) {
  JSRT_TransformStream* stream = JS_GetOpaque(val, JSRT_TransformStreamClassID);
  if (stream) {
    if (!JS_IsUndefined(stream->readable)) {
      JS_FreeValueRT(rt, stream->readable);
    }
    if (!JS_IsUndefined(stream->writable)) {
      JS_FreeValueRT(rt, stream->writable);
    }
    free(stream);
  }
}

static JSClassDef JSRT_TransformStreamClass = {
    .class_name = "TransformStream",
    .finalizer = JSRT_TransformStreamFinalize,
};

static JSValue JSRT_TransformStreamConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSRT_TransformStream* stream = malloc(sizeof(JSRT_TransformStream));

  // Create readable and writable streams
  JSValue readable_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "ReadableStream");
  JSValue writable_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "WritableStream");

  stream->readable = JS_CallConstructor(ctx, readable_ctor, 0, NULL);
  stream->writable = JS_CallConstructor(ctx, writable_ctor, 0, NULL);

  JS_FreeValue(ctx, readable_ctor);
  JS_FreeValue(ctx, writable_ctor);

  JSValue obj = JS_NewObjectClass(ctx, JSRT_TransformStreamClassID);
  JS_SetOpaque(obj, stream);
  return obj;
}

static JSValue JSRT_TransformStreamGetReadable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_TransformStream* stream = JS_GetOpaque(this_val, JSRT_TransformStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }
  return JS_DupValue(ctx, stream->readable);
}

static JSValue JSRT_TransformStreamGetWritable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_TransformStream* stream = JS_GetOpaque(this_val, JSRT_TransformStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }
  return JS_DupValue(ctx, stream->writable);
}

void JSRT_RuntimeSetupStdStreams(JSRT_Runtime* rt) {
  JSRT_Debug("JSRT_RuntimeSetupStdStreams: initializing Streams API");

  JSContext* ctx = rt->ctx;

  // Register ReadableStream class
  JS_NewClassID(&JSRT_ReadableStreamClassID);
  JS_NewClass(rt->rt, JSRT_ReadableStreamClassID, &JSRT_ReadableStreamClass);

  JSValue readable_proto = JS_NewObject(ctx);

  // Properties
  JSValue get_locked = JS_NewCFunction(ctx, JSRT_ReadableStreamGetLocked, "get locked", 0);
  JSAtom locked_atom = JS_NewAtom(ctx, "locked");
  JS_DefinePropertyGetSet(ctx, readable_proto, locked_atom, get_locked, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, locked_atom);

  // Methods
  JS_SetPropertyStr(ctx, readable_proto, "getReader",
                    JS_NewCFunction(ctx, JSRT_ReadableStreamGetReader, "getReader", 0));

  JS_SetClassProto(ctx, JSRT_ReadableStreamClassID, readable_proto);

  JSValue readable_ctor =
      JS_NewCFunction2(ctx, JSRT_ReadableStreamConstructor, "ReadableStream", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "ReadableStream", readable_ctor);

  // Register WritableStream class
  JS_NewClassID(&JSRT_WritableStreamClassID);
  JS_NewClass(rt->rt, JSRT_WritableStreamClassID, &JSRT_WritableStreamClass);

  JSValue writable_proto = JS_NewObject(ctx);

  // Properties
  JSValue get_locked_w = JS_NewCFunction(ctx, JSRT_WritableStreamGetLocked, "get locked", 0);
  JSAtom locked_atom_w = JS_NewAtom(ctx, "locked");
  JS_DefinePropertyGetSet(ctx, writable_proto, locked_atom_w, get_locked_w, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, locked_atom_w);

  JS_SetClassProto(ctx, JSRT_WritableStreamClassID, writable_proto);

  JSValue writable_ctor =
      JS_NewCFunction2(ctx, JSRT_WritableStreamConstructor, "WritableStream", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "WritableStream", writable_ctor);

  // Register TransformStream class
  JS_NewClassID(&JSRT_TransformStreamClassID);
  JS_NewClass(rt->rt, JSRT_TransformStreamClassID, &JSRT_TransformStreamClass);

  JSValue transform_proto = JS_NewObject(ctx);

  // Properties
  JSValue get_readable = JS_NewCFunction(ctx, JSRT_TransformStreamGetReadable, "get readable", 0);
  JSValue get_writable = JS_NewCFunction(ctx, JSRT_TransformStreamGetWritable, "get writable", 0);
  JSAtom readable_atom = JS_NewAtom(ctx, "readable");
  JSAtom writable_atom = JS_NewAtom(ctx, "writable");
  JS_DefinePropertyGetSet(ctx, transform_proto, readable_atom, get_readable, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, transform_proto, writable_atom, get_writable, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, readable_atom);
  JS_FreeAtom(ctx, writable_atom);

  JS_SetClassProto(ctx, JSRT_TransformStreamClassID, transform_proto);

  JSValue transform_ctor =
      JS_NewCFunction2(ctx, JSRT_TransformStreamConstructor, "TransformStream", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "TransformStream", transform_ctor);
}