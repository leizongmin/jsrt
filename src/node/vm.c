#include <string.h>
#include "../runtime.h"
#include "../std/assert.h"
#include "../util/debug.h"
#include "node_modules.h"

// VM module implementation - provides script compilation and execution in contexts
// This is a minimal implementation focused on npm package compatibility

// Forward declarations
static JSValue js_vm_script_runInContext(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_vm_script_runInNewContext(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_vm_script_runInThisContext(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// VM Context constructor
static JSValue js_vm_createContext(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue sandbox;

  if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    sandbox = JS_DupValue(ctx, argv[0]);
  } else {
    sandbox = JS_NewObject(ctx);
  }

  // Create context object with sandbox as global
  JSValue context_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, context_obj, "global", JS_DupValue(ctx, sandbox));
  JS_SetPropertyStr(ctx, context_obj, "sandbox", sandbox);

  return context_obj;
}

// VM Script constructor
static JSValue js_vm_script_ctor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue code;
  JSValue options = JS_UNDEFINED;

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Script constructor requires at least code argument");
  }

  code = argv[0];
  if (!JS_IsString(code)) {
    return JS_ThrowTypeError(ctx, "Script code must be a string");
  }

  if (argc > 1) {
    options = argv[1];
  }

  JSValue script_obj = JS_NewObjectClass(ctx, 1);
  if (JS_IsException(script_obj)) {
    return script_obj;
  }

  // Store code and options
  JS_SetPropertyStr(ctx, script_obj, "code", JS_DupValue(ctx, code));
  if (!JS_IsUndefined(options)) {
    JS_SetPropertyStr(ctx, script_obj, "options", JS_DupValue(ctx, options));
  }

  // Set methods on the Script object
  JS_SetPropertyStr(ctx, script_obj, "runInContext",
                    JS_NewCFunction(ctx, js_vm_script_runInContext, "runInContext", 1));
  JS_SetPropertyStr(ctx, script_obj, "runInNewContext",
                    JS_NewCFunction(ctx, js_vm_script_runInNewContext, "runInNewContext", 0));
  JS_SetPropertyStr(ctx, script_obj, "runInThisContext",
                    JS_NewCFunction(ctx, js_vm_script_runInThisContext, "runInThisContext", 0));

  return script_obj;
}

// Run compiled script in context
static JSValue js_vm_script_runInContext(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue script_code = JS_GetPropertyStr(ctx, this_val, "code");
  if (JS_IsException(script_code)) {
    return JS_EXCEPTION;
  }

  JSValue context_obj;
  if (argc > 0) {
    context_obj = argv[0];
  } else {
    JS_FreeValue(ctx, script_code);
    return JS_ThrowTypeError(ctx, "runInContext requires a context argument");
  }

  JSValue sandbox = JS_GetPropertyStr(ctx, context_obj, "sandbox");
  if (JS_IsException(sandbox)) {
    JS_FreeValue(ctx, script_code);
    return JS_EXCEPTION;
  }

  // Execute code in sandbox context
  const char* code_str = JS_ToCString(ctx, script_code);
  if (!code_str) {
    JS_FreeValue(ctx, script_code);
    JS_FreeValue(ctx, sandbox);
    return JS_EXCEPTION;
  }

  // Create new context with sandbox as globalThis
  JSValue result = JS_Eval(ctx, code_str, strlen(code_str), "vm_script", JS_EVAL_TYPE_GLOBAL);

  JS_FreeCString(ctx, code_str);
  JS_FreeValue(ctx, script_code);
  JS_FreeValue(ctx, sandbox);

  return result;
}

// Run script in new context
static JSValue js_vm_script_runInNewContext(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue script_code = JS_GetPropertyStr(ctx, this_val, "code");
  if (JS_IsException(script_code)) {
    return JS_EXCEPTION;
  }

  JSValue sandbox = JS_UNDEFINED;
  if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    sandbox = argv[0];
  } else {
    sandbox = JS_NewObject(ctx);
  }

  const char* code_str = JS_ToCString(ctx, script_code);
  if (!code_str) {
    JS_FreeValue(ctx, script_code);
    return JS_EXCEPTION;
  }

  // Evaluate code with sandbox as global scope
  JSValue result = JS_Eval(ctx, code_str, strlen(code_str), "vm_script", JS_EVAL_TYPE_GLOBAL);

  JS_FreeCString(ctx, code_str);
  JS_FreeValue(ctx, script_code);

  return result;
}

