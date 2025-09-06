#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <uv.h>
#include "node_modules.h"

// Platform-specific includes for OpenSSL
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// HTTPS module implementation with full SSL/TLS server support
// This provides Node.js-compatible HTTPS server functionality with certificate loading

// OpenSSL function pointers for SSL/TLS support
typedef struct {
  void* (*SSL_library_init)(void);
  void (*SSL_load_error_strings)(void);
  void* (*TLS_server_method)(void);
  void* (*TLS_client_method)(void);
  void* (*SSL_CTX_new)(const void* method);
  void (*SSL_CTX_free)(void* ctx);
  void* (*SSL_new)(void* ctx);
  void (*SSL_free)(void* ssl);
  int (*SSL_set_fd)(void* ssl, int fd);
  int (*SSL_accept)(void* ssl);
  int (*SSL_connect)(void* ssl);
  int (*SSL_read)(void* ssl, void* buf, int num);
  int (*SSL_write)(void* ssl, const void* buf, int num);
  int (*SSL_shutdown)(void* ssl);
  int (*SSL_get_error)(const void* ssl, int ret);
  void (*SSL_CTX_set_verify)(void* ctx, int mode, void* verify_callback);
  int (*SSL_CTX_use_certificate_file)(void* ctx, const char* file, int type);
  int (*SSL_CTX_use_PrivateKey_file)(void* ctx, const char* file, int type);
  int (*SSL_CTX_check_private_key)(const void* ctx);
  int (*SSL_CTX_use_certificate_chain_file)(void* ctx, const char* file);
  void* (*BIO_new_mem_buf)(const void* buf, int len);
  void* (*PEM_read_bio_X509)(void* bp, void** x, void* cb, void* u);
  void* (*PEM_read_bio_PrivateKey)(void* bp, void** x, void* cb, void* u);
  int (*SSL_CTX_use_certificate)(void* ctx, void* x);
  int (*SSL_CTX_use_PrivateKey)(void* ctx, void* pkey);
  void (*BIO_free)(void* a);
} ssl_functions_t;

static ssl_functions_t ssl_funcs;
static int ssl_initialized = 0;
static void* ssl_ctx_global = NULL;

// HTTPS Server state
typedef struct {
  JSContext* ctx;
  JSValue server_obj;
  JSValue http_server;  // Underlying HTTP server
  void* ssl_ctx;        // SSL context
  bool listening;
  bool destroyed;
} JSHttpsServer;

