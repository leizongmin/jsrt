#include "ssl_client.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform-specific includes for dynamic loading
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Global SSL state
static jsrt_ssl_functions_t g_ssl_funcs;
static bool g_ssl_initialized = false;

// External OpenSSL handle from crypto setup
#ifdef _WIN32
extern HMODULE openssl_handle;
#else
extern void* openssl_handle;
#endif

// Platform-specific symbol loading
#ifdef _WIN32
#define JSRT_DLSYM(handle, name) ((void*)GetProcAddress(handle, name))
#else
#define JSRT_DLSYM(handle, name) dlsym(handle, name)
#endif

bool jsrt_ssl_global_init(void) {
  if (g_ssl_initialized) {
    return true;
  }

  // Check if OpenSSL handle is available (from crypto setup)
  if (!openssl_handle) {
    JSRT_Debug("SSL_Client: OpenSSL handle not available");
    return false;
  }

  // Load SSL function pointers
  g_ssl_funcs.SSL_library_init = (int (*)(void))JSRT_DLSYM(openssl_handle, "SSL_library_init");
  g_ssl_funcs.SSL_load_error_strings = (void (*)(void))JSRT_DLSYM(openssl_handle, "SSL_load_error_strings");
  g_ssl_funcs.TLS_client_method = (void* (*)(void))JSRT_DLSYM(openssl_handle, "TLS_client_method");
  g_ssl_funcs.SSL_CTX_new = (void* (*)(const void*))JSRT_DLSYM(openssl_handle, "SSL_CTX_new");
  g_ssl_funcs.SSL_CTX_free = (void (*)(void*))JSRT_DLSYM(openssl_handle, "SSL_CTX_free");
  g_ssl_funcs.SSL_new = (void* (*)(void*))JSRT_DLSYM(openssl_handle, "SSL_new");
  g_ssl_funcs.SSL_free = (void (*)(void*))JSRT_DLSYM(openssl_handle, "SSL_free");
  g_ssl_funcs.SSL_set_fd = (int (*)(void*, int))JSRT_DLSYM(openssl_handle, "SSL_set_fd");
  g_ssl_funcs.SSL_connect = (int (*)(void*))JSRT_DLSYM(openssl_handle, "SSL_connect");
  g_ssl_funcs.SSL_read = (int (*)(void*, void*, int))JSRT_DLSYM(openssl_handle, "SSL_read");
  g_ssl_funcs.SSL_write = (int (*)(void*, const void*, int))JSRT_DLSYM(openssl_handle, "SSL_write");
  g_ssl_funcs.SSL_shutdown = (int (*)(void*))JSRT_DLSYM(openssl_handle, "SSL_shutdown");
  g_ssl_funcs.SSL_get_error = (int (*)(const void*, int))JSRT_DLSYM(openssl_handle, "SSL_get_error");
  g_ssl_funcs.SSL_CTX_set_verify = (void (*)(void*, int, void*))JSRT_DLSYM(openssl_handle, "SSL_CTX_set_verify");
  g_ssl_funcs.SSL_CTX_set_default_verify_paths =
      (int (*)(void*))JSRT_DLSYM(openssl_handle, "SSL_CTX_set_default_verify_paths");
  g_ssl_funcs.SSL_CTX_ctrl = (long (*)(void*, int, long, void*))JSRT_DLSYM(openssl_handle, "SSL_CTX_ctrl");
  g_ssl_funcs.SSL_set_tlsext_host_name =
      (int (*)(void*, const char*))JSRT_DLSYM(openssl_handle, "SSL_set_tlsext_host_name");
  g_ssl_funcs.SSL_ctrl = (long (*)(void*, int, long, void*))JSRT_DLSYM(openssl_handle, "SSL_ctrl");

  // Initialize SSL library
  if (g_ssl_funcs.SSL_library_init) {
    g_ssl_funcs.SSL_library_init();
  }
  if (g_ssl_funcs.SSL_load_error_strings) {
    g_ssl_funcs.SSL_load_error_strings();
  }

  // Check if we have the essential functions
  if (!g_ssl_funcs.TLS_client_method || !g_ssl_funcs.SSL_CTX_new || !g_ssl_funcs.SSL_new || !g_ssl_funcs.SSL_connect ||
      !g_ssl_funcs.SSL_read || !g_ssl_funcs.SSL_write) {
    JSRT_Debug("SSL_Client: Failed to load essential SSL functions");
    return false;
  }

  g_ssl_initialized = true;
  JSRT_Debug("SSL_Client: SSL functions loaded successfully");
  return true;
}

