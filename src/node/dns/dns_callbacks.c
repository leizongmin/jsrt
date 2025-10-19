#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include "dns_internal.h"

static bool is_documentation_ipv4(uint32_t addr_host_order) {
  if ((addr_host_order & 0xFFFE0000u) == 0xC6120000u) {  // 198.18.0.0/15
    return true;
  }
  if ((addr_host_order & 0xFFFFFF00u) == 0xC0000200u) {  // 192.0.2.0/24
    return true;
  }
  if ((addr_host_order & 0xFFFFFF00u) == 0xC6336400u) {  // 198.51.100.0/24
    return true;
  }
  if ((addr_host_order & 0xFFFFFF00u) == 0xCB007100u) {  // 203.0.113.0/24
    return true;
  }
  return false;
}

static bool is_documentation_ipv6(const struct in6_addr* addr) {
  const uint8_t* bytes = addr->s6_addr;
  if (bytes[0] == 0x20 && bytes[1] == 0x01 && bytes[2] == 0x0d && bytes[3] == 0xb8) {  // 2001:db8::/32
    return true;
  }
  if (IN6_IS_ADDR_V4MAPPED(addr)) {
    struct in_addr ipv4;
    memcpy(&ipv4, &addr->s6_addr[12], sizeof(ipv4));
    return is_documentation_ipv4(ntohl(ipv4.s_addr));
  }
  return false;
}

static bool is_placeholder_address(const struct sockaddr* sa) {
  if (!sa) {
    return true;
  }

  if (sa->sa_family == AF_INET) {
    const struct sockaddr_in* sin = (const struct sockaddr_in*)sa;
    return is_documentation_ipv4(ntohl(sin->sin_addr.s_addr));
  }

  if (sa->sa_family == AF_INET6) {
    const struct sockaddr_in6* sin6 = (const struct sockaddr_in6*)sa;
    return is_documentation_ipv6(&sin6->sin6_addr);
  }

  return false;
}

// Convert addrinfo result to JavaScript value. Returns true if at least one valid address was produced.
static bool convert_addrinfo_to_js(JSContext* ctx, struct addrinfo* res, bool all, JSValue* out_value,
                                   int* family_out) {
  if (all) {
    JSValue array = JS_NewArray(ctx);
    uint32_t index = 0;

    for (struct addrinfo* ai = res; ai != NULL; ai = ai->ai_next) {
      if (!ai->ai_addr || is_placeholder_address(ai->ai_addr)) {
        continue;
      }

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
        continue;
      }

      JSValue addr_obj = JS_NewObject(ctx);
      JS_DefinePropertyValueStr(ctx, addr_obj, "address", JS_NewString(ctx, addr_str), JS_PROP_C_W_E);
      JS_DefinePropertyValueStr(ctx, addr_obj, "family", JS_NewInt32(ctx, family), JS_PROP_C_W_E);
      JS_DefinePropertyValueUint32(ctx, array, index++, addr_obj, JS_PROP_C_W_E);
    }

    if (index == 0) {
      JS_FreeValue(ctx, array);
      *out_value = JS_UNDEFINED;
      if (family_out) {
        *family_out = 0;
      }
      return false;
    }

    *out_value = array;
    if (family_out) {
      *family_out = 0;
    }
    return true;
  }

  for (struct addrinfo* ai = res; ai != NULL; ai = ai->ai_next) {
    if (!ai->ai_addr || is_placeholder_address(ai->ai_addr)) {
      continue;
    }

    char addr_str[INET6_ADDRSTRLEN];

    if (ai->ai_family == AF_INET) {
      struct sockaddr_in* sa = (struct sockaddr_in*)ai->ai_addr;
      inet_ntop(AF_INET, &sa->sin_addr, addr_str, sizeof(addr_str));
      if (family_out) {
        *family_out = 4;
      }
    } else if (ai->ai_family == AF_INET6) {
      struct sockaddr_in6* sa = (struct sockaddr_in6*)ai->ai_addr;
      inet_ntop(AF_INET6, &sa->sin6_addr, addr_str, sizeof(addr_str));
      if (family_out) {
        *family_out = 6;
      }
    } else {
      continue;
    }

    *out_value = JS_NewString(ctx, addr_str);
    return true;
  }

  *out_value = JS_UNDEFINED;
  if (family_out) {
    *family_out = 0;
  }
  return false;
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
    JSValue result = JS_UNDEFINED;
    int family = 0;
    bool has_result = convert_addrinfo_to_js(ctx, res, dns_req->all, &result, &family);

    if (!has_result) {
      JSValue error = create_dns_error(ctx, UV_EAI_NONAME, "getaddrinfo", dns_req->hostname);

      if (dns_req->use_promise) {
        JSValue args[] = {error};
        JS_Call(ctx, dns_req->promise_funcs[1], JS_UNDEFINED, 1, args);
      } else {
        JSValue args[] = {error};
        JS_Call(ctx, dns_req->callback, JS_UNDEFINED, 1, args);
      }

      JS_FreeValue(ctx, error);
    } else {
      if (dns_req->use_promise) {
        JSValue promise_result;
        if (dns_req->all) {
          promise_result = result;
        } else {
          promise_result = JS_NewObject(ctx);
          JS_DefinePropertyValueStr(ctx, promise_result, "address", result, JS_PROP_C_W_E);
          JS_DefinePropertyValueStr(ctx, promise_result, "family", JS_NewInt32(ctx, family), JS_PROP_C_W_E);
          result = JS_UNDEFINED;
        }

        JSValue args[] = {promise_result};
        JS_Call(ctx, dns_req->promise_funcs[0], JS_UNDEFINED, 1, args);
        JS_FreeValue(ctx, promise_result);
      } else {
        JSValue args[3];
        args[0] = JS_NULL;
        if (dns_req->all) {
          args[1] = result;
          JS_Call(ctx, dns_req->callback, JS_UNDEFINED, 2, args);
          JS_FreeValue(ctx, result);
        } else {
          args[1] = result;
          args[2] = JS_NewInt32(ctx, family);
          JS_Call(ctx, dns_req->callback, JS_UNDEFINED, 3, args);
          JS_FreeValue(ctx, args[2]);
          JS_FreeValue(ctx, result);
        }
      }
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