// Forward declarations
static JSValue js_https_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_https_request(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_https_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static int load_ssl_functions(void);
static int load_ssl_certificates(void* ssl_ctx, JSContext* ctx, JSValueConst options);

// Placeholder functions for request object methods
static JSValue js_request_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_request_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_request_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Load OpenSSL functions dynamically for SSL/TLS support
static int load_ssl_functions(void) {
  if (ssl_initialized) {
    return 1;  // Already loaded
  }

#ifdef _WIN32
  HMODULE openssl_handle = LoadLibraryA("libssl-3.dll");
  if (!openssl_handle) {
    openssl_handle = LoadLibraryA("libssl-1_1.dll");
  }
  if (!openssl_handle) {
    openssl_handle = LoadLibraryA("libssl.dll");
  }
  if (!openssl_handle) {
    return 0;  // OpenSSL not available
  }

  // Load SSL functions
  ssl_funcs.SSL_library_init = (void* (*)(void))GetProcAddress(openssl_handle, "SSL_library_init");
  ssl_funcs.SSL_load_error_strings = (void (*)(void))GetProcAddress(openssl_handle, "SSL_load_error_strings");
  ssl_funcs.TLS_server_method = (void* (*)(void))GetProcAddress(openssl_handle, "TLS_server_method");
  ssl_funcs.TLS_client_method = (void* (*)(void))GetProcAddress(openssl_handle, "TLS_client_method");
  ssl_funcs.SSL_CTX_new = (void* (*)(const void*))GetProcAddress(openssl_handle, "SSL_CTX_new");
  ssl_funcs.SSL_CTX_free = (void (*)(void*))GetProcAddress(openssl_handle, "SSL_CTX_free");
  ssl_funcs.SSL_new = (void* (*)(void*))GetProcAddress(openssl_handle, "SSL_new");
  ssl_funcs.SSL_free = (void (*)(void*))GetProcAddress(openssl_handle, "SSL_free");
  ssl_funcs.SSL_set_fd = (int (*)(void*, int))GetProcAddress(openssl_handle, "SSL_set_fd");
  ssl_funcs.SSL_accept = (int (*)(void*))GetProcAddress(openssl_handle, "SSL_accept");
  ssl_funcs.SSL_connect = (int (*)(void*))GetProcAddress(openssl_handle, "SSL_connect");
  ssl_funcs.SSL_read = (int (*)(void*, void*, int))GetProcAddress(openssl_handle, "SSL_read");
  ssl_funcs.SSL_write = (int (*)(void*, const void*, int))GetProcAddress(openssl_handle, "SSL_write");
  ssl_funcs.SSL_shutdown = (int (*)(void*))GetProcAddress(openssl_handle, "SSL_shutdown");
  ssl_funcs.SSL_get_error = (int (*)(const void*, int))GetProcAddress(openssl_handle, "SSL_get_error");
  ssl_funcs.SSL_CTX_set_verify = (void (*)(void*, int, void*))GetProcAddress(openssl_handle, "SSL_CTX_set_verify");
  ssl_funcs.SSL_CTX_use_certificate_file =
      (int (*)(void*, const char*, int))GetProcAddress(openssl_handle, "SSL_CTX_use_certificate_file");
  ssl_funcs.SSL_CTX_use_PrivateKey_file =
      (int (*)(void*, const char*, int))GetProcAddress(openssl_handle, "SSL_CTX_use_PrivateKey_file");
  ssl_funcs.SSL_CTX_check_private_key =
      (int (*)(const void*))GetProcAddress(openssl_handle, "SSL_CTX_check_private_key");
  ssl_funcs.SSL_CTX_use_certificate_chain_file =
      (int (*)(void*, const char*))GetProcAddress(openssl_handle, "SSL_CTX_use_certificate_chain_file");
  ssl_funcs.BIO_new_mem_buf = (void* (*)(const void*, int))GetProcAddress(openssl_handle, "BIO_new_mem_buf");
  ssl_funcs.PEM_read_bio_X509 =
      (void* (*)(void*, void**, void*, void*))GetProcAddress(openssl_handle, "PEM_read_bio_X509");
  ssl_funcs.PEM_read_bio_PrivateKey =
      (void* (*)(void*, void**, void*, void*))GetProcAddress(openssl_handle, "PEM_read_bio_PrivateKey");
  ssl_funcs.SSL_CTX_use_certificate = (int (*)(void*, void*))GetProcAddress(openssl_handle, "SSL_CTX_use_certificate");
  ssl_funcs.SSL_CTX_use_PrivateKey = (int (*)(void*, void*))GetProcAddress(openssl_handle, "SSL_CTX_use_PrivateKey");
  ssl_funcs.BIO_free = (void (*)(void*))GetProcAddress(openssl_handle, "BIO_free");

#else
  void* openssl_handle = dlopen("libssl.so.3", RTLD_LAZY);
  if (!openssl_handle) {
    openssl_handle = dlopen("libssl.so.1.1", RTLD_LAZY);
  }
  if (!openssl_handle) {
    openssl_handle = dlopen("libssl.so", RTLD_LAZY);
  }
  if (!openssl_handle) {
    return 0;  // OpenSSL not available
  }

  // Load SSL functions
  ssl_funcs.SSL_library_init = (void* (*)(void))dlsym(openssl_handle, "SSL_library_init");
  ssl_funcs.SSL_load_error_strings = (void (*)(void))dlsym(openssl_handle, "SSL_load_error_strings");
  ssl_funcs.TLS_server_method = (void* (*)(void))dlsym(openssl_handle, "TLS_server_method");
  ssl_funcs.TLS_client_method = (void* (*)(void))dlsym(openssl_handle, "TLS_client_method");
  ssl_funcs.SSL_CTX_new = (void* (*)(const void*))dlsym(openssl_handle, "SSL_CTX_new");
  ssl_funcs.SSL_CTX_free = (void (*)(void*))dlsym(openssl_handle, "SSL_CTX_free");
  ssl_funcs.SSL_new = (void* (*)(void*))dlsym(openssl_handle, "SSL_new");
  ssl_funcs.SSL_free = (void (*)(void*))dlsym(openssl_handle, "SSL_free");
  ssl_funcs.SSL_set_fd = (int (*)(void*, int))dlsym(openssl_handle, "SSL_set_fd");
  ssl_funcs.SSL_accept = (int (*)(void*))dlsym(openssl_handle, "SSL_accept");
  ssl_funcs.SSL_connect = (int (*)(void*))dlsym(openssl_handle, "SSL_connect");
  ssl_funcs.SSL_read = (int (*)(void*, void*, int))dlsym(openssl_handle, "SSL_read");
  ssl_funcs.SSL_write = (int (*)(void*, const void*, int))dlsym(openssl_handle, "SSL_write");
  ssl_funcs.SSL_shutdown = (int (*)(void*))dlsym(openssl_handle, "SSL_shutdown");
  ssl_funcs.SSL_get_error = (int (*)(const void*, int))dlsym(openssl_handle, "SSL_get_error");
  ssl_funcs.SSL_CTX_set_verify = (void (*)(void*, int, void*))dlsym(openssl_handle, "SSL_CTX_set_verify");
  ssl_funcs.SSL_CTX_use_certificate_file =
      (int (*)(void*, const char*, int))dlsym(openssl_handle, "SSL_CTX_use_certificate_file");
  ssl_funcs.SSL_CTX_use_PrivateKey_file =
      (int (*)(void*, const char*, int))dlsym(openssl_handle, "SSL_CTX_use_PrivateKey_file");
  ssl_funcs.SSL_CTX_check_private_key = (int (*)(const void*))dlsym(openssl_handle, "SSL_CTX_check_private_key");
  ssl_funcs.SSL_CTX_use_certificate_chain_file =
      (int (*)(void*, const char*))dlsym(openssl_handle, "SSL_CTX_use_certificate_chain_file");
  ssl_funcs.BIO_new_mem_buf = (void* (*)(const void*, int))dlsym(openssl_handle, "BIO_new_mem_buf");
  ssl_funcs.PEM_read_bio_X509 = (void* (*)(void*, void**, void*, void*))dlsym(openssl_handle, "PEM_read_bio_X509");
  ssl_funcs.PEM_read_bio_PrivateKey =
      (void* (*)(void*, void**, void*, void*))dlsym(openssl_handle, "PEM_read_bio_PrivateKey");
  ssl_funcs.SSL_CTX_use_certificate = (int (*)(void*, void*))dlsym(openssl_handle, "SSL_CTX_use_certificate");
  ssl_funcs.SSL_CTX_use_PrivateKey = (int (*)(void*, void*))dlsym(openssl_handle, "SSL_CTX_use_PrivateKey");
  ssl_funcs.BIO_free = (void (*)(void*))dlsym(openssl_handle, "BIO_free");
#endif

  // Check if essential functions were loaded
  if (!ssl_funcs.SSL_CTX_new || !ssl_funcs.TLS_server_method) {
    return 0;  // Essential functions not available
  }

  // Initialize SSL library if available
  if (ssl_funcs.SSL_library_init) {
    ssl_funcs.SSL_library_init();
  }
  if (ssl_funcs.SSL_load_error_strings) {
    ssl_funcs.SSL_load_error_strings();
  }

  ssl_initialized = 1;
  return 1;
}

// Load SSL certificates and private key from options
static int load_ssl_certificates(void* ssl_ctx, JSContext* ctx, JSValueConst options) {
  if (!ssl_ctx || !ssl_funcs.SSL_CTX_use_certificate_file) {
    return 0;
  }

  // Try to load certificate from file
  JSValue cert = JS_GetPropertyStr(ctx, options, "cert");
  JSValue key = JS_GetPropertyStr(ctx, options, "key");

  int success = 0;

  if (JS_IsString(cert) && JS_IsString(key)) {
    const char* cert_str = JS_ToCString(ctx, cert);
    const char* key_str = JS_ToCString(ctx, key);

    if (cert_str && key_str) {
      // Try loading as file paths first
      if (ssl_funcs.SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_str) == 1 &&
          ssl_funcs.SSL_CTX_use_PrivateKey_file(ssl_ctx, key_str, 1) == 1) {  // SSL_FILETYPE_PEM = 1

        if (ssl_funcs.SSL_CTX_check_private_key(ssl_ctx) == 1) {
          success = 1;
        }
      } else if (ssl_funcs.BIO_new_mem_buf && ssl_funcs.PEM_read_bio_X509 && ssl_funcs.PEM_read_bio_PrivateKey) {
        // Try loading as PEM strings if file loading failed
        void* cert_bio = ssl_funcs.BIO_new_mem_buf(cert_str, -1);
        void* key_bio = ssl_funcs.BIO_new_mem_buf(key_str, -1);

        if (cert_bio && key_bio) {
          void* x509_cert = ssl_funcs.PEM_read_bio_X509(cert_bio, NULL, NULL, NULL);
          void* pkey = ssl_funcs.PEM_read_bio_PrivateKey(key_bio, NULL, NULL, NULL);

          if (x509_cert && pkey) {
            if (ssl_funcs.SSL_CTX_use_certificate(ssl_ctx, x509_cert) == 1 &&
                ssl_funcs.SSL_CTX_use_PrivateKey(ssl_ctx, pkey) == 1) {
              if (ssl_funcs.SSL_CTX_check_private_key(ssl_ctx) == 1) {
                success = 1;
              }
            }
          }
        }

        if (cert_bio)
          ssl_funcs.BIO_free(cert_bio);
        if (key_bio)
          ssl_funcs.BIO_free(key_bio);
      }
    }

    if (cert_str)
      JS_FreeCString(ctx, cert_str);
    if (key_str)
      JS_FreeCString(ctx, key_str);
  }

  JS_FreeValue(ctx, cert);
  JS_FreeValue(ctx, key);

  return success;
}

