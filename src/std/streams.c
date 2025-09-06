#include "streams.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"

// Forward declare class IDs
JSClassID JSRT_ReadableStreamClassID;
JSClassID JSRT_ReadableStreamDefaultReaderClassID;
JSClassID JSRT_ReadableStreamDefaultControllerClassID;
JSClassID JSRT_WritableStreamClassID;
JSClassID JSRT_WritableStreamDefaultControllerClassID;
JSClassID JSRT_TransformStreamClassID;

// ReadableStreamDefaultController implementation
typedef struct {
  JSValue stream;
  char** queue;  // Simple queue of string chunks
  size_t queue_size;
  size_t queue_capacity;
  bool closed;
} JSRT_ReadableStreamDefaultController;

static void JSRT_ReadableStreamDefaultControllerFinalize(JSRuntime* rt, JSValue val) {
  JSRT_ReadableStreamDefaultController* controller = JS_GetOpaque(val, JSRT_ReadableStreamDefaultControllerClassID);
  if (controller) {
    if (!JS_IsUndefined(controller->stream)) {
      JS_FreeValueRT(rt, controller->stream);
    }
    // Free queue
    for (size_t i = 0; i < controller->queue_size; i++) {
      free(controller->queue[i]);
    }
    free(controller->queue);
    free(controller);
  }
}

static JSClassDef JSRT_ReadableStreamDefaultControllerClass = {
    .class_name = "ReadableStreamDefaultController",
    .finalizer = JSRT_ReadableStreamDefaultControllerFinalize,
};

static JSValue JSRT_ReadableStreamDefaultControllerEnqueue(JSContext* ctx, JSValueConst this_val, int argc,
                                                           JSValueConst* argv) {
  JSRT_ReadableStreamDefaultController* controller =
      JS_GetOpaque(this_val, JSRT_ReadableStreamDefaultControllerClassID);
  if (!controller) {
    return JS_EXCEPTION;
  }

  if (controller->closed) {
    return JS_ThrowTypeError(ctx, "Cannot enqueue a chunk into a readable stream that is closed or errored");
  }

  if (argc > 0) {
    // Convert chunk to string for simplicity
    const char* chunk_str = JS_ToCString(ctx, argv[0]);
    if (chunk_str) {
      // Expand queue if needed
      if (controller->queue_size >= controller->queue_capacity) {
        size_t new_capacity = controller->queue_capacity * 2 + 1;
        char** new_queue = realloc(controller->queue, new_capacity * sizeof(char*));
        if (new_queue) {
          controller->queue = new_queue;
          controller->queue_capacity = new_capacity;
        }
      }

      // Add chunk to queue
      if (controller->queue_size < controller->queue_capacity) {
        controller->queue[controller->queue_size] = strdup(chunk_str);
        controller->queue_size++;
      }

      JS_FreeCString(ctx, chunk_str);
    }
  }

  return JS_UNDEFINED;
}

static JSValue JSRT_ReadableStreamDefaultControllerClose(JSContext* ctx, JSValueConst this_val, int argc,
                                                         JSValueConst* argv) {
  JSRT_ReadableStreamDefaultController* controller =
      JS_GetOpaque(this_val, JSRT_ReadableStreamDefaultControllerClassID);
  if (!controller) {
    return JS_EXCEPTION;
  }

  controller->closed = true;
  return JS_UNDEFINED;
}

static JSValue JSRT_ReadableStreamDefaultControllerError(JSContext* ctx, JSValueConst this_val, int argc,
                                                         JSValueConst* argv) {
  JSRT_ReadableStreamDefaultController* controller =
      JS_GetOpaque(this_val, JSRT_ReadableStreamDefaultControllerClassID);
  if (!controller) {
    return JS_EXCEPTION;
  }

  controller->closed = true;  // Error also closes the stream
  return JS_UNDEFINED;
}

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
  stream->locked = false;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_ReadableStreamClassID);
  JS_SetOpaque(obj, stream);

  // Create the controller
  JSRT_ReadableStreamDefaultController* controller_data = malloc(sizeof(JSRT_ReadableStreamDefaultController));
  controller_data->stream = JS_DupValue(ctx, obj);
  controller_data->queue = NULL;
  controller_data->queue_size = 0;
  controller_data->queue_capacity = 0;
  controller_data->closed = false;

  JSValue controller = JS_NewObjectClass(ctx, JSRT_ReadableStreamDefaultControllerClassID);
  JS_SetOpaque(controller, controller_data);

  // Add methods to controller
  JS_SetPropertyStr(ctx, controller, "enqueue",
                    JS_NewCFunction(ctx, JSRT_ReadableStreamDefaultControllerEnqueue, "enqueue", 1));
  JS_SetPropertyStr(ctx, controller, "close",
                    JS_NewCFunction(ctx, JSRT_ReadableStreamDefaultControllerClose, "close", 0));
  JS_SetPropertyStr(ctx, controller, "error",
                    JS_NewCFunction(ctx, JSRT_ReadableStreamDefaultControllerError, "error", 1));

  stream->controller = controller;

  // If underlyingSource is provided, call its start method
  if (argc > 0 && !JS_IsUndefined(argv[0]) && JS_IsObject(argv[0])) {
    JSValue underlyingSource = argv[0];
    JSValue start = JS_GetPropertyStr(ctx, underlyingSource, "start");

    if (!JS_IsUndefined(start) && JS_IsFunction(ctx, start)) {
      JSValue start_args[] = {controller};
      JSValue result = JS_Call(ctx, start, underlyingSource, 1, start_args);
      if (JS_IsException(result)) {
        JS_FreeValue(ctx, start);
        JS_FreeValue(ctx, controller);
        free(controller_data);
        free(stream);
        return JS_EXCEPTION;
      }
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, start);
  }

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
  // Call the ReadableStreamDefaultReader constructor with this stream
  JSValue reader_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "ReadableStreamDefaultReader");
  JSValue reader = JS_CallConstructor(ctx, reader_ctor, 1, &this_val);
  JS_FreeValue(ctx, reader_ctor);
  return reader;
}

