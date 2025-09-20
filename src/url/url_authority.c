#include <ctype.h>
#include <errno.h>
#include "url.h"

// Parse authority section: [userinfo@]host[:port]
// Returns 0 on success, -1 on error
int parse_authority(JSRT_URL* parsed, const char* authority_str) {
  if (!authority_str || !parsed) {
    return -1;
  }

#ifdef DEBUG
  fprintf(stderr, "[DEBUG] parse_authority: authority_str='%s'\n", authority_str);
#endif

  char* authority = strdup(authority_str);
  if (!authority) {
    return -1;
  }

  // Track whether authority contains special characters that indicate userinfo/port syntax
  int authority_has_at_sign = (strchr(authority_str, '@') != NULL);
  int authority_has_colon_in_host = 0;  // Will be set later for host-level colons

  char* at_pos = strrchr(authority, '@');  // Use rightmost @ to handle @ in userinfo
  char* host_part = authority;

  if (at_pos) {
    *at_pos = '\0';

    // Special validation: check if what looks like userinfo actually contains an invalid port
    // For URLs like "http://foo.com:b@d/", we need to detect that ":b@" is not valid
    char* colon_in_userinfo = strchr(authority, ':');
    if (colon_in_userinfo) {
      char* potential_port = colon_in_userinfo + 1;

      // Check if this looks like hostname:port@host pattern (which should fail)
      // vs. legitimate user:pass@host pattern
      // The key difference: hostname:port@host has invalid port, user:pass@host has valid password

      // Look at the text before the colon - if it looks like a hostname (contains dots),
      // then the text after colon should be a valid port number
      int looks_like_hostname = 0;
      size_t prefix_len = colon_in_userinfo - authority;
      if (prefix_len > 0) {
        // Simple heuristic: if the prefix contains dots, it's likely a hostname
        for (size_t i = 0; i < prefix_len; i++) {
          if (authority[i] == '.') {
            looks_like_hostname = 1;
            break;
          }
        }
      }

      if (looks_like_hostname) {
        // If prefix looks like hostname, validate that what follows colon is a valid port
        int has_non_digit = 0;
        for (char* p = potential_port; *p; p++) {
          if (!isdigit(*p)) {
            has_non_digit = 1;
            break;
          }
        }

        // If potential port contains non-digits, this is an invalid pattern for hostname:port@host
        // But we should treat it as userinfo instead per WHATWG URL spec
        if (has_non_digit) {
          // This is not hostname:port@host pattern, treat as normal user:pass@host
          // Continue normal parsing
        }
      }
      // If prefix doesn't look like hostname, treat as normal user:pass@host
    }

    // Check if userinfo contains unencoded forward slashes (invalid per WHATWG)
    // If so, drop the userinfo entirely and use only the hostname part
    if (strchr(authority, '/') != NULL) {
      // Invalid userinfo containing forward slashes - drop it completely
      // Use only the hostname part after the '@'
      host_part = at_pos + 1;

      // Clear any existing userinfo since it's invalid
      free(parsed->username);
      parsed->username = strdup("");
      free(parsed->password);
      parsed->password = strdup("");
      parsed->has_password_field = 0;
    } else {
      // Valid userinfo - parse and validate
      char* colon_pos = strchr(authority, ':');
      if (colon_pos) {
        *colon_pos = '\0';

        // Validate username and password for invalid characters
        if (!validate_credentials(authority) || !validate_credentials(colon_pos + 1)) {
          goto cleanup_and_return_error;
        }

        free(parsed->username);
        parsed->username = strdup(authority);
        free(parsed->password);
        parsed->password = strdup(colon_pos + 1);
        parsed->has_password_field = 1;  // Original URL had password field
      } else {
        // Validate username for invalid characters
        if (!validate_credentials(authority)) {
          goto cleanup_and_return_error;
        }

        free(parsed->username);
        parsed->username = strdup(authority);
        parsed->has_password_field = 0;  // No password field in original URL
      }
      host_part = at_pos + 1;
    }
  }

  // Parse host and port
  char* port_colon = NULL;
  int is_ipv6_with_port = 0;

#ifdef DEBUG
  fprintf(stderr, "[DEBUG] parse_authority: about to parse host_part='%s'\n", host_part);
#endif

  // Check if this is IPv6 with port: [ipv6]:port format
  if (host_part[0] == '[') {
    char* close_bracket = strchr(host_part, ']');
    if (close_bracket && *(close_bracket + 1) == ':') {
      // IPv6 with port: [ipv6]:port
      port_colon = close_bracket + 1;
      is_ipv6_with_port = 1;
    }
  } else {
    // Check for invalid IPv6 addresses without brackets in special schemes first
    // Per WHATWG URL spec, IPv6 addresses must be enclosed in brackets for special schemes
    if (parsed->protocol && is_special_scheme(parsed->protocol)) {
      // Simple check for IPv6-like patterns: contains multiple colons
      int colon_count = 0;
      for (const char* p = host_part; *p; p++) {
        if (*p == ':')
          colon_count++;
      }
      // If we have 2+ colons, it's likely an invalid IPv6 address without brackets
      if (colon_count >= 2) {
#ifdef DEBUG
        fprintf(stderr, "[DEBUG] parse_authority: detected invalid IPv6 without brackets, colon_count=%d\n",
                colon_count);
#endif
        goto cleanup_and_return_error;
      }
    }

    // Check for invalid characters in authority section before finding port
    // Per WHATWG URL spec, backslashes are invalid in authority sections
    for (const char* p = host_part; *p; p++) {
      if (*p == '\\') {
        // Backslash in authority section is invalid
#ifdef DEBUG
        fprintf(stderr, "[DEBUG] parse_authority: invalid backslash character found\n");
#endif
        goto cleanup_and_return_error;
      }
    }

    // Regular hostname - find rightmost colon for port
    port_colon = strrchr(host_part, ':');
  }

#ifdef DEBUG
  fprintf(stderr, "[DEBUG] parse_authority: port_colon=%s\n", port_colon ? port_colon : "NULL");
#endif

  // For file URLs, single letter + colon should be treated as drive letter, not port
  int is_file_drive = 0;
  if (strcmp(parsed->protocol, "file:") == 0 && port_colon && !is_ipv6_with_port && (port_colon - host_part) == 1 &&
      isalpha(host_part[0])) {
    is_file_drive = 1;
  }

  // For file URLs, only allow drive letters, not real ports
  if (strcmp(parsed->protocol, "file:") == 0 && port_colon && !is_file_drive) {
    // file URL with port that's not a drive letter is invalid
    goto cleanup_and_return_error;
  }

  if (port_colon && !is_file_drive) {  // Has port and not file drive
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] parse_authority: processing host with port\n");
#endif
    *port_colon = '\0';
    authority_has_colon_in_host = 1;  // Mark that we found colon in host section

    // Handle IPv6 addresses - extract the part inside brackets
    if (is_ipv6_with_port) {
      // For IPv6, host_part is like "[::1]" - we need to extract just the IPv6 part
      if (host_part[0] == '[') {
        char* close_bracket = strchr(host_part, ']');
        if (close_bracket) {
          size_t ipv6_len = close_bracket - host_part - 1;  // Length without brackets
          char* ipv6_part = malloc(ipv6_len + 1);
          if (!ipv6_part) {
            goto cleanup_and_return_error;
          }
          strncpy(ipv6_part, host_part + 1, ipv6_len);  // Skip opening bracket
          ipv6_part[ipv6_len] = '\0';

          // Canonicalize the IPv6 address
          char* canonical_ipv6 = canonicalize_ipv6(ipv6_part);
          if (canonical_ipv6) {
            // Format as [canonical_ipv6] for hostname
            free(parsed->hostname);
            parsed->hostname = malloc(strlen(canonical_ipv6) + 3);
            if (!parsed->hostname) {
              free(canonical_ipv6);
              free(ipv6_part);
              goto cleanup_and_return_error;
            }
            snprintf(parsed->hostname, strlen(canonical_ipv6) + 3, "[%s]", canonical_ipv6);
            free(canonical_ipv6);
          } else {
            // Invalid IPv6 address - reject it entirely
            // Per WHATWG URL spec, IPv6 addresses must not contain percent-encoded characters
            free(ipv6_part);
            goto cleanup_and_return_error;
          }
          free(ipv6_part);
        } else {
          // Pre-validate hostname before decoding to catch invalid percent-encoding
          if (!validate_hostname_characters_with_scheme(host_part, parsed->protocol)) {
            goto cleanup_and_return_error;
          }
          char* decoded_host = url_decode_hostname_with_scheme(host_part, parsed->protocol);
          free(parsed->hostname);
          parsed->hostname = decoded_host ? decoded_host : strdup(host_part);
        }
      } else {
        // Pre-validate hostname before decoding to catch invalid percent-encoding
        if (!validate_hostname_characters_with_scheme(host_part, parsed->protocol)) {
#ifdef DEBUG
          fprintf(stderr, "[DEBUG] parse_authority: hostname validation failed for '%s'\n", host_part);
#endif
          goto cleanup_and_return_error;
        }
        char* decoded_host = url_decode_hostname_with_scheme(host_part, parsed->protocol);
        free(parsed->hostname);
        parsed->hostname = decoded_host ? decoded_host : strdup(host_part);
        if (!parsed->hostname) {
          goto cleanup_and_return_error;
        }
      }
    } else {
      // Decode percent-encoded hostname first
      char* decoded_host = url_decode_hostname_with_scheme(host_part, parsed->protocol);
      if (!decoded_host) {
#ifdef DEBUG
        fprintf(stderr, "[DEBUG] parse_authority: url_decode_hostname_with_scheme failed for '%s'\n", host_part);
#endif
        goto cleanup_and_return_error;
      }

      // For file URLs, convert pipe to colon in hostname
      char* final_hostname;
      if (strcmp(parsed->protocol, "file:") == 0 && strchr(decoded_host, '|')) {
        size_t len = strlen(decoded_host);
        char* converted_host = malloc(len + 1);
        if (!converted_host) {
          free(decoded_host);
          goto cleanup_and_return_error;
        }
        for (size_t i = 0; i < len; i++) {
          converted_host[i] = (decoded_host[i] == '|') ? ':' : decoded_host[i];
        }
        converted_host[len] = '\0';
        final_hostname = converted_host;
        free(decoded_host);
      } else {
        final_hostname = decoded_host;
      }

      // Store the final hostname
      free(parsed->hostname);
      parsed->hostname = final_hostname;
    }

    // Parse port, handling leading zeros
    char* port_str = port_colon + 1;
    if (strlen(port_str) > 0) {
      // Validate port string - check if it contains only digits
      int has_invalid_chars = 0;
      for (char* p = port_str; *p; p++) {
        if (!isdigit(*p)) {
          has_invalid_chars = 1;
          break;
        }
      }

      // Per WHATWG URL spec: ports with invalid characters should cause failure
      // Only numeric ports in range 0-65535 are valid
      if (has_invalid_chars) {
        goto cleanup_and_return_error;
      }

      // Check for port number range (0-65535)
      char* endptr;
      errno = 0;
      long port_long = strtol(port_str, &endptr, 10);

      // Check for conversion errors or out of range - these should cause URL failure
      if (errno == ERANGE || port_long < 0 || port_long > 65535 || *endptr != '\0') {
        goto cleanup_and_return_error;
      }

      // Remove leading zeros but keep at least one digit
      while (*port_str == '0' && *(port_str + 1) != '\0') {
        port_str++;
      }

      // Convert to number and back to remove leading zeros completely
      int port_num = (int)port_long;
      char normalized_port[8];
      snprintf(normalized_port, sizeof(normalized_port), "%d", port_num);

      // Extract scheme from protocol for default port check
      char* scheme = strdup(parsed->protocol);
      if (!scheme) {
        goto cleanup_and_return_error;
      }
      char* colon = strchr(scheme, ':');
      if (colon)
        *colon = '\0';

      // Check if port is default port (port 0 should be kept as per WHATWG spec)
      if (is_default_port(scheme, normalized_port)) {
        free(parsed->port);
        parsed->port = strdup("");
      } else {
        free(parsed->port);
        parsed->port = strdup(normalized_port);
      }

      free(scheme);
    }
  } else {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] parse_authority: processing host without port\n");
