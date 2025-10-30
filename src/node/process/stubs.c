#include <quickjs.h>
#include "process.h"

// Phase 7: Stub implementations for future features
// These APIs are not fully implemented yet but provide placeholders
// for Node.js compatibility

// ============================================================================
// process.report - Diagnostic report stubs
// ============================================================================

// process.report.writeReport([filename][, err])
static JSValue js_process_report_write_report(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Stub: Return null for now
  // TODO: Implement diagnostic report generation
  return JS_NULL;
}

// process.report.getReport([err])
static JSValue js_process_report_get_report(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Stub: Return null for now
  // TODO: Implement diagnostic report generation
  return JS_NULL;
}

// Create process.report object
JSValue js_process_get_report(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue report = JS_NewObject(ctx);

  // Methods
  JS_SetPropertyStr(ctx, report, "writeReport", JS_NewCFunction(ctx, js_process_report_write_report, "writeReport", 2));
  JS_SetPropertyStr(ctx, report, "getReport", JS_NewCFunction(ctx, js_process_report_get_report, "getReport", 1));

  // Properties (stubs)
  JS_SetPropertyStr(ctx, report, "directory", JS_NewString(ctx, ""));
  JS_SetPropertyStr(ctx, report, "filename", JS_NewString(ctx, ""));
  JS_SetPropertyStr(ctx, report, "reportOnFatalError", JS_NewBool(ctx, 0));
  JS_SetPropertyStr(ctx, report, "reportOnSignal", JS_NewBool(ctx, 0));
  JS_SetPropertyStr(ctx, report, "reportOnUncaughtException", JS_NewBool(ctx, 0));
  JS_SetPropertyStr(ctx, report, "signal", JS_NewString(ctx, "SIGUSR2"));

  return report;
}

// ============================================================================
// process.permission - Permission system stubs
// ============================================================================

// process.permission.has(scope[, reference])
static JSValue js_process_permission_has(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Stub: Always return true (no permission system yet)
  // TODO: Implement permission system
  return JS_TRUE;
}

// Create process.permission object
JSValue js_process_get_permission(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue permission = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, permission, "has", JS_NewCFunction(ctx, js_process_permission_has, "has", 2));

  return permission;
}

// ============================================================================
// process.finalization - Finalization registry stubs
// ============================================================================

// process.finalization.register(ref, callback)
static JSValue js_process_finalization_register(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Stub: No-op for now
  // TODO: Implement finalization registry
  return JS_UNDEFINED;
}

// process.finalization.unregister(token)
static JSValue js_process_finalization_unregister(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Stub: No-op for now
  // TODO: Implement finalization registry
  return JS_UNDEFINED;
}

// Create process.finalization object
JSValue js_process_get_finalization(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue finalization = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, finalization, "register",
                    JS_NewCFunction(ctx, js_process_finalization_register, "register", 2));
  JS_SetPropertyStr(ctx, finalization, "unregister",
                    JS_NewCFunction(ctx, js_process_finalization_unregister, "unregister", 1));

  return finalization;
}

// ============================================================================
// process.dlopen() - Dynamic loading stub
// ============================================================================

JSValue js_process_dlopen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "process.dlopen() is not implemented - native addons not supported yet");
}

// ============================================================================
// process.getBuiltinModule() - Get builtin module
// ============================================================================

JSValue js_process_get_builtin_module(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "getBuiltinModule requires a module name argument");
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  // Check if it's a builtin module (jsrt:* or node:*)
  // For now, return null (not a builtin)
  // TODO: Integrate with module loader to check builtin registry
  JS_FreeCString(ctx, name);
  return JS_NULL;
}

// ============================================================================
// Initialization
// ============================================================================

void jsrt_process_init_stubs(void) {
  // No initialization needed for stubs
}