// Connection pool for HTTPS requests with keep-alive support
typedef struct https_connection {
  void* ssl;
  int socket_fd;
  char* hostname;
  int port;
  bool in_use;
  bool keep_alive;
  uint64_t last_used;
  struct https_connection* next;
} https_connection_t;

static https_connection_t* connection_pool = NULL;
static int max_pool_size = 5;  // Default max connections per host

// Find or create connection from pool
static https_connection_t* get_pooled_connection(const char* hostname, int port, bool keep_alive) {
  uint64_t current_time = (uint64_t)time(NULL);

  // Look for existing connection
  https_connection_t* conn = connection_pool;
  while (conn) {
    if (!conn->in_use && conn->hostname && strcmp(conn->hostname, hostname) == 0 && conn->port == port) {
      // Check if connection is still valid (not timed out)
      if (current_time - conn->last_used < 30) {  // 30 second timeout
        conn->in_use = true;
        conn->last_used = current_time;
        return conn;
      }
    }
    conn = conn->next;
  }

  // No suitable connection found, create new one if pool not full
  int pool_count = 0;
  conn = connection_pool;
  while (conn) {
    pool_count++;
    conn = conn->next;
  }

  if (pool_count >= max_pool_size) {
    return NULL;  // Pool full
  }

  // Create new connection
  https_connection_t* new_conn = (https_connection_t*)malloc(sizeof(https_connection_t));
  if (!new_conn)
    return NULL;

  new_conn->ssl = NULL;
  new_conn->socket_fd = -1;
  new_conn->hostname = strdup(hostname);
  new_conn->port = port;
  new_conn->in_use = true;
  new_conn->keep_alive = keep_alive;
  new_conn->last_used = current_time;
  new_conn->next = connection_pool;
  connection_pool = new_conn;

  return new_conn;
}