#endif
    // Handle IPv6 addresses without port
    if (host_part[0] == '[' && strchr(host_part, ']')) {
      char* close_bracket = strchr(host_part, ']');
      if (close_bracket && *(close_bracket + 1) == '\0') {
        // IPv6 without port: [ipv6]
        size_t ipv6_len = close_bracket - host_part - 1;  // Length without brackets
        char* ipv6_part = malloc(ipv6_len + 1);
        strncpy(ipv6_part, host_part + 1, ipv6_len);  // Skip opening bracket
        ipv6_part[ipv6_len] = '\0';

        // Canonicalize the IPv6 address
        char* canonical_ipv6 = canonicalize_ipv6(ipv6_part);
        char* final_hostname;
        if (canonical_ipv6) {
          // Format as [canonical_ipv6] for hostname
          final_hostname = malloc(strlen(canonical_ipv6) + 3);
          if (!final_hostname) {
            free(canonical_ipv6);
            free(ipv6_part);
            goto cleanup_and_return_error;
          }
          snprintf(final_hostname, strlen(canonical_ipv6) + 3, "[%s]", canonical_ipv6);
          free(canonical_ipv6);
        } else {
          // Invalid IPv6 address - reject it entirely
          // Per WHATWG URL spec, IPv6 addresses must not contain percent-encoded characters
          free(ipv6_part);
          goto cleanup_and_return_error;
        }
        free(parsed->hostname);
        parsed->hostname = final_hostname;
        free(ipv6_part);
      } else {
        // Pre-validate hostname before decoding to catch invalid percent-encoding
        if (!validate_hostname_characters_with_scheme(host_part, parsed->protocol)) {
#ifdef DEBUG
          fprintf(stderr, "[DEBUG] parse_authority: hostname validation failed for '%s'\n", host_part);
#endif
          goto cleanup_and_return_error;
        }

        char* decoded_host = url_decode_hostname_with_scheme(host_part, parsed->protocol);
        char* final_hostname = decoded_host ? decoded_host : strdup(host_part);
        free(parsed->hostname);
        parsed->hostname = final_hostname;
      }
    } else {
      // Decode percent-encoded hostname first
      char* decoded_host = url_decode_hostname_with_scheme(host_part, parsed->protocol);
      if (!decoded_host) {
#ifdef DEBUG
        fprintf(stderr, "[DEBUG] parse_authority: url_decode_hostname_with_scheme failed for '%s'\n", host_part);
#endif
        goto cleanup_and_return_error;
      }

      // For file URLs, convert pipe to colon in hostname
      char* final_hostname;
      if (strcmp(parsed->protocol, "file:") == 0 && strchr(decoded_host, '|')) {
        size_t len = strlen(decoded_host);
        char* converted_host = malloc(len + 1);
        for (size_t i = 0; i < len; i++) {
          converted_host[i] = (decoded_host[i] == '|') ? ':' : decoded_host[i];
        }
        converted_host[len] = '\0';
        final_hostname = converted_host;
        free(decoded_host);
      } else {
        final_hostname = decoded_host;
      }

      // Only replace hostname after successful processing
      free(parsed->hostname);
      parsed->hostname = final_hostname;
    }
  }

  // Common hostname processing for both branches (with and without port)
  // Apply Unicode normalization to hostname (fullwidth -> halfwidth, case normalization)
  // But skip it for Windows drive letters to preserve case

  // Safety check: ensure hostname is valid before processing
  if (!parsed->hostname) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] parse_authority: hostname is NULL after processing\n");
