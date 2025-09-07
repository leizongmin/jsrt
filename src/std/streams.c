#include "streams.h"

#include <math.h>
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
JSClassID JSRT_WritableStreamDefaultWriterClassID;
JSClassID JSRT_TransformStreamClassID;

// ReadableStreamDefaultController implementation
typedef struct {
  JSValue stream;
  char** queue;  // Simple queue of string chunks
  size_t queue_size;
  size_t queue_capacity;
  bool closed;
  bool errored;         // Whether the controller is in error state
  JSValue error_value;  // The error value (if errored)
} JSRT_ReadableStreamDefaultController;

static void JSRT_ReadableStreamDefaultControllerFinalize(JSRuntime* rt, JSValue val) {
  JSRT_ReadableStreamDefaultController* controller = JS_GetOpaque(val, JSRT_ReadableStreamDefaultControllerClassID);
  if (controller) {
    if (!JS_IsUndefined(controller->stream)) {
      JS_FreeValueRT(rt, controller->stream);
    }
    if (!JS_IsUndefined(controller->error_value)) {
      JS_FreeValueRT(rt, controller->error_value);
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

  controller->closed = true;   // Error also closes the stream
  controller->errored = true;  // Mark as errored

  // Store the error value (undefined if no argument provided)
  JSValue error_value = (argc > 0) ? argv[0] : JS_UNDEFINED;
  controller->error_value = JS_DupValue(ctx, error_value);

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
  controller_data->errored = false;
  controller_data->error_value = JS_UNDEFINED;

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
  // Validate the optional options parameter
  if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    // Check if it's an object
    if (!JS_IsObject(argv[0])) {
      return JS_ThrowTypeError(ctx, "getReader() options must be an object");
    }

    // Check for mode property
    JSValue mode = JS_GetPropertyStr(ctx, argv[0], "mode");
    if (!JS_IsException(mode) && !JS_IsUndefined(mode)) {
      const char* mode_str = JS_ToCString(ctx, mode);
      if (mode_str) {
        // Only allow "byob" as a valid mode (besides undefined)
        if (strcmp(mode_str, "byob") != 0) {
          JS_FreeCString(ctx, mode_str);
          JS_FreeValue(ctx, mode);
          return JS_ThrowRangeError(ctx, "getReader() mode must be \"byob\" or undefined");
        }
        JS_FreeCString(ctx, mode_str);
      }
    }
    JS_FreeValue(ctx, mode);
  }

  // Call the ReadableStreamDefaultReader constructor with this stream
  JSValue reader_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "ReadableStreamDefaultReader");
  JSValue reader = JS_CallConstructor(ctx, reader_ctor, 1, &this_val);
  JS_FreeValue(ctx, reader_ctor);
  return reader;
}

static JSValue JSRT_ReadableStreamCancel(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_ReadableStream* stream = JS_GetOpaque(this_val, JSRT_ReadableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  JSValue reason = argc > 0 ? argv[0] : JS_UNDEFINED;

  // Get the controller and close it
  if (!JS_IsUndefined(stream->controller)) {
    JSRT_ReadableStreamDefaultController* controller =
        JS_GetOpaque(stream->controller, JSRT_ReadableStreamDefaultControllerClassID);
    if (controller) {
      controller->closed = true;
    }
  }

  // Return a resolved promise with the reason
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
  JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, &reason);
  JS_FreeValue(ctx, resolve_method);
  JS_FreeValue(ctx, promise_ctor);
  return promise;
}

// ReadableStreamDefaultReader implementation
typedef struct {
  JSValue stream;
  bool closed;
  JSValue closed_promise;  // Cached closed promise
} JSRT_ReadableStreamDefaultReader;

