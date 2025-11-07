#include "dns_internal.h"

// Stub implementations for resolve* methods
static JSValue js_dns_resolve_stub(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2 || !JS_IsFunction(ctx, argv[argc - 1])) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "callback required");
  }

  JSValue callback = argv[argc - 1];
  JSValue error = JS_NewError(ctx);
  JS_DefinePropertyValueStr(ctx, error, "code", JS_NewString(ctx, "ENOTIMPL"), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValueStr(ctx, error, "message",
                            JS_NewString(ctx, "DNS network queries not implemented - use dns.lookup() instead"),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  JSValue args[] = {error};
  JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
  JS_FreeValue(ctx, error);

  return JS_UNDEFINED;
}

// Promise stub for resolve* methods
static JSValue js_dns_resolve_promise_stub(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue promise_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, promise_funcs);
  if (JS_IsException(promise)) {
    return promise;
  }

  JSValue error = JS_NewError(ctx);
  JS_DefinePropertyValueStr(ctx, error, "code", JS_NewString(ctx, "ENOTIMPL"), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValueStr(ctx, error, "message",
                            JS_NewString(ctx, "DNS network queries not implemented - use dns.lookup() instead"),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  JSValue args[] = {error};
  JS_Call(ctx, promise_funcs[1], JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, error);
  JS_FreeValue(ctx, promise_funcs[0]);
  JS_FreeValue(ctx, promise_funcs[1]);

  return promise;
}

// Initialize node:dns module for CommonJS
JSValue JSRT_InitNodeDns(JSContext* ctx) {
  JSValue dns_obj = JS_NewObject(ctx);

  // Core DNS functions - implemented
  JS_DefinePropertyValueStr(ctx, dns_obj, "lookup", JS_NewCFunction(ctx, js_dns_lookup, "lookup", 3), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "lookupService",
                            JS_NewCFunction(ctx, js_dns_lookupservice, "lookupService", 3), JS_PROP_C_W_E);

  // Resolve methods - stubs (not implemented without c-ares)
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolve", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolve", 3),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolve4", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolve4", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolve6", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolve6", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolveMx", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolveMx", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolveTxt", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolveTxt", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolveCname", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolveCname", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolveNs", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolveNs", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolveSoa", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolveSoa", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolveSrv", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolveSrv", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolveNaptr", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolveNaptr", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolvePtr", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolvePtr", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "resolveCaa", JS_NewCFunction(ctx, js_dns_resolve_stub, "resolveCaa", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "reverse", JS_NewCFunction(ctx, js_dns_resolve_stub, "reverse", 2),
                            JS_PROP_C_W_E);

  // DNS record type constants
  JSValue RRTYPE = JS_NewObject(ctx);
  JS_DefinePropertyValueStr(ctx, RRTYPE, "A", JS_NewInt32(ctx, 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, RRTYPE, "AAAA", JS_NewInt32(ctx, 28), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, RRTYPE, "CNAME", JS_NewInt32(ctx, 5), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, RRTYPE, "MX", JS_NewInt32(ctx, 15), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, RRTYPE, "NS", JS_NewInt32(ctx, 2), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, RRTYPE, "PTR", JS_NewInt32(ctx, 12), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, RRTYPE, "SOA", JS_NewInt32(ctx, 6), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, RRTYPE, "TXT", JS_NewInt32(ctx, 16), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_obj, "RRTYPE", RRTYPE, JS_PROP_C_W_E);

  // Promises API
  JSValue promises = JS_NewObject(ctx);
  JS_DefinePropertyValueStr(ctx, promises, "lookup", JS_NewCFunction(ctx, js_dns_lookup_promise, "lookup", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "lookupService",
                            JS_NewCFunction(ctx, js_dns_lookupservice_promise, "lookupService", 2), JS_PROP_C_W_E);

  // Promise stubs for resolve methods
  JS_DefinePropertyValueStr(ctx, promises, "resolve", JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolve", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolve4", JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolve4", 1),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolve6", JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolve6", 1),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolveMx",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveMx", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolveTxt",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveTxt", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolveCname",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveCname", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolveNs",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveNs", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolveSoa",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveSoa", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolveSrv",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveSrv", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolveNaptr",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveNaptr", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolvePtr",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolvePtr", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "resolveCaa",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveCaa", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, promises, "reverse", JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "reverse", 1),
                            JS_PROP_C_W_E);

  JS_DefinePropertyValueStr(ctx, dns_obj, "promises", promises, JS_PROP_C_W_E);

  return dns_obj;
}

// Initialize node:dns module for ES modules
int js_node_dns_init(JSContext* ctx, JSModuleDef* m) {
  JSValue dns_module = JSRT_InitNodeDns(ctx);

  // Export as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, dns_module));

  // Export individual functions with proper memory management
  JSValue lookup = JS_GetPropertyStr(ctx, dns_module, "lookup");
  JS_SetModuleExport(ctx, m, "lookup", JS_DupValue(ctx, lookup));
  JS_FreeValue(ctx, lookup);

  JSValue lookupService = JS_GetPropertyStr(ctx, dns_module, "lookupService");
  JS_SetModuleExport(ctx, m, "lookupService", JS_DupValue(ctx, lookupService));
  JS_FreeValue(ctx, lookupService);

  JSValue resolve = JS_GetPropertyStr(ctx, dns_module, "resolve");
  JS_SetModuleExport(ctx, m, "resolve", JS_DupValue(ctx, resolve));
  JS_FreeValue(ctx, resolve);

  JSValue resolve4 = JS_GetPropertyStr(ctx, dns_module, "resolve4");
  JS_SetModuleExport(ctx, m, "resolve4", JS_DupValue(ctx, resolve4));
  JS_FreeValue(ctx, resolve4);

  JSValue resolve6 = JS_GetPropertyStr(ctx, dns_module, "resolve6");
  JS_SetModuleExport(ctx, m, "resolve6", JS_DupValue(ctx, resolve6));
  JS_FreeValue(ctx, resolve6);

  JSValue reverse = JS_GetPropertyStr(ctx, dns_module, "reverse");
  JS_SetModuleExport(ctx, m, "reverse", JS_DupValue(ctx, reverse));
  JS_FreeValue(ctx, reverse);

  JSValue RRTYPE = JS_GetPropertyStr(ctx, dns_module, "RRTYPE");
  JS_SetModuleExport(ctx, m, "RRTYPE", JS_DupValue(ctx, RRTYPE));
  JS_FreeValue(ctx, RRTYPE);

  JSValue promises = JS_GetPropertyStr(ctx, dns_module, "promises");
  JS_SetModuleExport(ctx, m, "promises", JS_DupValue(ctx, promises));
  JS_FreeValue(ctx, promises);

  JS_FreeValue(ctx, dns_module);

  return 0;
}

// dns/promises module initialization
JSValue JSRT_InitNodeDnsPromises(JSContext* ctx) {
  JSValue dns_promises = JS_NewObject(ctx);

  // Core DNS functions with promises - implemented
  JS_DefinePropertyValueStr(ctx, dns_promises, "lookup", JS_NewCFunction(ctx, js_dns_lookup_promise, "lookup", 2),
                            JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "lookupService",
                            JS_NewCFunction(ctx, js_dns_lookupservice_promise, "lookupService", 2), JS_PROP_C_W_E);

  // Promise stubs for resolve methods
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolve",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolve", 2), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolve4",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolve4", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolve6",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolve6", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolveMx",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveMx", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolveTxt",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveTxt", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolveCname",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveCname", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolveNs",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveNs", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolveSoa",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveSoa", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolveSrv",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveSrv", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolveNaptr",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveNaptr", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolvePtr",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolvePtr", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "resolveCaa",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "resolveCaa", 1), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, dns_promises, "reverse",
                            JS_NewCFunction(ctx, js_dns_resolve_promise_stub, "reverse", 1), JS_PROP_C_W_E);

  return dns_promises;
}

// dns/promises ES module initialization
int js_node_dns_promises_init(JSContext* ctx, JSModuleDef* m) {
  JSValue dns_promises = JSRT_InitNodeDnsPromises(ctx);

  // Export individual functions
  JSValue lookup = JS_GetPropertyStr(ctx, dns_promises, "lookup");
  JS_SetModuleExport(ctx, m, "lookup", JS_DupValue(ctx, lookup));
  JS_FreeValue(ctx, lookup);

  JSValue lookupService = JS_GetPropertyStr(ctx, dns_promises, "lookupService");
  JS_SetModuleExport(ctx, m, "lookupService", JS_DupValue(ctx, lookupService));
  JS_FreeValue(ctx, lookupService);

  JS_SetModuleExport(ctx, m, "default", dns_promises);

  return 0;
}