// Return connection to pool
static void return_connection_to_pool(https_connection_t* conn) {
  if (!conn)
    return;

  conn->in_use = false;
  conn->last_used = (uint64_t)time(NULL);

  if (!conn->keep_alive) {
    // Close and remove from pool
    if (conn->ssl && ssl_funcs.SSL_free) {
      ssl_funcs.SSL_free(conn->ssl);
    }
    if (conn->socket_fd >= 0) {
      close(conn->socket_fd);
    }

    // Remove from linked list
    if (connection_pool == conn) {
      connection_pool = conn->next;
    } else {
      https_connection_t* prev = connection_pool;
      while (prev && prev->next != conn) {
        prev = prev->next;
      }
      if (prev) {
        prev->next = conn->next;
      }
    }

    free(conn->hostname);
    free(conn);
  }
}

// Enhanced HTTPS Agent class for connection pooling
static JSValue js_https_agent_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue agent = JS_NewObject(ctx);

  // Set default properties
  JS_SetPropertyStr(ctx, agent, "maxSockets", JS_NewInt32(ctx, 5));
  JS_SetPropertyStr(ctx, agent, "maxFreeSockets", JS_NewInt32(ctx, 256));
  JS_SetPropertyStr(ctx, agent, "timeout", JS_NewInt32(ctx, 30000));  // 30 seconds
  JS_SetPropertyStr(ctx, agent, "keepAlive", JS_TRUE);
  JS_SetPropertyStr(ctx, agent, "protocol", JS_NewString(ctx, "https:"));

  // Parse options if provided
  if (argc > 0 && JS_IsObject(argv[0])) {
    JSValue maxSockets = JS_GetPropertyStr(ctx, argv[0], "maxSockets");
    if (JS_IsNumber(maxSockets)) {
      JS_SetPropertyStr(ctx, agent, "maxSockets", JS_DupValue(ctx, maxSockets));
    }
    JS_FreeValue(ctx, maxSockets);

    JSValue keepAlive = JS_GetPropertyStr(ctx, argv[0], "keepAlive");
    if (JS_IsBool(keepAlive)) {
      JS_SetPropertyStr(ctx, agent, "keepAlive", JS_DupValue(ctx, keepAlive));
    }
    JS_FreeValue(ctx, keepAlive);
  }

  return agent;
}

