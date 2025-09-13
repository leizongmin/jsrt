#ifndef JSRT_URL_H
#define JSRT_URL_H

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../std/encoding.h"
#include "../std/formdata.h"
#include "../util/debug.h"

// Forward declare class IDs
extern JSClassID JSRT_URLClassID;
extern JSClassID JSRT_URLSearchParamsClassID;
extern JSClassID JSRT_URLSearchParamsIteratorClassID;

// External class ID from formdata.c
extern JSClassID JSRT_FormDataClassID;

// Forward declare structures
struct JSRT_URL;

// URLSearchParams parameter structure
typedef struct JSRT_URLSearchParam {
  char* name;
  char* value;
  size_t name_len;   // Length of name (may contain null bytes)
  size_t value_len;  // Length of value (may contain null bytes)
  struct JSRT_URLSearchParam* next;
} JSRT_URLSearchParam;

// URLSearchParams structure
typedef struct JSRT_URLSearchParams {
  JSRT_URLSearchParam* params;
  struct JSRT_URL* parent_url;  // Reference to parent URL for href updates
  JSContext* ctx;               // Context for parent URL updates
} JSRT_URLSearchParams;

// URL component structure
typedef struct JSRT_URL {
  char* href;
  char* protocol;
  char* username;
  char* password;
  char* host;
  char* hostname;
  char* port;
  char* pathname;
  char* search;
  char* hash;
  char* origin;
  JSValue search_params;  // Cached URLSearchParams object
  JSContext* ctx;         // Context for managing search_params
} JSRT_URL;

// URLSearchParams Iterator structure
typedef struct {
  JSRT_URLSearchParams* params;
  JSRT_URLSearchParam* current;
  int type;  // 0=entries, 1=keys, 2=values
} JSRT_URLSearchParamsIterator;

// Function declarations

// Core URL functions
JSRT_URL* JSRT_ParseURL(const char* url, const char* base);
void JSRT_FreeURL(JSRT_URL* url);

// URL validation functions
int is_valid_scheme(const char* scheme);
int validate_url_characters(const char* url);
int validate_hostname_characters(const char* hostname);
int validate_credentials(const char* credentials);

// IP address functions
char* canonicalize_ipv4_address(const char* input);
char* canonicalize_ipv6(const char* ipv6_str);

// URL normalization functions
char* normalize_fullwidth_characters(const char* input);
char* normalize_dot_segments(const char* path);
char* normalize_port(const char* port_str, const char* protocol);
char* strip_url_whitespace(const char* url);
char* remove_all_ascii_whitespace(const char* url);
char* normalize_spaces_in_path(const char* path);
char* normalize_url_backslashes(const char* url);

// URL encoding/decoding functions
char* url_encode_with_len(const char* str, size_t len);
char* url_encode(const char* str);
char* url_component_encode(const char* str);
char* url_fragment_encode(const char* str);
char* url_nonspecial_path_encode(const char* str);
char* url_userinfo_encode(const char* str);
char* url_decode_with_length_and_output_len(const char* str, size_t len, size_t* output_len);
char* url_decode_with_length(const char* str, size_t len);
char* url_decode(const char* str);
char* url_decode_query_with_length_and_output_len(const char* str, size_t len, size_t* output_len);

// URL utility functions
int is_default_port(const char* scheme, const char* port);
int is_special_scheme(const char* protocol);
char* compute_origin(const char* protocol, const char* hostname, const char* port);
int hex_to_int(char c);

// URLSearchParams functions
JSRT_URLSearchParams* JSRT_ParseSearchParams(const char* search_string, size_t string_len);
JSRT_URLSearchParams* JSRT_ParseSearchParamsFromSequence(JSContext* ctx, JSValue seq);
JSRT_URLSearchParams* JSRT_ParseSearchParamsFromRecord(JSContext* ctx, JSValue record);
JSRT_URLSearchParams* JSRT_ParseSearchParamsFromFormData(JSContext* ctx, JSValueConst formdata_val);
JSRT_URLSearchParams* JSRT_CreateEmptySearchParams(void);
void JSRT_AddSearchParam(JSRT_URLSearchParams* search_params, const char* name, const char* value);
void JSRT_AddSearchParamWithLength(JSRT_URLSearchParams* search_params, const char* name, size_t name_len,
                                   const char* value, size_t value_len);
void JSRT_FreeSearchParams(JSRT_URLSearchParams* search_params);
JSRT_URLSearchParam* create_url_param(const char* name, size_t name_len, const char* value, size_t value_len);
void update_parent_url_href(JSRT_URLSearchParams* search_params);

// External functions from encoding.h
int JSRT_ValidateUTF8Sequence(const uint8_t* bytes, size_t len, const uint8_t** next);
char* JSRT_StringToUTF8WithSurrogateReplacement(JSContext* ctx, JSValue str, size_t* len_p);

// URLSearchParams JavaScript API functions
void JSRT_RegisterURLSearchParamsMethods(JSContext* ctx, JSValue proto);

// Runtime setup function
void JSRT_RuntimeSetupStdURL(JSRT_Runtime* rt);

#endif  // JSRT_URL_H