// ReadableStreamDefaultReader implementation
typedef struct {
  JSValue stream;
  bool closed;
} JSRT_ReadableStreamDefaultReader;

static void JSRT_ReadableStreamDefaultReaderFinalize(JSRuntime* rt, JSValue val) {
  JSRT_ReadableStreamDefaultReader* reader = JS_GetOpaque(val, JSRT_ReadableStreamDefaultReaderClassID);
  if (reader) {
    if (!JS_IsUndefined(reader->stream)) {
      JS_FreeValueRT(rt, reader->stream);
    }
    free(reader);
  }
}

static JSClassDef JSRT_ReadableStreamDefaultReaderClass = {
    .class_name = "ReadableStreamDefaultReader",
    .finalizer = JSRT_ReadableStreamDefaultReaderFinalize,
};

static JSValue JSRT_ReadableStreamDefaultReaderConstructor(JSContext* ctx, JSValueConst new_target, int argc,
                                                           JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "ReadableStreamDefaultReader constructor requires a ReadableStream argument");
  }

  JSValue stream_val = argv[0];
  JSRT_ReadableStream* stream = JS_GetOpaque(stream_val, JSRT_ReadableStreamClassID);
  if (!stream) {
    return JS_ThrowTypeError(ctx,
                             "ReadableStreamDefaultReader constructor should get a ReadableStream object as argument");
  }

  if (stream->locked) {
    return JS_ThrowTypeError(ctx, "ReadableStream is already locked to a reader");
  }

  stream->locked = true;

  JSRT_ReadableStreamDefaultReader* reader = malloc(sizeof(JSRT_ReadableStreamDefaultReader));
  reader->stream = JS_DupValue(ctx, stream_val);
  reader->closed = false;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_ReadableStreamDefaultReaderClassID);
  JS_SetOpaque(obj, reader);
  return obj;
}

static JSValue JSRT_ReadableStreamDefaultReaderGetClosed(JSContext* ctx, JSValueConst this_val, int argc,
                                                         JSValueConst* argv) {
  JSRT_ReadableStreamDefaultReader* reader = JS_GetOpaque(this_val, JSRT_ReadableStreamDefaultReaderClassID);
  if (!reader) {
    return JS_EXCEPTION;
  }
  // For now, return a resolved promise (simplified)
  return JS_UNDEFINED;
}

static JSValue JSRT_ReadableStreamDefaultReaderRead(JSContext* ctx, JSValueConst this_val, int argc,
                                                    JSValueConst* argv) {
  JSRT_ReadableStreamDefaultReader* reader = JS_GetOpaque(this_val, JSRT_ReadableStreamDefaultReaderClassID);
  if (!reader) {
    return JS_EXCEPTION;
  }

  // Get the stream and its controller
  JSRT_ReadableStream* stream = JS_GetOpaque(reader->stream, JSRT_ReadableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  JSRT_ReadableStreamDefaultController* controller =
      JS_GetOpaque(stream->controller, JSRT_ReadableStreamDefaultControllerClassID);
  if (!controller) {
    return JS_EXCEPTION;
  }

  JSValue result = JS_NewObject(ctx);

  // Check if there are chunks in the queue
  if (controller->queue_size > 0) {
    // Return the first chunk from the queue
    char* chunk = controller->queue[0];
    JS_SetPropertyStr(ctx, result, "value", JS_NewString(ctx, chunk));
    JS_SetPropertyStr(ctx, result, "done", JS_NewBool(ctx, false));

    // Remove the chunk from queue
    free(chunk);
    for (size_t i = 1; i < controller->queue_size; i++) {
      controller->queue[i - 1] = controller->queue[i];
    }
    controller->queue_size--;
  } else if (controller->closed) {
    // Stream is closed and no more chunks
    JS_SetPropertyStr(ctx, result, "value", JS_UNDEFINED);
    JS_SetPropertyStr(ctx, result, "done", JS_NewBool(ctx, true));
  } else {
    // Stream is still open but no chunks yet - return undefined for now
    // In a full implementation, this would return a pending promise
    JS_SetPropertyStr(ctx, result, "value", JS_UNDEFINED);
    JS_SetPropertyStr(ctx, result, "done", JS_NewBool(ctx, false));
  }

  // Return a resolved promise with the result
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
  JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue resolve_args[] = {result};
  JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, resolve_args);

  JS_FreeValue(ctx, promise_ctor);
  JS_FreeValue(ctx, resolve_method);
  JS_FreeValue(ctx, result);

  return promise;
}

