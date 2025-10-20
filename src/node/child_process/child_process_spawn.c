#include "../../util/debug.h"
#include "child_process_internal.h"

// TODO: Implement spawn() functionality
JSValue js_child_process_spawn(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "child_process.spawn() not yet implemented");
}

// TODO: Implement ChildProcess.kill()
JSValue js_child_process_kill(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "ChildProcess.kill() not yet implemented");
}

// TODO: Implement ChildProcess.ref()
JSValue js_child_process_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_UNDEFINED;
}

// TODO: Implement ChildProcess.unref()
JSValue js_child_process_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_UNDEFINED;
}
