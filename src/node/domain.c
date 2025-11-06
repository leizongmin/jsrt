/**
 * Node.js Domain Module Implementation
 *
 * Provides minimal domain compatibility for AWS SDK and other packages
 * that depend on domain functionality for error handling.
 */

#include "domain.h"
#include <stdlib.h>
#include <string.h>
#include "../runtime.h"
#include "../util/debug.h"

// Forward declare the class ID
static JSClassID js_domain_class_id;

// ============================================================================
// Domain Class Implementation
// ============================================================================

// Domain class structure
typedef struct {
  JSContext* ctx;
  JSValue members;        // Array of domain members
  JSValue error_handler;  // Error handler function
  bool disposed;
} JSDomain;

// Global domain context
static JSDomain* current_domain = NULL;

// ============================================================================
// Domain Constructor and Factory Functions
// ============================================================================

// Domain constructor
static JSValue js_domain_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_UNDEFINED;
  JSValue proto;

  // Create domain object
  if (JS_IsUndefined(new_target)) {
    proto = JS_GetClassProto(ctx, js_domain_class_id);
  } else {
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  }

  obj = JS_NewObjectProtoClass(ctx, proto, js_domain_class_id);
  JS_FreeValue(ctx, proto);

  if (JS_IsException(obj))
    return obj;

  // Create domain structure
  JSDomain* domain = js_malloc(ctx, sizeof(JSDomain));
  if (!domain) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  memset(domain, 0, sizeof(JSDomain));
  domain->ctx = ctx;
  domain->disposed = false;

  // Initialize members array
  domain->members = JS_NewArray(ctx);
  if (JS_IsException(domain->members)) {
    JS_FreeValue(ctx, obj);
    js_free(ctx, domain);
    return JS_EXCEPTION;
  }

  // Initialize error handler as undefined
  domain->error_handler = JS_UNDEFINED;

  // Set opaque pointer
  JS_SetOpaque(obj, domain);

  JSRT_Debug("Domain created: %p", domain);
  return obj;
}

// domain.create() factory function
static JSValue js_domain_create(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return js_domain_constructor(ctx, JS_UNDEFINED, argc, argv);
}

// ============================================================================
// Domain Core Methods
// ============================================================================

// domain.run() method - execute callback in domain context
static JSValue js_domain_run(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDomain* domain = JS_GetOpaque2(ctx, this_val, js_domain_class_id);
  if (!domain)
    return JS_EXCEPTION;

  if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
    return JS_ThrowTypeError(ctx, "Domain.run() requires a function callback");

  if (domain->disposed) {
    JSRT_Debug("Attempted to run disposed domain: %p", domain);
    return JS_UNDEFINED;
  }

  // Set current domain
  JSDomain* previous_domain = current_domain;
  current_domain = domain;

  JSRT_Debug("Domain.run() started: %p", domain);

  // Execute the function
  JSValue result = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, NULL);

  // Restore previous domain
  current_domain = previous_domain;

  JSRT_Debug("Domain.run() completed: %p", domain);

  return result;
}

// domain.add() method - add emitter to domain
static JSValue js_domain_add(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDomain* domain = JS_GetOpaque2(ctx, this_val, js_domain_class_id);
  if (!domain)
    return JS_EXCEPTION;

  if (argc < 1)
    return JS_ThrowTypeError(ctx, "Domain.add() requires an emitter argument");

  if (domain->disposed) {
    JSRT_Debug("Attempted to add to disposed domain: %p", domain);
    return JS_UNDEFINED;
  }

  // Add to members array
  JSValue length = JS_GetPropertyStr(ctx, domain->members, "length");
  int32_t len;
  if (JS_ToInt32(ctx, &len, length) < 0) {
    JS_FreeValue(ctx, length);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length);

  JS_SetPropertyUint32(ctx, domain->members, len, JS_DupValue(ctx, argv[0]));

  JSRT_Debug("Added member to domain %p, total members: %d", domain, len + 1);
  return JS_UNDEFINED;
}

// domain.remove() method - remove emitter from domain
static JSValue js_domain_remove(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDomain* domain = JS_GetOpaque2(ctx, this_val, js_domain_class_id);
  if (!domain)
    return JS_EXCEPTION;

  if (argc < 1)
    return JS_ThrowTypeError(ctx, "Domain.remove() requires an emitter argument");

  if (domain->disposed) {
    JSRT_Debug("Attempted to remove from disposed domain: %p", domain);
    return JS_UNDEFINED;
  }

  // Find and remove from members array (simplified implementation)
  JSValue length = JS_GetPropertyStr(ctx, domain->members, "length");
  int32_t len;
  if (JS_ToInt32(ctx, &len, length) < 0) {
    JS_FreeValue(ctx, length);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length);

  // Simple linear search and remove
  for (int32_t i = 0; i < len; i++) {
    JSValue member = JS_GetPropertyUint32(ctx, domain->members, i);
    bool is_same = JS_StrictEq(ctx, member, argv[0]);
    JS_FreeValue(ctx, member);

    if (is_same) {
      // Remove element (simple approach: create new array without this element)
      JSValue new_members = JS_NewArray(ctx);
      for (int32_t j = 0, k = 0; j < len; j++) {
        if (j != i) {
          JSValue elem = JS_GetPropertyUint32(ctx, domain->members, j);
          JS_SetPropertyUint32(ctx, new_members, k++, elem);
        }
      }
      JS_FreeValue(ctx, domain->members);
      domain->members = new_members;

      JSRT_Debug("Removed member from domain %p", domain);
      return JS_UNDEFINED;
    }
  }

  JSRT_Debug("Member not found in domain %p", domain);
  return JS_UNDEFINED;
}

