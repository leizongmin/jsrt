#ifndef JSRT_NODE_DNS_INTERNAL_H
#define JSRT_NODE_DNS_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../runtime.h"
#include "../node_modules.h"

// DNS Lookup request state
typedef struct {
  uv_getaddrinfo_t req;  // Must be first (for req.data access)
  JSContext* ctx;
  JSValue callback;          // JavaScript callback function
  JSValue promise_funcs[2];  // [resolve, reject] for promises
  bool use_promise;          // true if promises API
  bool all;                  // Return all addresses
  int family;                // 0, 4, or 6
  int hints_flags;           // Additional getaddrinfo hints
  char* hostname;            // Saved for error messages
  bool verbatim;             // If true, don't reorder IPv6/IPv4
} DNSLookupRequest;

// Lookup service request state
typedef struct {
  uv_getnameinfo_t req;  // Must be first
  JSContext* ctx;
  JSValue callback;
  JSValue promise_funcs[2];
  bool use_promise;
  char* address;  // Saved for error messages
  int port;
} DNSLookupServiceRequest;

// Error handling functions (from dns_errors.c)
JSValue create_dns_error(JSContext* ctx, int status, const char* syscall, const char* hostname);
const char* get_dns_error_code(int status);

// Callback functions (from dns_callbacks.c)
void on_getaddrinfo_callback(uv_getaddrinfo_t* req, int status, struct addrinfo* res);
void on_getnameinfo_callback(uv_getnameinfo_t* req, int status, const char* hostname, const char* service);

// Lookup functions (from dns_lookup.c)
JSValue js_dns_lookup(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dns_lookup_promise(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Lookup service functions (from dns_lookupservice.c)
JSValue js_dns_lookupservice(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dns_lookupservice_promise(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Module functions (from dns_module.c)
JSValue JSRT_InitNodeDns(JSContext* ctx);
int js_node_dns_init(JSContext* ctx, JSModuleDef* m);
JSValue JSRT_InitNodeDnsPromises(JSContext* ctx);
int js_node_dns_promises_init(JSContext* ctx, JSModuleDef* m);

#endif  // JSRT_NODE_DNS_INTERNAL_H
