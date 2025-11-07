#include <string.h>
#include "../runtime.h"
#include "../std/assert.h"
#include "../util/debug.h"
#include "node_modules.h"

// Readline module - provides interface for reading data from a stream one line at a time
// This is a minimal implementation focused on npm package compatibility

// Forward declarations
static JSValue js_readline_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_readline_prompt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_readline_question(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_readline_pause(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_readline_resume(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_readline_addHistory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Readline interface structure
typedef struct {
  JSValue input;
  JSValue output;
  JSValue terminal;
  JSValue history;
  bool completer;
} ReadlineInterface;

// Readline constructor
static JSValue js_readline_createInterface(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue options = JS_UNDEFINED;
  JSValue input = JS_UNDEFINED;
  JSValue output = JS_UNDEFINED;
  JSValue terminal = JS_UNDEFINED;

  if (argc > 0) {
    options = argv[0];
    if (JS_IsObject(options)) {
      input = JS_GetPropertyStr(ctx, options, "input");
      output = JS_GetPropertyStr(ctx, options, "output");
      terminal = JS_GetPropertyStr(ctx, options, "terminal");
    }
  }

  // Create interface object
  ReadlineInterface* rl = js_malloc(ctx, sizeof(ReadlineInterface));
  if (!rl) {
    return JS_ThrowOutOfMemory(ctx);
  }

  memset(rl, 0, sizeof(ReadlineInterface));

  // Store references (duplicates will be handled by GC)
  rl->input = JS_IsUndefined(input) ? JS_UNDEFINED : JS_DupValue(ctx, input);
  rl->output = JS_IsUndefined(output) ? JS_UNDEFINED : JS_DupValue(ctx, output);
  rl->terminal = JS_IsUndefined(terminal) ? JS_UNDEFINED : JS_DupValue(ctx, terminal);
  rl->history = JS_NewArray(ctx);
  rl->completer = false;

  // Check for completer function
  if (!JS_IsUndefined(options)) {
    JSValue completer = JS_GetPropertyStr(ctx, options, "completer");
    rl->completer = !JS_IsUndefined(completer) && !JS_IsNull(completer);
    JS_FreeValue(ctx, completer);
  }

  JSValue obj = JS_NewObjectClass(ctx, 1);
  if (JS_IsException(obj)) {
    if (!JS_IsUndefined(rl->input))
      JS_FreeValue(ctx, rl->input);
    if (!JS_IsUndefined(rl->output))
      JS_FreeValue(ctx, rl->output);
    if (!JS_IsUndefined(rl->terminal))
      JS_FreeValue(ctx, rl->terminal);
    JS_FreeValue(ctx, rl->history);
    js_free(ctx, rl);
    return obj;
  }

  JS_SetOpaque(obj, rl);

  // Set properties
  JS_SetPropertyStr(ctx, obj, "input", JS_IsUndefined(rl->input) ? JS_NULL : JS_DupValue(ctx, rl->input));
  JS_SetPropertyStr(ctx, obj, "output", JS_IsUndefined(rl->output) ? JS_NULL : JS_DupValue(ctx, rl->output));
  JS_SetPropertyStr(ctx, obj, "terminal",
                    JS_IsUndefined(rl->terminal) ? JS_NewBool(ctx, false) : JS_DupValue(ctx, rl->terminal));
  JS_SetPropertyStr(ctx, obj, "history", JS_DupValue(ctx, rl->history));

  // Set methods on the object
  JS_SetPropertyStr(ctx, obj, "close", JS_NewCFunction(ctx, js_readline_close, "close", 0));
  JS_SetPropertyStr(ctx, obj, "prompt", JS_NewCFunction(ctx, js_readline_prompt, "prompt", 2));
  JS_SetPropertyStr(ctx, obj, "question", JS_NewCFunction(ctx, js_readline_question, "question", 2));
  JS_SetPropertyStr(ctx, obj, "pause", JS_NewCFunction(ctx, js_readline_pause, "pause", 0));
  JS_SetPropertyStr(ctx, obj, "resume", JS_NewCFunction(ctx, js_readline_resume, "resume", 0));
  JS_SetPropertyStr(ctx, obj, "addHistory", JS_NewCFunction(ctx, js_readline_addHistory, "addHistory", 1));

  if (!JS_IsUndefined(input))
    JS_FreeValue(ctx, input);
  if (!JS_IsUndefined(output))
    JS_FreeValue(ctx, output);
  if (!JS_IsUndefined(terminal))
    JS_FreeValue(ctx, terminal);

  return obj;
}

// Readline interface finalizer
static void js_readline_finalizer(JSRuntime* rt, JSValue val) {
  ReadlineInterface* rl = JS_GetOpaque(val, 1);
  if (rl) {
    // Note: We don't free JS values here as they'll be handled by GC
    js_free_rt(rt, rl);
  }
}

// Close readline interface
static JSValue js_readline_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  ReadlineInterface* rl = JS_GetOpaque(this_val, 1);
  if (!rl) {
    return JS_ThrowTypeError(ctx, "Invalid ReadlineInterface object");
  }

  // Emit close event if EventEmitter
  JSValue emit_fn = JS_GetPropertyStr(ctx, this_val, "emit");
  if (!JS_IsUndefined(emit_fn) && !JS_IsNull(emit_fn) && JS_IsFunction(ctx, emit_fn)) {
    JSValue event_name = JS_NewString(ctx, "close");
    JSValue result = JS_Call(ctx, emit_fn, this_val, 1, &event_name);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, event_name);
  }
  JS_FreeValue(ctx, emit_fn);

  return JS_UNDEFINED;
}

// Prompt for input
static JSValue js_readline_prompt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* prompt_text = "> ";
  JSValue preserve_cursor = JS_UNDEFINED;

  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    const char* temp = JS_ToCString(ctx, argv[0]);
    if (temp) {
      prompt_text = temp;
      JS_FreeCString(ctx, temp);
    }
  }

  if (argc > 1) {
    preserve_cursor = argv[1];
  }

  ReadlineInterface* rl = JS_GetOpaque(this_val, 1);
  if (!rl) {
    return JS_ThrowTypeError(ctx, "Invalid ReadlineInterface object");
  }

  // For this minimal implementation, just write prompt to output
  if (!JS_IsUndefined(rl->output) && !JS_IsNull(rl->output)) {
    JSValue write_fn = JS_GetPropertyStr(ctx, rl->output, "write");
    if (JS_IsFunction(ctx, write_fn)) {
      JSValue prompt_str = JS_NewString(ctx, prompt_text);
      JSValue result = JS_Call(ctx, write_fn, rl->output, 1, &prompt_str);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, prompt_str);
    }
    JS_FreeValue(ctx, write_fn);
  }

  return JS_UNDEFINED;
}