// WritableStreamDefaultController implementation
typedef struct {
  JSValue stream;
  bool closed;
  bool errored;
} JSRT_WritableStreamDefaultController;

static void JSRT_WritableStreamDefaultControllerFinalize(JSRuntime* rt, JSValue val) {
  JSRT_WritableStreamDefaultController* controller = JS_GetOpaque(val, JSRT_WritableStreamDefaultControllerClassID);
  if (controller) {
    if (!JS_IsUndefined(controller->stream)) {
      JS_FreeValueRT(rt, controller->stream);
    }
    free(controller);
  }
}

static JSClassDef JSRT_WritableStreamDefaultControllerClass = {
    .class_name = "WritableStreamDefaultController",
    .finalizer = JSRT_WritableStreamDefaultControllerFinalize,
};

static JSValue JSRT_WritableStreamDefaultControllerError(JSContext* ctx, JSValueConst this_val, int argc,
                                                         JSValueConst* argv) {
  JSRT_WritableStreamDefaultController* controller =
      JS_GetOpaque(this_val, JSRT_WritableStreamDefaultControllerClassID);
  if (!controller) {
    return JS_EXCEPTION;
  }

  controller->errored = true;
  controller->closed = true;  // Error also closes the stream
  return JS_UNDEFINED;
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
  stream->locked = false;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_WritableStreamClassID);
  JS_SetOpaque(obj, stream);

  // Create the controller
  JSRT_WritableStreamDefaultController* controller_data = malloc(sizeof(JSRT_WritableStreamDefaultController));
  controller_data->stream = JS_DupValue(ctx, obj);
  controller_data->closed = false;
  controller_data->errored = false;

  JSValue controller = JS_NewObjectClass(ctx, JSRT_WritableStreamDefaultControllerClassID);
  JS_SetOpaque(controller, controller_data);

  // Add methods to controller
  JS_SetPropertyStr(ctx, controller, "error",
                    JS_NewCFunction(ctx, JSRT_WritableStreamDefaultControllerError, "error", 1));

  stream->controller = controller;

  // If underlyingSink is provided, call its start method
  if (argc > 0 && !JS_IsUndefined(argv[0]) && JS_IsObject(argv[0])) {
    JSValue underlyingSink = argv[0];
    JSValue start = JS_GetPropertyStr(ctx, underlyingSink, "start");

    if (!JS_IsUndefined(start) && JS_IsFunction(ctx, start)) {
      JSValue start_args[] = {controller};
      JSValue result = JS_Call(ctx, start, underlyingSink, 1, start_args);
      if (JS_IsException(result)) {
        JS_FreeValue(ctx, start);
        JS_FreeValue(ctx, controller);
        free(controller_data);
        free(stream);
        return JS_EXCEPTION;
      }
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, start);
  }

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

  // Register ReadableStreamDefaultController class
  JS_NewClassID(&JSRT_ReadableStreamDefaultControllerClassID);
  JS_NewClass(rt->rt, JSRT_ReadableStreamDefaultControllerClassID, &JSRT_ReadableStreamDefaultControllerClass);

  // Register ReadableStreamDefaultReader class
  JS_NewClassID(&JSRT_ReadableStreamDefaultReaderClassID);
  JS_NewClass(rt->rt, JSRT_ReadableStreamDefaultReaderClassID, &JSRT_ReadableStreamDefaultReaderClass);

  JSValue reader_proto = JS_NewObject(ctx);

  // Properties
  JSValue get_closed = JS_NewCFunction(ctx, JSRT_ReadableStreamDefaultReaderGetClosed, "get closed", 0);
  JSAtom closed_atom = JS_NewAtom(ctx, "closed");
  JS_DefinePropertyGetSet(ctx, reader_proto, closed_atom, get_closed, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, closed_atom);

  // Methods
  JS_SetPropertyStr(ctx, reader_proto, "read", JS_NewCFunction(ctx, JSRT_ReadableStreamDefaultReaderRead, "read", 0));

  JS_SetClassProto(ctx, JSRT_ReadableStreamDefaultReaderClassID, reader_proto);

  JSValue reader_ctor = JS_NewCFunction2(ctx, JSRT_ReadableStreamDefaultReaderConstructor,
                                         "ReadableStreamDefaultReader", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "ReadableStreamDefaultReader", reader_ctor);

  // Register WritableStream class
  JS_NewClassID(&JSRT_WritableStreamClassID);
  JS_NewClass(rt->rt, JSRT_WritableStreamClassID, &JSRT_WritableStreamClass);

  // Register WritableStreamDefaultController class
  JS_NewClassID(&JSRT_WritableStreamDefaultControllerClassID);
  JS_NewClass(rt->rt, JSRT_WritableStreamDefaultControllerClassID, &JSRT_WritableStreamDefaultControllerClass);

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