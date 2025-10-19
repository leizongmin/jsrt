#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include "dns_internal.h"

// Common implementation for both callback and promise versions
static JSValue dns_lookup_impl(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, bool use_promise) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "dns.lookup requires hostname");
  }

  const char* hostname = JS_ToCString(ctx, argv[0]);
  if (!hostname) {
    return JS_EXCEPTION;
  }

  // Parse options
  int family = 0;  // 0 = any, 4 = IPv4, 6 = IPv6
  bool all = false;
  int hints_flags = 0;
  bool verbatim = false;
  JSValue callback = JS_UNDEFINED;

  // Determine if second argument is options or callback
  int options_idx = -1;
  int callback_idx = -1;

  if (argc >= 2) {
    if (JS_IsObject(argv[1]) && !JS_IsFunction(ctx, argv[1])) {
      // Second arg is options
      options_idx = 1;
      if (argc >= 3 && !use_promise) {
        callback_idx = 2;
      }
    } else if (JS_IsFunction(ctx, argv[1]) && !use_promise) {
      // Second arg is callback
      callback_idx = 1;
    }
  }

  // Parse options object
  if (options_idx >= 0) {
    JSValue family_val = JS_GetPropertyStr(ctx, argv[options_idx], "family");
    if (!JS_IsUndefined(family_val)) {
      int32_t family_num;
      if (JS_ToInt32(ctx, &family_num, family_val) == 0) {
        family = family_num;
      }
    }
    JS_FreeValue(ctx, family_val);

    JSValue all_val = JS_GetPropertyStr(ctx, argv[options_idx], "all");
    all = JS_ToBool(ctx, all_val);
    JS_FreeValue(ctx, all_val);

    JSValue verbatim_val = JS_GetPropertyStr(ctx, argv[options_idx], "verbatim");
    verbatim = JS_ToBool(ctx, verbatim_val);
    JS_FreeValue(ctx, verbatim_val);

    JSValue hints_val = JS_GetPropertyStr(ctx, argv[options_idx], "hints");
    if (!JS_IsUndefined(hints_val)) {
      int32_t hints_num;
      if (JS_ToInt32(ctx, &hints_num, hints_val) == 0) {
        hints_flags = hints_num;
      }
    }
    JS_FreeValue(ctx, hints_val);
  }

  // Get callback if not using promise
  if (!use_promise) {
    if (callback_idx >= 0) {
      callback = JS_DupValue(ctx, argv[callback_idx]);
    } else {
      // No callback provided - automatically use promise mode for backward compatibility
      use_promise = true;
    }
  }

  // Allocate request structure
  DNSLookupRequest* req = js_malloc(ctx, sizeof(DNSLookupRequest));
  if (!req) {
    if (!use_promise) {
      JS_FreeValue(ctx, callback);
    }
    JS_FreeCString(ctx, hostname);
    return JS_EXCEPTION;
  }

  req->ctx = ctx;
  req->use_promise = use_promise;
  req->all = all;
  req->family = family;
  req->hints_flags = hints_flags;
  req->verbatim = verbatim;
  req->hostname = js_strdup(ctx, hostname);
  if (!req->hostname) {
    if (!use_promise) {
      JS_FreeValue(ctx, callback);
    }
    js_free(ctx, req);
    JS_FreeCString(ctx, hostname);
    return JS_EXCEPTION;
  }
  req->req.data = req;

  JSValue promise = JS_UNDEFINED;
  if (use_promise) {
    // Create promise
    promise = JS_NewPromiseCapability(ctx, req->promise_funcs);
    if (JS_IsException(promise)) {
      js_free(ctx, req->hostname);
      js_free(ctx, req);
      JS_FreeCString(ctx, hostname);
      return promise;
    }
    req->callback = JS_UNDEFINED;
  } else {
    req->callback = callback;
  }

  // Build addrinfo hints
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = (family == 4) ? AF_INET : (family == 6) ? AF_INET6 : AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = hints_flags;

  // Get event loop
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->uv_loop) {
    JSValue error = node_throw_error(ctx, NODE_ERR_SYSTEM_ERROR, "event loop not available");
    if (use_promise) {
      JSValue args[] = {error};
      JS_Call(ctx, req->promise_funcs[1], JS_UNDEFINED, 1, args);
      JS_FreeValue(ctx, req->promise_funcs[0]);
      JS_FreeValue(ctx, req->promise_funcs[1]);
      JS_FreeValue(ctx, error);
      js_free(ctx, req->hostname);
      js_free(ctx, req);
      JS_FreeCString(ctx, hostname);
      return promise;
    } else {
      JS_FreeValue(ctx, req->callback);
      js_free(ctx, req->hostname);
      js_free(ctx, req);
      JS_FreeCString(ctx, hostname);
      return error;
    }
  }

  // Start async DNS lookup
  int r = uv_getaddrinfo(rt->uv_loop, &req->req, on_getaddrinfo_callback, req->hostname, NULL, &hints);

  JS_FreeCString(ctx, hostname);

  if (r < 0) {
    // Error starting lookup
    JSValue error = create_dns_error(ctx, r, "getaddrinfo", req->hostname);

    if (use_promise) {
      JSValue args[] = {error};
      JS_Call(ctx, req->promise_funcs[1], JS_UNDEFINED, 1, args);
      JS_FreeValue(ctx, req->promise_funcs[0]);
      JS_FreeValue(ctx, req->promise_funcs[1]);
      JS_FreeValue(ctx, error);
      js_free(ctx, req->hostname);
      js_free(ctx, req);
      return promise;
    } else {
      JSValue args[] = {error};
      JS_Call(ctx, req->callback, JS_UNDEFINED, 1, args);
      JS_FreeValue(ctx, error);
      JS_FreeValue(ctx, req->callback);
      js_free(ctx, req->hostname);
      js_free(ctx, req);
      return JS_UNDEFINED;
    }
  }

  // Success - operation is in progress
  if (use_promise) {
    return promise;
  } else {
    return JS_UNDEFINED;
  }
}

// Callback version
JSValue js_dns_lookup(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return dns_lookup_impl(ctx, this_val, argc, argv, false);
}

// Promise version
JSValue js_dns_lookup_promise(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return dns_lookup_impl(ctx, this_val, argc, argv, true);
}
