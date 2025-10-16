#include "../url.h"

// Helper function to create a parameter with proper length handling
JSRT_URLSearchParam* create_url_param(const char* name, size_t name_len, const char* value, size_t value_len) {
  if (!name || !value) {
    return NULL;
  }

  JSRT_URLSearchParam* param = malloc(sizeof(JSRT_URLSearchParam));
  if (!param) {
    return NULL;
  }

  param->name = malloc(name_len + 1);
  if (!param->name) {
    free(param);
    return NULL;
  }

  param->value = malloc(value_len + 1);
  if (!param->value) {
    free(param->name);
    free(param);
    return NULL;
  }

  memcpy(param->name, name, name_len);
  param->name[name_len] = '\0';
  param->name_len = name_len;

  memcpy(param->value, value, value_len);
  param->value[value_len] = '\0';
  param->value_len = value_len;

  param->next = NULL;
  return param;
}

// Helper function to update parent URL's href when URLSearchParams change
void update_parent_url_href(JSRT_URLSearchParams* search_params) {
  if (!search_params || !search_params->parent_url) {
    return;
  }

  JSRT_URL* url = search_params->parent_url;

  // Generate new search string directly from parameters
  JSContext* ctx = search_params->ctx;
  if (!ctx) {
    return;  // No context available for updates
  }

  // Build search string manually to avoid circular references
  char* new_search_str = NULL;
  if (!search_params->params) {
    new_search_str = strdup("");
  } else {
    // Calculate required length
    size_t total_len = 0;
    JSRT_URLSearchParam* param = search_params->params;
    bool first = true;
    while (param) {
      char* encoded_name = url_encode_with_len(param->name, param->name_len);
      char* encoded_value = url_encode_with_len(param->value, param->value_len);
      size_t pair_len = strlen(encoded_name) + strlen(encoded_value) + 1;  // name=value
      if (!first)
        pair_len += 1;  // &
      total_len += pair_len;
      free(encoded_name);
      free(encoded_value);
      param = param->next;
      first = false;
    }

    // Build the string
    new_search_str = malloc(total_len + 1);
    if (!new_search_str) {
      return;  // Cannot update href, memory allocation failed
    }
    new_search_str[0] = '\0';
    param = search_params->params;
    first = true;
    while (param) {
      char* encoded_name = url_encode_with_len(param->name, param->name_len);
      char* encoded_value = url_encode_with_len(param->value, param->value_len);

      if (!first) {
        size_t current_len = strlen(new_search_str);
        size_t remaining = total_len + 1 - current_len;                    // +1 for null terminator
        size_t needed = strlen(encoded_name) + strlen(encoded_value) + 2;  // &name=value + null
        if (needed > remaining) {
          // Buffer overflow protection - shouldn't happen but fail safe
          free(encoded_name);
          free(encoded_value);
          free(new_search_str);
          return;
        }
        snprintf(new_search_str + current_len, remaining, "&%s=%s", encoded_name, encoded_value);
      } else {
        size_t needed = strlen(encoded_name) + strlen(encoded_value) + 2;  // name=value + null
        if (needed > total_len + 1) {
          // Buffer overflow protection - shouldn't happen but fail safe
          free(encoded_name);
          free(encoded_value);
          free(new_search_str);
          return;
        }
        snprintf(new_search_str, total_len + 1, "%s=%s", encoded_name, encoded_value);  // +1 for null terminator
      }

      free(encoded_name);
      free(encoded_value);
      param = param->next;
      first = false;
    }
  }

  // Update URL's search component
  free(url->search);
  if (strlen(new_search_str) > 0) {
    size_t search_len = strlen(new_search_str) + 2;  // +1 for '?', +1 for '\0'
    url->search = malloc(search_len);
    if (!url->search) {
      free(new_search_str);
      return;  // Cannot update search, memory allocation failed
    }
    snprintf(url->search, search_len, "?%s", new_search_str);
  } else {
    url->search = strdup("");
  }

  // Rebuild href
  free(url->href);
  size_t href_len = strlen(url->protocol) + 2 + strlen(url->host) + strlen(url->pathname);
  if (url->search && strlen(url->search) > 0) {
    href_len += strlen(url->search);
  }
  if (url->hash && strlen(url->hash) > 0) {
    href_len += strlen(url->hash);
  }

  url->href = malloc(href_len + 1);
  if (!url->href) {
    free(new_search_str);
    return;  // Cannot update href, memory allocation failed
  }
  int written = snprintf(url->href, href_len + 1, "%s//%s%s", url->protocol, url->host, url->pathname);

  if (url->search && strlen(url->search) > 0) {
    written += snprintf(url->href + written, href_len + 1 - written, "%s", url->search);
  }

  if (url->hash && strlen(url->hash) > 0) {
    snprintf(url->href + written, href_len + 1 - written, "%s", url->hash);
  }

  free(new_search_str);
}

void JSRT_FreeSearchParams(JSRT_URLSearchParams* search_params) {
  if (search_params) {
    JSRT_URLSearchParam* param = search_params->params;
    while (param) {
      JSRT_URLSearchParam* next = param->next;
      free(param->name);
      free(param->value);
      free(param);
      param = next;
    }
    free(search_params);
  }
}

JSRT_URLSearchParams* JSRT_CreateEmptySearchParams(void) {
  JSRT_URLSearchParams* search_params = malloc(sizeof(JSRT_URLSearchParams));
  if (!search_params) {
    return NULL;
  }

  search_params->params = NULL;
  search_params->parent_url = NULL;
  search_params->ctx = NULL;
  return search_params;
}

void JSRT_AddSearchParam(JSRT_URLSearchParams* search_params, const char* name, const char* value) {
  if (!search_params || !name || !value) {
    return;
  }

  size_t name_len = strlen(name);
  size_t value_len = strlen(value);
  JSRT_URLSearchParam* param = create_url_param(name, name_len, value, value_len);

  if (!param) {
    return;  // Failed to create parameter
  }

  // Add at end to maintain insertion order
  if (!search_params->params) {
    search_params->params = param;
  } else {
    JSRT_URLSearchParam* tail = search_params->params;
    while (tail->next) {
      tail = tail->next;
    }
    tail->next = param;
  }
}

// Length-aware version for handling strings with null bytes
void JSRT_AddSearchParamWithLength(JSRT_URLSearchParams* search_params, const char* name, size_t name_len,
                                   const char* value, size_t value_len) {
  if (!search_params || !name || !value) {
    return;
  }

  JSRT_URLSearchParam* param = create_url_param(name, name_len, value, value_len);

  if (!param) {
    return;  // Failed to create parameter
  }

  // Add at end to maintain insertion order
  if (!search_params->params) {
    search_params->params = param;
  } else {
    JSRT_URLSearchParam* tail = search_params->params;
    while (tail->next) {
      tail = tail->next;
    }
    tail->next = param;
  }
}