// Run script in current context (this global)
static JSValue js_vm_script_runInThisContext(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue script_code = JS_GetPropertyStr(ctx, this_val, "code");
  if (JS_IsException(script_code)) {
    return JS_EXCEPTION;
  }

  const char* code_str = JS_ToCString(ctx, script_code);
  if (!code_str) {
    JS_FreeValue(ctx, script_code);
    return JS_EXCEPTION;
  }

  JSValue result = JS_Eval(ctx, code_str, strlen(code_str), "vm_script", JS_EVAL_TYPE_GLOBAL);

  JS_FreeCString(ctx, code_str);
  JS_FreeValue(ctx, script_code);

  return result;
}

// Create script from code
static JSValue js_vm_createScript(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue code;
  JSValue options = JS_UNDEFINED;

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "createScript requires code argument");
  }

  code = argv[0];
  if (!JS_IsString(code)) {
    return JS_ThrowTypeError(ctx, "Script code must be a string");
  }

  if (argc > 1) {
    options = argv[1];
  }

  // Use Script constructor
  JSValue args[2] = {code, options};
  return js_vm_script_ctor(ctx, JS_UNDEFINED, 2, args);
}

// VM module initialization (CommonJS)
JSValue JSRT_InitNodeVM(JSContext* ctx) {
  JSValue vm_obj = JS_NewObject(ctx);

  // Script constructor
  JSValue script_ctor = JS_NewCFunction2(ctx, js_vm_script_ctor, "Script", 2, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, vm_obj, "Script", script_ctor);

  // Script prototype methods
  JSValue script_proto = JS_GetPropertyStr(ctx, script_ctor, "prototype");
  JS_SetPropertyStr(ctx, script_proto, "runInContext",
                    JS_NewCFunction(ctx, js_vm_script_runInContext, "runInContext", 1));
  JS_SetPropertyStr(ctx, script_proto, "runInNewContext",
                    JS_NewCFunction(ctx, js_vm_script_runInNewContext, "runInNewContext", 0));
  JS_SetPropertyStr(ctx, script_proto, "runInThisContext",
                    JS_NewCFunction(ctx, js_vm_script_runInThisContext, "runInThisContext", 0));
  JS_FreeValue(ctx, script_proto);

  // VM functions
  JS_SetPropertyStr(ctx, vm_obj, "createContext", JS_NewCFunction(ctx, js_vm_createContext, "createContext", 1));
  JS_SetPropertyStr(ctx, vm_obj, "createScript", JS_NewCFunction(ctx, js_vm_createScript, "createScript", 1));

  // Add Node.js compatibility constants
  JSValue constants_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, constants_obj, "SCRIPT_TYPE_INVALID", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, constants_obj, "SCRIPT_TYPE_SCRIPT", JS_NewInt32(ctx, 1));
  JS_SetPropertyStr(ctx, constants_obj, "SCRIPT_TYPE_MODULE", JS_NewInt32(ctx, 2));
  JS_SetPropertyStr(ctx, constants_obj, "SCRIPT_TYPE_FUNCTION", JS_NewInt32(ctx, 3));
  JS_SetPropertyStr(ctx, vm_obj, "SCRIPT_TYPES_INVALID", JS_DupValue(ctx, constants_obj));

  JS_FreeValue(ctx, constants_obj);
  return vm_obj;
}

// VM module initialization (ES Module)
int js_node_vm_init(JSContext* ctx, JSModuleDef* m) {
  JSValue vm_obj = JSRT_InitNodeVM(ctx);

  // Add exports
  JS_SetModuleExport(ctx, m, "Script", JS_GetPropertyStr(ctx, vm_obj, "Script"));
  JS_SetModuleExport(ctx, m, "createContext", JS_GetPropertyStr(ctx, vm_obj, "createContext"));
  JS_SetModuleExport(ctx, m, "createScript", JS_GetPropertyStr(ctx, vm_obj, "createScript"));
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, vm_obj));

  JS_FreeValue(ctx, vm_obj);
  return 0;
}