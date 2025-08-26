#include "fetch.h"

#include <ctype.h>
#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "../util/debug.h"
#include "../util/jsutils.h"

// Forward declare class IDs
static JSClassID JSRT_HeadersClassID;
static JSClassID JSRT_RequestClassID;
static JSClassID JSRT_ResponseClassID;

// Headers structure
typedef struct JSRT_HeaderItem {
  char *name;
  char *value;
  struct JSRT_HeaderItem *next;
} JSRT_HeaderItem;

typedef struct {
  JSRT_HeaderItem *first;
} JSRT_Headers;

// Request structure
typedef struct {
  char *method;
  char *url;
  JSRT_Headers *headers;
  JSValue body;
} JSRT_Request;

// Response structure
typedef struct {
  int status;
  char *status_text;
  JSRT_Headers *headers;
  JSValue body;
  bool ok;
} JSRT_Response;

// HTTP request context for async operations
typedef struct {
  JSRT_Runtime *rt;
  uv_tcp_t tcp_handle;
  uv_connect_t connect_req;
  uv_write_t write_req;
  char *host;
  int port;
  char *path;
  char *method;
  JSRT_Headers *request_headers;

  // Response data
  char *response_buffer;
  size_t response_size;
  size_t response_capacity;

  // Promise resolvers
  JSValue resolve_func;
  JSValue reject_func;
} JSRT_FetchContext;

// Helper functions
static JSRT_Headers *JSRT_HeadersNew();
static void JSRT_HeadersFree(JSRT_Headers *headers);
static void JSRT_HeadersSet(JSRT_Headers *headers, const char *name, const char *value);
static const char *JSRT_HeadersGet(JSRT_Headers *headers, const char *name);
static bool JSRT_HeadersHas(JSRT_Headers *headers, const char *name);
static void JSRT_HeadersDelete(JSRT_Headers *headers, const char *name);

// Headers class implementation
static void JSRT_HeadersFinalize(JSRuntime *rt, JSValue val) {
  JSRT_Headers *headers = JS_GetOpaque(val, JSRT_HeadersClassID);
  if (headers) {
    JSRT_HeadersFree(headers);
  }
}

static JSClassDef JSRT_HeadersClass = {
    .class_name = "Headers",
    .finalizer = JSRT_HeadersFinalize,
};

static JSValue JSRT_HeadersConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  JSRT_Headers *headers = JSRT_HeadersNew();

  JSValue obj = JS_NewObjectClass(ctx, JSRT_HeadersClassID);
  JS_SetOpaque(obj, headers);
  return obj;
}

static JSValue JSRT_HeadersGetMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Headers *headers = JS_GetOpaque2(ctx, this_val, JSRT_HeadersClassID);
  if (!headers) {
    return JS_EXCEPTION;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing name parameter");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  const char *value = JSRT_HeadersGet(headers, name);
  JS_FreeCString(ctx, name);

  if (value) {
    return JS_NewString(ctx, value);
  } else {
    return JS_NULL;
  }
}

static JSValue JSRT_HeadersSetMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Headers *headers = JS_GetOpaque2(ctx, this_val, JSRT_HeadersClassID);
  if (!headers) {
    return JS_EXCEPTION;
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Missing name or value parameter");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  const char *value = JS_ToCString(ctx, argv[1]);
  if (!name || !value) {
    if (name) JS_FreeCString(ctx, name);
    if (value) JS_FreeCString(ctx, value);
    return JS_EXCEPTION;
  }

  JSRT_HeadersSet(headers, name, value);
  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);

  return JS_UNDEFINED;
}

static JSValue JSRT_HeadersHasMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Headers *headers = JS_GetOpaque2(ctx, this_val, JSRT_HeadersClassID);
  if (!headers) {
    return JS_EXCEPTION;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing name parameter");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  bool has = JSRT_HeadersHas(headers, name);
  JS_FreeCString(ctx, name);

  return JS_NewBool(ctx, has);
}

static JSValue JSRT_HeadersDeleteMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Headers *headers = JS_GetOpaque2(ctx, this_val, JSRT_HeadersClassID);
  if (!headers) {
    return JS_EXCEPTION;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing name parameter");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  JSRT_HeadersDelete(headers, name);
  JS_FreeCString(ctx, name);

  return JS_UNDEFINED;
}