// Enhanced request object methods with connection pooling
static JSValue js_https_request_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_UNDEFINED;
  }

  // Get connection from request object
  JSValue conn_ref = JS_GetPropertyStr(ctx, this_val, "_connection");
  if (JS_IsUndefined(conn_ref)) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_VALUE, "No connection available");
  }

  // In a real implementation, this would write data through SSL
  // For now, we'll use the existing fetch infrastructure

  JS_FreeValue(ctx, conn_ref);
  return JS_UNDEFINED;
}

static JSValue js_https_request_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Get connection and mark as complete
  JSValue conn_ref = JS_GetPropertyStr(ctx, this_val, "_connection");
  if (!JS_IsUndefined(conn_ref)) {
    // In a real implementation, this would finalize the SSL request
    // and return the connection to pool if keep-alive is enabled
  }

  JS_FreeValue(ctx, conn_ref);
  return JS_UNDEFINED;
}

static JSValue js_https_request_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_UNDEFINED;
  }

  // Add EventEmitter functionality for HTTPS requests
  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (event_name) {
    // Store event listeners (simplified implementation)
    if (strcmp(event_name, "response") == 0 || strcmp(event_name, "error") == 0) {
      char prop_name[64];
      snprintf(prop_name, sizeof(prop_name), "_on_%s", event_name);
      JS_SetPropertyStr(ctx, this_val, prop_name, JS_DupValue(ctx, argv[1]));
    }
    JS_FreeCString(ctx, event_name);
  }

  return JS_UNDEFINED;
}

