#include <ctype.h>
#include "url.h"

// Canonicalize IPv6 address according to RFC 5952
// Handles IPv4-mapped IPv6 addresses like ::127.0.0.1 -> ::7f00:1
// Also handles zero compression like 1:0:: -> 1::
char* canonicalize_ipv6(const char* ipv6_str) {
  if (!ipv6_str) {
    return NULL;
  }

  // Check for empty or invalid IPv6 address patterns - these are invalid
  if (strcmp(ipv6_str, "[]") == 0) {
    return NULL;
  }

  // Check for colon-only IPv6 address [:]
  if (strcmp(ipv6_str, "[:]") == 0) {
    return NULL;
  }

  if (strlen(ipv6_str) < 3) {
    return strdup(ipv6_str);
  }

  // WPT expects IPv4 in IPv6 to be converted to hexadecimal format
  // Remove the preservation logic and use standard normalization

  // Remove brackets if present
  const char* addr_start = ipv6_str;
  const char* addr_end = ipv6_str + strlen(ipv6_str);
  if (ipv6_str[0] == '[') {
    addr_start = ipv6_str + 1;
    if (ipv6_str[strlen(ipv6_str) - 1] == ']') {
      addr_end = ipv6_str + strlen(ipv6_str) - 1;
    }
  }

  // Create a working copy of the address part
  size_t addr_len = addr_end - addr_start;
  char* addr = malloc(addr_len + 1);
  if (!addr) {
    return NULL;
  }
  strncpy(addr, addr_start, addr_len);
  addr[addr_len] = '\0';

  // Validate IPv6 format before parsing
  // A valid IPv6 address must contain at least one colon
  if (!strchr(addr, ':')) {
    free(addr);
    return NULL;
  }

  // Count colon-separated groups to validate IPv6 format
  // A valid IPv6 address can have at most 8 groups (16-bit each)
  // Groups are separated by colons, so max 7 colons in normal form
  // Exception: with :: compression, we can have more flexibility
  int initial_group_count = 1;
  int double_colon_count = 0;
  const char* p = addr;

  while (*p) {
    if (*p == ':') {
      if (*(p + 1) == ':') {
        double_colon_count++;
        if (double_colon_count > 1) {
          // Multiple :: sequences are invalid
          free(addr);
          return NULL;
        }
        p += 2;  // Skip ::
      } else {
        initial_group_count++;
        p++;
      }
    } else {
      p++;
    }
  }

  // Validate group count
  // Check if IPv4 is present - it counts as 2 groups
  int has_ipv4 = strchr(addr, '.') != NULL;
  int effective_group_count = initial_group_count;
  if (has_ipv4) {
    // IPv4 address counts as 2 groups, but we already counted it as 1 in initial_group_count
    effective_group_count += 1;
  }

  if (double_colon_count == 0 && effective_group_count != 8) {
    // Without compression (::), must have exactly 8 groups (counting IPv4 as 2)
    free(addr);
    return NULL;
  } else if (double_colon_count == 1 && effective_group_count > 8) {
    // With compression (::), cannot have more than 8 groups total
    free(addr);
    return NULL;
  }

  // Check for invalid characters (anything other than hex digits, colons, and dots for IPv4)
  // Per WHATWG URL spec, percent-encoded characters are not allowed in IPv6 addresses
  for (size_t i = 0; i < addr_len; i++) {
    char c = addr[i];
    if (!(isxdigit(c) || c == ':' || c == '.')) {
      // Specifically reject percent-encoded sequences in IPv6 addresses
      if (c == '%') {
        // IPv6 addresses must not contain percent-encoded characters
        free(addr);
        return NULL;
      }
      free(addr);
      return NULL;
    }
  }

  // Parse IPv6 address into 16-bit groups
  uint16_t groups[8] = {0};
  int group_count = 0;
  int double_colon_pos = -1;

  // Check for double colon - there can only be one
  char* double_colon = strstr(addr, "::");
  if (double_colon) {
    double_colon_pos = double_colon - addr;

    // Check if there's another double colon after this one (invalid)
    char* second_double_colon = strstr(double_colon + 2, "::");
    if (second_double_colon) {
      free(addr);
      return NULL;  // Invalid - multiple double colons
    }
  }

  // First, count the number of groups without double colon to validate
  // IPv6 addresses can only have max 8 groups, and if no double colon is present,
  // we must have exactly 8 groups or fewer (with IPv4 at the end taking 2 groups)
  if (double_colon_pos == -1) {
    // No double colon - count colons to determine group count
    int colon_count = 0;
    for (size_t i = 0; i < addr_len; i++) {
      if (addr[i] == ':')
        colon_count++;
    }

    // Without double colon, we can have at most 7 colons (for 8 groups)
    // or fewer if IPv4 is at the end
    if (strchr(addr, '.')) {
      // Has IPv4 at end - max 6 colons (6 hex groups + 1 IPv4 group = 8 total)
      // Examples: ::ffff:192.0.2.1 (1 colon), 0:0:0:0:0:0:13.1.68.3 (6 colons)
      if (colon_count > 6) {
        free(addr);
        return NULL;  // Too many groups for IPv6 with IPv4
      }
    } else {
      // No IPv4 - must have exactly 7 colons for 8 groups
      if (colon_count != 7) {
        free(addr);
        return NULL;  // Invalid number of groups for IPv6
      }
    }
  }

  // Parse groups before double colon (if any)
  if (double_colon_pos >= 0) {
    if (double_colon_pos > 0) {
      char* before_dc = malloc(double_colon_pos + 1);
      if (!before_dc) {
        free(addr);
        return NULL;
      }
      strncpy(before_dc, addr, double_colon_pos);
      before_dc[double_colon_pos] = '\0';

      char* token = strtok(before_dc, ":");
      while (token && group_count < 8) {
        groups[group_count++] = (uint16_t)strtoul(token, NULL, 16);
        token = strtok(NULL, ":");
      }
      free(before_dc);
    }

    // Parse groups after double colon
    char* after_dc = addr + double_colon_pos + 2;
    if (strlen(after_dc) > 0) {
      // Check for IPv4 suffix
      if (strchr(after_dc, '.')) {
        // Handle IPv4-mapped IPv6
        char* ipv4_start = strrchr(after_dc, ':');
        if (ipv4_start) {
          ipv4_start++;
        } else {
          ipv4_start = after_dc;
        }

        int a, b, c, d;
        char extra;
        // Use sscanf with %c to detect any trailing characters (like dots)
        int parsed = sscanf(ipv4_start, "%d.%d.%d.%d%c", &a, &b, &c, &d, &extra);
        // Only accept exactly 4 parsed values - any extra characters make it invalid
        if (parsed == 4 && a >= 0 && a <= 255 && b >= 0 && b <= 255 && c >= 0 && c <= 255 && d >= 0 && d <= 255) {
          // Convert IPv4 to two 16-bit groups
          uint16_t high = (a << 8) | b;
          uint16_t low = (c << 8) | d;

          // Parse any hex groups before the IPv4 part
          int after_groups[6];  // Max 6 hex groups before IPv4
          int after_count = 0;

          if (ipv4_start > after_dc) {
            char* before_ipv4 = malloc(ipv4_start - after_dc);
            if (!before_ipv4) {
              free(addr);
              return NULL;
            }
            strncpy(before_ipv4, after_dc, ipv4_start - after_dc - 1);
            before_ipv4[ipv4_start - after_dc - 1] = '\0';

            char* token = strtok(before_ipv4, ":");
            while (token && after_count < 6) {
              after_groups[after_count++] = (uint16_t)strtoul(token, NULL, 16);
              token = strtok(NULL, ":");
            }
            free(before_ipv4);
          }

          // Calculate how many zero groups we need
          int total_after_groups = after_count + 2;  // after_count hex groups + 2 IPv4 groups
          int zeros_needed = 8 - group_count - total_after_groups;

          // Fill in the zero groups
          for (int i = 0; i < zeros_needed && group_count < 8; i++) {
            groups[group_count++] = 0;
          }

          // Add the hex groups after ::
          for (int i = 0; i < after_count && group_count < 8; i++) {
            groups[group_count++] = after_groups[i];
          }

          // Add IPv4 as two groups at the end
          if (group_count < 8)
            groups[group_count++] = high;
          if (group_count < 8)
            groups[group_count++] = low;
        } else {
          // Invalid IPv4 format in IPv6 address
          free(addr);
          return NULL;
        }
      } else {
        // Regular hex groups after ::
        char* token = strtok(after_dc, ":");
        int after_groups[8];
        int after_count = 0;
        while (token && after_count < 8) {
          after_groups[after_count++] = (uint16_t)strtoul(token, NULL, 16);
          token = strtok(NULL, ":");
        }

        // Fill in the zero groups
        int zeros_needed = 8 - group_count - after_count;
        for (int i = 0; i < zeros_needed && group_count < 8; i++) {
          groups[group_count++] = 0;
        }

        // Add the groups after ::
        for (int i = 0; i < after_count && group_count < 8; i++) {
          groups[group_count++] = after_groups[i];
        }
      }
    } else {
      // :: at the end, fill with zeros
      while (group_count < 8) {
        groups[group_count++] = 0;
      }
    }
  } else {
    // No double colon, parse all groups
    // Check if there's an IPv4 address at the end
    if (strchr(addr, '.')) {
      // Find the last colon to separate hex groups from IPv4
      char* ipv4_start = strrchr(addr, ':');
      if (ipv4_start) {
        ipv4_start++;

        // Parse hex groups before IPv4
        char* hex_part = malloc(ipv4_start - addr);
        if (!hex_part) {
          free(addr);
          return NULL;
        }
        strncpy(hex_part, addr, ipv4_start - addr - 1);
        hex_part[ipv4_start - addr - 1] = '\0';

        char* token = strtok(hex_part, ":");
        while (token && group_count < 6) {  // Max 6 hex groups + 2 IPv4 groups = 8
          groups[group_count++] = (uint16_t)strtoul(token, NULL, 16);
          token = strtok(NULL, ":");
        }
        free(hex_part);

        // Parse IPv4 part
        int a, b, c, d;
        char extra;
        // Use sscanf with %c to detect any trailing characters (like dots)
        int parsed = sscanf(ipv4_start, "%d.%d.%d.%d%c", &a, &b, &c, &d, &extra);
        // Only accept exactly 4 parsed values - any extra characters make it invalid
        if (parsed == 4 && a >= 0 && a <= 255 && b >= 0 && b <= 255 && c >= 0 && c <= 255 && d >= 0 && d <= 255) {
          uint16_t high = (a << 8) | b;
          uint16_t low = (c << 8) | d;
          if (group_count < 8)
            groups[group_count++] = high;
          if (group_count < 8)
            groups[group_count++] = low;
        } else {
          // Invalid IPv4 format in IPv6 address
          free(addr);
          return NULL;
        }
      }
    } else {
      // Regular hex groups only
      char* token = strtok(addr, ":");
      while (token && group_count < 8) {
        groups[group_count++] = (uint16_t)strtoul(token, NULL, 16);
        token = strtok(NULL, ":");
      }
    }
  }

  // Find the longest run of zeros for compression
  int best_start = -1, best_len = 0;
  int current_start = -1, current_len = 0;

  for (int i = 0; i < 8; i++) {
    if (groups[i] == 0) {
      if (current_start == -1) {
        current_start = i;
        current_len = 1;
      } else {
        current_len++;
      }
    } else {
      if (current_len > best_len && current_len > 1) {
        best_start = current_start;
        best_len = current_len;
      }
      current_start = -1;
      current_len = 0;
    }
  }

  // Check the final run
  if (current_len > best_len && current_len > 1) {
    best_start = current_start;
    best_len = current_len;
  }

  // Build the canonical form (without brackets for hostname storage)
  char* result = malloc(64);
  if (!result) {
    free(addr);
    return NULL;
  }
  result[0] = '\0';
  size_t result_len = 0;
  const size_t max_len = 40;  // Maximum IPv6 string length

  int i = 0;
  while (i < 8) {
    if (i == best_start && best_len > 1) {
      // Use :: compression
      if (i == 0) {
        result_len += snprintf(result + result_len, max_len - result_len, "::");
      } else {
        result_len += snprintf(result + result_len, max_len - result_len, "::");
      }
      i += best_len;
    } else {
      if (i > 0 && !(i == best_start + best_len && best_len > 1)) {
        result_len += snprintf(result + result_len, max_len - result_len, ":");
      }
      result_len += snprintf(result + result_len, max_len - result_len, "%x", groups[i]);
      i++;
    }
  }

  free(addr);
  return result;
}