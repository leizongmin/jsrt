#ifndef __JSRT_HTTP_SECURITY_H__
#define __JSRT_HTTP_SECURITY_H__

#include <stdbool.h>

// Security validation results
typedef enum {
  JSRT_HTTP_SECURITY_OK = 0,
  JSRT_HTTP_SECURITY_PROTOCOL_FORBIDDEN,
  JSRT_HTTP_SECURITY_DOMAIN_NOT_ALLOWED,
  JSRT_HTTP_SECURITY_CONTENT_TYPE_INVALID,
  JSRT_HTTP_SECURITY_SIZE_TOO_LARGE,
  JSRT_HTTP_SECURITY_INVALID_URL
} JSRT_HttpSecurityResult;

// Configuration structure
typedef struct {
  bool enabled;
  bool https_only;
  char** allowed_domains;
  size_t allowed_domains_count;
  size_t max_module_size;
  int timeout_ms;
  const char* user_agent;
} JSRT_HttpConfig;

// Initialize HTTP module loading configuration from environment
JSRT_HttpConfig* jsrt_http_config_init(void);

// Free HTTP configuration
void jsrt_http_config_free(JSRT_HttpConfig* config);

// Check if HTTP module loading is enabled
bool jsrt_http_is_enabled(void);

// Validate a URL for security requirements
JSRT_HttpSecurityResult jsrt_http_validate_url(const char* url);

// Check if a domain is in the allowlist
bool jsrt_http_is_domain_allowed(const char* domain);

// Validate HTTP response content
JSRT_HttpSecurityResult jsrt_http_validate_response_content(const char* content_type, size_t content_size);

// Check if URL is HTTP/HTTPS
bool jsrt_is_http_url(const char* url);

// Extract domain from URL
char* jsrt_http_extract_domain(const char* url);

#endif