#endif
    goto cleanup_and_return_error;
  }

#ifdef DEBUG
  fprintf(stderr, "[DEBUG] parse_authority: hostname after processing='%s'\n", parsed->hostname);
#endif

  int is_windows_drive = 0;
  if (parsed->protocol && strcmp(parsed->protocol, "file:") == 0 && parsed->hostname && strlen(parsed->hostname) >= 2) {
    if (isalpha(parsed->hostname[0]) && (parsed->hostname[1] == ':' || parsed->hostname[1] == '|') &&
        (strlen(parsed->hostname) == 2 || parsed->hostname[2] == '/' || parsed->hostname[2] == '\0')) {
      is_windows_drive = 1;
    }
  }

  char* current_hostname = parsed->hostname;
  char* final_processed_hostname = current_hostname;

  if (!is_windows_drive) {
    // For non-special schemes, preserve ASCII case; for special schemes, normalize to lowercase
    int preserve_case = !is_special_scheme(parsed->protocol);
    char* normalized_hostname = normalize_hostname_unicode_with_case(current_hostname, preserve_case);
    if (normalized_hostname) {
      if (final_processed_hostname != current_hostname) {
        free(final_processed_hostname);
      }
      final_processed_hostname = normalized_hostname;
    }
  }

  // Validate hostname characters (including Unicode validation)
