#include <arpa/inet.h>
#include "dns_internal.h"

// Common implementation for both callback and promise versions
static JSValue dns_lookupservice_impl(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                                      bool use_promise) {
  if (argc < 2) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "dns.lookupService requires address and port");
  }

  const char* address = JS_ToCString(ctx, argv[0]);
  if (!address) {
    return JS_EXCEPTION;
  }

  int32_t port;
  if (JS_ToInt32(ctx, &port, argv[1]) != 0) {
    JS_FreeCString(ctx, address);
    return JS_EXCEPTION;
  }

  // Validate port range
  if (port < 0 || port > 65535) {
    JS_FreeCString(ctx, address);
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_VALUE, "port must be between 0 and 65535");
  }

  // Get callback if not using promise
  JSValue callback = JS_UNDEFINED;
  if (!use_promise) {
    if (argc < 3 || !JS_IsFunction(ctx, argv[2])) {
      JS_FreeCString(ctx, address);
      return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "dns.lookupService requires callback");
    }
    callback = JS_DupValue(ctx, argv[2]);
  }

  // Allocate request structure
  DNSLookupServiceRequest* req = js_malloc(ctx, sizeof(DNSLookupServiceRequest));
  if (!req) {
    if (!use_promise) {
      JS_FreeValue(ctx, callback);
    }
    JS_FreeCString(ctx, address);
    return JS_EXCEPTION;
  }

  req->ctx = ctx;
  req->use_promise = use_promise;
  req->address = js_strdup(ctx, address);
  req->port = port;
  req->req.data = req;

  if (use_promise) {
    // Create promise
    JSValue promise = JS_NewPromiseCapability(ctx, req->promise_funcs);
    if (JS_IsException(promise)) {
      js_free(ctx, req->address);
      js_free(ctx, req);
      JS_FreeCString(ctx, address);
      return promise;
    }
    req->callback = JS_UNDEFINED;
    req->promise_funcs[0] = JS_DupValue(ctx, req->promise_funcs[0]);
    req->promise_funcs[1] = JS_DupValue(ctx, req->promise_funcs[1]);
  } else {
    req->callback = callback;
  }

  // Convert address+port to sockaddr
  struct sockaddr_storage addr_storage;
  memset(&addr_storage, 0, sizeof(addr_storage));

  // Try IPv4 first
  struct sockaddr_in* addr4 = (struct sockaddr_in*)&addr_storage;
  if (inet_pton(AF_INET, address, &addr4->sin_addr) == 1) {
    addr4->sin_family = AF_INET;
    addr4->sin_port = htons(port);
  } else {
    // Try IPv6
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&addr_storage;
    if (inet_pton(AF_INET6, address, &addr6->sin6_addr) == 1) {
      addr6->sin6_family = AF_INET6;
      addr6->sin6_port = htons(port);
    } else {
      // Invalid address format
      JSValue error = node_throw_error(ctx, NODE_ERR_INVALID_ARG_VALUE, "invalid IP address");

      if (use_promise) {
        JSValue args[] = {error};
        JS_Call(ctx, req->promise_funcs[1], JS_UNDEFINED, 1, args);
        JS_FreeValue(ctx, req->promise_funcs[0]);
        JS_FreeValue(ctx, req->promise_funcs[1]);
      } else {
        JS_FreeValue(ctx, req->callback);
      }

      js_free(ctx, req->address);
      js_free(ctx, req);
      JS_FreeCString(ctx, address);
      return error;
    }
  }

  JS_FreeCString(ctx, address);

  // Get event loop
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->uv_loop) {
    JSValue error = node_throw_error(ctx, NODE_ERR_SYSTEM_ERROR, "event loop not available");
    if (use_promise) {
      JSValue args[] = {error};
      JS_Call(ctx, req->promise_funcs[1], JS_UNDEFINED, 1, args);
      JS_FreeValue(ctx, req->promise_funcs[0]);
      JS_FreeValue(ctx, req->promise_funcs[1]);
    } else {
      JS_FreeValue(ctx, req->callback);
    }
    js_free(ctx, req->address);
    js_free(ctx, req);
    return error;
  }

  // Start async reverse lookup
  int r = uv_getnameinfo(rt->uv_loop, &req->req, on_getnameinfo_callback, (struct sockaddr*)&addr_storage, 0);

  if (r < 0) {
    // Error starting lookup
    JSValue error = create_dns_error(ctx, r, "getnameinfo", req->address);

    if (use_promise) {
      JSValue args[] = {error};
      JS_Call(ctx, req->promise_funcs[1], JS_UNDEFINED, 1, args);
      JS_FreeValue(ctx, req->promise_funcs[0]);
      JS_FreeValue(ctx, req->promise_funcs[1]);
      JSValue promise_copy = JS_DupValue(ctx, req->promise_funcs[1]);
      JS_FreeValue(ctx, error);
      js_free(ctx, req->address);
      js_free(ctx, req);
      return promise_copy;
    } else {
      JSValue args[] = {error};
      JS_Call(ctx, req->callback, JS_UNDEFINED, 1, args);
      JS_FreeValue(ctx, error);
      JS_FreeValue(ctx, req->callback);
      js_free(ctx, req->address);
      js_free(ctx, req);
      return JS_UNDEFINED;
    }
  }

  // Success - operation is in progress
  if (use_promise) {
    JSValue promise_funcs_copy[2];
    promise_funcs_copy[0] = req->promise_funcs[0];
    promise_funcs_copy[1] = req->promise_funcs[1];
    JSValue promise = JS_NewPromiseCapability(ctx, promise_funcs_copy);
    return promise;
  } else {
    return JS_UNDEFINED;
  }
}

// Callback version
JSValue js_dns_lookupservice(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return dns_lookupservice_impl(ctx, this_val, argc, argv, false);
}

// Promise version
JSValue js_dns_lookupservice_promise(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return dns_lookupservice_impl(ctx, this_val, argc, argv, true);
}
