#include "dom.h"

#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"

// DOMException name-to-code mapping according to Web IDL spec
typedef struct {
  const char* name;
  uint16_t code;
} DOMExceptionInfo;

static const DOMExceptionInfo dom_exception_names[] = {
    {"IndexSizeError", 1},
    {"DOMStringSizeError", 2},  // Deprecated but kept for compatibility
    {"HierarchyRequestError", 3},
    {"WrongDocumentError", 4},
    {"InvalidCharacterError", 5},
    {"NoDataAllowedError", 6},  // Deprecated
    {"NoModificationAllowedError", 7},
    {"NotFoundError", 8},
    {"NotSupportedError", 9},
    {"InUseAttributeError", 10},
    {"InvalidStateError", 11},
    {"SyntaxError", 12},
    {"InvalidModificationError", 13},
    {"NamespaceError", 14},
    {"InvalidAccessError", 15},
    {"ValidationError", 16},  // Deprecated
    {"TypeMismatchError", 17},
    {"SecurityError", 18},
    {"NetworkError", 19},
    {"AbortError", 20},
    {"URLMismatchError", 21},
    {"QuotaExceededError", 22},
    {"TimeoutError", 23},
    {"InvalidNodeTypeError", 24},
    {"DataCloneError", 25},
    // Modern exceptions without legacy codes
    {"EncodingError", 0},
    {"NotReadableError", 0},
    {"UnknownError", 0},
    {"ConstraintError", 0},
    {"DataError", 0},
    {"TransactionInactiveError", 0},
    {"ReadOnlyError", 0},
    {"VersionError", 0},
    {"OperationError", 0},
    {"NotAllowedError", 0},
    {NULL, 0}};

static uint16_t get_exception_code(const char* name) {
  for (int i = 0; dom_exception_names[i].name != NULL; i++) {
    if (strcmp(dom_exception_names[i].name, name) == 0) {
      return dom_exception_names[i].code;
    }
  }
  return 0;  // Default for unknown exceptions
}

static JSClassID JSRT_DOMExceptionClassID;

typedef struct {
  char* name;
  char* message;
  uint16_t code;
} JSRT_DOMException;

static void JSRT_DOMExceptionFinalize(JSRuntime* rt, JSValue val) {
  JSRT_DOMException* exception = JS_GetOpaque(val, JSRT_DOMExceptionClassID);
  if (exception) {
    free(exception->name);
    free(exception->message);
    free(exception);
  }
}

static JSClassDef JSRT_DOMExceptionClass = {
    .class_name = "DOMException",
    .finalizer = JSRT_DOMExceptionFinalize,
};

static JSValue JSRT_DOMExceptionConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  const char* message = "";
  const char* name = "Error";

  // Get message (optional first argument)
  if (argc >= 1 && !JS_IsUndefined(argv[0])) {
    message = JS_ToCString(ctx, argv[0]);
    if (!message) {
      return JS_EXCEPTION;
    }
  }

  // Get name (optional second argument)
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    const char* name_arg = JS_ToCString(ctx, argv[1]);
    if (!name_arg) {
      if (argc >= 1)
        JS_FreeCString(ctx, message);
      return JS_EXCEPTION;
    }
    name = name_arg;
  }

  JSRT_DOMException* exception = malloc(sizeof(JSRT_DOMException));
  exception->message = strdup(message ? message : "");
  exception->name = strdup(name ? name : "Error");
  exception->code = get_exception_code(exception->name);

  JSValue obj = JS_NewObjectClass(ctx, JSRT_DOMExceptionClassID);
  JS_SetOpaque(obj, exception);

  if (argc >= 1)
    JS_FreeCString(ctx, message);
  if (argc >= 2)
    JS_FreeCString(ctx, name);

  return obj;
}

static JSValue JSRT_DOMExceptionGetName(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_DOMException* exception = JS_GetOpaque2(ctx, this_val, JSRT_DOMExceptionClassID);
  if (!exception)
    return JS_EXCEPTION;
  return JS_NewString(ctx, exception->name);
}

static JSValue JSRT_DOMExceptionGetMessage(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_DOMException* exception = JS_GetOpaque2(ctx, this_val, JSRT_DOMExceptionClassID);
  if (!exception)
    return JS_EXCEPTION;
  return JS_NewString(ctx, exception->message);
}

static JSValue JSRT_DOMExceptionGetCode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_DOMException* exception = JS_GetOpaque2(ctx, this_val, JSRT_DOMExceptionClassID);
  if (!exception)
    return JS_EXCEPTION;
  return JS_NewUint32(ctx, exception->code);
}