static void JSRT_ReadableStreamDefaultReaderFinalize(JSRuntime* rt, JSValue val) {
  JSRT_ReadableStreamDefaultReader* reader = JS_GetOpaque(val, JSRT_ReadableStreamDefaultReaderClassID);
  if (reader) {
    if (!JS_IsUndefined(reader->stream)) {
      JS_FreeValueRT(rt, reader->stream);
    }
    if (!JS_IsUndefined(reader->closed_promise)) {
      JS_FreeValueRT(rt, reader->closed_promise);
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
  reader->closed_promise = JS_UNINITIALIZED;  // Initialize as uninitialized

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

  // Return the same cached promise instance if available
  if (!JS_IsUninitialized(reader->closed_promise)) {
    return JS_DupValue(ctx, reader->closed_promise);
  }

  // Get the stream and its controller to check error state
  JSRT_ReadableStream* stream = JS_GetOpaque(reader->stream, JSRT_ReadableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  JSRT_ReadableStreamDefaultController* controller =
      JS_GetOpaque(stream->controller, JSRT_ReadableStreamDefaultControllerClassID);
  if (!controller) {
    return JS_EXCEPTION;
  }

  // Create and cache the promise
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");

  if (controller->errored) {
    // Stream is errored, return a rejected promise with the error value
    JSValue reject_method = JS_GetPropertyStr(ctx, promise_ctor, "reject");
    JSValue promise = JS_Call(ctx, reject_method, promise_ctor, 1, &controller->error_value);
    JS_FreeValue(ctx, reject_method);
    reader->closed_promise = JS_DupValue(ctx, promise);
    JS_FreeValue(ctx, promise_ctor);
    return promise;
  } else if (reader->closed) {
    // Reader is closed normally, return a resolved promise
    JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
    JSValue undefined_val = JS_UNDEFINED;
    JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, &undefined_val);
    JS_FreeValue(ctx, resolve_method);
    reader->closed_promise = JS_DupValue(ctx, promise);
    JS_FreeValue(ctx, promise_ctor);
    return promise;
  } else {
    // Reader is not closed and not errored, return a pending promise (resolved for now)
    // In a complete implementation, this would be a truly pending promise
    JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
    JSValue undefined_val = JS_UNDEFINED;
    JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, &undefined_val);
    JS_FreeValue(ctx, resolve_method);
    reader->closed_promise = JS_DupValue(ctx, promise);
    JS_FreeValue(ctx, promise_ctor);
    return promise;
  }
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

static JSValue JSRT_ReadableStreamDefaultReaderCancel(JSContext* ctx, JSValueConst this_val, int argc,
                                                      JSValueConst* argv) {
  JSRT_ReadableStreamDefaultReader* reader = JS_GetOpaque(this_val, JSRT_ReadableStreamDefaultReaderClassID);
  if (!reader) {
    return JS_EXCEPTION;
  }

  // Get the stream and mark it as cancelled
  JSRT_ReadableStream* stream = JS_GetOpaque(reader->stream, JSRT_ReadableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  // Cancel the stream - unlock it and close it
  stream->locked = false;
  reader->closed = true;

  // Get the controller and close it
  if (!JS_IsUndefined(stream->controller)) {
    JSRT_ReadableStreamDefaultController* controller =
        JS_GetOpaque(stream->controller, JSRT_ReadableStreamDefaultControllerClassID);
    if (controller) {
      controller->closed = true;
    }
  }

  // Return a resolved promise (simplified implementation)
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
  JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue undefined_val = JS_UNDEFINED;
  JSValue resolve_args[] = {undefined_val};
  JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, resolve_args);

  JS_FreeValue(ctx, promise_ctor);
  JS_FreeValue(ctx, resolve_method);
  return promise;
}

static JSValue JSRT_ReadableStreamDefaultReaderReleaseLock(JSContext* ctx, JSValueConst this_val, int argc,
                                                           JSValueConst* argv) {
  JSRT_ReadableStreamDefaultReader* reader = JS_GetOpaque(this_val, JSRT_ReadableStreamDefaultReaderClassID);
  if (!reader) {
    return JS_EXCEPTION;
  }

  // Get the associated stream
  JSRT_ReadableStream* stream = JS_GetOpaque(reader->stream, JSRT_ReadableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  // Release the lock on the stream
  stream->locked = false;

  // Mark the reader as closed
  reader->closed = true;

  // Clear the cached closed promise so a new one will be created
  // This is required by WPT test: "closed is replaced when stream closes and reader releases its lock"
  if (!JS_IsUninitialized(reader->closed_promise)) {
    JS_FreeValue(ctx, reader->closed_promise);
    reader->closed_promise = JS_UNINITIALIZED;
  }

  return JS_UNDEFINED;
}

// WritableStreamDefaultController implementation
typedef struct {
  JSValue stream;
  bool closed;
  bool errored;
  JSValue error_value;  // Store the error value passed to controller.error()
} JSRT_WritableStreamDefaultController;

static void JSRT_WritableStreamDefaultControllerFinalize(JSRuntime* rt, JSValue val) {
  JSRT_WritableStreamDefaultController* controller = JS_GetOpaque(val, JSRT_WritableStreamDefaultControllerClassID);
  if (controller) {
    if (!JS_IsUndefined(controller->stream)) {
      JS_FreeValueRT(rt, controller->stream);
    }
    if (!JS_IsUndefined(controller->error_value)) {
      JS_FreeValueRT(rt, controller->error_value);
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

  // Store the error value if provided
  if (argc > 0) {
    if (!JS_IsUndefined(controller->error_value)) {
      JS_FreeValue(ctx, controller->error_value);
    }
    controller->error_value = JS_DupValue(ctx, argv[0]);
  }

  controller->errored = true;
  controller->closed = true;  // Error also closes the stream
  return JS_UNDEFINED;
}

static JSValue JSRT_WritableStreamDefaultControllerClose(JSContext* ctx, JSValueConst this_val, int argc,
                                                         JSValueConst* argv) {
  JSRT_WritableStreamDefaultController* controller =
      JS_GetOpaque(this_val, JSRT_WritableStreamDefaultControllerClassID);
  if (!controller) {
    return JS_EXCEPTION;
  }

  // Close the controller
  controller->closed = true;
  return JS_UNDEFINED;
}

// WritableStream implementation
typedef struct {
  JSValue controller;
  bool locked;
  JSValue underlying_sink;           // Store the underlyingSink for write method calls
  int high_water_mark;               // Store the highWaterMark from queuingStrategy
  bool high_water_mark_is_infinity;  // True if highWaterMark is Infinity
} JSRT_WritableStream;

static void JSRT_WritableStreamFinalize(JSRuntime* rt, JSValue val) {
  JSRT_WritableStream* stream = JS_GetOpaque(val, JSRT_WritableStreamClassID);
  if (stream) {
    if (!JS_IsUndefined(stream->controller)) {
      JS_FreeValueRT(rt, stream->controller);
    }
    if (!JS_IsUndefined(stream->underlying_sink)) {
      JS_FreeValueRT(rt, stream->underlying_sink);
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
  stream->underlying_sink = JS_UNDEFINED;
  stream->high_water_mark = 1;  // Default highWaterMark
  stream->high_water_mark_is_infinity = false;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_WritableStreamClassID);
  JS_SetOpaque(obj, stream);

  // Create the controller
  JSRT_WritableStreamDefaultController* controller_data = malloc(sizeof(JSRT_WritableStreamDefaultController));
  controller_data->stream = JS_DupValue(ctx, obj);
  controller_data->closed = false;
  controller_data->errored = false;
  controller_data->error_value = JS_UNDEFINED;

  JSValue controller = JS_NewObjectClass(ctx, JSRT_WritableStreamDefaultControllerClassID);
  JS_SetOpaque(controller, controller_data);

  // Add methods to controller
  JS_SetPropertyStr(ctx, controller, "error",
                    JS_NewCFunction(ctx, JSRT_WritableStreamDefaultControllerError, "error", 1));
  JS_SetPropertyStr(ctx, controller, "close",
                    JS_NewCFunction(ctx, JSRT_WritableStreamDefaultControllerClose, "close", 0));

  stream->controller = controller;

  // Handle queuingStrategy parameter (second argument)
  // Per spec, queuingStrategy is processed at IDL layer (before underlyingSink)
  if (argc > 1 && !JS_IsUndefined(argv[1]) && JS_IsObject(argv[1])) {
    JSValue queuingStrategy = argv[1];

    // Access size property first - this may throw (as expected by WPT tests)
    JSValue size_prop = JS_GetPropertyStr(ctx, queuingStrategy, "size");
    if (JS_IsException(size_prop)) {
      // Clean up and propagate the exception
      JS_FreeValue(ctx, controller);  // This will free controller_data via finalizer
      free(stream);
      return size_prop;
    }
    JS_FreeValue(ctx, size_prop);

    // Then access highWaterMark property
    JSValue highWaterMark = JS_GetPropertyStr(ctx, queuingStrategy, "highWaterMark");
    if (JS_IsException(highWaterMark)) {
      // Clean up and propagate the exception
      JS_FreeValue(ctx, controller);  // This will free controller_data via finalizer
      free(stream);
      return highWaterMark;
    }

    if (!JS_IsUndefined(highWaterMark) && JS_IsNumber(highWaterMark)) {
      double hwm_double;
      JS_ToFloat64(ctx, &hwm_double, highWaterMark);
      if (isinf(hwm_double)) {
        stream->high_water_mark_is_infinity = true;
        stream->high_water_mark = 0;  // Not used when infinity
      } else {
        int32_t hwm;
        JS_ToInt32(ctx, &hwm, highWaterMark);
        stream->high_water_mark = hwm;
        stream->high_water_mark_is_infinity = false;
      }
    }
    JS_FreeValue(ctx, highWaterMark);
  }

  // If underlyingSink is provided, store it and call its start method
  if (argc > 0 && !JS_IsUndefined(argv[0]) && JS_IsObject(argv[0])) {
    JSValue underlyingSink = argv[0];

    // Check for invalid type property - per spec, WritableStream cannot have 'bytes' type
    JSValue type_prop = JS_GetPropertyStr(ctx, underlyingSink, "type");
    if (!JS_IsUndefined(type_prop)) {
      const char* type_str = JS_ToCString(ctx, type_prop);
      if (type_str && strcmp(type_str, "bytes") == 0) {
        JS_FreeCString(ctx, type_str);
        JS_FreeValue(ctx, type_prop);
        // Don't free controller_data - JS_FreeValue(ctx, controller) handles it
        JS_FreeValue(ctx, controller);
        free(stream);
        return JS_ThrowRangeError(ctx, "WritableStream does not support 'bytes' type");
      }
      if (type_str) {
        JS_FreeCString(ctx, type_str);
      }
    }
    JS_FreeValue(ctx, type_prop);

    stream->underlying_sink = JS_DupValue(ctx, underlyingSink);  // Store the underlyingSink
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

static JSValue JSRT_WritableStreamGetWriter(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Call the WritableStreamDefaultWriter constructor with this stream
  JSValue writer_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "WritableStreamDefaultWriter");
  JSValue writer = JS_CallConstructor(ctx, writer_ctor, 1, &this_val);
  JS_FreeValue(ctx, writer_ctor);
  return writer;
}

static JSValue JSRT_WritableStreamAbort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_WritableStream* stream = JS_GetOpaque(this_val, JSRT_WritableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  // Get the controller and error it
  if (!JS_IsUndefined(stream->controller)) {
    JSRT_WritableStreamDefaultController* controller =
        JS_GetOpaque(stream->controller, JSRT_WritableStreamDefaultControllerClassID);
    if (controller) {
      controller->errored = true;
      controller->closed = true;
    }
  }

  // Return a resolved promise
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
  JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue undefined_val = JS_UNDEFINED;
  JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, &undefined_val);
  JS_FreeValue(ctx, resolve_method);
  JS_FreeValue(ctx, promise_ctor);
  return promise;
}

static JSValue JSRT_WritableStreamClose(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_WritableStream* stream = JS_GetOpaque(this_val, JSRT_WritableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  // Get the controller and close it
  if (!JS_IsUndefined(stream->controller)) {
    JSRT_WritableStreamDefaultController* controller =
        JS_GetOpaque(stream->controller, JSRT_WritableStreamDefaultControllerClassID);
    if (controller) {
      controller->closed = true;
    }
  }

  // Return a resolved promise
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
  JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue undefined_val = JS_UNDEFINED;
  JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, &undefined_val);
  JS_FreeValue(ctx, resolve_method);
  JS_FreeValue(ctx, promise_ctor);
  return promise;
}

// WritableStreamDefaultWriter implementation
typedef struct {
  JSValue stream;
  bool closed;
  bool errored;
  int desired_size;
  int high_water_mark;               // Inherited from the stream's highWaterMark
  bool high_water_mark_is_infinity;  // True if highWaterMark is Infinity
} JSRT_WritableStreamDefaultWriter;

static void JSRT_WritableStreamDefaultWriterFinalize(JSRuntime* rt, JSValue val) {
  JSRT_WritableStreamDefaultWriter* writer = JS_GetOpaque(val, JSRT_WritableStreamDefaultWriterClassID);
  if (writer) {
    if (!JS_IsUndefined(writer->stream)) {
      JS_FreeValueRT(rt, writer->stream);
    }
    free(writer);
  }
}

static JSClassDef JSRT_WritableStreamDefaultWriterClass = {
    .class_name = "WritableStreamDefaultWriter",
    .finalizer = JSRT_WritableStreamDefaultWriterFinalize,
};

static JSValue JSRT_WritableStreamDefaultWriterConstructor(JSContext* ctx, JSValueConst new_target, int argc,
                                                           JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "WritableStreamDefaultWriter constructor requires a WritableStream argument");
  }

  JSValue stream_val = argv[0];

  // Check if it's a WritableStream by trying to get the opaque data
  JSRT_WritableStream* stream = JS_GetOpaque(stream_val, JSRT_WritableStreamClassID);
  if (!stream) {
    // Check if the argument is actually a WritableStream by checking prototype or constructor
    JSValue proto = JS_GetPrototype(ctx, stream_val);
    JSValue writablestream_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "WritableStream");
    JSValue writablestream_proto = JS_GetPropertyStr(ctx, writablestream_ctor, "prototype");

    int is_equal = JS_StrictEq(ctx, proto, writablestream_proto);
    bool is_writablestream = (is_equal == 1);

    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, writablestream_ctor);
    JS_FreeValue(ctx, writablestream_proto);

    if (!is_writablestream) {
      return JS_ThrowTypeError(
          ctx, "WritableStreamDefaultWriter constructor should get a WritableStream object as argument");
    }

    // If we reach here, it might be a WritableStream but without opaque data somehow
    // This shouldn't happen in normal cases
    return JS_ThrowTypeError(ctx, "Invalid WritableStream object");
  }

  if (stream->locked) {
    return JS_ThrowTypeError(ctx, "WritableStream is already locked to a writer");
  }

  stream->locked = true;

  JSRT_WritableStreamDefaultWriter* writer = malloc(sizeof(JSRT_WritableStreamDefaultWriter));
  writer->stream = JS_DupValue(ctx, stream_val);
  writer->closed = false;
  writer->errored = false;
  writer->desired_size = 1;  // Default desired size

  // Inherit highWaterMark from the stream
  writer->high_water_mark = stream->high_water_mark;
  writer->high_water_mark_is_infinity = stream->high_water_mark_is_infinity;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_WritableStreamDefaultWriterClassID);
  JS_SetOpaque(obj, writer);
  return obj;
}

static JSValue JSRT_WritableStreamDefaultWriterGetDesiredSize(JSContext* ctx, JSValueConst this_val, int argc,
                                                              JSValueConst* argv) {
  JSRT_WritableStreamDefaultWriter* writer = JS_GetOpaque(this_val, JSRT_WritableStreamDefaultWriterClassID);
  if (!writer) {
    return JS_EXCEPTION;
  }

  // Check if the stream is errored
  JSRT_WritableStream* stream = JS_GetOpaque(writer->stream, JSRT_WritableStreamClassID);
  if (stream && !JS_IsUndefined(stream->controller)) {
    JSRT_WritableStreamDefaultController* controller =
        JS_GetOpaque(stream->controller, JSRT_WritableStreamDefaultControllerClassID);
    if (controller && controller->errored) {
      return JS_NULL;  // Return null for errored streams
    }
  }

  if (writer->high_water_mark_is_infinity) {
    return JS_NewFloat64(ctx, INFINITY);
  }
  return JS_NewInt32(ctx, writer->high_water_mark);
}

static JSValue JSRT_WritableStreamDefaultWriterGetClosed(JSContext* ctx, JSValueConst this_val, int argc,
                                                         JSValueConst* argv) {
  JSRT_WritableStreamDefaultWriter* writer = JS_GetOpaque(this_val, JSRT_WritableStreamDefaultWriterClassID);
  if (!writer) {
    return JS_EXCEPTION;
  }

  // Check if the stream is errored and return rejected promise
  JSRT_WritableStream* stream = JS_GetOpaque(writer->stream, JSRT_WritableStreamClassID);
  if (stream && !JS_IsUndefined(stream->controller)) {
    JSRT_WritableStreamDefaultController* controller =
        JS_GetOpaque(stream->controller, JSRT_WritableStreamDefaultControllerClassID);
    if (controller && controller->errored) {
      // Return a rejected promise with the stored error
      JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
      JSValue reject_method = JS_GetPropertyStr(ctx, promise_ctor, "reject");
      JSValue error_val = JS_IsUndefined(controller->error_value) ? JS_NewString(ctx, "Stream errored")
                                                                  : JS_DupValue(ctx, controller->error_value);
      JSValue reject_args[] = {error_val};
      JSValue promise = JS_Call(ctx, reject_method, promise_ctor, 1, reject_args);

      JS_FreeValue(ctx, promise_ctor);
      JS_FreeValue(ctx, reject_method);
      JS_FreeValue(ctx, error_val);
      return promise;
    }
  }

  // Return a resolved promise for now (simplified implementation)
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
  JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue undefined_val = JS_UNDEFINED;
  JSValue resolve_args[] = {undefined_val};
  JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, resolve_args);

  JS_FreeValue(ctx, promise_ctor);
  JS_FreeValue(ctx, resolve_method);
  return promise;
}

