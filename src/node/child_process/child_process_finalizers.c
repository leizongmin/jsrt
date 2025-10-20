#include "../../util/debug.h"
#include "child_process_internal.h"

// Finalizer for ChildProcess instances
void js_child_process_finalizer(JSRuntime* rt, JSValue val) {
  JSChildProcess* child = JS_GetOpaque(val, js_child_process_class_id);
  if (!child) {
    return;
  }

  JSRT_Debug("Finalizing ChildProcess (PID: %d)", child->pid);

  // Don't finalize if we're in a callback
  if (child->in_callback) {
    JSRT_Debug("Skipping finalization - in callback");
    return;
  }

  // Free owned string resources
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

  // Note: JSValue fields (child_obj, stdin_stream, stdout_stream, stderr_stream)
  // cannot be freed here because we don't have JSContext in finalizer.
  // They will be garbage collected by QuickJS.

  // Free the child structure
  js_free_rt(rt, child);
}

// Close callback for stdio pipes and process handle
void child_process_close_callback(uv_handle_t* handle) {
  JSRT_Debug("ChildProcess handle closed");

  // For stdio pipes, we just free the pipe structure
  // The pipe is malloc'd, not js_malloc'd
  if (handle->type == UV_NAMED_PIPE || handle->type == UV_TCP) {
    free(handle);
  }

  // For process handle, we don't free anything
  // The handle is embedded in JSChildProcess structure
}