// Question method (alias for prompt with callback)
static JSValue js_readline_question(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* question_text = "> ";
  JSValue callback = JS_UNDEFINED;

  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    const char* temp = JS_ToCString(ctx, argv[0]);
    if (temp) {
      question_text = temp;
      JS_FreeCString(ctx, temp);
    }
  }

  if (argc > 1) {
    callback = argv[1];
  }

  if (JS_IsUndefined(callback) || !JS_IsFunction(ctx, callback)) {
    return JS_ThrowTypeError(ctx, "question() requires a callback function");
  }

  ReadlineInterface* rl = JS_GetOpaque(this_val, 1);
  if (!rl) {
    return JS_ThrowTypeError(ctx, "Invalid ReadlineInterface object");
  }

  // Write question and call callback with empty response (simplified)
  if (!JS_IsUndefined(rl->output) && !JS_IsNull(rl->output)) {
    JSValue write_fn = JS_GetPropertyStr(ctx, rl->output, "write");
    if (JS_IsFunction(ctx, write_fn)) {
      JSValue question_str = JS_NewString(ctx, question_text);
      JSValue result = JS_Call(ctx, write_fn, rl->output, 1, &question_str);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, question_str);
    }
    JS_FreeValue(ctx, write_fn);
  }

  // Call callback with empty string (simplified - no actual input reading)
  JSValue response = JS_NewString(ctx, "");
  JSValue callback_result = JS_Call(ctx, callback, JS_UNDEFINED, 1, &response);
  JS_FreeValue(ctx, callback_result);
  JS_FreeValue(ctx, response);

  return JS_UNDEFINED;
}

// Pause input
static JSValue js_readline_pause(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  ReadlineInterface* rl = JS_GetOpaque(this_val, 1);
  if (!rl) {
    return JS_ThrowTypeError(ctx, "Invalid ReadlineInterface object");
  }

  // Return this for chaining
  return JS_DupValue(ctx, this_val);
}

// Resume input
static JSValue js_readline_resume(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  ReadlineInterface* rl = JS_GetOpaque(this_val, 1);
  if (!rl) {
    return JS_ThrowTypeError(ctx, "Invalid ReadlineInterface object");
  }

  // Return this for chaining
  return JS_DupValue(ctx, this_val);
}