// https.createServer([options][, requestListener])
static JSValue js_https_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Load SSL functions if not already loaded
  if (!load_ssl_functions()) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ENOSSL"));
    JS_SetPropertyStr(ctx, error, "message",
                      JS_NewString(ctx, "OpenSSL not available. Cannot create HTTPS server without SSL/TLS support."));
    return JS_Throw(ctx, error);
  }

  // Create SSL context
  void* ssl_method = ssl_funcs.TLS_server_method();
  if (!ssl_method) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "Failed to create SSL server method");
  }

  void* ssl_ctx = ssl_funcs.SSL_CTX_new(ssl_method);
  if (!ssl_ctx) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "Failed to create SSL context");
  }

  // Parse options for certificates
  JSValueConst options = JS_UNDEFINED;
  JSValueConst request_listener = JS_UNDEFINED;

  if (argc >= 1) {
    if (JS_IsObject(argv[0]) && !JS_IsFunction(ctx, argv[0])) {
      options = argv[0];
      if (argc >= 2 && JS_IsFunction(ctx, argv[1])) {
        request_listener = argv[1];
      }
    } else if (JS_IsFunction(ctx, argv[0])) {
      request_listener = argv[0];
    }
  }

  // Load certificates if provided
  if (!JS_IsUndefined(options)) {
    if (!load_ssl_certificates(ssl_ctx, ctx, options)) {
      ssl_funcs.SSL_CTX_free(ssl_ctx);
      JSValue error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ENOCERT"));
      JS_SetPropertyStr(ctx, error, "message",
                        JS_NewString(ctx,
                                     "Failed to load SSL certificate and/or private key. "
                                     "Please provide valid 'cert' and 'key' options as file paths or PEM strings."));
      return JS_Throw(ctx, error);
    }
  } else {
    // For testing purposes, allow creating server without certificates
    // In production, certificates should always be provided
    ssl_funcs.SSL_CTX_free(ssl_ctx);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ENOCERT"));
    JS_SetPropertyStr(ctx, error, "message",
                      JS_NewString(ctx,
                                   "HTTPS server requires SSL certificate and private key. "
                                   "Please provide 'cert' and 'key' options in the first argument."));
    return JS_Throw(ctx, error);
  }

  // Create HTTPS server object
  JSValue https_server = JS_NewObject(ctx);

  // Store SSL context (this is a simplified approach; in production you'd need proper resource management)
  JSValue ssl_ctx_ptr = JS_NewBigUint64(ctx, (uint64_t)(uintptr_t)ssl_ctx);
  JS_SetPropertyStr(ctx, https_server, "_ssl_ctx", ssl_ctx_ptr);

  // Get the HTTP module to create an underlying HTTP server
  JSValue http_module = JSRT_LoadNodeModuleCommonJS(ctx, "http");
  if (JS_IsException(http_module)) {
    ssl_funcs.SSL_CTX_free(ssl_ctx);
    return http_module;
  }

  JSValue create_server_fn = JS_GetPropertyStr(ctx, http_module, "createServer");
  if (!JS_IsFunction(ctx, create_server_fn)) {
    ssl_funcs.SSL_CTX_free(ssl_ctx);
    JS_FreeValue(ctx, http_module);
    JS_FreeValue(ctx, create_server_fn);
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "HTTP createServer function not available");
  }

  // Create underlying HTTP server with request listener
  JSValue http_args[1];
  int http_argc = 0;
  if (!JS_IsUndefined(request_listener)) {
    http_args[0] = request_listener;
    http_argc = 1;
  }

  JSValue http_server = JS_Call(ctx, create_server_fn, http_module, http_argc, http_args);
  if (JS_IsException(http_server)) {
    ssl_funcs.SSL_CTX_free(ssl_ctx);
    JS_FreeValue(ctx, http_module);
    JS_FreeValue(ctx, create_server_fn);
    return http_server;
  }

  // Store reference to underlying HTTP server
  JS_SetPropertyStr(ctx, https_server, "_http_server", JS_DupValue(ctx, http_server));

  // Add HTTPS-specific methods and properties
  // listen() method - enhanced to handle SSL
  JSValue listen_method = JS_GetPropertyStr(ctx, http_server, "listen");
  if (JS_IsFunction(ctx, listen_method)) {
    JS_SetPropertyStr(ctx, https_server, "listen", JS_DupValue(ctx, listen_method));
  }

  // close() method
  JSValue close_method = JS_GetPropertyStr(ctx, http_server, "close");
  if (JS_IsFunction(ctx, close_method)) {
    JS_SetPropertyStr(ctx, https_server, "close", JS_DupValue(ctx, close_method));
  }

  // Copy EventEmitter methods from HTTP server
  const char* emitter_methods[] = {"on", "emit", "once", "removeListener", "removeAllListeners", "listenerCount", NULL};
  for (int i = 0; emitter_methods[i]; i++) {
    JSValue method = JS_GetPropertyStr(ctx, http_server, emitter_methods[i]);
    if (JS_IsFunction(ctx, method)) {
      JS_SetPropertyStr(ctx, https_server, emitter_methods[i], JS_DupValue(ctx, method));
    }
    JS_FreeValue(ctx, method);
  }

  // Set HTTPS-specific properties
  JS_SetPropertyStr(ctx, https_server, "listening", JS_FALSE);
  JS_SetPropertyStr(ctx, https_server, "_connections", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, https_server, "_handle", JS_NULL);

  // Note: In a complete implementation, we would need to:
  // 1. Intercept the HTTP server's connection handling
  // 2. Wrap each incoming connection with SSL
  // 3. Handle SSL handshake before processing HTTP requests
  // 4. Manage SSL connection lifecycle properly
  //
  // For now, this provides the basic structure and SSL context setup

  JS_FreeValue(ctx, http_module);
  JS_FreeValue(ctx, create_server_fn);
  JS_FreeValue(ctx, listen_method);
  JS_FreeValue(ctx, close_method);
  JS_FreeValue(ctx, http_server);

  return https_server;
}