#ifdef DEBUG
  fprintf(stderr, "[DEBUG] parse_authority: validating hostname characters for '%s'\n", final_processed_hostname);
#endif
  if (!validate_hostname_characters_with_scheme_and_port(final_processed_hostname, parsed->protocol,
                                                         (port_colon && !is_file_drive) ? 1 : 0)) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] parse_authority: hostname character validation FAILED for '%s'\n",
            final_processed_hostname);
#endif
    if (final_processed_hostname != current_hostname) {
      free(final_processed_hostname);
    }
    goto cleanup_and_return_error;
  }

  // Convert Unicode hostname to ASCII (Punycode) per WHATWG URL spec
  // Only apply IDN conversion for special schemes (http, https, ws, wss, ftp, file)
  // For non-special schemes, Unicode should be percent-encoded instead
  // Skip IDNA conversion for IPv6 addresses (enclosed in brackets) and Windows drive letters
  int should_apply_idna = is_special_scheme(parsed->protocol);
  if (should_apply_idna && !is_windows_drive &&
      !(final_processed_hostname[0] == '[' && strchr(final_processed_hostname, ']'))) {
    char* ascii_hostname = hostname_to_ascii_with_case(final_processed_hostname, 0);  // Always lowercase for IDN
    if (ascii_hostname) {
      if (final_processed_hostname != current_hostname) {
        free(final_processed_hostname);
      }
      final_processed_hostname = ascii_hostname;
    }
    // Note: If hostname_to_ascii fails, we continue with the original hostname
    // This allows for graceful handling of invalid Unicode domains
  } else if (!should_apply_idna && !is_windows_drive &&
             !(final_processed_hostname[0] == '[' && strchr(final_processed_hostname, ']'))) {
    // For non-special schemes, percent-encode Unicode characters in hostname
    char* encoded_hostname = url_component_encode(final_processed_hostname);
    if (encoded_hostname) {
      if (final_processed_hostname != current_hostname) {
        free(final_processed_hostname);
      }
      final_processed_hostname = encoded_hostname;
    }
  }

  // Try to canonicalize as IPv4 address if it looks like one
