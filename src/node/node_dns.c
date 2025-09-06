#include <stdlib.h>
#include <string.h>
#include "node_modules.h"

// Simplified DNS implementation - provides API compatibility without actual network operations
// This is suitable for testing and basic compatibility layer development

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
        family = family_num;
      }
    }
    JS_FreeValue(ctx, family_val);

    JSValue all_val = JS_GetPropertyStr(ctx, argv[1], "all");
    all = JS_ToBool(ctx, all_val);
    JS_FreeValue(ctx, all_val);
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

  // For testing/compatibility, return mock data immediately
  JSValue result;

  if (all) {
    // Return array of address objects
    result = JS_NewArray(ctx);
    JSValue addr_obj = JS_NewObject(ctx);

    if (family == 6) {
      JS_SetPropertyStr(ctx, addr_obj, "address", JS_NewString(ctx, "::1"));
      JS_SetPropertyStr(ctx, addr_obj, "family", JS_NewInt32(ctx, 6));
    } else {
      JS_SetPropertyStr(ctx, addr_obj, "address", JS_NewString(ctx, "127.0.0.1"));
      JS_SetPropertyStr(ctx, addr_obj, "family", JS_NewInt32(ctx, 4));
    }

    JS_SetPropertyUint32(ctx, result, 0, addr_obj);
  } else {
    // Return single address string
    if (family == 6) {
      result = JS_NewString(ctx, "::1");
    } else {
      result = JS_NewString(ctx, "127.0.0.1");
    }
  }

  // Resolve promise immediately
  JSValue args[] = {result};
  JS_Call(ctx, resolver, JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, result);
  JS_FreeValue(ctx, resolver);
  JS_FreeValue(ctx, rejecter);
  JS_FreeCString(ctx, hostname);

  return promise;
}

// dns.resolve(hostname[, rrtype], callback)
static JSValue js_dns_resolve(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "dns.resolve requires hostname");
  }

  // For now, just redirect to lookup for A records
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

  // Create promise that rejects with "not implemented"
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

  JSValue RRTYPE = JS_GetPropertyStr(ctx, dns_module, "RRTYPE");
  JS_SetModuleExport(ctx, m, "RRTYPE", RRTYPE);

  JS_FreeValue(ctx, dns_module);

  return 0;
}