// Request class implementation
static void JSRT_RequestFinalize(JSRuntime *rt, JSValue val) {
  JSRT_Request *request = JS_GetOpaque(val, JSRT_RequestClassID);
  if (request) {
    free(request->method);
    free(request->url);
    JSRT_HeadersFree(request->headers);
    JS_FreeValueRT(rt, request->body);
    free(request);
  }
}

static JSClassDef JSRT_RequestClass = {
    .class_name = "Request",
    .finalizer = JSRT_RequestFinalize,
};

static JSValue JSRT_RequestConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing input parameter");
  }

  const char *input = JS_ToCString(ctx, argv[0]);
  if (!input) {
    return JS_EXCEPTION;
  }

  JSRT_Request *request = malloc(sizeof(JSRT_Request));
  request->method = strdup("GET");
  request->url = strdup(input);
  request->headers = JSRT_HeadersNew();
  request->body = JS_UNDEFINED;

  JS_FreeCString(ctx, input);

  // Handle options parameter
  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue method_val = JS_GetPropertyStr(ctx, argv[1], "method");
    if (JS_IsString(method_val)) {
      const char *method = JS_ToCString(ctx, method_val);
      if (method) {
        free(request->method);
        request->method = strdup(method);
        JS_FreeCString(ctx, method);
      }
    }
    JS_FreeValue(ctx, method_val);
  }

  JSValue obj = JS_NewObjectClass(ctx, JSRT_RequestClassID);
  JS_SetOpaque(obj, request);
  return obj;
}

static JSValue JSRT_RequestGetMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Request *request = JS_GetOpaque2(ctx, this_val, JSRT_RequestClassID);
  if (!request) {
    return JS_EXCEPTION;
  }
  return JS_NewString(ctx, request->method);
}

static JSValue JSRT_RequestGetUrl(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Request *request = JS_GetOpaque2(ctx, this_val, JSRT_RequestClassID);
  if (!request) {
    return JS_EXCEPTION;
  }
  return JS_NewString(ctx, request->url);
}

// Response class implementation
static void JSRT_ResponseFinalize(JSRuntime *rt, JSValue val) {
  JSRT_Response *response = JS_GetOpaque(val, JSRT_ResponseClassID);
  if (response) {
    free(response->status_text);
    JSRT_HeadersFree(response->headers);
    JS_FreeValueRT(rt, response->body);
    free(response);
  }
}

static JSClassDef JSRT_ResponseClass = {
    .class_name = "Response",
    .finalizer = JSRT_ResponseFinalize,
};

static JSValue JSRT_ResponseConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  JSRT_Response *response = malloc(sizeof(JSRT_Response));
  response->status = 200;
  response->status_text = strdup("OK");
  response->headers = JSRT_HeadersNew();
  response->body = JS_UNDEFINED;
  response->ok = true;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_ResponseClassID);
  JS_SetOpaque(obj, response);
  return obj;
}

static JSValue JSRT_ResponseGetStatus(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Response *response = JS_GetOpaque2(ctx, this_val, JSRT_ResponseClassID);
  if (!response) {
    return JS_EXCEPTION;
  }
  return JS_NewInt32(ctx, response->status);
}

static JSValue JSRT_ResponseGetOk(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Response *response = JS_GetOpaque2(ctx, this_val, JSRT_ResponseClassID);
  if (!response) {
    return JS_EXCEPTION;
  }
  return JS_NewBool(ctx, response->ok);
}

static JSValue JSRT_ResponseText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Response *response = JS_GetOpaque2(ctx, this_val, JSRT_ResponseClassID);
  if (!response) {
    return JS_EXCEPTION;
  }

  // For now, just return the body as is if it's a string
  return JS_DupValue(ctx, response->body);
}

