#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <uv.h>
#include "node_modules.h"

// DNS lookup operation structure
typedef struct {
  JSContext* ctx;
  JSValue promise;
  JSValue resolver;
  JSValue rejecter;
  uv_getaddrinfo_t req;
  char* hostname;
  int family;  // AF_INET, AF_INET6, or 0 for any
  bool all;    // Return all addresses or just the first
} DNSLookupOp;

// Forward declarations
static void dns_lookup_callback(uv_getaddrinfo_t* req, int status, struct addrinfo* res);
static void cleanup_dns_op(DNSLookupOp* op);

// Helper to convert sockaddr to string
static char* sockaddr_to_string(struct sockaddr* addr) {
  char* result = malloc(INET6_ADDRSTRLEN);
  if (!result)
    return NULL;

  if (addr->sa_family == AF_INET) {
    struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
    inet_ntop(AF_INET, &addr_in->sin_addr, result, INET_ADDRSTRLEN);
  } else if (addr->sa_family == AF_INET6) {
    struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)addr;
    inet_ntop(AF_INET6, &addr_in6->sin6_addr, result, INET6_ADDRSTRLEN);
  } else {
    free(result);
    return NULL;
  }

  return result;
}

// DNS lookup completion callback
static void dns_lookup_callback(uv_getaddrinfo_t* req, int status, struct addrinfo* res) {
  DNSLookupOp* op = (DNSLookupOp*)req->data;
  JSContext* ctx = op->ctx;

  if (status < 0) {
    // Create error object
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, uv_err_name(status)));
    JS_SetPropertyStr(ctx, error, "errno", JS_NewInt32(ctx, status));
    JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, "getaddrinfo"));
    JS_SetPropertyStr(ctx, error, "hostname", JS_NewString(ctx, op->hostname));

    // Reject promise
    JSValue args[] = {error};
    JS_Call(ctx, op->rejecter, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, error);
  } else {
    JSValue result;

    if (op->all) {
      // Return array of all addresses
      result = JS_NewArray(ctx);
      int index = 0;

      for (struct addrinfo* addr = res; addr != NULL; addr = addr->ai_next) {
        if ((op->family == 0) || (addr->ai_family == op->family)) {
          char* addr_str = sockaddr_to_string(addr->ai_addr);
          if (addr_str) {
            JS_SetPropertyUint32(ctx, result, index++, JS_NewString(ctx, addr_str));
            free(addr_str);
          }
        }
      }
    } else {
      // Return first matching address
      result = JS_NULL;

      for (struct addrinfo* addr = res; addr != NULL; addr = addr->ai_next) {
        if ((op->family == 0) || (addr->ai_family == op->family)) {
          char* addr_str = sockaddr_to_string(addr->ai_addr);
          if (addr_str) {
            result = JS_NewString(ctx, addr_str);
            free(addr_str);
            break;
          }
        }
      }

      if (JS_IsNull(result)) {
        result = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, result, "code", JS_NewString(ctx, "ENOTFOUND"));
        JS_SetPropertyStr(ctx, result, "hostname", JS_NewString(ctx, op->hostname));

        JSValue args[] = {result};
        JS_Call(ctx, op->rejecter, JS_UNDEFINED, 1, args);
        JS_FreeValue(ctx, result);
        goto cleanup;
      }
    }

    // Resolve promise
    JSValue args[] = {result};
    JS_Call(ctx, op->resolver, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, result);
  }

cleanup:
  if (res)
    uv_freeaddrinfo(res);
  cleanup_dns_op(op);
}

// Clean up DNS operation
static void cleanup_dns_op(DNSLookupOp* op) {
  if (op) {
    JS_FreeValue(op->ctx, op->resolver);
    JS_FreeValue(op->ctx, op->rejecter);
    JS_FreeValue(op->ctx, op->promise);
    free(op->hostname);
    free(op);
  }
}