static JSValue JSRT_WritableStreamDefaultWriterClose(JSContext* ctx, JSValueConst this_val, int argc,
                                                     JSValueConst* argv) {
  JSRT_WritableStreamDefaultWriter* writer = JS_GetOpaque(this_val, JSRT_WritableStreamDefaultWriterClassID);
  if (!writer) {
    return JS_EXCEPTION;
  }

  // Get the stream
  JSRT_WritableStream* stream = JS_GetOpaque(writer->stream, JSRT_WritableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  // Mark writer as closed
  writer->closed = true;

  // Check if we have an underlyingSink with a close method
  if (!JS_IsUndefined(stream->underlying_sink)) {
    JSValue close_method = JS_GetPropertyStr(ctx, stream->underlying_sink, "close");
    if (!JS_IsUndefined(close_method) && JS_IsFunction(ctx, close_method)) {
      // Call the close method WITHOUT controller argument per spec
      JSValue result = JS_Call(ctx, close_method, stream->underlying_sink, 0, NULL);
      JS_FreeValue(ctx, close_method);
      if (JS_IsException(result)) {
        return result;
      }
      JS_FreeValue(ctx, result);
    } else {
      JS_FreeValue(ctx, close_method);
    }
  }

  // Get the controller and close it
  if (!JS_IsUndefined(stream->controller)) {
    JSRT_WritableStreamDefaultController* controller =
        JS_GetOpaque(stream->controller, JSRT_WritableStreamDefaultControllerClassID);
    if (controller) {
      controller->closed = true;
    }
  }

  // Return resolved promise
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
  JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue undefined_val = JS_UNDEFINED;
  JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, &undefined_val);
  JS_FreeValue(ctx, resolve_method);
  JS_FreeValue(ctx, promise_ctor);
  return promise;
}

