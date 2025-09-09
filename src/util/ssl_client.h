#ifndef __JSRT_UTIL_SSL_CLIENT_H__
#define __JSRT_UTIL_SSL_CLIENT_H__

#include <stdbool.h>

// Unified SSL function pointers structure
typedef struct {
  // Core SSL functions
  void* (*TLS_client_method)(void);
  void* (*SSL_CTX_new)(const void* method);
  void (*SSL_CTX_free)(void* ctx);
  void* (*SSL_new)(void* ctx);
  void (*SSL_free)(void* ssl);

  // SSL connection functions
  int (*SSL_set_fd)(void* ssl, int fd);
  int (*SSL_connect)(void* ssl);
  int (*SSL_read)(void* ssl, void* buf, int num);
  int (*SSL_write)(void* ssl, const void* buf, int num);
  int (*SSL_shutdown)(void* ssl);
  int (*SSL_get_error)(const void* ssl, int ret);

  // SSL context configuration
  void (*SSL_CTX_set_verify)(void* ctx, int mode, void* verify_callback);
  int (*SSL_CTX_set_default_verify_paths)(void* ctx);
  long (*SSL_CTX_ctrl)(void* ctx, int cmd, long larg, void* parg);

  // SSL connection configuration
  int (*SSL_set_tlsext_host_name)(void* ssl, const char* name);
  long (*SSL_ctrl)(void* ssl, int cmd, long larg, void* parg);

  // Initialization functions
  int (*SSL_library_init)(void);
  void (*SSL_load_error_strings)(void);
} jsrt_ssl_functions_t;

// SSL client abstraction
typedef struct {
  jsrt_ssl_functions_t* ssl_funcs;
  void* ssl_ctx;
  void* ssl;
  bool initialized;
} jsrt_ssl_client_t;

// Initialize the global SSL functions (call once at startup)
// Returns: true on success, false if SSL/TLS not available
bool jsrt_ssl_global_init(void);

// Cleanup global SSL resources
void jsrt_ssl_global_cleanup(void);

// Get the global SSL functions pointer (NULL if not initialized)
jsrt_ssl_functions_t* jsrt_ssl_get_functions(void);

// Check if SSL/TLS is available
bool jsrt_ssl_is_available(void);

// Create a new SSL client context for a connection
// Returns: pointer to SSL client on success, NULL on failure
jsrt_ssl_client_t* jsrt_ssl_client_new(void);

// Free SSL client context
void jsrt_ssl_client_free(jsrt_ssl_client_t* client);

// Set up SSL client for connection to specified hostname
// Returns: 0 on success, -1 on failure
int jsrt_ssl_client_setup(jsrt_ssl_client_t* client, int sockfd, const char* hostname);

// Perform SSL handshake
// Returns: 1 on success, 0 on failure, -1 on would block (for async)
int jsrt_ssl_client_handshake(jsrt_ssl_client_t* client);

// Read data from SSL connection
// Returns: number of bytes read, -1 on error
int jsrt_ssl_client_read(jsrt_ssl_client_t* client, void* buf, int len);

// Write data to SSL connection
// Returns: number of bytes written, -1 on error
int jsrt_ssl_client_write(jsrt_ssl_client_t* client, const void* buf, int len);

// Shutdown SSL connection
// Returns: 0 on success, -1 on error
int jsrt_ssl_client_shutdown(jsrt_ssl_client_t* client);

#endif