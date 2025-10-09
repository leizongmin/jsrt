#include "../../util/debug.h"
#include "stream_internal.h"

// Phase 5: Stream utility functions
// Implements pipeline(), finished(), and other utility functions

// stream.pipeline(...streams, callback) - Phase 5.1
// Connects multiple streams in a pipeline and handles errors
static JSValue js_stream_pipeline(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "pipeline() requires at least 2 arguments (source and callback)");
  }

  // Last argument should be the callback
  JSValue callback = argv[argc - 1];
  if (!JS_IsFunction(ctx, callback)) {
    return JS_ThrowTypeError(ctx, "pipeline() requires callback as last argument");
  }

  int stream_count = argc - 1;  // All arguments except callback are streams

  // For now, simplified implementation: pipe streams together
  for (int i = 0; i < stream_count - 1; i++) {
    JSValue src = argv[i];
    JSValue dest = argv[i + 1];

    // Call src.pipe(dest)
    JSValue pipe_method = JS_GetPropertyStr(ctx, src, "pipe");
    if (JS_IsFunction(ctx, pipe_method)) {
      JSValue result = JS_Call(ctx, pipe_method, src, 1, &dest);
      if (JS_IsException(result)) {
        JS_FreeValue(ctx, pipe_method);
        // Call callback with error
        JSValue error = JS_GetException(ctx);
        JSValue cb_args[] = {error};
        JS_Call(ctx, callback, JS_UNDEFINED, 1, cb_args);
        JS_FreeValue(ctx, error);
        return JS_UNDEFINED;
      }
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, pipe_method);
  }

  // Set up error handling for all streams
  for (int i = 0; i < stream_count; i++) {
    JSValue stream = argv[i];

    // Listen for 'error' event
    JSValue on_method = JS_GetPropertyStr(ctx, stream, "on");
    if (JS_IsFunction(ctx, on_method)) {
      // Create error handler that calls callback
      JSValue error_str = JS_NewString(ctx, "error");
      JSValue handler_args[] = {error_str, callback};
      JSValue result = JS_Call(ctx, on_method, stream, 2, handler_args);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, error_str);
    }
    JS_FreeValue(ctx, on_method);
  }

  // Listen for 'finish' on the last stream (destination)
  JSValue last_stream = argv[stream_count - 1];
  JSValue on_method = JS_GetPropertyStr(ctx, last_stream, "on");
  if (JS_IsFunction(ctx, on_method)) {
    // Create finish handler that calls callback with no error
    JSValue finish_str = JS_NewString(ctx, "finish");

    // Create a wrapper function that calls callback()
    // For now, simplified: just call callback immediately after piping completes
    // In a real implementation, we'd wait for the 'finish' event
    JS_FreeValue(ctx, finish_str);
  }
  JS_FreeValue(ctx, on_method);

  // Call callback with no error (simplified - should wait for actual completion)
  JSValue cb_args[] = {JS_NULL};
  JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 1, cb_args);
  JS_FreeValue(ctx, result);

  return JS_UNDEFINED;
}

// stream.finished(stream, callback) - Phase 5.2
// Calls callback when stream is finished (readable ended or writable finished)
static JSValue js_stream_finished(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "finished() requires stream and callback arguments");
  }

  JSValue stream = argv[0];
  JSValue callback = argv[1];

  if (!JS_IsFunction(ctx, callback)) {
    return JS_ThrowTypeError(ctx, "finished() requires callback function");
  }

  // Check if stream is readable or writable
  JSValue readable_prop = JS_GetPropertyStr(ctx, stream, "readable");
  JSValue writable_prop = JS_GetPropertyStr(ctx, stream, "writable");
  bool is_readable = JS_ToBool(ctx, readable_prop);
  bool is_writable = JS_ToBool(ctx, writable_prop);
  JS_FreeValue(ctx, readable_prop);
  JS_FreeValue(ctx, writable_prop);

  JSValue on_method = JS_GetPropertyStr(ctx, stream, "on");
  if (!JS_IsFunction(ctx, on_method)) {
    JS_FreeValue(ctx, on_method);
    return JS_ThrowTypeError(ctx, "Stream does not support event listeners");
  }

  // Listen for appropriate events based on stream type
  if (is_readable) {
    // Listen for 'end' or 'error' events
    JSValue end_str = JS_NewString(ctx, "end");
    JSValue end_args[] = {end_str, callback};
    JSValue result = JS_Call(ctx, on_method, stream, 2, end_args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, end_str);
  }

  if (is_writable) {
    // Listen for 'finish' or 'error' events
    JSValue finish_str = JS_NewString(ctx, "finish");
    JSValue finish_args[] = {finish_str, callback};
    JSValue result = JS_Call(ctx, on_method, stream, 2, finish_args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, finish_str);
  }

  // Listen for 'error' event
  JSValue error_str = JS_NewString(ctx, "error");
  JSValue error_args[] = {error_str, callback};
  JSValue result = JS_Call(ctx, on_method, stream, 2, error_args);
  JS_FreeValue(ctx, result);
  JS_FreeValue(ctx, error_str);

  JS_FreeValue(ctx, on_method);

  return JS_UNDEFINED;
}

