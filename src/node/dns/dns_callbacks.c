#include <arpa/inet.h>
#include "dns_internal.h"

// Convert addrinfo result to JavaScript value
static JSValue convert_addrinfo_to_js(JSContext* ctx, struct addrinfo* res, bool all, int* family_out) {
  if (all) {
    // Return array of address objects
    JSValue array = JS_NewArray(ctx);
    uint32_t index = 0;

    for (struct addrinfo* ai = res; ai != NULL; ai = ai->ai_next) {
      JSValue addr_obj = JS_NewObject(ctx);
      char addr_str[INET6_ADDRSTRLEN];
      int family = 0;

      if (ai->ai_family == AF_INET) {
        struct sockaddr_in* sa = (struct sockaddr_in*)ai->ai_addr;
        inet_ntop(AF_INET, &sa->sin_addr, addr_str, sizeof(addr_str));
        family = 4;
      } else if (ai->ai_family == AF_INET6) {
        struct sockaddr_in6* sa = (struct sockaddr_in6*)ai->ai_addr;
        inet_ntop(AF_INET6, &sa->sin6_addr, addr_str, sizeof(addr_str));
        family = 6;
      } else {
        continue;  // Skip unknown families
      }

      JS_DefinePropertyValueStr(ctx, addr_obj, "address", JS_NewString(ctx, addr_str), JS_PROP_C_W_E);
      JS_DefinePropertyValueStr(ctx, addr_obj, "family", JS_NewInt32(ctx, family), JS_PROP_C_W_E);
      JS_DefinePropertyValueUint32(ctx, array, index++, addr_obj, JS_PROP_C_W_E);
    }

    return array;
  } else {
    // Return first address as string and family
    char addr_str[INET6_ADDRSTRLEN];

    if (res->ai_family == AF_INET) {
      struct sockaddr_in* sa = (struct sockaddr_in*)res->ai_addr;
      inet_ntop(AF_INET, &sa->sin_addr, addr_str, sizeof(addr_str));
      *family_out = 4;
    } else if (res->ai_family == AF_INET6) {
      struct sockaddr_in6* sa = (struct sockaddr_in6*)res->ai_addr;
      inet_ntop(AF_INET6, &sa->sin6_addr, addr_str, sizeof(addr_str));
      *family_out = 6;
    } else {
      *family_out = 0;
      return JS_NULL;
    }

    return JS_NewString(ctx, addr_str);
  }
}

// Callback for getaddrinfo (dns.lookup)
void on_getaddrinfo_callback(uv_getaddrinfo_t* req, int status, struct addrinfo* res) {
  DNSLookupRequest* dns_req = (DNSLookupRequest*)req;
  JSContext* ctx = dns_req->ctx;

  if (status != 0) {
    // Error occurred
    JSValue error = create_dns_error(ctx, status, "getaddrinfo", dns_req->hostname);

    if (dns_req->use_promise) {
      // Reject promise
      JSValue args[] = {error};
      JS_Call(ctx, dns_req->promise_funcs[1], JS_UNDEFINED, 1, args);
    } else {
      // Call callback with error
      JSValue args[] = {error};
      JS_Call(ctx, dns_req->callback, JS_UNDEFINED, 1, args);
    }

    JS_FreeValue(ctx, error);
  } else {
    // Success - convert result
    int family = 0;
    JSValue result = convert_addrinfo_to_js(ctx, res, dns_req->all, &family);

    if (dns_req->use_promise) {
      // Resolve promise with object {address, family} or array
      JSValue promise_result;
      if (dns_req->all) {
        // For 'all' option, return the array as-is
        promise_result = result;
      } else {
        // For single address, return {address, family}
        promise_result = JS_NewObject(ctx);
        JS_DefinePropertyValueStr(ctx, promise_result, "address", result, JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, promise_result, "family", JS_NewInt32(ctx, family), JS_PROP_C_W_E);
      }
      JSValue args[] = {promise_result};
      JS_Call(ctx, dns_req->promise_funcs[0], JS_UNDEFINED, 1, args);
      if (!dns_req->all) {
        JS_FreeValue(ctx, promise_result);
      }
    } else {
      // Call callback with (null, address, family)
      JSValue args[3];
      args[0] = JS_NULL;  // No error
      if (dns_req->all) {
        args[1] = result;
        JS_Call(ctx, dns_req->callback, JS_UNDEFINED, 2, args);
      } else {
        args[1] = result;
        args[2] = JS_NewInt32(ctx, family);
        JS_Call(ctx, dns_req->callback, JS_UNDEFINED, 3, args);
        JS_FreeValue(ctx, args[2]);
      }
    }

    if (!dns_req->use_promise || dns_req->all) {
      JS_FreeValue(ctx, result);
    }
  }

  // Cleanup (CRITICAL)
  if (res) {
    uv_freeaddrinfo(res);
  }
  JS_FreeValue(ctx, dns_req->callback);
  if (dns_req->use_promise) {
    JS_FreeValue(ctx, dns_req->promise_funcs[0]);
    JS_FreeValue(ctx, dns_req->promise_funcs[1]);
  }
  if (dns_req->hostname) {
    js_free(ctx, dns_req->hostname);
  }
  js_free(ctx, dns_req);
}

// Callback for getnameinfo (dns.lookupService)
void on_getnameinfo_callback(uv_getnameinfo_t* req, int status, const char* hostname, const char* service) {
  DNSLookupServiceRequest* dns_req = (DNSLookupServiceRequest*)req;
  JSContext* ctx = dns_req->ctx;

  if (status != 0) {
    // Error occurred
    JSValue error = create_dns_error(ctx, status, "getnameinfo", dns_req->address);

    if (dns_req->use_promise) {
      // Reject promise
      JSValue args[] = {error};
      JS_Call(ctx, dns_req->promise_funcs[1], JS_UNDEFINED, 1, args);
    } else {
      // Call callback with error
      JSValue args[] = {error};
      JS_Call(ctx, dns_req->callback, JS_UNDEFINED, 1, args);
    }

    JS_FreeValue(ctx, error);
  } else {
    // Success - return hostname and service
    if (dns_req->use_promise) {
      // Resolve promise with object {hostname, service}
      JSValue result = JS_NewObject(ctx);
      JS_DefinePropertyValueStr(ctx, result, "hostname", JS_NewString(ctx, hostname), JS_PROP_C_W_E);
      JS_DefinePropertyValueStr(ctx, result, "service", JS_NewString(ctx, service), JS_PROP_C_W_E);

      JSValue args[] = {result};
      JS_Call(ctx, dns_req->promise_funcs[0], JS_UNDEFINED, 1, args);
      JS_FreeValue(ctx, result);
    } else {
      // Call callback with (null, hostname, service)
      JSValue args[3];
      args[0] = JS_NULL;  // No error
      args[1] = JS_NewString(ctx, hostname);
      args[2] = JS_NewString(ctx, service);
      JS_Call(ctx, dns_req->callback, JS_UNDEFINED, 3, args);
      JS_FreeValue(ctx, args[1]);
      JS_FreeValue(ctx, args[2]);
    }
  }

  // Cleanup (CRITICAL)
  JS_FreeValue(ctx, dns_req->callback);
  if (dns_req->use_promise) {
    JS_FreeValue(ctx, dns_req->promise_funcs[0]);
    JS_FreeValue(ctx, dns_req->promise_funcs[1]);
  }
  if (dns_req->address) {
    js_free(ctx, dns_req->address);
  }
  js_free(ctx, dns_req);
}