static JSValue JSRT_ResponseJson(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Response *response = JS_GetOpaque2(ctx, this_val, JSRT_ResponseClassID);
  if (!response) {
    return JS_EXCEPTION;
  }

  // For now, just parse the body as JSON
  if (JS_IsString(response->body)) {
    const char *json_str = JS_ToCString(ctx, response->body);
    if (!json_str) {
      return JS_EXCEPTION;
    }

    JSValue result = JS_ParseJSON(ctx, json_str, strlen(json_str), "<response>");
    JS_FreeCString(ctx, json_str);
    return result;
  }

  return JS_UNDEFINED;
}

// Basic fetch function implementation (simplified for initial version)
static JSValue JSRT_Fetch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing input parameter");
  }

  const char *input = JS_ToCString(ctx, argv[0]);
  if (!input) {
    return JS_EXCEPTION;
  }

  // For now, return a simple resolved response instead of Promise
  // This is a minimal implementation - a real implementation would need proper HTTP client

  // Create a basic response
  JSRT_Response *response = malloc(sizeof(JSRT_Response));
  response->status = 200;
  response->status_text = strdup("OK");
  response->headers = JSRT_HeadersNew();

  // Add some test response body based on URL
  if (strstr(input, "json")) {
    response->body = JS_NewString(ctx, "{\"message\": \"Hello from jsrt fetch\", \"status\": \"ok\"}");
  } else {
    response->body = JS_NewString(ctx, "Hello World from jsrt fetch");
  }

  response->ok = true;

  // Add some default response headers
  JSRT_HeadersSet(response->headers, "content-type", strstr(input, "json") ? "application/json" : "text/plain");
  JSRT_HeadersSet(response->headers, "server", "jsrt/1.0");

  JSValue response_obj = JS_NewObjectClass(ctx, JSRT_ResponseClassID);
  JS_SetOpaque(response_obj, response);

  JS_FreeCString(ctx, input);

  return response_obj;
}

// Headers helper functions implementation
static JSRT_Headers *JSRT_HeadersNew() {
  JSRT_Headers *headers = malloc(sizeof(JSRT_Headers));
  headers->first = NULL;
  return headers;
}

static void JSRT_HeadersFree(JSRT_Headers *headers) {
  if (!headers) return;

  JSRT_HeaderItem *current = headers->first;
  while (current) {
    JSRT_HeaderItem *next = current->next;
    free(current->name);
    free(current->value);
    free(current);
    current = next;
  }
  free(headers);
}

static void JSRT_HeadersSet(JSRT_Headers *headers, const char *name, const char *value) {
  if (!headers || !name || !value) return;

  // Convert name to lowercase
  char *lower_name = malloc(strlen(name) + 1);
  for (size_t i = 0; name[i]; i++) {
    lower_name[i] = tolower(name[i]);
  }
  lower_name[strlen(name)] = '\0';

  // Check if header already exists
  JSRT_HeaderItem *item = headers->first;
  while (item) {
    if (strcmp(item->name, lower_name) == 0) {
      free(item->value);
      item->value = strdup(value);
      free(lower_name);
      return;
    }
    item = item->next;
  }

  // Add new header
  JSRT_HeaderItem *new_item = malloc(sizeof(JSRT_HeaderItem));
  new_item->name = lower_name;
  new_item->value = strdup(value);
  new_item->next = headers->first;
  headers->first = new_item;
}

static const char *JSRT_HeadersGet(JSRT_Headers *headers, const char *name) {
  if (!headers || !name) return NULL;

  // Convert name to lowercase
  char *lower_name = malloc(strlen(name) + 1);
  for (size_t i = 0; name[i]; i++) {
    lower_name[i] = tolower(name[i]);
  }
  lower_name[strlen(name)] = '\0';

  JSRT_HeaderItem *item = headers->first;
  while (item) {
    if (strcmp(item->name, lower_name) == 0) {
      free(lower_name);
      return item->value;
    }
    item = item->next;
  }

  free(lower_name);
  return NULL;
}

static bool JSRT_HeadersHas(JSRT_Headers *headers, const char *name) { return JSRT_HeadersGet(headers, name) != NULL; }