void jsrt_ssl_global_cleanup(void) {
  if (g_ssl_initialized) {
    memset(&g_ssl_funcs, 0, sizeof(g_ssl_funcs));
    g_ssl_initialized = false;
  }
}

jsrt_ssl_functions_t* jsrt_ssl_get_functions(void) {
  return g_ssl_initialized ? &g_ssl_funcs : NULL;
}

bool jsrt_ssl_is_available(void) {
  return g_ssl_initialized;
}

jsrt_ssl_client_t* jsrt_ssl_client_new(void) {
  if (!g_ssl_initialized) {
    return NULL;
  }

  jsrt_ssl_client_t* client = malloc(sizeof(jsrt_ssl_client_t));
  if (!client) {
    return NULL;
  }

  memset(client, 0, sizeof(jsrt_ssl_client_t));
  client->ssl_funcs = &g_ssl_funcs;

  return client;
}

void jsrt_ssl_client_free(jsrt_ssl_client_t* client) {
  if (!client) {
    return;
  }

  // Shutdown and free SSL connection
  if (client->ssl) {
    if (client->ssl_funcs->SSL_shutdown) {
      client->ssl_funcs->SSL_shutdown(client->ssl);
    }
    if (client->ssl_funcs->SSL_free) {
      client->ssl_funcs->SSL_free(client->ssl);
    }
  }

  // Free SSL context
  if (client->ssl_ctx && client->ssl_funcs->SSL_CTX_free) {
    client->ssl_funcs->SSL_CTX_free(client->ssl_ctx);
  }

  free(client);
}

int jsrt_ssl_client_setup(jsrt_ssl_client_t* client, int sockfd, const char* hostname) {
  if (!client || !client->ssl_funcs) {
    return -1;
  }

  // Create SSL method
  void* method = client->ssl_funcs->TLS_client_method();
  if (!method) {
    JSRT_Debug("SSL_Client: Failed to get TLS client method");
    return -1;
  }

  // Create SSL context
  client->ssl_ctx = client->ssl_funcs->SSL_CTX_new(method);
  if (!client->ssl_ctx) {
    JSRT_Debug("SSL_Client: Failed to create SSL context");
    return -1;
  }

  // Set default verification paths
  if (client->ssl_funcs->SSL_CTX_set_default_verify_paths) {
    client->ssl_funcs->SSL_CTX_set_default_verify_paths(client->ssl_ctx);
  }

  // Set verification mode (optional - don't require certificate verification)
  if (client->ssl_funcs->SSL_CTX_set_verify) {
    client->ssl_funcs->SSL_CTX_set_verify(client->ssl_ctx, 0, NULL);  // SSL_VERIFY_NONE = 0
  }

  // Create SSL connection
  client->ssl = client->ssl_funcs->SSL_new(client->ssl_ctx);
  if (!client->ssl) {
    JSRT_Debug("SSL_Client: Failed to create SSL connection");
    return -1;
  }

  // Set socket file descriptor
  if (client->ssl_funcs->SSL_set_fd(client->ssl, sockfd) != 1) {
    JSRT_Debug("SSL_Client: Failed to set SSL file descriptor");
    return -1;
  }

  // Set SNI hostname if available
  if (hostname && client->ssl_funcs->SSL_set_tlsext_host_name) {
    client->ssl_funcs->SSL_set_tlsext_host_name(client->ssl, hostname);
  }

  client->initialized = true;
  return 0;
}

int jsrt_ssl_client_handshake(jsrt_ssl_client_t* client) {
  if (!client || !client->initialized || !client->ssl) {
    return -1;
  }

  int result = client->ssl_funcs->SSL_connect(client->ssl);
  if (result == 1) {
    return 1;  // Success
  }

  if (client->ssl_funcs->SSL_get_error) {
    int error = client->ssl_funcs->SSL_get_error(client->ssl, result);
    // For async operations, you might want to handle SSL_ERROR_WANT_READ/WRITE
    JSRT_Debug("SSL_Client: Handshake failed with error %d", error);
  }

  return 0;  // Failure
}

int jsrt_ssl_client_read(jsrt_ssl_client_t* client, void* buf, int len) {
  if (!client || !client->ssl) {
    return -1;
  }

  return client->ssl_funcs->SSL_read(client->ssl, buf, len);
}

int jsrt_ssl_client_write(jsrt_ssl_client_t* client, const void* buf, int len) {
  if (!client || !client->ssl) {
    return -1;
  }

  return client->ssl_funcs->SSL_write(client->ssl, buf, len);
}

int jsrt_ssl_client_shutdown(jsrt_ssl_client_t* client) {
  if (!client || !client->ssl) {
    return -1;
  }

  return client->ssl_funcs->SSL_shutdown(client->ssl);
}