// ============================================================================
// Domain Interop and Cleanup
// ============================================================================

// domain.on() method - simplified event handling
static JSValue js_domain_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDomain* domain = JS_GetOpaque2(ctx, this_val, js_domain_class_id);
  if (!domain)
    return JS_EXCEPTION;

  if (argc < 2)
    return JS_ThrowTypeError(ctx, "Domain.on() requires event and listener arguments");

  const char* event = JS_ToCString(ctx, argv[0]);
  if (!event)
    return JS_EXCEPTION;

  if (strcmp(event, "error") == 0 && JS_IsFunction(ctx, argv[1])) {
    // Store error handler
    if (!JS_IsUndefined(domain->error_handler)) {
      JS_FreeValue(ctx, domain->error_handler);
    }
    domain->error_handler = JS_DupValue(ctx, argv[1]);
  }

  JS_FreeCString(ctx, event);
  return JS_UNDEFINED;
}

// domain.dispose() method
static JSValue js_domain_dispose(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDomain* domain = JS_GetOpaque2(ctx, this_val, js_domain_class_id);
  if (!domain)
    return JS_EXCEPTION;

  if (domain->disposed) {
    return JS_UNDEFINED;
  }

  JSRT_Debug("Disposing domain: %p", domain);

  // Clear current domain if this is the active one
  if (current_domain == domain) {
    current_domain = NULL;
  }

  // Dispose members
  if (!JS_IsUndefined(domain->members)) {
    JS_FreeValue(ctx, domain->members);
    domain->members = JS_UNDEFINED;
  }

  if (!JS_IsUndefined(domain->error_handler)) {
    JS_FreeValue(ctx, domain->error_handler);
    domain->error_handler = JS_UNDEFINED;
  }

  domain->disposed = true;
  return JS_UNDEFINED;
}

// Forward declare finalizer (needed before JSClassDef)
static void js_domain_finalizer(JSRuntime* rt, JSValue val);

// ============================================================================
// Module Initialization
// ============================================================================

static const JSCFunctionListEntry js_domain_proto_funcs[] = {
    JS_CFUNC_DEF("run", 1, js_domain_run),
    JS_CFUNC_DEF("add", 1, js_domain_add),
    JS_CFUNC_DEF("remove", 1, js_domain_remove),
    JS_CFUNC_DEF("on", 2, js_domain_on),
    JS_CFUNC_DEF("dispose", 0, js_domain_dispose),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Domain", JS_PROP_CONFIGURABLE),
};

static const JSCFunctionListEntry js_domain_static_funcs[] = {
    JS_CFUNC_DEF("create", 0, js_domain_create),
};

static JSClassDef js_domain_class = {
    "Domain",
    .finalizer = js_domain_finalizer,
};

// Initialize domain module
JSValue JSRT_InitNodeDomain(JSContext* ctx) {
  JSRT_Debug("Initializing Node.js Domain module");

  // Create domain class
  JS_NewClassID(&js_domain_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_domain_class_id, &js_domain_class);

  JSValue domain_proto = JS_NewObject(ctx);
  JS_SetPropertyFunctionList(ctx, domain_proto, js_domain_proto_funcs,
                             sizeof(js_domain_proto_funcs) / sizeof(js_domain_proto_funcs[0]));

  JSValue domain_class = JS_NewCFunction2(ctx, js_domain_constructor, "Domain", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, domain_class, "prototype", domain_proto);
  JS_SetPropertyFunctionList(ctx, domain_class, js_domain_static_funcs,
                             sizeof(js_domain_static_funcs) / sizeof(js_domain_static_funcs[0]));

  // Create module exports
  JSValue module_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, module_obj, "Domain", domain_class);

  JSRT_Debug("Node.js Domain module initialized successfully");
  return module_obj;
}

// Module initializer for CommonJS
int js_node_domain_init(JSContext* ctx, JSModuleDef* m) {
  JSValue domain_obj = JSRT_InitNodeDomain(ctx);

  // Set module exports
  JS_SetModuleExport(ctx, m, "Domain", JS_GetPropertyStr(ctx, domain_obj, "Domain"));
  JS_SetModuleExport(ctx, m, "create", JS_GetPropertyStr(ctx, domain_obj, "create"));
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, domain_obj));

  JS_FreeValue(ctx, domain_obj);
  return 0;
}

// ============================================================================
// Cleanup and Finalizer
// ============================================================================

static void js_domain_finalizer(JSRuntime* rt, JSValue val) {
  JSDomain* domain = JS_GetOpaque(val, js_domain_class_id);
  if (!domain)
    return;

  JSContext* ctx = domain->ctx;
  JSRT_Debug("Finalizing domain: %p", domain);

  if (!JS_IsUndefined(domain->members)) {
    JS_FreeValue(ctx, domain->members);
  }

  if (!JS_IsUndefined(domain->error_handler)) {
    JS_FreeValue(ctx, domain->error_handler);
  }

  // Clear from current domain if needed
  if (current_domain == domain) {
    current_domain = NULL;
  }

  js_free(ctx, domain);
}

// Get current domain (for interop with other modules)
void* js_domain_get_current(void) {
  return current_domain;
}

// Emit error in domain context
void js_domain_emit_error(JSContext* ctx, JSValue error) {
  if (!current_domain || current_domain->disposed) {
    return;
  }

  // Call the error handler if it exists
  if (!JS_IsUndefined(current_domain->error_handler) && JS_IsFunction(ctx, current_domain->error_handler)) {
    JS_Call(ctx, current_domain->error_handler, JS_UNDEFINED, 1, &error);
    JSRT_Debug("Error handled in domain %p", current_domain);
  } else {
    JSRT_Debug("No error handler in domain %p", current_domain);
  }
}