static JSValue JSRT_WritableStreamDefaultWriterAbort(JSContext* ctx, JSValueConst this_val, int argc,
                                                     JSValueConst* argv) {
  JSRT_WritableStreamDefaultWriter* writer = JS_GetOpaque(this_val, JSRT_WritableStreamDefaultWriterClassID);
  if (!writer) {
    return JS_EXCEPTION;
  }

  // Get the stream
  JSRT_WritableStream* stream = JS_GetOpaque(writer->stream, JSRT_WritableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  // Mark writer as closed/errored
  writer->closed = true;
  writer->errored = true;

  // Get the abort reason
  JSValue reason = argc > 0 ? argv[0] : JS_UNDEFINED;

  // Check if we have an underlyingSink with an abort method
  if (!JS_IsUndefined(stream->underlying_sink)) {
    JSValue abort_method = JS_GetPropertyStr(ctx, stream->underlying_sink, "abort");
    if (!JS_IsUndefined(abort_method) && JS_IsFunction(ctx, abort_method)) {
      // Call the abort method with reason
      JSValue result = JS_Call(ctx, abort_method, stream->underlying_sink, 1, &reason);
      JS_FreeValue(ctx, abort_method);
      if (JS_IsException(result)) {
        return result;
      }
      JS_FreeValue(ctx, result);
    } else {
      JS_FreeValue(ctx, abort_method);
    }
  }

  // Return resolved promise with reason
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
  JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, &reason);
  JS_FreeValue(ctx, resolve_method);
  JS_FreeValue(ctx, promise_ctor);
  return promise;
}