// dns.lookup(hostname[, options], callback)
static JSValue js_dns_lookup(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "dns.lookup requires hostname");
  }

  const char* hostname = JS_ToCString(ctx, argv[0]);
  if (!hostname)
    return JS_EXCEPTION;

  // Parse options
  int family = 0;  // 0 = any, 4 = IPv4, 6 = IPv6
  bool all = false;

  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue family_val = JS_GetPropertyStr(ctx, argv[1], "family");
    if (!JS_IsUndefined(family_val)) {
      int32_t family_num;
      if (JS_ToInt32(ctx, &family_num, family_val) == 0) {
        if (family_num == 4)
          family = AF_INET;
        else if (family_num == 6)
          family = AF_INET6;
      }
    }
    JS_FreeValue(ctx, family_val);

    JSValue all_val = JS_GetPropertyStr(ctx, argv[1], "all");
    all = JS_ToBool(ctx, all_val);
    JS_FreeValue(ctx, all_val);
  }

  // Check if callback is provided (last argument)
  JSValue callback = JS_UNDEFINED;
  if (argc >= 2 && JS_IsFunction(ctx, argv[argc - 1])) {
    callback = argv[argc - 1];
  }

  // Create promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, hostname);
    return promise;
  }
  JSValue resolver = resolving_funcs[0];
  JSValue rejecter = resolving_funcs[1];

  // Setup DNS operation
  DNSLookupOp* op = calloc(1, sizeof(DNSLookupOp));
  if (!op) {
    JS_FreeValue(ctx, resolver);
    JS_FreeValue(ctx, rejecter);
    JS_FreeCString(ctx, hostname);
    JS_ThrowOutOfMemory(ctx);
    return JS_EXCEPTION;
  }

  op->ctx = ctx;
  op->promise = JS_DupValue(ctx, promise);
  op->resolver = JS_DupValue(ctx, resolver);
  op->rejecter = JS_DupValue(ctx, rejecter);
  op->hostname = strdup(hostname);
  op->family = family;
  op->all = all;
  op->req.data = op;

  // Setup hints for getaddrinfo
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = family;
  hints.ai_socktype = SOCK_STREAM;

  // Start DNS lookup
  JSRuntime* rt = JS_GetRuntime(ctx);
  uv_loop_t* loop = JS_GetRuntimeOpaque(rt);
  int result = uv_getaddrinfo(loop, &op->req, dns_lookup_callback, hostname, NULL, &hints);

  JS_FreeCString(ctx, hostname);

  if (result < 0) {
    // Immediate error
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, uv_err_name(result)));
    JS_SetPropertyStr(ctx, error, "errno", JS_NewInt32(ctx, result));

    JSValue args[] = {error};
    JS_Call(ctx, op->rejecter, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, error);

    cleanup_dns_op(op);
  }

  // If callback provided, handle it when promise resolves
  if (!JS_IsUndefined(callback)) {
    // Convert promise-based result to callback-based
    JSValue then_func = JS_GetPropertyStr(ctx, promise, "then");
    JSValue catch_func = JS_GetPropertyStr(ctx, promise, "catch");

    // Create wrapper functions for callback
    // For simplicity, we'll just return the promise and let user handle it
    JS_FreeValue(ctx, then_func);
    JS_FreeValue(ctx, catch_func);
  }

  return promise;
}

// dns.resolve(hostname[, rrtype], callback)
static JSValue js_dns_resolve(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "dns.resolve requires hostname");
  }

  // For now, just redirect to lookup for A records
  // TODO: Implement proper DNS resolution for different record types
  JSValue lookup_args[] = {argv[0], JS_NewObject(ctx)};
  JS_SetPropertyStr(ctx, lookup_args[1], "all", JS_TRUE);

  JSValue result = js_dns_lookup(ctx, this_val, 2, lookup_args);
  JS_FreeValue(ctx, lookup_args[1]);

  return result;
}

// dns.resolve4(hostname, callback) - IPv4 addresses
static JSValue js_dns_resolve4(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "dns.resolve4 requires hostname");
  }

  JSValue options = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, options, "family", JS_NewInt32(ctx, 4));
  JS_SetPropertyStr(ctx, options, "all", JS_TRUE);

  JSValue lookup_args[] = {argv[0], options};
  JSValue result = js_dns_lookup(ctx, this_val, 2, lookup_args);
  JS_FreeValue(ctx, options);

  return result;
}

