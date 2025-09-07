#include "security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Default allowed domains
static const char* DEFAULT_ALLOWED_DOMAINS[] = {"esm.run",          "esm.sh",    "cdn.skypack.dev",
                                                "cdn.jsdelivr.net", "unpkg.com", NULL};

// Global configuration instance
static JSRT_HttpConfig* g_http_config = NULL;

// Helper function to parse comma-separated domains
static char** parse_domains_string(const char* domains_str, size_t* count) {
  if (!domains_str || !count) {
    return NULL;
  }

  // Count commas to estimate array size
  size_t estimated_count = 1;
  for (const char* p = domains_str; *p; p++) {
    if (*p == ',')
      estimated_count++;
  }

  char** domains = malloc(estimated_count * sizeof(char*));
  if (!domains)
    return NULL;

  *count = 0;
  char* str_copy = strdup(domains_str);
  if (!str_copy) {
    free(domains);
    return NULL;
  }

  char* token = strtok(str_copy, ",");
  while (token && *count < estimated_count) {
    // Skip whitespace
    while (*token == ' ' || *token == '\t')
      token++;

    // Remove trailing whitespace
    char* end = token + strlen(token) - 1;
    while (end > token && (*end == ' ' || *end == '\t')) {
      *end = '\0';
      end--;
    }

    if (*token) {  // Only add non-empty tokens
      domains[*count] = strdup(token);
      if (domains[*count]) {
        (*count)++;
      }
    }
    token = strtok(NULL, ",");
  }

  free(str_copy);
  return domains;
}

JSRT_HttpConfig* jsrt_http_config_init(void) {
  if (g_http_config) {
    return g_http_config;  // Already initialized
  }

  g_http_config = malloc(sizeof(JSRT_HttpConfig));
  if (!g_http_config) {
    return NULL;
  }

  // Initialize with defaults
  g_http_config->enabled = true;
  g_http_config->https_only = true;
  g_http_config->allowed_domains = NULL;
  g_http_config->allowed_domains_count = 0;
  g_http_config->max_module_size = 10 * 1024 * 1024;  // 10MB default
  g_http_config->timeout_ms = 30000;                  // 30 seconds
  g_http_config->user_agent = "jsrt/1.0";

  // Read configuration from environment variables
  const char* enabled = getenv("JSRT_HTTP_MODULES_ENABLED");
  if (enabled) {
    if (strcmp(enabled, "1") == 0 || strcmp(enabled, "true") == 0) {
      g_http_config->enabled = true;
    } else if (strcmp(enabled, "0") == 0 || strcmp(enabled, "false") == 0) {
      g_http_config->enabled = false;
    }
  }

  const char* https_only = getenv("JSRT_HTTP_MODULES_HTTPS_ONLY");
  if (https_only && (strcmp(https_only, "0") == 0 || strcmp(https_only, "false") == 0)) {
    g_http_config->https_only = false;
  }

  // Parse allowed domains
  const char* allowed_domains_env = getenv("JSRT_HTTP_MODULES_ALLOWED");
  if (allowed_domains_env) {
    g_http_config->allowed_domains = parse_domains_string(allowed_domains_env, &g_http_config->allowed_domains_count);
  } else {
    // Use default domains
    size_t default_count = 0;
    while (DEFAULT_ALLOWED_DOMAINS[default_count])
      default_count++;

    g_http_config->allowed_domains = malloc(default_count * sizeof(char*));
    if (g_http_config->allowed_domains) {
      for (size_t i = 0; i < default_count; i++) {
        g_http_config->allowed_domains[i] = strdup(DEFAULT_ALLOWED_DOMAINS[i]);
      }
      g_http_config->allowed_domains_count = default_count;
    }
  }

  // Parse other config values
  const char* max_size = getenv("JSRT_HTTP_MODULES_MAX_SIZE");
  if (max_size) {
    g_http_config->max_module_size = (size_t)atoll(max_size);
  }

  const char* timeout = getenv("JSRT_HTTP_MODULES_TIMEOUT");
  if (timeout) {
    g_http_config->timeout_ms = atoi(timeout) * 1000;  // Convert seconds to milliseconds
  }

  const char* user_agent = getenv("JSRT_HTTP_MODULES_USER_AGENT");
  if (user_agent) {
    g_http_config->user_agent = strdup(user_agent);
  }

  return g_http_config;
}