// Add to history
static JSValue js_readline_addHistory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_UNDEFINED;
  }

  JSValue line = argv[0];
  if (!JS_IsString(line)) {
    return JS_UNDEFINED;
  }

  ReadlineInterface* rl = JS_GetOpaque(this_val, 1);
  if (!rl) {
    return JS_ThrowTypeError(ctx, "Invalid ReadlineInterface object");
  }

  // Add to history array
  uint32_t length;
  JSValue length_val = JS_GetPropertyStr(ctx, rl->history, "length");
  JS_ToUint32(ctx, &length, length_val);
  JS_FreeValue(ctx, length_val);

  JS_SetPropertyUint32(ctx, rl->history, length, JS_DupValue(ctx, line));

  return JS_UNDEFINED;
}

// Create completer function
static JSValue js_readline_createCompleter(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue completer = JS_UNDEFINED;

  if (argc > 0) {
    completer = argv[0];
  }

  if (JS_IsUndefined(completer) || !JS_IsFunction(ctx, completer)) {
    return JS_ThrowTypeError(ctx, "createCompleter requires a function argument");
  }

  // Return the completer function as-is
  return JS_DupValue(ctx, completer);
}

// Cursor positioning methods (stubs)
static JSValue js_readline_clearLine(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_UNDEFINED;
}

static JSValue js_readline_clearScreenDown(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_UNDEFINED;
}

static JSValue js_readline_cursorTo(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_UNDEFINED;
}

static JSValue js_readline_moveCursor(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_UNDEFINED;
}

// Readline module initialization (CommonJS)
JSValue JSRT_InitNodeReadline(JSContext* ctx) {
  JSValue readline_obj = JS_NewObject(ctx);

  // Interface constructor
  JSValue ctor = JS_NewCFunction(ctx, js_readline_createInterface, "createInterface", 1);
  JS_SetPropertyStr(ctx, readline_obj, "createInterface", ctor);

  // Interface prototype methods
  JSValue proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, proto, "close", JS_NewCFunction(ctx, js_readline_close, "close", 0));
  JS_SetPropertyStr(ctx, proto, "prompt", JS_NewCFunction(ctx, js_readline_prompt, "prompt", 2));
  JS_SetPropertyStr(ctx, proto, "question", JS_NewCFunction(ctx, js_readline_question, "question", 2));
  JS_SetPropertyStr(ctx, proto, "pause", JS_NewCFunction(ctx, js_readline_pause, "pause", 0));
  JS_SetPropertyStr(ctx, proto, "resume", JS_NewCFunction(ctx, js_readline_resume, "resume", 0));
  JS_SetPropertyStr(ctx, proto, "addHistory", JS_NewCFunction(ctx, js_readline_addHistory, "addHistory", 1));

  // Set prototype on constructor
  JS_SetPropertyStr(ctx, ctor, "prototype", proto);

  // Module-level functions
  JS_SetPropertyStr(ctx, readline_obj, "createCompleter",
                    JS_NewCFunction(ctx, js_readline_createCompleter, "createCompleter", 1));
  JS_SetPropertyStr(ctx, readline_obj, "clearLine", JS_NewCFunction(ctx, js_readline_clearLine, "clearLine", 2));
  JS_SetPropertyStr(ctx, readline_obj, "clearScreenDown",
                    JS_NewCFunction(ctx, js_readline_clearScreenDown, "clearScreenDown", 0));
  JS_SetPropertyStr(ctx, readline_obj, "cursorTo", JS_NewCFunction(ctx, js_readline_cursorTo, "cursorTo", 3));
  JS_SetPropertyStr(ctx, readline_obj, "moveCursor", JS_NewCFunction(ctx, js_readline_moveCursor, "moveCursor", 3));

  JS_FreeValue(ctx, proto);
  return readline_obj;
}

// Readline module initialization (ES Module)
int js_node_readline_init(JSContext* ctx, JSModuleDef* m) {
  JSValue readline_obj = JSRT_InitNodeReadline(ctx);

  // Add exports
  JS_SetModuleExport(ctx, m, "createInterface", JS_GetPropertyStr(ctx, readline_obj, "createInterface"));
  JS_SetModuleExport(ctx, m, "createCompleter", JS_GetPropertyStr(ctx, readline_obj, "createCompleter"));
  JS_SetModuleExport(ctx, m, "clearLine", JS_GetPropertyStr(ctx, readline_obj, "clearLine"));
  JS_SetModuleExport(ctx, m, "clearScreenDown", JS_GetPropertyStr(ctx, readline_obj, "clearScreenDown"));
  JS_SetModuleExport(ctx, m, "cursorTo", JS_GetPropertyStr(ctx, readline_obj, "cursorTo"));
  JS_SetModuleExport(ctx, m, "moveCursor", JS_GetPropertyStr(ctx, readline_obj, "moveCursor"));
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, readline_obj));

  JS_FreeValue(ctx, readline_obj);
  return 0;
}