static JSValue JSRT_WritableStreamDefaultWriterGetReady(JSContext* ctx, JSValueConst this_val, int argc,
                                                        JSValueConst* argv) {
  JSRT_WritableStreamDefaultWriter* writer = JS_GetOpaque(this_val, JSRT_WritableStreamDefaultWriterClassID);
  if (!writer) {
    return JS_EXCEPTION;
  }

  // Return a resolved promise (simplified implementation)
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
  JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue undefined_val = JS_UNDEFINED;
  JSValue resolve_args[] = {undefined_val};
  JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, resolve_args);

  JS_FreeValue(ctx, promise_ctor);
  JS_FreeValue(ctx, resolve_method);
  return promise;
}

static JSValue JSRT_WritableStreamDefaultWriterWrite(JSContext* ctx, JSValueConst this_val, int argc,
                                                     JSValueConst* argv) {
  JSRT_WritableStreamDefaultWriter* writer = JS_GetOpaque(this_val, JSRT_WritableStreamDefaultWriterClassID);
  if (!writer) {
    return JS_EXCEPTION;
  }

  // Get the stream
  JSRT_WritableStream* stream = JS_GetOpaque(writer->stream, JSRT_WritableStreamClassID);
  if (!stream) {
    return JS_EXCEPTION;
  }

  // Check if we have an underlyingSink with a write method
  if (!JS_IsUndefined(stream->underlying_sink)) {
    JSValue write_method = JS_GetPropertyStr(ctx, stream->underlying_sink, "write");
    if (!JS_IsUndefined(write_method) && JS_IsFunction(ctx, write_method)) {
      // Call the write method with chunk and controller
      JSValue chunk = argc > 0 ? argv[0] : JS_UNDEFINED;
      JSValue write_args[] = {chunk, stream->controller};
      JSValue result = JS_Call(ctx, write_method, stream->underlying_sink, 2, write_args);

      if (JS_IsException(result)) {
        JS_FreeValue(ctx, write_method);
        return result;
      }
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, write_method);
  }

  // Return a resolved promise (simplified implementation)
  JSValue promise_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise");
  JSValue resolve_method = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue undefined_val = JS_UNDEFINED;
  JSValue resolve_args[] = {undefined_val};
  JSValue promise = JS_Call(ctx, resolve_method, promise_ctor, 1, resolve_args);

  JS_FreeValue(ctx, promise_ctor);
  JS_FreeValue(ctx, resolve_method);
  return promise;
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
  JS_SetPropertyStr(ctx, readable_proto, "cancel", JS_NewCFunction(ctx, JSRT_ReadableStreamCancel, "cancel", 1));

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
  JS_SetPropertyStr(ctx, reader_proto, "cancel",
                    JS_NewCFunction(ctx, JSRT_ReadableStreamDefaultReaderCancel, "cancel", 1));
  JS_SetPropertyStr(ctx, reader_proto, "releaseLock",
                    JS_NewCFunction(ctx, JSRT_ReadableStreamDefaultReaderReleaseLock, "releaseLock", 0));

  JSValue reader_ctor = JS_NewCFunction2(ctx, JSRT_ReadableStreamDefaultReaderConstructor,
                                         "ReadableStreamDefaultReader", 1, JS_CFUNC_constructor, 0);

  // Set prototype property on constructor function
  JS_SetPropertyStr(ctx, reader_ctor, "prototype", JS_DupValue(ctx, reader_proto));

  // Set constructor property on prototype
  JS_SetPropertyStr(ctx, reader_proto, "constructor", JS_DupValue(ctx, reader_ctor));

  JS_SetClassProto(ctx, JSRT_ReadableStreamDefaultReaderClassID, reader_proto);
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

  // Methods
  JS_SetPropertyStr(ctx, writable_proto, "getWriter",
                    JS_NewCFunction(ctx, JSRT_WritableStreamGetWriter, "getWriter", 0));
  JS_SetPropertyStr(ctx, writable_proto, "abort", JS_NewCFunction(ctx, JSRT_WritableStreamAbort, "abort", 1));
  JS_SetPropertyStr(ctx, writable_proto, "close", JS_NewCFunction(ctx, JSRT_WritableStreamClose, "close", 0));

  JS_SetClassProto(ctx, JSRT_WritableStreamClassID, writable_proto);

  JSValue writable_ctor =
      JS_NewCFunction2(ctx, JSRT_WritableStreamConstructor, "WritableStream", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "WritableStream", writable_ctor);

  // Register WritableStreamDefaultWriter class
  JS_NewClassID(&JSRT_WritableStreamDefaultWriterClassID);
  JS_NewClass(rt->rt, JSRT_WritableStreamDefaultWriterClassID, &JSRT_WritableStreamDefaultWriterClass);

  JSValue writer_proto = JS_NewObject(ctx);

  // Properties
  JSValue get_desired_size = JS_NewCFunction(ctx, JSRT_WritableStreamDefaultWriterGetDesiredSize, "get desiredSize", 0);
  JSValue get_closed_writer = JS_NewCFunction(ctx, JSRT_WritableStreamDefaultWriterGetClosed, "get closed", 0);
  JSValue get_ready = JS_NewCFunction(ctx, JSRT_WritableStreamDefaultWriterGetReady, "get ready", 0);

  JSAtom desired_size_atom = JS_NewAtom(ctx, "desiredSize");
  JSAtom closed_writer_atom = JS_NewAtom(ctx, "closed");
  JSAtom ready_atom = JS_NewAtom(ctx, "ready");

  JS_DefinePropertyGetSet(ctx, writer_proto, desired_size_atom, get_desired_size, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, writer_proto, closed_writer_atom, get_closed_writer, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, writer_proto, ready_atom, get_ready, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  JS_FreeAtom(ctx, desired_size_atom);
  JS_FreeAtom(ctx, closed_writer_atom);
  JS_FreeAtom(ctx, ready_atom);

  // Methods
  JS_SetPropertyStr(ctx, writer_proto, "write",
                    JS_NewCFunction(ctx, JSRT_WritableStreamDefaultWriterWrite, "write", 1));
  JS_SetPropertyStr(ctx, writer_proto, "close",
                    JS_NewCFunction(ctx, JSRT_WritableStreamDefaultWriterClose, "close", 0));
  JS_SetPropertyStr(ctx, writer_proto, "abort",
                    JS_NewCFunction(ctx, JSRT_WritableStreamDefaultWriterAbort, "abort", 1));

  JSValue writer_ctor = JS_NewCFunction2(ctx, JSRT_WritableStreamDefaultWriterConstructor,
                                         "WritableStreamDefaultWriter", 1, JS_CFUNC_constructor, 0);

  // Set prototype property on constructor function
  JS_SetPropertyStr(ctx, writer_ctor, "prototype", JS_DupValue(ctx, writer_proto));

  // Set constructor property on prototype
  JS_SetPropertyStr(ctx, writer_proto, "constructor", JS_DupValue(ctx, writer_ctor));

  JS_SetClassProto(ctx, JSRT_WritableStreamDefaultWriterClassID, writer_proto);
  JS_SetPropertyStr(ctx, rt->global, "WritableStreamDefaultWriter", writer_ctor);

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