void JSRT_RuntimeSetupStdDOM(JSRT_Runtime* rt) {
  JSContext* ctx = rt->ctx;

  JSRT_Debug("JSRT_RuntimeSetupStdDOM: initializing DOMException");

  // Register DOMException class
  JS_NewClassID(&JSRT_DOMExceptionClassID);
  JS_NewClass(rt->rt, JSRT_DOMExceptionClassID, &JSRT_DOMExceptionClass);

  JSValue dom_exception_proto = JS_NewObject(ctx);

  // Add property getters
  JSValue get_name = JS_NewCFunction(ctx, JSRT_DOMExceptionGetName, "get name", 0);
  JSValue get_message = JS_NewCFunction(ctx, JSRT_DOMExceptionGetMessage, "get message", 0);
  JSValue get_code = JS_NewCFunction(ctx, JSRT_DOMExceptionGetCode, "get code", 0);

  JSAtom name_atom = JS_NewAtom(ctx, "name");
  JSAtom message_atom = JS_NewAtom(ctx, "message");
  JSAtom code_atom = JS_NewAtom(ctx, "code");

  JS_DefinePropertyGetSet(ctx, dom_exception_proto, name_atom, get_name, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, dom_exception_proto, message_atom, get_message, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, dom_exception_proto, code_atom, get_code, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  JS_FreeAtom(ctx, name_atom);
  JS_FreeAtom(ctx, message_atom);
  JS_FreeAtom(ctx, code_atom);

  JS_SetClassProto(ctx, JSRT_DOMExceptionClassID, dom_exception_proto);

  JSValue dom_exception_ctor =
      JS_NewCFunction2(ctx, JSRT_DOMExceptionConstructor, "DOMException", 2, JS_CFUNC_constructor, 0);

  // Add legacy static constants to the DOMException constructor
  // These are required for WPT compliance and backwards compatibility
  JS_SetPropertyStr(ctx, dom_exception_ctor, "INDEX_SIZE_ERR", JS_NewInt32(ctx, 1));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "DOMSTRING_SIZE_ERR", JS_NewInt32(ctx, 2));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "HIERARCHY_REQUEST_ERR", JS_NewInt32(ctx, 3));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "WRONG_DOCUMENT_ERR", JS_NewInt32(ctx, 4));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "INVALID_CHARACTER_ERR", JS_NewInt32(ctx, 5));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "NO_DATA_ALLOWED_ERR", JS_NewInt32(ctx, 6));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "NO_MODIFICATION_ALLOWED_ERR", JS_NewInt32(ctx, 7));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "NOT_FOUND_ERR", JS_NewInt32(ctx, 8));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "NOT_SUPPORTED_ERR", JS_NewInt32(ctx, 9));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "INUSE_ATTRIBUTE_ERR", JS_NewInt32(ctx, 10));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "INVALID_STATE_ERR", JS_NewInt32(ctx, 11));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "SYNTAX_ERR", JS_NewInt32(ctx, 12));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "INVALID_MODIFICATION_ERR", JS_NewInt32(ctx, 13));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "NAMESPACE_ERR", JS_NewInt32(ctx, 14));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "INVALID_ACCESS_ERR", JS_NewInt32(ctx, 15));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "VALIDATION_ERR", JS_NewInt32(ctx, 16));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "TYPE_MISMATCH_ERR", JS_NewInt32(ctx, 17));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "SECURITY_ERR", JS_NewInt32(ctx, 18));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "NETWORK_ERR", JS_NewInt32(ctx, 19));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "ABORT_ERR", JS_NewInt32(ctx, 20));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "URL_MISMATCH_ERR", JS_NewInt32(ctx, 21));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "QUOTA_EXCEEDED_ERR", JS_NewInt32(ctx, 22));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "TIMEOUT_ERR", JS_NewInt32(ctx, 23));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "INVALID_NODE_TYPE_ERR", JS_NewInt32(ctx, 24));
  JS_SetPropertyStr(ctx, dom_exception_ctor, "DATA_CLONE_ERR", JS_NewInt32(ctx, 25));

  // Set the constructor's prototype property (non-enumerable for WPT compliance)
  JS_DefinePropertyValueStr(ctx, dom_exception_ctor, "prototype", JS_DupValue(ctx, dom_exception_proto),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  JS_SetPropertyStr(ctx, rt->global, "DOMException", dom_exception_ctor);

  JSRT_Debug("DOMException setup completed");
}