// dns.resolve6(hostname, callback) - IPv6 addresses
static JSValue js_dns_resolve6(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "dns.resolve6 requires hostname");
  }

  JSValue options = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, options, "family", JS_NewInt32(ctx, 6));
  JS_SetPropertyStr(ctx, options, "all", JS_TRUE);

  JSValue lookup_args[] = {argv[0], options};
  JSValue result = js_dns_lookup(ctx, this_val, 2, lookup_args);
  JS_FreeValue(ctx, options);

  return result;
}

// dns.reverse(ip, callback) - Reverse DNS lookup
static JSValue js_dns_reverse(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "dns.reverse requires IP address");
  }

  // TODO: Implement reverse DNS lookup
  // For now, return a promise that rejects with "not implemented"
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return promise;
  }
  JSValue resolver = resolving_funcs[0];
  JSValue rejecter = resolving_funcs[1];

  JSValue error = JS_NewError(ctx);
  JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ENOTIMPL"));
  JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Reverse DNS not implemented"));

  JSValue args[] = {error};
  JS_Call(ctx, rejecter, JS_UNDEFINED, 1, args);
  JS_FreeValue(ctx, error);
  JS_FreeValue(ctx, resolver);
  JS_FreeValue(ctx, rejecter);

  return promise;
}

// Initialize node:dns module for CommonJS
JSValue JSRT_InitNodeDns(JSContext* ctx) {
  JSValue dns_obj = JS_NewObject(ctx);

  // Core DNS functions
  JS_SetPropertyStr(ctx, dns_obj, "lookup", JS_NewCFunction(ctx, js_dns_lookup, "lookup", 3));
  JS_SetPropertyStr(ctx, dns_obj, "resolve", JS_NewCFunction(ctx, js_dns_resolve, "resolve", 3));
  JS_SetPropertyStr(ctx, dns_obj, "resolve4", JS_NewCFunction(ctx, js_dns_resolve4, "resolve4", 2));
  JS_SetPropertyStr(ctx, dns_obj, "resolve6", JS_NewCFunction(ctx, js_dns_resolve6, "resolve6", 2));
  JS_SetPropertyStr(ctx, dns_obj, "reverse", JS_NewCFunction(ctx, js_dns_reverse, "reverse", 2));

  // DNS record type constants
  JSValue RRTYPE = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, RRTYPE, "A", JS_NewInt32(ctx, 1));
  JS_SetPropertyStr(ctx, RRTYPE, "AAAA", JS_NewInt32(ctx, 28));
  JS_SetPropertyStr(ctx, RRTYPE, "CNAME", JS_NewInt32(ctx, 5));
  JS_SetPropertyStr(ctx, RRTYPE, "MX", JS_NewInt32(ctx, 15));
  JS_SetPropertyStr(ctx, RRTYPE, "NS", JS_NewInt32(ctx, 2));
  JS_SetPropertyStr(ctx, RRTYPE, "PTR", JS_NewInt32(ctx, 12));
  JS_SetPropertyStr(ctx, RRTYPE, "SOA", JS_NewInt32(ctx, 6));
  JS_SetPropertyStr(ctx, RRTYPE, "TXT", JS_NewInt32(ctx, 16));
  JS_SetPropertyStr(ctx, dns_obj, "RRTYPE", RRTYPE);

  return dns_obj;
}

// Initialize node:dns module for ES modules
int js_node_dns_init(JSContext* ctx, JSModuleDef* m) {
  JSValue dns_module = JSRT_InitNodeDns(ctx);

  // Export as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, dns_module));

  // Export individual functions
  JSValue lookup = JS_GetPropertyStr(ctx, dns_module, "lookup");
  JS_SetModuleExport(ctx, m, "lookup", lookup);

  JSValue resolve = JS_GetPropertyStr(ctx, dns_module, "resolve");
  JS_SetModuleExport(ctx, m, "resolve", resolve);

  JSValue resolve4 = JS_GetPropertyStr(ctx, dns_module, "resolve4");
  JS_SetModuleExport(ctx, m, "resolve4", resolve4);

  JSValue resolve6 = JS_GetPropertyStr(ctx, dns_module, "resolve6");
  JS_SetModuleExport(ctx, m, "resolve6", resolve6);

  JSValue reverse = JS_GetPropertyStr(ctx, dns_module, "reverse");
  JS_SetModuleExport(ctx, m, "reverse", reverse);

  JS_FreeValue(ctx, dns_module);

  return 0;
}