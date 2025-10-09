#ifndef JSRT_NODE_STREAM_INTERNAL_H
#define JSRT_NODE_STREAM_INTERNAL_H

#include <quickjs.h>
#include <stdbool.h>
#include <stdlib.h>

// Forward declare class IDs (defined in stream.c)
extern JSClassID js_readable_class_id;
extern JSClassID js_writable_class_id;
extern JSClassID js_duplex_class_id;
extern JSClassID js_transform_class_id;
extern JSClassID js_passthrough_class_id;

// Stream options structure
typedef struct {
  int highWaterMark;
  bool objectMode;
  const char* encoding;
  const char* defaultEncoding;
  bool emitClose;
  bool autoDestroy;
} StreamOptions;

// Callback structure for queued write callbacks
typedef struct {
  JSValue callback;  // Callback function to call when write completes
} WriteCallback;

// Stream base class - all streams extend EventEmitter
typedef struct {
  // Note: event_emitter is stored as "_emitter" property on the stream object
  // NOT stored in this struct to avoid reference counting issues
  bool readable;
  bool writable;
  bool destroyed;
  bool ended;
  bool errored;
  JSValue error_value;  // Store error object when errored
  JSValue* buffered_data;
  size_t buffer_size;
  size_t buffer_capacity;
  StreamOptions options;  // Stream options

  // Phase 2: Readable stream state
  bool flowing;                // Flowing vs paused mode
  bool reading;                // Currently reading flag
  bool ended_emitted;          // 'end' event emitted flag
  bool readable_emitted;       // 'readable' event emitted flag
  JSValue* pipe_destinations;  // Array of piped destinations
  size_t pipe_count;
  size_t pipe_capacity;

  // Phase 3: Writable stream state
  bool writable_ended;             // end() has been called
  bool writable_finished;          // All writes completed
  int writable_corked;             // Cork count (can be nested)
  bool need_drain;                 // Need to emit 'drain' event
  WriteCallback* write_callbacks;  // Array of pending write callbacks
  size_t write_callback_count;
  size_t write_callback_capacity;
} JSStreamData;

// event_emitter.c
void parse_stream_options(JSContext* ctx, JSValueConst options_obj, StreamOptions* opts);
JSValue init_stream_event_emitter(JSContext* ctx, JSValue stream_obj);
void stream_emit(JSContext* ctx, JSValueConst stream_obj, const char* event_name, int argc, JSValueConst* argv);
JSValue js_stream_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_stream_once(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_stream_emit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_stream_off(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_stream_remove_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_stream_add_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_stream_remove_all_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_stream_listener_count(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// readable.c
JSValue js_readable_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
void js_readable_init_prototype(JSContext* ctx, JSValue readable_proto);

// writable.c
JSValue js_writable_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
void js_writable_init_prototype(JSContext* ctx, JSValue writable_proto);

// duplex.c
JSValue js_duplex_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
void js_duplex_init_prototype(JSContext* ctx, JSValue duplex_proto);

// transform.c
JSValue js_transform_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
void js_transform_init_prototype(JSContext* ctx, JSValue transform_proto);

// passthrough.c
JSValue js_passthrough_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
void js_passthrough_init_prototype(JSContext* ctx, JSValue passthrough_proto);

// stream.c
JSValue js_stream_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_stream_get_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_stream_get_errored(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// utilities.c - Phase 5 utility functions
void js_stream_init_utilities(JSContext* ctx, JSValue stream_module);

#endif  // JSRT_NODE_STREAM_INTERNAL_H