#ifdef DEBUG
  fprintf(stderr, "[DEBUG] parse_authority: attempting IPv4 canonicalization for '%s'\n", final_processed_hostname);
#endif
  char* ipv4_canonical = canonicalize_ipv4_address(final_processed_hostname);
  if (ipv4_canonical) {
    // Replace hostname with canonical IPv4 form
    if (final_processed_hostname != current_hostname) {
      free(final_processed_hostname);
    }
    final_processed_hostname = ipv4_canonical;
  } else if (looks_like_ipv4_address(final_processed_hostname)) {
    // Hostname looks like IPv4 but failed canonicalization - this means invalid IPv4
    // According to WHATWG URL spec, this should fail URL parsing
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] parse_authority: hostname looks like IPv4 but canonicalization failed for '%s'\n",
            final_processed_hostname);
#endif
    if (final_processed_hostname != current_hostname) {
      free(final_processed_hostname);
    }
    goto cleanup_and_return_error;
  } else {
    // Check for invalid IPv6 addresses without brackets in special schemes
    // Per WHATWG URL spec, IPv6 addresses must be enclosed in brackets for special schemes
    if (parsed->protocol && is_special_scheme(parsed->protocol)) {
      // Simple check for IPv6-like patterns: contains multiple colons
      int colon_count = 0;
      for (const char* p = final_processed_hostname; *p; p++) {
        if (*p == ':')
          colon_count++;
      }
      // If we have 2+ colons and no brackets, it's likely an invalid IPv6 address
      if (colon_count >= 2 && final_processed_hostname[0] != '[') {
        if (final_processed_hostname != current_hostname) {
          free(final_processed_hostname);
        }
        goto cleanup_and_return_error;
      }
    }

    // Not an IPv4 address - check for special cases before lowercasing
    int should_lowercase = 1;

    // For non-special schemes, preserve hostname case per WHATWG URL spec
    if (parsed->protocol && !is_special_scheme(parsed->protocol)) {
      should_lowercase = 0;  // Preserve case for non-special schemes
    }

    // For file URLs, check if hostname is actually a Windows drive letter
    if (parsed->protocol && strcmp(parsed->protocol, "file:") == 0 && final_processed_hostname &&
        strlen(final_processed_hostname) >= 2) {
      // Check for patterns like "C:" or "C|" that should be treated as drive letters
      if (isalpha(final_processed_hostname[0]) &&
          (final_processed_hostname[1] == ':' || final_processed_hostname[1] == '|') &&
          (strlen(final_processed_hostname) == 2 || final_processed_hostname[2] == '/' ||
           final_processed_hostname[2] == '\0')) {
        should_lowercase = 0;  // Preserve case for Windows drive letters
      }
    }
    if (should_lowercase) {
      // Normalize hostname case to lowercase (required for DNS hostnames per WHATWG URL spec)
      for (size_t i = 0; final_processed_hostname[i]; i++) {
        final_processed_hostname[i] = tolower(final_processed_hostname[i]);
      }
    }
  }

  // Special handling for file URLs with localhost
  if (strcmp(parsed->protocol, "file:") == 0 && strcmp(final_processed_hostname, "localhost") == 0) {
    // For file URLs, localhost should be normalized to empty hostname
    if (final_processed_hostname != current_hostname) {
      free(final_processed_hostname);
    }
    final_processed_hostname = strdup("");
    if (!final_processed_hostname) {
      goto cleanup_and_return_error;
    }
  }

  // Finally, replace the original hostname with the processed one
  if (final_processed_hostname != current_hostname) {
    free(parsed->hostname);
    parsed->hostname = final_processed_hostname;
  }

  // Enhanced validation for URLs with empty hostnames per WHATWG URL spec
  if (strlen(parsed->hostname) == 0) {
    int has_userinfo = (strlen(parsed->username) > 0 || strlen(parsed->password) > 0 || parsed->has_password_field);
    int has_port = (strlen(parsed->port) > 0);

    // Case 1: Special schemes (except file:) MUST have non-empty hostnames
    if (is_special_scheme(parsed->protocol) && strcmp(parsed->protocol, "file:") != 0) {
      goto cleanup_and_return_error;  // Empty host for special scheme (not file:) is invalid
    }

    // Case 2: Any URL with userinfo but empty hostname is invalid (per WPT tests)
    // URLs like "sc://@/", "sc://user@/", "sc://:pass@/" should fail
    if (has_userinfo) {
      goto cleanup_and_return_error;
    }

    // Case 3: Any URL with port but empty hostname is invalid (per WPT tests)
    // URLs like "sc://:/", "data://:443" should fail
    if (has_port) {
      goto cleanup_and_return_error;
    }

    // Case 4: Authority contained @ or : but resulted in empty hostname - this is invalid per WPT
    // This catches cases like "sc://@/" (has @) and "sc://:/" (has :)
    if (authority_has_at_sign) {
      goto cleanup_and_return_error;  // Empty hostname after @ is invalid
    }

    if (authority_has_colon_in_host) {
      goto cleanup_and_return_error;  // Empty hostname with colon is invalid
    }
  }

  // Set host field
  free(parsed->host);
  if (strlen(parsed->port) > 0) {
    size_t host_len = strlen(parsed->hostname) + strlen(parsed->port) + 2;
    parsed->host = malloc(host_len);
    if (!parsed->host) {
      goto cleanup_and_return_error;
    }
    snprintf(parsed->host, host_len, "%s:%s", parsed->hostname, parsed->port);
  } else {
    parsed->host = strdup(parsed->hostname);
    if (!parsed->host) {
      goto cleanup_and_return_error;
    }
  }

  free(authority);
  return 0;