void jsrt_http_config_free(JSRT_HttpConfig* config) {
  if (!config)
    return;

  if (config->allowed_domains) {
    for (size_t i = 0; i < config->allowed_domains_count; i++) {
      free(config->allowed_domains[i]);
    }
    free(config->allowed_domains);
  }

  if (config->user_agent && config->user_agent != "jsrt/1.0") {
    free((char*)config->user_agent);
  }

  free(config);

  if (config == g_http_config) {
    g_http_config = NULL;
  }
}

bool jsrt_http_is_enabled(void) {
  JSRT_HttpConfig* config = jsrt_http_config_init();
  return config ? config->enabled : false;
}

bool jsrt_is_http_url(const char* url) {
  if (!url)
    return false;
  return strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0;
}

char* jsrt_http_extract_domain(const char* url) {
  if (!jsrt_is_http_url(url)) {
    return NULL;
  }

  const char* start = url;
  if (strncmp(url, "https://", 8) == 0) {
    start = url + 8;
  } else if (strncmp(url, "http://", 7) == 0) {
    start = url + 7;
  }

  // Find the end of domain (first '/' or ':' or end of string)
  const char* end = start;
  while (*end && *end != '/' && *end != ':') {
    end++;
  }

  size_t domain_len = end - start;
  if (domain_len == 0) {
    return NULL;
  }

  char* domain = malloc(domain_len + 1);
  if (!domain)
    return NULL;

  strncpy(domain, start, domain_len);
  domain[domain_len] = '\0';

  return domain;
}

bool jsrt_http_is_domain_allowed(const char* domain) {
  if (!domain)
    return false;

  JSRT_HttpConfig* config = jsrt_http_config_init();
  if (!config || !config->allowed_domains) {
    return false;
  }

  for (size_t i = 0; i < config->allowed_domains_count; i++) {
    if (strcmp(domain, config->allowed_domains[i]) == 0) {
      return true;
    }
  }

  return false;
}

JSRT_HttpSecurityResult jsrt_http_validate_url(const char* url) {
  if (!url) {
    return JSRT_HTTP_SECURITY_INVALID_URL;
  }

  if (!jsrt_is_http_url(url)) {
    return JSRT_HTTP_SECURITY_INVALID_URL;
  }

  JSRT_HttpConfig* config = jsrt_http_config_init();
  if (!config || !config->enabled) {
    return JSRT_HTTP_SECURITY_PROTOCOL_FORBIDDEN;
  }

  // Check protocol restrictions
  if (config->https_only && strncmp(url, "https://", 8) != 0) {
    return JSRT_HTTP_SECURITY_PROTOCOL_FORBIDDEN;
  }

  // Extract and validate domain
  char* domain = jsrt_http_extract_domain(url);
  if (!domain) {
    return JSRT_HTTP_SECURITY_INVALID_URL;
  }

  bool domain_allowed = jsrt_http_is_domain_allowed(domain);
  free(domain);

  if (!domain_allowed) {
    return JSRT_HTTP_SECURITY_DOMAIN_NOT_ALLOWED;
  }

  return JSRT_HTTP_SECURITY_OK;
}

JSRT_HttpSecurityResult jsrt_http_validate_response_content(const char* content_type, size_t content_size) {
  JSRT_HttpConfig* config = jsrt_http_config_init();
  if (!config) {
    return JSRT_HTTP_SECURITY_PROTOCOL_FORBIDDEN;
  }

  // Check size limit
  if (content_size > config->max_module_size) {
    return JSRT_HTTP_SECURITY_SIZE_TOO_LARGE;
  }

  // Check content type for JavaScript
  if (content_type) {
    if (strstr(content_type, "application/javascript") || strstr(content_type, "text/javascript") ||
        strstr(content_type, "application/ecmascript") || strstr(content_type, "text/ecmascript") ||
        strstr(content_type, "text/plain")) {
      return JSRT_HTTP_SECURITY_OK;
    }
    return JSRT_HTTP_SECURITY_CONTENT_TYPE_INVALID;
  }

  // If no content type provided, allow it (many CDNs don't set proper content-type)
  return JSRT_HTTP_SECURITY_OK;
}