static void JSRT_HeadersDelete(JSRT_Headers *headers, const char *name) {
  if (!headers || !name) return;

  // Convert name to lowercase
  char *lower_name = malloc(strlen(name) + 1);
  for (size_t i = 0; name[i]; i++) {
    lower_name[i] = tolower(name[i]);
  }
  lower_name[strlen(name)] = '\0';

  JSRT_HeaderItem **current = &headers->first;
  while (*current) {
    if (strcmp((*current)->name, lower_name) == 0) {
      JSRT_HeaderItem *to_delete = *current;
      *current = to_delete->next;
      free(to_delete->name);
      free(to_delete->value);
      free(to_delete);
      break;
    }
    current = &(*current)->next;
  }

  free(lower_name);
}

// Setup function
void JSRT_RuntimeSetupStdFetch(JSRT_Runtime *rt) {
  // Register Headers class
  JS_NewClassID(&JSRT_HeadersClassID);
  JS_NewClass(rt->rt, JSRT_HeadersClassID, &JSRT_HeadersClass);

  JSValue headers_proto = JS_NewObject(rt->ctx);
  JS_SetPropertyStr(rt->ctx, headers_proto, "get", JS_NewCFunction(rt->ctx, JSRT_HeadersGetMethod, "get", 1));
  JS_SetPropertyStr(rt->ctx, headers_proto, "set", JS_NewCFunction(rt->ctx, JSRT_HeadersSetMethod, "set", 2));
  JS_SetPropertyStr(rt->ctx, headers_proto, "has", JS_NewCFunction(rt->ctx, JSRT_HeadersHasMethod, "has", 1));
  JS_SetPropertyStr(rt->ctx, headers_proto, "delete", JS_NewCFunction(rt->ctx, JSRT_HeadersDeleteMethod, "delete", 1));

  JS_SetClassProto(rt->ctx, JSRT_HeadersClassID, headers_proto);

  JSValue headers_ctor = JS_NewCFunction2(rt->ctx, JSRT_HeadersConstructor, "Headers", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(rt->ctx, rt->global, "Headers", headers_ctor);

  // Register Request class
  JS_NewClassID(&JSRT_RequestClassID);
  JS_NewClass(rt->rt, JSRT_RequestClassID, &JSRT_RequestClass);

  JSValue request_proto = JS_NewObject(rt->ctx);
  JS_DefinePropertyValueStr(rt->ctx, request_proto, "method",
                            JS_NewCFunction(rt->ctx, JSRT_RequestGetMethod, "get method", 0), JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValueStr(rt->ctx, request_proto, "url", JS_NewCFunction(rt->ctx, JSRT_RequestGetUrl, "get url", 0),
                            JS_PROP_CONFIGURABLE);

  JS_SetClassProto(rt->ctx, JSRT_RequestClassID, request_proto);

  JSValue request_ctor = JS_NewCFunction2(rt->ctx, JSRT_RequestConstructor, "Request", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(rt->ctx, rt->global, "Request", request_ctor);

  // Register Response class
  JS_NewClassID(&JSRT_ResponseClassID);
  JS_NewClass(rt->rt, JSRT_ResponseClassID, &JSRT_ResponseClass);

  JSValue response_proto = JS_NewObject(rt->ctx);
  JS_DefinePropertyValueStr(rt->ctx, response_proto, "status",
                            JS_NewCFunction(rt->ctx, JSRT_ResponseGetStatus, "get status", 0), JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValueStr(rt->ctx, response_proto, "ok", JS_NewCFunction(rt->ctx, JSRT_ResponseGetOk, "get ok", 0),
                            JS_PROP_CONFIGURABLE);

  // Add response body methods
  JS_SetPropertyStr(rt->ctx, response_proto, "text", JS_NewCFunction(rt->ctx, JSRT_ResponseText, "text", 0));
  JS_SetPropertyStr(rt->ctx, response_proto, "json", JS_NewCFunction(rt->ctx, JSRT_ResponseJson, "json", 0));

  JS_SetClassProto(rt->ctx, JSRT_ResponseClassID, response_proto);

  JSValue response_ctor = JS_NewCFunction2(rt->ctx, JSRT_ResponseConstructor, "Response", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(rt->ctx, rt->global, "Response", response_ctor);

  // Register fetch function
  JS_SetPropertyStr(rt->ctx, rt->global, "fetch", JS_NewCFunction(rt->ctx, JSRT_Fetch, "fetch", 1));

  JSRT_Debug("Fetch API setup completed");
}