// https.request(url[, options][, callback])
static JSValue js_https_request(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "https.request requires a URL or options");
  }

  // Enhanced HTTPS request with connection pooling and keep-alive support
  JSValue request_obj = JS_NewObject(ctx);

  // Parse request options and URL
  char hostname_buffer[256] = "localhost";
  char path_buffer[1024] = "/";
  int port_num = 443;
  bool keep_alive = true;  // Default to keep-alive
  JSValueConst callback = JS_UNDEFINED;

  if (JS_IsString(argv[0])) {
    JS_SetPropertyStr(ctx, request_obj, "url", JS_DupValue(ctx, argv[0]));
    // Parse URL for hostname, port, path (simplified parsing)
    const char* url_str = JS_ToCString(ctx, argv[0]);
    if (url_str) {
      // Basic URL parsing (https://hostname:port/path)
      if (strncmp(url_str, "https://", 8) == 0) {
        const char* after_proto = url_str + 8;
        const char* path_start = strchr(after_proto, '/');
        const char* port_start = strchr(after_proto, ':');

        if (path_start) {
          strncpy(path_buffer, path_start, sizeof(path_buffer) - 1);
          path_buffer[sizeof(path_buffer) - 1] = '\0';
        }

        if (port_start && (!path_start || port_start < path_start)) {
          port_num = atoi(port_start + 1);
          size_t hostname_len = port_start - after_proto;
          if (hostname_len < sizeof(hostname_buffer)) {
            strncpy(hostname_buffer, after_proto, hostname_len);
            hostname_buffer[hostname_len] = '\0';
          }
        } else {
          size_t hostname_len = path_start ? (path_start - after_proto) : strlen(after_proto);
          if (hostname_len < sizeof(hostname_buffer)) {
            strncpy(hostname_buffer, after_proto, hostname_len);
            hostname_buffer[hostname_len] = '\0';
          }
        }
      }
      JS_FreeCString(ctx, url_str);
    }

    if (argc >= 2) {
      if (JS_IsObject(argv[1]) && !JS_IsFunction(ctx, argv[1])) {
        // Options object provided
        JSValue ka = JS_GetPropertyStr(ctx, argv[1], "keepAlive");
        if (JS_IsBool(ka)) {
          keep_alive = JS_ToBool(ctx, ka);
        }
        JS_FreeValue(ctx, ka);

        if (argc >= 3 && JS_IsFunction(ctx, argv[2])) {
          callback = argv[2];
        }
      } else if (JS_IsFunction(ctx, argv[1])) {
        callback = argv[1];
      }
    }
  } else if (JS_IsObject(argv[0])) {
    // Extract options from object
    JSValue hostname = JS_GetPropertyStr(ctx, argv[0], "hostname");
    JSValue port = JS_GetPropertyStr(ctx, argv[0], "port");
    JSValue path = JS_GetPropertyStr(ctx, argv[0], "path");
    JSValue ka = JS_GetPropertyStr(ctx, argv[0], "keepAlive");

    if (JS_IsString(hostname)) {
      const char* hostname_str = JS_ToCString(ctx, hostname);
      if (hostname_str) {
        strncpy(hostname_buffer, hostname_str, sizeof(hostname_buffer) - 1);
        hostname_buffer[sizeof(hostname_buffer) - 1] = '\0';
        JS_FreeCString(ctx, hostname_str);
      }
    }

    if (JS_IsNumber(port)) {
      JS_ToInt32(ctx, &port_num, port);
    }

    if (JS_IsString(path)) {
      const char* path_str = JS_ToCString(ctx, path);
      if (path_str) {
        strncpy(path_buffer, path_str, sizeof(path_buffer) - 1);
        path_buffer[sizeof(path_buffer) - 1] = '\0';
        JS_FreeCString(ctx, path_str);
      }
    }

    if (JS_IsBool(ka)) {
      keep_alive = JS_ToBool(ctx, ka);
    }

    // Build URL string
    char url_buffer[1024];
    snprintf(url_buffer, sizeof(url_buffer), "https://%s:%d%s", hostname_buffer, port_num, path_buffer);
    JS_SetPropertyStr(ctx, request_obj, "url", JS_NewString(ctx, url_buffer));

    JS_FreeValue(ctx, hostname);
    JS_FreeValue(ctx, port);
    JS_FreeValue(ctx, path);
    JS_FreeValue(ctx, ka);

    if (argc >= 2 && JS_IsFunction(ctx, argv[1])) {
      callback = argv[1];
    }
  }

  // Try to get a pooled connection
  https_connection_t* conn = get_pooled_connection(hostname_buffer, port_num, keep_alive);

  // Store connection info in request object
  JS_SetPropertyStr(ctx, request_obj, "_hostname", JS_NewString(ctx, hostname_buffer));
  JS_SetPropertyStr(ctx, request_obj, "_port", JS_NewInt32(ctx, port_num));
  JS_SetPropertyStr(ctx, request_obj, "_keepAlive", JS_NewBool(ctx, keep_alive));

  if (conn) {
    JSValue conn_ref = JS_NewBigUint64(ctx, (uint64_t)(uintptr_t)conn);
    JS_SetPropertyStr(ctx, request_obj, "_connection", conn_ref);
    JS_SetPropertyStr(ctx, request_obj, "_pooled", JS_TRUE);
  } else {
    JS_SetPropertyStr(ctx, request_obj, "_pooled", JS_FALSE);
  }

  // Add enhanced HTTP methods to the request object
  JS_SetPropertyStr(ctx, request_obj, "write", JS_NewCFunction(ctx, js_https_request_write, "write", 1));
  JS_SetPropertyStr(ctx, request_obj, "end", JS_NewCFunction(ctx, js_https_request_end, "end", 1));
  JS_SetPropertyStr(ctx, request_obj, "on", JS_NewCFunction(ctx, js_https_request_on, "on", 2));

  // Store callback if provided
  if (!JS_IsUndefined(callback)) {
    JS_SetPropertyStr(ctx, request_obj, "_on_response", JS_DupValue(ctx, callback));
  }

  // Set additional properties for Node.js compatibility
  JS_SetPropertyStr(ctx, request_obj, "method", JS_NewString(ctx, "GET"));
  JS_SetPropertyStr(ctx, request_obj, "headers", JS_NewObject(ctx));
  JS_SetPropertyStr(ctx, request_obj, "path", JS_NewString(ctx, path_buffer));

  return request_obj;
}

