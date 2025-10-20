#include "../../util/debug.h"
#include "child_process_internal.h"

// TODO: Implement finalizer
void js_child_process_finalizer(JSRuntime* rt, JSValue val) {
  JSChildProcess* child = JS_GetOpaque(val, js_child_process_class_id);
  if (!child) {
    return;
  }

  JSRT_Debug("Finalizing ChildProcess");

  // Don't finalize if we're in a callback
  if (child->in_callback) {
    return;
  }

  // Free resources
  if (child->cwd)
    free(child->cwd);
  if (child->env)
    free_string_array(child->env);
  if (child->args)
    free_string_array(child->args);
  if (child->file)
    free(child->file);
  if (child->stdout_buffer)
    free(child->stdout_buffer);
  if (child->stderr_buffer)
    free(child->stderr_buffer);

  // Free streams (need runtime context - skip for now in finalizer)
  // JS_FreeValue is not safe in finalizer without context

  js_free_rt(rt, child);
}

// TODO: Implement close callback
void child_process_close_callback(uv_handle_t* handle) {
  JSRT_Debug("ChildProcess handle closed");
  // Placeholder
}