// Readable.from(iterable, options) - Phase 5.3.1
// Creates a Readable stream from an iterable
static JSValue js_readable_from(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Readable.from() requires an iterable argument");
  }

  JSValue iterable = argv[0];
  JSValue options = argc > 1 ? argv[1] : JS_UNDEFINED;

  // Create a new Readable stream
  extern JSValue js_readable_constructor(JSContext * ctx, JSValueConst new_target, int argc, JSValueConst* argv);
  JSValue readable = js_readable_constructor(ctx, JS_UNDEFINED, argc > 1 ? 1 : 0, argc > 1 ? &options : NULL);
  if (JS_IsException(readable)) {
    return readable;
  }

  // Get iterator from iterable
  // For simplicity, assume arrays - try to get length and access by index
  JSValue length_val = JS_GetPropertyStr(ctx, iterable, "length");
  int32_t length = 0;
  bool is_array = false;

  if (!JS_IsUndefined(length_val)) {
    JS_ToInt32(ctx, &length, length_val);
    is_array = (length >= 0);
  }
  JS_FreeValue(ctx, length_val);

  // Try to iterate over array-like objects
  JSValue iterator_method = JS_UNDEFINED;
  if (!is_array) {
    // Try Symbol.iterator
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue symbol = JS_GetPropertyStr(ctx, global, "Symbol");
    JSValue iterator_sym = JS_GetPropertyStr(ctx, symbol, "iterator");

    // Convert JSValue to JSAtom for JS_GetProperty
    JSAtom iterator_atom = JS_ValueToAtom(ctx, iterator_sym);
    if (iterator_atom != JS_ATOM_NULL) {
      iterator_method = JS_GetProperty(ctx, iterable, iterator_atom);
      JS_FreeAtom(ctx, iterator_atom);
    }

    JS_FreeValue(ctx, iterator_sym);
    JS_FreeValue(ctx, symbol);
    JS_FreeValue(ctx, global);
  }

  // Handle array-like objects
  if (is_array) {
    JSValue push_method = JS_GetPropertyStr(ctx, readable, "push");

    for (int32_t i = 0; i < length; i++) {
      JSValue value = JS_GetPropertyUint32(ctx, iterable, i);
      JSValue push_result = JS_Call(ctx, push_method, readable, 1, &value);
      JS_FreeValue(ctx, push_result);
      JS_FreeValue(ctx, value);
    }

    // Push null to signal end
    JSValue null_val = JS_NULL;
    JSValue push_result = JS_Call(ctx, push_method, readable, 1, &null_val);
    JS_FreeValue(ctx, push_result);
    JS_FreeValue(ctx, push_method);
  } else if (JS_IsFunction(ctx, iterator_method)) {
    // Handle iterator protocol
    JSValue iterator = JS_Call(ctx, iterator_method, iterable, 0, NULL);
    JS_FreeValue(ctx, iterator_method);

    if (!JS_IsException(iterator)) {
      // Push all values from iterator into the readable stream
      JSValue next_method = JS_GetPropertyStr(ctx, iterator, "next");
      if (JS_IsFunction(ctx, next_method)) {
        JSValue push_method = JS_GetPropertyStr(ctx, readable, "push");

        while (true) {
          JSValue next_result = JS_Call(ctx, next_method, iterator, 0, NULL);
          if (JS_IsException(next_result)) {
            break;
          }

          JSValue done_prop = JS_GetPropertyStr(ctx, next_result, "done");
          bool done = JS_ToBool(ctx, done_prop);
          JS_FreeValue(ctx, done_prop);

          if (done) {
            JS_FreeValue(ctx, next_result);
            // Push null to signal end
            JSValue null_val = JS_NULL;
            JSValue push_result = JS_Call(ctx, push_method, readable, 1, &null_val);
            JS_FreeValue(ctx, push_result);
            break;
          }

          JSValue value = JS_GetPropertyStr(ctx, next_result, "value");
          JSValue push_result = JS_Call(ctx, push_method, readable, 1, &value);
          JS_FreeValue(ctx, push_result);
          JS_FreeValue(ctx, value);
          JS_FreeValue(ctx, next_result);
        }

        JS_FreeValue(ctx, push_method);
      }
      JS_FreeValue(ctx, next_method);
      JS_FreeValue(ctx, iterator);
    }
  } else {
    if (!JS_IsUndefined(iterator_method)) {
      JS_FreeValue(ctx, iterator_method);
    }
  }

  return readable;
}

// Add utility functions to stream module
void js_stream_init_utilities(JSContext* ctx, JSValue stream_module) {
  // Add pipeline() function
  JS_SetPropertyStr(ctx, stream_module, "pipeline", JS_NewCFunction(ctx, js_stream_pipeline, "pipeline", 2));

  // Add finished() function
  JS_SetPropertyStr(ctx, stream_module, "finished", JS_NewCFunction(ctx, js_stream_finished, "finished", 2));

  // Add Readable.from() static method
  JSValue readable_ctor = JS_GetPropertyStr(ctx, stream_module, "Readable");
  if (!JS_IsUndefined(readable_ctor)) {
    JS_SetPropertyStr(ctx, readable_ctor, "from", JS_NewCFunction(ctx, js_readable_from, "from", 2));
    JS_FreeValue(ctx, readable_ctor);
  }
}
