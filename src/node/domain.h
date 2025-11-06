/**
 * Node.js Domain Module Header
 */

#ifndef JSRT_NODE_DOMAIN_H
#define JSRT_NODE_DOMAIN_H

#include <quickjs.h>

// Function declarations
JSValue JSRT_InitNodeDomain(JSContext* ctx);
int js_node_domain_init(JSContext* ctx, JSModuleDef* m);

// Domain interop functions
void* js_domain_get_current(void);
void js_domain_emit_error(JSContext* ctx, JSValue error);

#endif  // JSRT_NODE_DOMAIN_H