cleanup_and_return_error:
#ifdef DEBUG
  fprintf(stderr, "[DEBUG] parse_authority: cleanup_and_return_error - authority parsing failed\n");
#endif
  free(authority);
  // Note: Other allocated variables are either:
  // 1. Stored in parsed structure (ownership transferred)
  // 2. Freed immediately after use within their scope
  // 3. Freed in their specific error handling blocks
  // The main authority string is the only one that needs global cleanup
  return -1;
}

// Parse userinfo@host:port format with empty userinfo (like ::@host:port)
// Returns 0 on success, -1 on error
int parse_empty_userinfo_authority(JSRT_URL* parsed, const char* str) {
  if (!str || !parsed) {
    return -1;
  }

  char* input = strdup(str);
  char* at_pos = strchr(input, '@');
  if (!at_pos) {
    free(input);
    return -1;
  }

  *at_pos = '\0';

  // Parse userinfo (everything before @)
  char* colon_in_userinfo = strchr(input, ':');
  if (colon_in_userinfo) {
    *colon_in_userinfo = '\0';
    free(parsed->username);
    parsed->username = strdup(input);
    if (!parsed->username) {
      free(input);
      return -1;
    }
    free(parsed->password);
    parsed->password = strdup(colon_in_userinfo + 1);
    if (!parsed->password) {
      free(input);
      return -1;
    }
    parsed->has_password_field = 1;
  } else {
    free(parsed->username);
    parsed->username = strdup(input);
    if (!parsed->username) {
      free(input);
      return -1;
    }
    parsed->has_password_field = 0;
  }

  // Parse host:port part after @
  char* host_part = at_pos + 1;
  char* port_colon = strchr(host_part, ':');
  if (port_colon) {
    *port_colon = '\0';
    free(parsed->hostname);
    parsed->hostname = strdup(host_part);

    // Parse port
    char* port_str = port_colon + 1;
    char* normalized_port = normalize_port(port_str, parsed->protocol);
    if (normalized_port) {
      free(parsed->port);
      parsed->port = normalized_port;

      // Set host to hostname:port if port is non-empty (non-default)
      free(parsed->host);
      if (strlen(normalized_port) > 0) {
        size_t host_len = strlen(host_part) + strlen(normalized_port) + 2;
        parsed->host = malloc(host_len);
        if (!parsed->host) {
          free(input);
          return -1;
        }
        snprintf(parsed->host, host_len, "%s:%s", host_part, normalized_port);
      } else {
        parsed->host = strdup(host_part);
        if (!parsed->host) {
          free(input);
          return -1;
        }
      }
    } else {
      // Invalid port
      free(input);
      return -1;
    }
  } else {
    free(parsed->hostname);
    parsed->hostname = strdup(host_part);
    if (!parsed->hostname) {
      free(input);
      return -1;
    }
    free(parsed->host);
    parsed->host = strdup(host_part);
    if (!parsed->host) {
      free(input);
      return -1;
    }
  }

  free(input);
  return 0;
}

// Find the end of authority section in a URL string
// Returns pointer to the end of authority, or end of string if no path/query/fragment
char* find_authority_end(const char* ptr, const char* rightmost_at) {
  char* authority_end;

  if (rightmost_at) {
    // Found '@' - authority extends to end of hostname after the '@'
    char* hostname_start = (char*)(rightmost_at + 1);
    authority_end = hostname_start;

    // Find end of hostname (before port, path, query, or fragment)
    while (*authority_end && *authority_end != '/' && *authority_end != '?' && *authority_end != '#' &&
           *authority_end != ':') {
      authority_end++;
    }

    // Handle port if present
    if (*authority_end == ':') {
      authority_end++;  // Skip the colon
      // Skip port number
      while (*authority_end && *authority_end != '/' && *authority_end != '?' && *authority_end != '#') {
        authority_end++;
      }
    }
  } else {
    // No '@' found - authority ends at first path separator
    authority_end = (char*)ptr;
    while (*authority_end && *authority_end != '/' && *authority_end != '?' && *authority_end != '#') {
      authority_end++;
    }
  }

  return authority_end;
}