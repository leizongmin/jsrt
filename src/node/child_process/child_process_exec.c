#include "../../util/debug.h"
#include "child_process_internal.h"

// TODO: Implement exec()
JSValue js_child_process_exec(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "child_process.exec() not yet implemented");
}

// TODO: Implement execFile()
JSValue js_child_process_exec_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "child_process.execFile() not yet implemented");
}
