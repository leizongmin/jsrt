#ifndef JSRT_NODE_HTTP_CLIENT_H
#define JSRT_NODE_HTTP_CLIENT_H

#include "http_internal.h"

// Client class functions (future implementation)

// Helper function to send headers (used by http_module.c)
void send_headers(JSHTTPClientRequest* client_req);

#endif  // JSRT_NODE_HTTP_CLIENT_H
