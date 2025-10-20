#include "../../util/debug.h"
#include "child_process_internal.h"

// TODO: Implement fork()
JSValue js_child_process_fork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "child_process.fork() not yet implemented");
}

// TODO: Implement ChildProcess.send()
JSValue js_child_process_send(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "ChildProcess.send() not yet implemented");
}

// TODO: Implement ChildProcess.disconnect()
JSValue js_child_process_disconnect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_UNDEFINED;
}