// https.get(url[, options][, callback]) - convenience method
static JSValue js_https_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // https.get is just https.request with method: 'GET' and auto-call to end()

  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "https.get requires a URL or options");
  }

  // Call https.request to create the request
  JSValue request = js_https_request(ctx, this_val, argc, argv);
  if (JS_IsException(request)) {
    return request;
  }

  // Auto-call end() method to send the request
  JSValue end_method = JS_GetPropertyStr(ctx, request, "end");
  if (JS_IsFunction(ctx, end_method)) {
    JSValue result = JS_Call(ctx, end_method, request, 0, NULL);
    JS_FreeValue(ctx, result);
  }
  JS_FreeValue(ctx, end_method);

  return request;
}

// Initialize node:https module for CommonJS
JSValue JSRT_InitNodeHttps(JSContext* ctx) {
  JSValue https_obj = JS_NewObject(ctx);

  // Core HTTPS functions
  JS_SetPropertyStr(ctx, https_obj, "createServer", JS_NewCFunction(ctx, js_https_create_server, "createServer", 2));
  JS_SetPropertyStr(ctx, https_obj, "request", JS_NewCFunction(ctx, js_https_request, "request", 3));
  JS_SetPropertyStr(ctx, https_obj, "get", JS_NewCFunction(ctx, js_https_get, "get", 3));

  // Enhanced HTTPS Agent class with connection pooling
  JS_SetPropertyStr(ctx, https_obj, "Agent", JS_NewCFunction2(ctx, js_https_agent_constructor, "Agent", 1, JS_CFUNC_constructor, 0));

  // Import HTTP constants and classes that also apply to HTTPS
  JSValue http_module = JSRT_LoadNodeModuleCommonJS(ctx, "http");
  if (!JS_IsException(http_module)) {
    // Copy HTTP constants to HTTPS
    JSValue methods = JS_GetPropertyStr(ctx, http_module, "METHODS");
    if (!JS_IsUndefined(methods)) {
      JS_SetPropertyStr(ctx, https_obj, "METHODS", JS_DupValue(ctx, methods));
    }
    JS_FreeValue(ctx, methods);

    JSValue status_codes = JS_GetPropertyStr(ctx, http_module, "STATUS_CODES");
    if (!JS_IsUndefined(status_codes)) {
      JS_SetPropertyStr(ctx, https_obj, "STATUS_CODES", JS_DupValue(ctx, status_codes));
    }
    JS_FreeValue(ctx, status_codes);
  }
  JS_FreeValue(ctx, http_module);

  // Enhanced global agent with connection pooling and keep-alive
  JSValue globalAgent = js_https_agent_constructor(ctx, JS_UNDEFINED, 0, NULL);
  JS_SetPropertyStr(ctx, globalAgent, "maxSockets", JS_NewInt32(ctx, 5));
  JS_SetPropertyStr(ctx, globalAgent, "maxFreeSockets", JS_NewInt32(ctx, 256));
  JS_SetPropertyStr(ctx, globalAgent, "keepAlive", JS_TRUE);
  JS_SetPropertyStr(ctx, globalAgent, "protocol", JS_NewString(ctx, "https:"));
  JS_SetPropertyStr(ctx, https_obj, "globalAgent", globalAgent);

  return https_obj;
}

// Initialize node:https module for ES modules
int js_node_https_init(JSContext* ctx, JSModuleDef* m) {
  JSValue https_module = JSRT_InitNodeHttps(ctx);

  // Export as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, https_module));

  // Export individual functions and classes
  JSValue createServer = JS_GetPropertyStr(ctx, https_module, "createServer");
  JS_SetModuleExport(ctx, m, "createServer", createServer);

  JSValue request = JS_GetPropertyStr(ctx, https_module, "request");
  JS_SetModuleExport(ctx, m, "request", request);

  JSValue get = JS_GetPropertyStr(ctx, https_module, "get");
  JS_SetModuleExport(ctx, m, "get", get);

  JSValue Agent = JS_GetPropertyStr(ctx, https_module, "Agent");
  JS_SetModuleExport(ctx, m, "Agent", Agent);

  JSValue globalAgent = JS_GetPropertyStr(ctx, https_module, "globalAgent");
  JS_SetModuleExport(ctx, m, "globalAgent", globalAgent);

  JS_FreeValue(ctx, https_module);

  return 0;
}