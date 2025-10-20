#include "../../util/debug.h"
#include "child_process_internal.h"

// TODO: Implement spawnSync()
JSValue js_child_process_spawn_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "child_process.spawnSync() not yet implemented");
}

// TODO: Implement execSync()
JSValue js_child_process_exec_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "child_process.execSync() not yet implemented");
}

// TODO: Implement execFileSync()
JSValue js_child_process_exec_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "child_process.execFileSync() not yet implemented");
}
