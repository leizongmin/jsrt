---
Created: 2025-10-07T19:00:00Z
Last Updated: 2025-10-07T19:00:00Z
Status: 📋 PLANNING - Ready for Implementation
Overall Progress: 0/75 tasks (0%)
API Coverage: 20% (2/10 APIs implemented - WHATWG only)
Code Reuse: 85% from existing src/url/ implementation
---

# Node.js url Module Implementation Plan

## 🎯 Executive Summary

### Objective
Implement complete Node.js-compatible `node:url` module in jsrt by maximally reusing the existing WPT-compliant URL infrastructure from `src/url/` (97% WPT pass rate).

### Current Status
- ✅ **WHATWG URL API**: Complete (URL class, URLSearchParams class)
- ✅ **WPT Compliance**: 97% (29/32 tests passing)
- ❌ **Legacy API**: Not implemented (parse, format, resolve)
- ❌ **Utility Functions**: Not implemented (domainToASCII, fileURLToPath, etc.)
- ❌ **Node.js module**: Not registered as node:url

### Key Success Factors
1. **Maximum Code Reuse**: 85% of functionality already exists in src/url/
2. **Dual API Support**: Both WHATWG (URL/URLSearchParams) and Legacy (parse/format/resolve)
3. **Direct Export**: URL and URLSearchParams can be exported directly
4. **Wrapper Pattern**: Legacy APIs wrap existing parsing/encoding functions
5. **Simple Integration**: Add node_url.c + register in node_modules.c

### Implementation Strategy
- **Phase 1**: Module setup + WHATWG API export (4-5 hours) - export existing classes
- **Phase 2**: Legacy parse/format/resolve (8-10 hours) - adapt existing parsers
- **Phase 3**: Utility functions (6-8 hours) - domain conversion, file URL helpers
- **Phase 4**: Testing and edge cases (6-8 hours) - comprehensive test suite
- **Phase 5**: Documentation and optimization (2-3 hours) - polish and docs
- **Total Estimated Time**: 26-34 hours

---

## 📊 API Coverage Matrix

### Complete API Inventory (10 API Categories)

| API Category | Status | Priority | Complexity | Code Reuse | Est. Time |
|-------------|--------|----------|------------|------------|-----------|
| **1. URL class** | ✅ COMPLETE | CRITICAL | COMPLEX | 100% (export) | 30min |
| **2. URLSearchParams class** | ✅ COMPLETE | CRITICAL | COMPLEX | 100% (export) | 30min |
| **3. url.parse()** | ❌ MISSING | HIGH | MEDIUM | 80% (adapt parser) | 4-5h |
| **4. url.format()** | ❌ MISSING | HIGH | MEDIUM | 70% (build from components) | 3-4h |
| **5. url.resolve()** | ❌ MISSING | HIGH | SIMPLE | 90% (use url_relative.c) | 2-3h |
| **6. url.domainToASCII()** | ❌ MISSING | MEDIUM | SIMPLE | 100% (hostname_to_ascii) | 1h |
| **7. url.domainToUnicode()** | ❌ MISSING | MEDIUM | SIMPLE | 100% (normalize_hostname) | 1h |
| **8. url.fileURLToPath()** | ❌ MISSING | MEDIUM | MEDIUM | 50% (new logic) | 3-4h |
| **9. url.pathToFileURL()** | ❌ MISSING | MEDIUM | MEDIUM | 50% (new logic) | 3-4h |
| **10. url.urlToHttpOptions()** | ❌ MISSING | LOW | SIMPLE | 80% (extract components) | 2h |

**Current Coverage**: 2/10 APIs (20%)
**Target Coverage**: 10/10 APIs (100%)
**Code Reuse Potential**: 85% average

---

## 🔍 Existing Implementation Analysis

### Available Assets in src/url/

#### 1. Complete URL Class (src/url/url_api.c)
**Status**: ✅ Production-ready, WPT-compliant (97%)

```c
// Fully implemented URL class with all properties
typedef struct JSRT_URL {
  char* href;           // ✅ Full URL string
  char* protocol;       // ✅ Scheme (e.g., "https:")
  char* username;       // ✅ User credentials
  char* password;       // ✅ Password
  char* host;           // ✅ hostname:port
  char* hostname;       // ✅ Domain or IP
  char* port;           // ✅ Port number
  char* pathname;       // ✅ Path component
  char* search;         // ✅ Query string (with ?)
  char* hash;           // ✅ Fragment (with #)
  char* origin;         // ✅ Origin (protocol://host:port)
  JSValue search_params; // ✅ URLSearchParams object
} JSRT_URL;

// Available functions
JSRT_URL* JSRT_ParseURL(const char* url, const char* base);
void JSRT_FreeURL(JSRT_URL* url);
```

**Direct Use**: Export URL class constructor directly to node:url module

#### 2. Complete URLSearchParams Class (src/url/search_params/)
**Status**: ✅ Production-ready, WPT-compliant

```c
// Fully implemented URLSearchParams with all methods
typedef struct JSRT_URLSearchParams {
  JSRT_URLSearchParam* params;
  struct JSRT_URL* parent_url;
} JSRT_URLSearchParams;

// Available methods (all implemented)
// append(name, value), delete(name), get(name), getAll(name)
// has(name), set(name, value), sort()
// forEach(callback), entries(), keys(), values()
// toString() → query string
```

**Direct Use**: Export URLSearchParams class directly to node:url module

#### 3. URL Parsing Infrastructure (src/url/parser/)
**Status**: ✅ Complete WHATWG URL parser

```c
// Main parser (url_main_parser.c)
JSRT_URL* parse_absolute_url(const char* preprocessed_url);
int detect_url_scheme(const char* url, char** scheme, char** remainder);

// Component parser (url_component_parser.c)
char* parse_url_components(JSRT_URL* parsed, const char* scheme, char* ptr);
int parse_authority_based_url(JSRT_URL* parsed, const char* scheme, char* ptr, int is_special);

// Relative URL resolution (url_relative.c)
JSRT_URL* resolve_relative_url(const char* url, const char* base);
```

**Reuse for**: url.parse() → wrap JSRT_ParseURL()
**Reuse for**: url.resolve() → wrap resolve_relative_url()

#### 4. URL Encoding/Decoding (src/url/encoding/)
**Status**: ✅ Complete encoding suite

```c
// Basic encoding (url_basic_encoding.c)
char* url_encode_with_len(const char* str, size_t len);  // Space → %20
char* url_decode_with_length(const char* str, size_t len);

// Component encoding (url_component_encoding.c)
char* url_component_encode(const char* str);      // Full component encoding
char* url_path_encode_special(const char* str);   // Path encoding for special schemes
char* url_userinfo_encode(const char* str);       // Username/password encoding
char* url_query_encode(const char* str);          // Query string encoding
char* url_fragment_encode(const char* str);       // Fragment encoding
```

**Reuse for**: url.format() → use encoding functions
**Reuse for**: url.parse() → already uses decoders

#### 5. Hostname Normalization (src/url/normalize/)
**Status**: ✅ Complete with Punycode support

```c
// Hostname functions (url_structure_normalization.c)
char* hostname_to_ascii(const char* hostname);           // Unicode → Punycode
char* normalize_hostname_unicode(const char* hostname);  // Punycode → Unicode
char* hostname_to_ascii_with_case(const char* hostname, int preserve_ascii_case);
```

**Reuse for**: url.domainToASCII() → direct wrapper
**Reuse for**: url.domainToUnicode() → direct wrapper

#### 6. Path Normalization (src/url/normalize/)
**Status**: ✅ Complete path normalization

```c
// Path functions (url_path_normalization.c)
char* normalize_dot_segments(const char* path);                    // Resolve . and ..
char* normalize_windows_drive_letters(const char* path);           // Windows drive handling
char* normalize_dot_segments_with_percent_decoding(const char* path);
```

**Reuse for**: url.parse() → already integrated
**Reuse for**: url.fileURLToPath() → Windows drive letter handling

#### 7. URL Builder (src/url/url_builder.c)
**Status**: ✅ Complete URL reconstruction

```c
// Build complete URL from components
void build_href(JSRT_URL* parsed);
char* build_url_string(const char* protocol, const char* username, const char* password,
                       const char* host, const char* pathname, const char* search,
                       const char* hash, int has_password_field);
```

**Reuse for**: url.format() → adapt for legacy format

---

## 📋 Detailed API Specifications

### WHATWG API (Already Implemented)

#### 1. URL Class
**Status**: ✅ COMPLETE
**Implementation**: src/url/url_api.c (ready to export)

```javascript
// Constructor
new URL(url[, base])

// Properties (all implemented)
url.href       // Full URL string
url.origin     // Origin (protocol://host:port)
url.protocol   // Scheme with colon (e.g., "https:")
url.username   // Username
url.password   // Password
url.host       // hostname:port
url.hostname   // Domain or IP address
url.port       // Port number as string
url.pathname   // Path component
url.search     // Query string (including ?)
url.hash       // Fragment (including #)
url.searchParams  // URLSearchParams object

// Methods
url.toString()
url.toJSON()
```

**Implementation Plan**: Export class directly in Phase 1

#### 2. URLSearchParams Class
**Status**: ✅ COMPLETE
**Implementation**: src/url/search_params/ (ready to export)

```javascript
// Constructor
new URLSearchParams([init])

// Methods (all implemented)
append(name, value)
delete(name)
get(name)
getAll(name)
has(name)
set(name, value)
sort()
forEach(callback[, thisArg])
entries()
keys()
values()
toString()

// Iterable
for (const [key, value] of searchParams) { }
```

**Implementation Plan**: Export class directly in Phase 1

---

### Legacy API (To Be Implemented)

#### 3. url.parse(urlString[, parseQueryString[, slashesDenoteHost]])
**Status**: ❌ NOT IMPLEMENTED
**Priority**: HIGH
**Complexity**: MEDIUM
**Code Reuse**: 80% from JSRT_ParseURL()

**Node.js Signature**:
```javascript
url.parse(urlString: string,
          parseQueryString?: boolean,
          slashesDenoteHost?: boolean): Url
```

**Return Type** (Legacy Url object):
```javascript
{
  href: 'http://user:pass@host.com:8080/p/a/t/h?query=string#hash',
  protocol: 'http:',
  slashes: true,
  auth: 'user:pass',
  host: 'host.com:8080',
  port: '8080',
  hostname: 'host.com',
  hash: '#hash',
  search: '?query=string',
  query: 'query=string',  // Or parsed object if parseQueryString=true
  pathname: '/p/a/t/h',
  path: '/p/a/t/h?query=string'
}
```

**Key Differences from WHATWG URL**:
1. Returns plain object (not URL instance)
2. `slashes` property (true for //-based URLs)
3. `auth` property combines username:password
4. `query` can be string or parsed object
5. `path` includes pathname + search
6. No `origin` property
7. No automatic URL encoding/normalization

**Implementation Strategy**:
```c
static JSValue js_url_parse(JSContext* ctx, JSValueConst this_val,
                             int argc, JSValueConst* argv) {
  // 1. Extract arguments
  const char* url_str = JS_ToCString(ctx, argv[0]);
  bool parse_query = argc > 1 && JS_ToBool(ctx, argv[1]);
  bool slashes_denote = argc > 2 && JS_ToBool(ctx, argv[2]);

  // 2. Parse URL using existing parser
  JSRT_URL* parsed = JSRT_ParseURL(url_str, NULL);
  if (!parsed) {
    return JS_NewObject(ctx);  // Return empty object on failure
  }

  // 3. Build legacy format object
  JSValue result = JS_NewObject(ctx);

  // Set basic properties
  JS_SetPropertyStr(ctx, result, "href", JS_NewString(ctx, parsed->href));
  JS_SetPropertyStr(ctx, result, "protocol", JS_NewString(ctx, parsed->protocol));
  JS_SetPropertyStr(ctx, result, "hostname", JS_NewString(ctx, parsed->hostname));
  JS_SetPropertyStr(ctx, result, "port", JS_NewString(ctx, parsed->port));
  JS_SetPropertyStr(ctx, result, "pathname", JS_NewString(ctx, parsed->pathname));
  JS_SetPropertyStr(ctx, result, "hash", JS_NewString(ctx, parsed->hash));

  // Compute auth (username:password)
  char* auth = NULL;
  if (parsed->username || parsed->password) {
    size_t len = strlen(parsed->username ? parsed->username : "") +
                 strlen(parsed->password ? parsed->password : "") + 2;
    auth = malloc(len);
    snprintf(auth, len, "%s:%s",
             parsed->username ? parsed->username : "",
             parsed->password ? parsed->password : "");
    JS_SetPropertyStr(ctx, result, "auth", JS_NewString(ctx, auth));
    free(auth);
  }

  // Compute slashes property
  bool has_slashes = is_special_scheme(parsed->protocol) ||
                     strstr(parsed->href, "//") != NULL;
  JS_SetPropertyStr(ctx, result, "slashes", JS_NewBool(ctx, has_slashes));

  // Handle query string
  if (parsed->search && strlen(parsed->search) > 0) {
    const char* query_str = parsed->search[0] == '?' ? parsed->search + 1 : parsed->search;
    JS_SetPropertyStr(ctx, result, "search", JS_NewString(ctx, parsed->search));

    if (parse_query) {
      // Parse query string into object (use querystring.parse logic)
      JSValue query_obj = parse_query_string(ctx, query_str);
      JS_SetPropertyStr(ctx, result, "query", query_obj);
    } else {
      JS_SetPropertyStr(ctx, result, "query", JS_NewString(ctx, query_str));
    }
  }

  // Compute path (pathname + search)
  char* path = malloc(strlen(parsed->pathname) +
                     (parsed->search ? strlen(parsed->search) : 0) + 1);
  sprintf(path, "%s%s", parsed->pathname, parsed->search ? parsed->search : "");
  JS_SetPropertyStr(ctx, result, "path", JS_NewString(ctx, path));
  free(path);

  // Set host (hostname:port)
  JS_SetPropertyStr(ctx, result, "host", JS_NewString(ctx, parsed->host));

  // Cleanup
  JSRT_FreeURL(parsed);
  JS_FreeCString(ctx, url_str);

  return result;
}
```

**Edge Cases**:
- Empty string → return empty object
- Relative URLs → parse without base
- Invalid URLs → return partial object
- No protocol → try to infer from slashesDenoteHost
- Query parsing → reuse querystring.parse()

**Testing Requirements**:
- 25+ test cases covering all properties
- Test with http, https, ftp, file, data URLs
- Test with and without parseQueryString
- Test edge cases (no protocol, relative, malformed)
- Compare with Node.js behavior

---

#### 4. url.format(urlObject)
**Status**: ❌ NOT IMPLEMENTED
**Priority**: HIGH
**Complexity**: MEDIUM
**Code Reuse**: 70% from build_url_string()

**Node.js Signature**:
```javascript
url.format(urlObject: Url | URL): string
```

**Accepts Two Input Types**:
1. Legacy Url object (from url.parse)
2. WHATWG URL object (from new URL())

**Priority Rules for Legacy Object**:
1. If `href` is provided, use it directly
2. Otherwise build from components:
   - protocol + slashes (//) + auth + host + pathname + search + hash
3. `host` overrides `hostname` + `port`
4. `search` overrides `query`
5. `path` overrides `pathname` + `search`

**Implementation Strategy**:
```c
static JSValue js_url_format(JSContext* ctx, JSValueConst this_val,
                              int argc, JSValueConst* argv) {
  if (argc < 1 || !JS_IsObject(argv[0])) {
    return JS_NewString(ctx, "");
  }

  JSValue obj = argv[0];

  // Check if it's a WHATWG URL object
  JSRT_URL* url = JS_GetOpaque(obj, JSRT_URLClassID);
  if (url) {
    // It's a URL instance - return href directly
    return JS_NewString(ctx, url->href);
  }

  // It's a legacy object - build from components

  // Try href first (highest priority)
  JSValue href_val = JS_GetPropertyStr(ctx, obj, "href");
  if (JS_IsString(href_val)) {
    return href_val;  // Return href directly
  }
  JS_FreeValue(ctx, href_val);

  // Build from components
  char result[4096] = "";

  // 1. Protocol
  JSValue protocol_val = JS_GetPropertyStr(ctx, obj, "protocol");
  if (JS_IsString(protocol_val)) {
    const char* protocol = JS_ToCString(ctx, protocol_val);
    strcat(result, protocol);
    if (protocol[strlen(protocol)-1] != ':') {
      strcat(result, ":");
    }
    JS_FreeCString(ctx, protocol);
  }
  JS_FreeValue(ctx, protocol_val);

  // 2. Slashes
  JSValue slashes_val = JS_GetPropertyStr(ctx, obj, "slashes");
  bool has_slashes = JS_ToBool(ctx, slashes_val);
  if (has_slashes) {
    strcat(result, "//");
  }
  JS_FreeValue(ctx, slashes_val);

  // 3. Auth (username:password)
  JSValue auth_val = JS_GetPropertyStr(ctx, obj, "auth");
  if (JS_IsString(auth_val)) {
    const char* auth = JS_ToCString(ctx, auth_val);
    strcat(result, auth);
    strcat(result, "@");
    JS_FreeCString(ctx, auth);
  }
  JS_FreeValue(ctx, auth_val);

  // 4. Host (or hostname + port)
  JSValue host_val = JS_GetPropertyStr(ctx, obj, "host");
  if (JS_IsString(host_val)) {
    const char* host = JS_ToCString(ctx, host_val);
    strcat(result, host);
    JS_FreeCString(ctx, host);
  } else {
    // Build from hostname + port
    JSValue hostname_val = JS_GetPropertyStr(ctx, obj, "hostname");
    if (JS_IsString(hostname_val)) {
      const char* hostname = JS_ToCString(ctx, hostname_val);
      strcat(result, hostname);
      JS_FreeCString(ctx, hostname);

      JSValue port_val = JS_GetPropertyStr(ctx, obj, "port");
      if (JS_IsString(port_val)) {
        const char* port = JS_ToCString(ctx, port_val);
        if (strlen(port) > 0) {
          strcat(result, ":");
          strcat(result, port);
        }
        JS_FreeCString(ctx, port);
      }
      JS_FreeValue(ctx, port_val);
    }
    JS_FreeValue(ctx, hostname_val);
  }
  JS_FreeValue(ctx, host_val);

  // 5. Path (or pathname + search)
  JSValue path_val = JS_GetPropertyStr(ctx, obj, "path");
  if (JS_IsString(path_val)) {
    const char* path = JS_ToCString(ctx, path_val);
    strcat(result, path);
    JS_FreeCString(ctx, path);
  } else {
    // Build from pathname + search
    JSValue pathname_val = JS_GetPropertyStr(ctx, obj, "pathname");
    if (JS_IsString(pathname_val)) {
      const char* pathname = JS_ToCString(ctx, pathname_val);
      strcat(result, pathname);
      JS_FreeCString(ctx, pathname);
    }
    JS_FreeValue(ctx, pathname_val);

    JSValue search_val = JS_GetPropertyStr(ctx, obj, "search");
    if (JS_IsString(search_val)) {
      const char* search = JS_ToCString(ctx, search_val);
      strcat(result, search);
      JS_FreeCString(ctx, search);
    }
    JS_FreeValue(ctx, search_val);
  }
  JS_FreeValue(ctx, path_val);

  // 6. Hash
  JSValue hash_val = JS_GetPropertyStr(ctx, obj, "hash");
  if (JS_IsString(hash_val)) {
    const char* hash = JS_ToCString(ctx, hash_val);
    strcat(result, hash);
    JS_FreeCString(ctx, hash);
  }
  JS_FreeValue(ctx, hash_val);

  return JS_NewString(ctx, result);
}
```

**Edge Cases**:
- Empty object → empty string
- Only protocol → "protocol:"
- WHATWG URL object → return href
- Missing components → skip them
- Invalid combinations → best effort

**Testing Requirements**:
- 20+ test cases covering all components
- Test priority rules (href > path > pathname)
- Test WHATWG URL input
- Test legacy object input
- Verify inverse relationship: format(parse(url)) ≈ url

---

#### 5. url.resolve(from, to)
**Status**: ❌ NOT IMPLEMENTED
**Priority**: HIGH
**Complexity**: SIMPLE
**Code Reuse**: 90% from resolve_relative_url()

**Node.js Signature**:
```javascript
url.resolve(from: string, to: string): string
```

**Behavior**: Resolves `to` URL relative to `from` URL

**Examples**:
```javascript
url.resolve('http://example.com/foo', 'bar')
// → 'http://example.com/bar'

url.resolve('http://example.com/foo/', 'bar')
// → 'http://example.com/foo/bar'

url.resolve('http://example.com/foo', '/bar')
// → 'http://example.com/bar'

url.resolve('http://example.com/foo', '../bar')
// → 'http://example.com/bar'

url.resolve('http://example.com/foo', 'http://other.com/bar')
// → 'http://other.com/bar'
```

**Implementation Strategy**:
```c
static JSValue js_url_resolve(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_NewString(ctx, "");
  }

  const char* from = JS_ToCString(ctx, argv[0]);
  const char* to = JS_ToCString(ctx, argv[1]);

  // Use existing relative URL resolution
  JSRT_URL* resolved = resolve_relative_url(to, from);

  if (!resolved) {
    // Failed to resolve - return 'to' as-is
    JS_FreeCString(ctx, from);
    JSValue result = JS_DupValue(ctx, argv[1]);
    JS_FreeCString(ctx, to);
    return result;
  }

  // Extract href from resolved URL
  JSValue result = JS_NewString(ctx, resolved->href);

  // Cleanup
  JSRT_FreeURL(resolved);
  JS_FreeCString(ctx, from);
  JS_FreeCString(ctx, to);

  return result;
}
```

**Edge Cases**:
- Absolute `to` URL → return `to` as-is
- Empty `to` → return `from`
- Invalid `from` → try to resolve anyway
- Fragment-only `to` (#hash) → replace fragment

**Testing Requirements**:
- 15+ test cases covering relative resolution
- Test absolute vs relative URLs
- Test with different base paths (trailing slash vs none)
- Test with query strings and fragments
- Test with special schemes (http, file, ftp)

---

#### 6. url.domainToASCII(domain)
**Status**: ❌ NOT IMPLEMENTED
**Priority**: MEDIUM
**Complexity**: SIMPLE
**Code Reuse**: 100% from hostname_to_ascii()

**Node.js Signature**:
```javascript
url.domainToASCII(domain: string): string
```

**Behavior**: Converts Unicode domain to ASCII (Punycode encoding)

**Examples**:
```javascript
url.domainToASCII('español.com')
// → 'xn--espaol-zwa.com'

url.domainToASCII('中文.com')
// → 'xn--fiq228c.com'

url.domainToASCII('example.com')
// → 'example.com' (already ASCII)
```

**Implementation Strategy**:
```c
static JSValue js_url_domain_to_ascii(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewString(ctx, "");
  }

  const char* domain = JS_ToCString(ctx, argv[0]);

  // Use existing Punycode encoder
  char* ascii = hostname_to_ascii(domain);

  JS_FreeCString(ctx, domain);

  if (!ascii) {
    return JS_NewString(ctx, "");
  }

  JSValue result = JS_NewString(ctx, ascii);
  free(ascii);

  return result;
}
```

**Edge Cases**:
- Empty string → empty string
- Already ASCII → return as-is
- Invalid domain → empty string
- Mixed ASCII/Unicode → convert Unicode parts only

**Testing Requirements**:
- 10+ test cases with Unicode domains
- Test ASCII domains (no-op)
- Test empty input
- Test invalid domains

---

#### 7. url.domainToUnicode(domain)
**Status**: ❌ NOT IMPLEMENTED
**Priority**: MEDIUM
**Complexity**: SIMPLE
**Code Reuse**: 100% from normalize_hostname_unicode()

**Node.js Signature**:
```javascript
url.domainToUnicode(domain: string): string
```

**Behavior**: Converts ASCII/Punycode domain to Unicode

**Examples**:
```javascript
url.domainToUnicode('xn--espaol-zwa.com')
// → 'español.com'

url.domainToUnicode('xn--fiq228c.com')
// → '中文.com'

url.domainToUnicode('example.com')
// → 'example.com' (already Unicode/ASCII)
```

**Implementation Strategy**:
```c
static JSValue js_url_domain_to_unicode(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewString(ctx, "");
  }

  const char* domain = JS_ToCString(ctx, argv[0]);

  // Use existing Unicode normalizer (decodes Punycode)
  char* unicode = normalize_hostname_unicode(domain);

  JS_FreeCString(ctx, domain);

  if (!unicode) {
    return JS_NewString(ctx, "");
  }

  JSValue result = JS_NewString(ctx, unicode);
  free(unicode);

  return result;
}
```

**Edge Cases**:
- Empty string → empty string
- No Punycode → return as-is
- Invalid Punycode → return as-is or empty
- Mixed ASCII/Punycode → decode Punycode parts

**Testing Requirements**:
- 10+ test cases with Punycode domains
- Test ASCII domains (no-op)
- Test invalid Punycode
- Test inverse relationship with domainToASCII

---

#### 8. url.fileURLToPath(url)
**Status**: ❌ NOT IMPLEMENTED
**Priority**: MEDIUM
**Complexity**: MEDIUM
**Code Reuse**: 50% (needs platform-specific logic)

**Node.js Signature**:
```javascript
url.fileURLToPath(url: string | URL): string
```

**Behavior**: Converts file:// URL to platform-specific file path

**Examples**:
```javascript
// Unix
url.fileURLToPath('file:///home/user/file.txt')
// → '/home/user/file.txt'

// Windows
url.fileURLToPath('file:///C:/Users/user/file.txt')
// → 'C:\\Users\\user\\file.txt'

url.fileURLToPath('file://hostname/share/file.txt')
// → '\\\\hostname\\share\\file.txt' (UNC on Windows)
```

**Implementation Strategy**:
```c
static JSValue js_url_file_to_path(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_EXCEPTION;
  }

  const char* url_str = NULL;
  JSRT_URL* url_obj = NULL;

  // Accept string or URL object
  if (JS_IsString(argv[0])) {
    url_str = JS_ToCString(ctx, argv[0]);
  } else {
    url_obj = JS_GetOpaque(argv[0], JSRT_URLClassID);
    if (url_obj) {
      url_str = url_obj->href;
    }
  }

  if (!url_str) {
    return JS_ThrowTypeError(ctx, "Invalid URL");
  }

  // Parse URL
  JSRT_URL* parsed = url_obj ? url_obj : JSRT_ParseURL(url_str, NULL);
  if (!parsed) {
    if (JS_IsString(argv[0])) JS_FreeCString(ctx, url_str);
    return JS_ThrowTypeError(ctx, "Invalid URL");
  }

  // Check protocol
  if (strcmp(parsed->protocol, "file:") != 0) {
    if (!url_obj) JSRT_FreeURL(parsed);
    if (JS_IsString(argv[0])) JS_FreeCString(ctx, url_str);
    return JS_ThrowTypeError(ctx, "URL must be a file: URL");
  }

  // Decode pathname
  char* decoded_path = url_decode(parsed->pathname);

#ifdef _WIN32
  // Windows-specific conversion
  char* result = NULL;

  // Handle UNC paths (//hostname/share)
  if (parsed->hostname && strlen(parsed->hostname) > 0) {
    // UNC path: \\hostname\share\path
    size_t len = strlen(parsed->hostname) + strlen(decoded_path) + 10;
    result = malloc(len);
    snprintf(result, len, "\\\\%s%s", parsed->hostname, decoded_path);

    // Convert / to \
    for (char* p = result; *p; p++) {
      if (*p == '/') *p = '\\';
    }
  } else {
    // Regular path: C:\path\to\file
    // Remove leading / from /C:/path
    const char* path = decoded_path;
    if (path[0] == '/' && path[2] == ':') {
      path++;  // Skip leading /
    }

    result = strdup(path);
    // Convert / to \
    for (char* p = result; *p; p++) {
      if (*p == '/') *p = '\\';
    }
  }
#else
  // Unix-specific conversion
  char* result = strdup(decoded_path);
#endif

  // Cleanup
  free(decoded_path);
  if (!url_obj) JSRT_FreeURL(parsed);
  if (JS_IsString(argv[0])) JS_FreeCString(ctx, url_str);

  JSValue js_result = JS_NewString(ctx, result);
  free(result);
  return js_result;
}
```

**Edge Cases**:
- Non-file:// URL → throw TypeError
- Empty pathname → platform-specific root
- UNC paths (Windows) → \\hostname\share
- Percent-encoded characters → decode them
- Drive letters (Windows) → preserve case

**Testing Requirements**:
- 15+ test cases (platform-specific)
- Test Unix paths (/path/to/file)
- Test Windows paths (C:\path\to\file)
- Test UNC paths (\\server\share)
- Test percent-encoded characters
- Test error cases (non-file URLs)

---

#### 9. url.pathToFileURL(path)
**Status**: ❌ NOT IMPLEMENTED
**Priority**: MEDIUM
**Complexity**: MEDIUM
**Code Reuse**: 50% (needs platform-specific logic)

**Node.js Signature**:
```javascript
url.pathToFileURL(path: string): URL
```

**Behavior**: Converts platform-specific file path to file:// URL object

**Examples**:
```javascript
// Unix
url.pathToFileURL('/home/user/file.txt')
// → URL { href: 'file:///home/user/file.txt', ... }

// Windows
url.pathToFileURL('C:\\Users\\user\\file.txt')
// → URL { href: 'file:///C:/Users/user/file.txt', ... }

url.pathToFileURL('\\\\hostname\\share\\file.txt')
// → URL { href: 'file://hostname/share/file.txt', ... }
```

**Implementation Strategy**:
```c
static JSValue js_url_path_to_file(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
  if (argc < 1 || !JS_IsString(argv[0])) {
    return JS_ThrowTypeError(ctx, "Path must be a string");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  char url_str[4096] = "file://";

#ifdef _WIN32
  // Windows-specific conversion
  if (path[0] == '\\' && path[1] == '\\') {
    // UNC path: \\hostname\share\path → file://hostname/share/path
    const char* p = path + 2;
    const char* hostname_end = strchr(p, '\\');
    if (hostname_end) {
      size_t hostname_len = hostname_end - p;
      strncat(url_str, p, hostname_len);
      p = hostname_end;
    }

    // Append path, converting \ to /
    char* encoded = url_path_encode_file(p);
    for (char* c = encoded; *c; c++) {
      if (*c == '\\') *c = '/';
    }
    strcat(url_str, encoded);
    free(encoded);
  } else {
    // Regular path: C:\path → file:///C:/path
    strcat(url_str, "/");

    char* encoded = url_path_encode_file(path);
    for (char* c = encoded; *c; c++) {
      if (*c == '\\') *c = '/';
    }
    strcat(url_str, encoded);
    free(encoded);
  }
#else
  // Unix-specific conversion
  strcat(url_str, "/");
  char* encoded = url_path_encode_file(path);
  strcat(url_str, encoded);
  free(encoded);
#endif

  JS_FreeCString(ctx, path);

  // Parse as URL and return URL object
  JSRT_URL* url = JSRT_ParseURL(url_str, NULL);
  if (!url) {
    return JS_ThrowError(ctx, "Failed to create file URL");
  }

  // Create URL object
  JSValue url_obj = JS_NewObjectClass(ctx, JSRT_URLClassID);
  JS_SetOpaque(url_obj, url);

  return url_obj;
}
```

**Edge Cases**:
- Relative paths → make absolute first (use cwd)
- Special characters → percent-encode
- Windows drive letters → normalize to uppercase
- UNC paths → extract hostname
- Symlinks → resolve or preserve?

**Testing Requirements**:
- 15+ test cases (platform-specific)
- Test Unix absolute paths
- Test Windows absolute paths
- Test UNC paths
- Test special characters (spaces, unicode)
- Test inverse relationship with fileURLToPath

---

#### 10. url.urlToHttpOptions(url)
**Status**: ❌ NOT IMPLEMENTED
**Priority**: LOW
**Complexity**: SIMPLE
**Code Reuse**: 80% (extract URL components)

**Node.js Signature**:
```javascript
url.urlToHttpOptions(url: URL): object
```

**Behavior**: Converts URL object to options object for http.request()

**Return Type**:
```javascript
{
  protocol: 'http:',
  hostname: 'example.com',
  port: 8080,
  path: '/foo?bar=baz',
  hash: '#hash',
  auth: 'user:pass'
}
```

**Implementation Strategy**:
```c
static JSValue js_url_to_http_options(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "URL is required");
  }

  // Extract URL object
  JSRT_URL* url = JS_GetOpaque(argv[0], JSRT_URLClassID);
  if (!url) {
    return JS_ThrowTypeError(ctx, "Argument must be a URL object");
  }

  // Build options object
  JSValue options = JS_NewObject(ctx);

  // protocol
  JS_SetPropertyStr(ctx, options, "protocol", JS_NewString(ctx, url->protocol));

  // hostname
  JS_SetPropertyStr(ctx, options, "hostname", JS_NewString(ctx, url->hostname));

  // port (convert to number if present)
  if (url->port && strlen(url->port) > 0) {
    int port_num = atoi(url->port);
    JS_SetPropertyStr(ctx, options, "port", JS_NewInt32(ctx, port_num));
  }

  // path (pathname + search)
  char path[4096];
  snprintf(path, sizeof(path), "%s%s", url->pathname, url->search ? url->search : "");
  JS_SetPropertyStr(ctx, options, "path", JS_NewString(ctx, path));

  // hash
  if (url->hash && strlen(url->hash) > 0) {
    JS_SetPropertyStr(ctx, options, "hash", JS_NewString(ctx, url->hash));
  }

  // auth (username:password)
  if (url->username || url->password) {
    char auth[512];
    snprintf(auth, sizeof(auth), "%s:%s",
             url->username ? url->username : "",
             url->password ? url->password : "");
    JS_SetPropertyStr(ctx, options, "auth", JS_NewString(ctx, auth));
  }

  return options;
}
```

**Edge Cases**:
- No port → omit property
- No auth → omit property
- Empty pathname → use "/"
- Query string → include in path

**Testing Requirements**:
- 10+ test cases
- Test with various URL types
- Test with and without auth
- Test with and without port
- Verify compatibility with http.request()

---

## 🏗️ Implementation Architecture

### File Structure

```
src/node/
├── node_url.c              [NEW] Core implementation (~800 lines)
│   ├── WHATWG API exports (URL, URLSearchParams)
│   ├── Legacy API (parse, format, resolve)
│   ├── Utility functions (domain, file, http)
│   └── Module initialization
└── node_modules.c          [MODIFY] Add url registration

src/url/                    [REUSE] Existing URL infrastructure (97% WPT)
├── url_api.c               ✅ URL class
├── url_core.c              ✅ Core structures
├── parser/                 ✅ Parsing functions
│   ├── url_main_parser.c
│   └── url_component_parser.c
├── encoding/               ✅ Encoding/decoding
│   ├── url_basic_encoding.c
│   └── url_component_encoding.c
├── normalize/              ✅ Normalization
│   ├── url_path_normalization.c
│   └── url_structure_normalization.c
├── search_params/          ✅ URLSearchParams
│   ├── url_search_params_core.c
│   ├── url_search_params_methods.c
│   └── url_search_params_parser.c
└── url_relative.c          ✅ Relative URL resolution

test/node/url/              [NEW] Test suite (~1200 lines)
├── test_url_whatwg.js      WHATWG API tests
├── test_url_parse.js       url.parse() tests
├── test_url_format.js      url.format() tests
├── test_url_resolve.js     url.resolve() tests
├── test_url_domain.js      domain conversion tests
├── test_url_file.js        file URL tests
├── test_url_http.js        urlToHttpOptions tests
├── test_url_cjs.js         CommonJS import tests
└── test_url_esm.mjs        ES module import tests
```

### Module Registration Flow

```c
// In src/node/node_modules.c
static const struct {
  const char* name;
  JSModuleDef* (*init)(JSContext* ctx, const char* module_name);
} node_modules[] = {
  // ... existing modules ...
  {"url", js_init_module_node_url},  // [ADD]
  {NULL, NULL}
};
```

### Memory Management Pattern

```c
// Pattern: Wrap existing functions, manage memory correctly
static JSValue js_url_function(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
  // 1. Extract JavaScript arguments
  const char* input = JS_ToCString(ctx, argv[0]);
  if (!input) return JS_EXCEPTION;

  // 2. Call existing C function
  char* result = existing_url_function(input);

  // 3. Free input string
  JS_FreeCString(ctx, input);

  // 4. Check result
  if (!result) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // 5. Create JavaScript value
  JSValue js_result = JS_NewString(ctx, result);

  // 6. Free C result
  free(result);

  return js_result;
}
```

---

## 📝 Detailed Task Breakdown

### Phase 1: Module Setup & WHATWG API Export (4-5 hours)

#### Task 1.1: [S][R:LOW][C:SIMPLE] Create node_url.c skeleton
**Duration**: 1 hour
**Dependencies**: None

**Subtasks**:
- **1.1.1** [15min] Create `src/node/node_url.c` file
- **1.1.2** [15min] Add includes (quickjs.h, url.h, node_modules.h)
- **1.1.3** [15min] Add module initialization function skeleton
- **1.1.4** [15min] Add to build system (Makefile)

**Acceptance Criteria**:
- File compiles without errors
- Module structure follows node_path.c pattern
- Includes all necessary headers

**Implementation Template**:
```c
#include <quickjs.h>
#include "../url/url.h"
#include "node_modules.h"

// Forward declarations
static JSValue js_url_parse(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_url_format(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
// ... more forward declarations ...

// Module initialization
static const JSCFunctionListEntry js_url_funcs[] = {
  JS_CFUNC_DEF("parse", 3, js_url_parse),
  JS_CFUNC_DEF("format", 1, js_url_format),
  // ... more functions ...
};

static int js_node_url_init(JSContext* ctx, JSModuleDef* m) {
  // Export functions and classes
  return JS_SetModuleExportList(ctx, m, js_url_funcs, countof(js_url_funcs));
}

JSModuleDef* js_init_module_node_url(JSContext* ctx, const char* module_name) {
  JSModuleDef* m = JS_NewCModule(ctx, module_name, js_node_url_init);
  if (!m) return NULL;
  JS_AddModuleExportList(ctx, m, js_url_funcs, countof(js_url_funcs));
  return m;
}
```

---

#### Task 1.2: [S][R:LOW][C:SIMPLE] Export URL class
**Duration**: 1 hour
**Dependencies**: [D:1.1] node_url.c created

**Subtasks**:
- **1.2.1** [30min] Add URL class export to module
- **1.2.2** [15min] Test URL constructor works
- **1.2.3** [15min] Verify all URL properties accessible

**Implementation**:
```c
static int js_node_url_init(JSContext* ctx, JSModuleDef* m) {
  // Get URL class
  JSValue url_class = JS_GetClassProto(ctx, JSRT_URLClassID);

  // Export URL class
  JS_SetModuleExport(ctx, m, "URL", url_class);

  // Export functions
  JS_SetModuleExportList(ctx, m, js_url_funcs, countof(js_url_funcs));

  return 0;
}

JSModuleDef* js_init_module_node_url(JSContext* ctx, const char* module_name) {
  JSModuleDef* m = JS_NewCModule(ctx, module_name, js_node_url_init);
  if (!m) return NULL;

  // Add URL class export
  JS_AddModuleExport(ctx, m, "URL");

  // Add function exports
  JS_AddModuleExportList(ctx, m, js_url_funcs, countof(js_url_funcs));

  return m;
}
```

**Acceptance Criteria**:
```javascript
import { URL } from 'node:url';
const url = new URL('https://example.com/path?query=1');
assert(url.href === 'https://example.com/path?query=1');
assert(url.protocol === 'https:');
assert(url.hostname === 'example.com');
```

---

#### Task 1.3: [S][R:LOW][C:SIMPLE] Export URLSearchParams class
**Duration**: 30 minutes
**Dependencies**: [D:1.2] URL class exported

**Subtasks**:
- **1.3.1** [15min] Add URLSearchParams export
- **1.3.2** [15min] Test URLSearchParams works

**Implementation**:
```c
// In js_node_url_init()
JSValue search_params_class = JS_GetClassProto(ctx, JSRT_URLSearchParamsClassID);
JS_SetModuleExport(ctx, m, "URLSearchParams", search_params_class);

// In js_init_module_node_url()
JS_AddModuleExport(ctx, m, "URLSearchParams");
```

**Acceptance Criteria**:
```javascript
import { URLSearchParams } from 'node:url';
const params = new URLSearchParams('a=1&b=2');
assert(params.get('a') === '1');
assert(params.toString() === 'a=1&b=2');
```

---

#### Task 1.4: [S][R:LOW][C:SIMPLE] Register module in node_modules.c
**Duration**: 30 minutes
**Dependencies**: [D:1.1, 1.2, 1.3] All exports added

**Subtasks**:
- **1.4.1** [15min] Add module to node_modules[] array
- **1.4.2** [15min] Test module loads correctly

**Implementation**:
```c
// In src/node/node_modules.c
extern JSModuleDef* js_init_module_node_url(JSContext* ctx, const char* module_name);

static const struct {
  const char* name;
  JSModuleDef* (*init)(JSContext* ctx, const char* module_name);
} node_modules[] = {
  // ... existing modules ...
  {"url", js_init_module_node_url},  // [ADD]
  {NULL, NULL}
};
```

**Acceptance Criteria**:
```javascript
// CommonJS
const url = require('node:url');
assert(url.URL);
assert(url.URLSearchParams);

// ESM
import * as url from 'node:url';
assert(url.URL);
assert(url.URLSearchParams);
```

---

#### Task 1.5: [S][R:LOW][C:SIMPLE] Basic WHATWG API tests
**Duration**: 1 hour
**Dependencies**: [D:1.4] Module registered

**Subtasks**:
- **1.5.1** [30min] Create test/node/url/test_url_whatwg.js
- **1.5.2** [30min] Test URL and URLSearchParams exports

**Test File**:
```javascript
// test/node/url/test_url_whatwg.js
const url = require('node:url');

// Test URL class export
const u = new url.URL('https://example.com/path?a=1#hash');
console.assert(u.href === 'https://example.com/path?a=1#hash', 'URL href');
console.assert(u.protocol === 'https:', 'URL protocol');
console.assert(u.hostname === 'example.com', 'URL hostname');
console.assert(u.pathname === '/path', 'URL pathname');
console.assert(u.search === '?a=1', 'URL search');
console.assert(u.hash === '#hash', 'URL hash');

// Test URLSearchParams class export
const params = new url.URLSearchParams('a=1&b=2&c=3');
console.assert(params.get('a') === '1', 'URLSearchParams get');
console.assert(params.has('b'), 'URLSearchParams has');
params.set('d', '4');
console.assert(params.toString() === 'a=1&b=2&c=3&d=4', 'URLSearchParams toString');

console.log('WHATWG API tests passed ✓');
```

**Acceptance Criteria**:
- Test file passes with `make test`
- No memory leaks (ASAN clean)

---

### Phase 2: Legacy parse/format/resolve (8-10 hours)

#### Task 2.1: [S][R:MEDIUM][C:MEDIUM] Implement url.parse()
**Duration**: 4-5 hours
**Dependencies**: [D:1.5] Module setup complete

**Subtasks**:
- **2.1.1** [2h] Implement js_url_parse() function
- **2.1.2** [1h] Handle parseQueryString parameter
- **2.1.3** [1h] Handle slashesDenoteHost parameter
- **2.1.4** [30min] Add error handling

**Implementation**: See detailed specification in section "3. url.parse()" above

**Acceptance Criteria**:
- Returns legacy Url object with all properties
- parseQueryString option works
- Handles edge cases (empty, invalid)
- 25+ test cases pass

---

#### Task 2.2: [S][R:MEDIUM][C:MEDIUM] Implement url.format()
**Duration**: 3-4 hours
**Dependencies**: [D:2.1] url.parse() complete

**Subtasks**:
- **2.2.1** [2h] Implement js_url_format() function
- **2.2.2** [1h] Handle WHATWG URL input
- **2.2.3** [1h] Handle legacy object input with priority rules

**Implementation**: See detailed specification in section "4. url.format()" above

**Acceptance Criteria**:
- Builds URL string from object
- Respects priority rules
- format(parse(url)) ≈ url
- 20+ test cases pass

---

#### Task 2.3: [S][R:LOW][C:SIMPLE] Implement url.resolve()
**Duration**: 2-3 hours
**Dependencies**: [D:2.1] url.parse() complete

**Subtasks**:
- **2.3.1** [1h] Implement js_url_resolve() function
- **2.3.2** [1h] Add edge case handling
- **2.3.3** [30min] Test with various base URLs

**Implementation**: See detailed specification in section "5. url.resolve()" above

**Acceptance Criteria**:
- Resolves relative URLs correctly
- Handles absolute URLs
- 15+ test cases pass

---

#### Task 2.4: [P][R:LOW][C:SIMPLE] Legacy API tests
**Duration**: 2 hours
**Dependencies**: [D:2.1, 2.2, 2.3] All legacy APIs done

**Subtasks**:
- **2.4.1** [1h] Create test/node/url/test_url_parse.js (25+ tests)
- **2.4.2** [30min] Create test/node/url/test_url_format.js (20+ tests)
- **2.4.3** [30min] Create test/node/url/test_url_resolve.js (15+ tests)

**Acceptance Criteria**:
- All legacy API tests pass
- No memory leaks
- Edge cases covered

---

### Phase 3: Utility Functions (6-8 hours)

#### Task 3.1: [P][R:LOW][C:SIMPLE] Implement domain functions
**Duration**: 2 hours
**Dependencies**: [D:1.5] Module setup complete

**Subtasks**:
- **3.1.1** [30min] Implement domainToASCII()
- **3.1.2** [30min] Implement domainToUnicode()
- **3.1.3** [1h] Test with Unicode domains

**Implementation**: See sections "6. domainToASCII()" and "7. domainToUnicode()" above

**Acceptance Criteria**:
- Converts Unicode to Punycode
- Converts Punycode to Unicode
- 20+ test cases pass

---

#### Task 3.2: [P][R:MEDIUM][C:MEDIUM] Implement file URL functions
**Duration**: 4-5 hours
**Dependencies**: [D:1.5] Module setup complete

**Subtasks**:
- **3.2.1** [2h] Implement fileURLToPath()
- **3.2.2** [2h] Implement pathToFileURL()
- **3.2.3** [1h] Platform-specific testing

**Implementation**: See sections "8. fileURLToPath()" and "9. pathToFileURL()" above

**Acceptance Criteria**:
- Unix path conversion works
- Windows path conversion works
- UNC paths handled
- 30+ test cases pass

---

#### Task 3.3: [P][R:LOW][C:SIMPLE] Implement urlToHttpOptions()
**Duration**: 1 hour
**Dependencies**: [D:1.5] Module setup complete

**Subtasks**:
- **3.3.1** [30min] Implement function
- **3.3.2** [30min] Test with various URLs

**Implementation**: See section "10. urlToHttpOptions()" above

**Acceptance Criteria**:
- Returns correct options object
- Compatible with http.request()
- 10+ test cases pass

---

### Phase 4: Testing & Edge Cases (6-8 hours)

#### Task 4.1: [P][R:MEDIUM][C:MEDIUM] Comprehensive edge case testing
**Duration**: 3-4 hours
**Dependencies**: [D:2.4, 3.1, 3.2, 3.3] All APIs implemented

**Test Categories**:
- Empty inputs
- Invalid URLs
- Special characters
- Unicode handling
- Platform-specific paths
- Protocol edge cases
- Very long URLs

**Acceptance Criteria**:
- 50+ edge case tests
- All tests pass
- No crashes on invalid input

---

#### Task 4.2: [P][R:LOW][C:SIMPLE] CommonJS and ESM tests
**Duration**: 2 hours
**Dependencies**: [D:2.4, 3.1, 3.2, 3.3] All APIs implemented

**Subtasks**:
- **4.2.1** [1h] Create test/node/url/test_url_cjs.js
- **4.2.2** [1h] Create test/node/url/test_url_esm.mjs

**Acceptance Criteria**:
- Both import styles work
- All 10 APIs accessible
- Named exports work

---

#### Task 4.3: [P][R:LOW][C:SIMPLE] Memory safety validation
**Duration**: 2 hours
**Dependencies**: [D:4.1, 4.2] All tests written

**Subtasks**:
- **4.3.1** [1h] Run all tests with ASAN
- **4.3.2** [1h] Fix any memory leaks found

**Commands**:
```bash
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m test/node/url/test_*.js
```

**Acceptance Criteria**:
- Zero memory leaks
- Zero buffer overflows
- Zero use-after-free

---

#### Task 4.4: [P][R:LOW][C:SIMPLE] WPT baseline verification
**Duration**: 1 hour
**Dependencies**: [D:4.3] Memory safety validated

**Subtasks**:
- **4.4.1** [30min] Run `make wpt`
- **4.4.2** [30min] Verify 97% pass rate maintained

**Acceptance Criteria**:
- WPT pass rate ≥ 97%
- No regressions from baseline

---

### Phase 5: Documentation & Polish (2-3 hours)

#### Task 5.1: [P][R:LOW][C:SIMPLE] Code cleanup
**Duration**: 1 hour
**Dependencies**: [D:4.4] All tests passing

**Subtasks**:
- **5.1.1** [30min] Run `make format`
- **5.1.2** [15min] Add function-level comments
- **5.1.3** [15min] Remove debug logging

**Acceptance Criteria**:
- Code properly formatted
- Functions documented
- No warnings in build

---

#### Task 5.2: [P][R:LOW][C:SIMPLE] Update documentation
**Duration**: 1-2 hours
**Dependencies**: [D:5.1] Code cleaned

**Subtasks**:
- **5.2.1** [30min] Update this plan with completion status
- **5.2.2** [30min] Add inline code documentation
- **5.2.3** [30min] Document platform differences

**Acceptance Criteria**:
- Plan document updated
- Code self-documented
- Platform notes added

---

## 🧪 Testing Strategy

### Test Coverage Requirements

#### Unit Tests (MANDATORY)
**Coverage**: 100% of all 10 API categories
**Location**: `test/node/url/`
**Execution**: `make test`

**Test Files Structure**:
```
test/node/url/
├── test_url_whatwg.js        [20 tests] WHATWG API
├── test_url_parse.js          [25 tests] url.parse()
├── test_url_format.js         [20 tests] url.format()
├── test_url_resolve.js        [15 tests] url.resolve()
├── test_url_domain.js         [20 tests] domain functions
├── test_url_file.js           [30 tests] file URL functions
├── test_url_http.js           [10 tests] urlToHttpOptions
├── test_url_edge_cases.js     [50 tests] edge cases
├── test_url_cjs.js            [20 tests] CommonJS import
└── test_url_esm.mjs           [20 tests] ES module import

Total: 230+ tests across 10 files
```

#### Test Categories

**1. WHATWG API (20 tests)**
```javascript
// URL constructor
test('URL with absolute URL', () => {
  const url = new URL('https://example.com/path');
  assert.strictEqual(url.href, 'https://example.com/path');
});

// URLSearchParams
test('URLSearchParams basic operations', () => {
  const params = new URLSearchParams('a=1&b=2');
  assert.strictEqual(params.get('a'), '1');
});
```

**2. url.parse() (25 tests)**
```javascript
test('parse returns object with all properties', () => {
  const parsed = url.parse('http://user:pass@host.com:8080/path?query=1#hash');
  assert.strictEqual(parsed.protocol, 'http:');
  assert.strictEqual(parsed.auth, 'user:pass');
  assert.strictEqual(parsed.hostname, 'host.com');
  assert.strictEqual(parsed.port, '8080');
  assert.strictEqual(parsed.pathname, '/path');
  assert.strictEqual(parsed.search, '?query=1');
  assert.strictEqual(parsed.hash, '#hash');
  assert.strictEqual(parsed.slashes, true);
});

test('parse with parseQueryString', () => {
  const parsed = url.parse('http://example.com?a=1&b=2', true);
  assert.deepStrictEqual(parsed.query, { a: '1', b: '2' });
});
```

**3. url.format() (20 tests)**
```javascript
test('format builds URL from object', () => {
  const formatted = url.format({
    protocol: 'http:',
    hostname: 'example.com',
    pathname: '/path'
  });
  assert.strictEqual(formatted, 'http://example.com/path');
});

test('format respects priority rules', () => {
  const formatted = url.format({
    href: 'http://override.com',
    hostname: 'example.com'  // Ignored
  });
  assert.strictEqual(formatted, 'http://override.com');
});
```

**4. url.resolve() (15 tests)**
```javascript
test('resolve relative URL', () => {
  const resolved = url.resolve('http://example.com/foo', 'bar');
  assert.strictEqual(resolved, 'http://example.com/bar');
});

test('resolve with trailing slash', () => {
  const resolved = url.resolve('http://example.com/foo/', 'bar');
  assert.strictEqual(resolved, 'http://example.com/foo/bar');
});
```

**5. Domain Functions (20 tests)**
```javascript
test('domainToASCII converts Unicode', () => {
  const ascii = url.domainToASCII('español.com');
  assert.strictEqual(ascii, 'xn--espaol-zwa.com');
});

test('domainToUnicode converts Punycode', () => {
  const unicode = url.domainToUnicode('xn--espaol-zwa.com');
  assert.strictEqual(unicode, 'español.com');
});

test('round-trip domain conversion', () => {
  const original = '中文.com';
  const ascii = url.domainToASCII(original);
  const unicode = url.domainToUnicode(ascii);
  assert.strictEqual(unicode, original);
});
```

**6. File URL Functions (30 tests)**
```javascript
// Unix
test('fileURLToPath on Unix', () => {
  const path = url.fileURLToPath('file:///home/user/file.txt');
  assert.strictEqual(path, '/home/user/file.txt');
});

test('pathToFileURL on Unix', () => {
  const fileUrl = url.pathToFileURL('/home/user/file.txt');
  assert.strictEqual(fileUrl.href, 'file:///home/user/file.txt');
});

// Windows (if on Windows)
test('fileURLToPath on Windows', () => {
  const path = url.fileURLToPath('file:///C:/Users/file.txt');
  assert.strictEqual(path, 'C:\\Users\\file.txt');
});

test('pathToFileURL UNC path', () => {
  const fileUrl = url.pathToFileURL('\\\\server\\share\\file.txt');
  assert.strictEqual(fileUrl.href, 'file://server/share/file.txt');
});
```

**7. urlToHttpOptions (10 tests)**
```javascript
test('urlToHttpOptions extracts all properties', () => {
  const u = new URL('http://user:pass@example.com:8080/path?query=1#hash');
  const options = url.urlToHttpOptions(u);

  assert.strictEqual(options.protocol, 'http:');
  assert.strictEqual(options.hostname, 'example.com');
  assert.strictEqual(options.port, 8080);
  assert.strictEqual(options.path, '/path?query=1');
  assert.strictEqual(options.auth, 'user:pass');
  assert.strictEqual(options.hash, '#hash');
});
```

**8. Edge Cases (50 tests)**
```javascript
// Empty inputs
test('parse empty string', () => {
  const parsed = url.parse('');
  assert.ok(typeof parsed === 'object');
});

// Invalid URLs
test('parse invalid URL', () => {
  const parsed = url.parse('not a url');
  assert.ok(typeof parsed === 'object');
});

// Special characters
test('parse URL with special characters', () => {
  const parsed = url.parse('http://example.com/path?q=hello%20world');
  assert.ok(parsed.pathname === '/path');
});

// Unicode
test('parse URL with Unicode', () => {
  const parsed = url.parse('http://example.com/путь');
  assert.ok(parsed.pathname.includes('%'));
});

// Very long URLs
test('parse very long URL', () => {
  const longPath = '/'.repeat(1000);
  const parsed = url.parse('http://example.com' + longPath);
  assert.ok(parsed.pathname === longPath);
});
```

#### Memory Safety Tests
**Tool**: AddressSanitizer (ASAN)
**Command**:
```bash
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m test/node/url/test_*.js
```

**Success Criteria**:
- Zero memory leaks
- Zero buffer overflows
- Zero use-after-free errors
- All allocations properly freed

#### Compliance Tests
**WPT**: Run `make wpt` to ensure no regressions
**Baseline**: Maintain 97% WPT pass rate (29/32 tests)

---

## 🎯 Success Criteria

### Functional Requirements
- ✅ All 10 API categories implemented (100% coverage)
- ✅ Both WHATWG and Legacy APIs available
- ✅ Both CommonJS and ESM support
- ✅ Platform-specific handling (Unix/Windows)

### Quality Requirements
- ✅ 230+ unit tests passing (100% pass rate)
- ✅ Zero memory leaks (ASAN clean)
- ✅ WPT baseline maintained (97%+)
- ✅ Code properly formatted (`make format`)

### Performance Requirements
- ⚡ parse/format: <1ms for typical URLs
- ⚡ resolve: <1ms for relative URLs
- ⚡ domain conversion: <1ms
- ⚡ No performance regression from baseline

### Compatibility Requirements
- ✅ Matches Node.js url module behavior exactly
- ✅ WHATWG URL Standard compliant (97% WPT)
- ✅ Legacy API backward compatible
- ✅ Platform-specific file URL handling

---

## 📈 Dependency Graph

```
Phase 1: Module Setup (Sequential)
  [1.1 skeleton] → [1.2 URL export] → [1.3 URLSearchParams export] → [1.4 register] → [1.5 tests]
         ↓
Phase 2: Legacy API (Sequential)
  [2.1 parse] → [2.2 format] → [2.3 resolve] → [2.4 tests]
         ↓
Phase 3: Utilities (Parallel)
  [3.1 domain] [3.2 file] [3.3 http]
         ↓          ↓         ↓
         └──────────┴─────────┘
                    ↓
Phase 4: Testing (Parallel)
  [4.1 edge cases] [4.2 ESM/CJS] [4.3 ASAN] [4.4 WPT]
         ↓              ↓            ↓         ↓
         └──────────────┴────────────┴─────────┘
                        ↓
Phase 5: Polish (Parallel)
  [5.1 cleanup] [5.2 documentation]
```

### Critical Path
`1.1 → 1.2 → 1.3 → 1.4 → 2.1 → 2.2 → 2.3 → 3.* → 4.* → 5.*`

**Estimated Total Duration**: 26-34 hours

### Parallel Execution Opportunities
- **Phase 3**: All utility functions can be implemented in parallel
- **Phase 4**: All testing tasks can run in parallel
- **Phase 5**: Cleanup and documentation can run in parallel

---

## 💡 Code Reuse Analysis

### Reuse Percentage by API

| API | Existing Function | Reuse % | New Code Needed |
|-----|------------------|---------|-----------------|
| URL class | JSRT_ParseURL | 100% | Export only |
| URLSearchParams | JSRT_URLSearchParams | 100% | Export only |
| parse() | JSRT_ParseURL | 80% | Legacy format wrapper |
| format() | build_url_string | 70% | Priority rules logic |
| resolve() | resolve_relative_url | 90% | Simple wrapper |
| domainToASCII() | hostname_to_ascii | 100% | Direct wrapper |
| domainToUnicode() | normalize_hostname_unicode | 100% | Direct wrapper |
| fileURLToPath() | url_decode + path logic | 50% | Platform-specific code |
| pathToFileURL() | url_path_encode_file | 50% | Platform-specific code |
| urlToHttpOptions() | URL component extraction | 80% | Object building |

**Overall Code Reuse**: 85% average
**New Code Estimate**: ~800 lines (node_url.c)
**Test Code Estimate**: ~1200 lines (10 test files)
**Total New Code**: ~2000 lines

### Existing Functions to Reuse

**Direct Wrappers (100% reuse)**:
- `hostname_to_ascii()` → domainToASCII()
- `normalize_hostname_unicode()` → domainToUnicode()
- URL/URLSearchParams classes → direct export

**High Reuse (80-90%)**:
- `JSRT_ParseURL()` → url.parse() wrapper
- `resolve_relative_url()` → url.resolve() wrapper
- URL component extraction → urlToHttpOptions()

**Medium Reuse (50-70%)**:
- `build_url_string()` → url.format() (add priority rules)
- `url_path_encode_file()` → pathToFileURL() (add platform logic)
- `url_decode()` → fileURLToPath() (add platform logic)

---

## 📚 References

### Node.js Documentation
- **Official Docs**: https://nodejs.org/api/url.html
- **Legacy API**: https://nodejs.org/api/url.html#legacy-url-api
- **WHATWG URL**: https://nodejs.org/api/url.html#the-whatwg-url-api

### Standards
- **WHATWG URL Standard**: https://url.spec.whatwg.org/
- **RFC 3986**: URI Generic Syntax
- **RFC 8089**: file: URI Scheme

### jsrt Resources
- **URL Implementation**: `/home/lei/work/jsrt/src/url/`
- **Existing Classes**: URL, URLSearchParams (97% WPT compliant)
- **Build Commands**: `make`, `make test`, `make wpt`, `make format`
- **Debug Build**: `make jsrt_g` (with symbols)
- **ASAN Build**: `make jsrt_m` (memory safety)

### Related Modules
- **node:path**: Path manipulation (for file URL conversion)
- **node:querystring**: Query string parsing (for parseQueryString option)
- **node:http**: HTTP request options (for urlToHttpOptions)

---

## 🔄 Progress Tracking

### Phase 1: Module Setup ⏳ NOT STARTED
| Task | Status | Start | Completion | Notes |
|------|--------|-------|------------|-------|
| 1.1 Create skeleton | ⏳ PENDING | - | - | - |
| 1.2 Export URL | ⏳ PENDING | - | - | - |
| 1.3 Export URLSearchParams | ⏳ PENDING | - | - | - |
| 1.4 Register module | ⏳ PENDING | - | - | - |
| 1.5 Basic tests | ⏳ PENDING | - | - | - |

### Phase 2: Legacy API ⏳ NOT STARTED
| Task | Status | Start | Completion | Notes |
|------|--------|-------|------------|-------|
| 2.1 url.parse() | ⏳ PENDING | - | - | - |
| 2.2 url.format() | ⏳ PENDING | - | - | - |
| 2.3 url.resolve() | ⏳ PENDING | - | - | - |
| 2.4 Legacy tests | ⏳ PENDING | - | - | - |

### Phase 3: Utilities ⏳ NOT STARTED
| Task | Status | Start | Completion | Notes |
|------|--------|-------|------------|-------|
| 3.1 Domain functions | ⏳ PENDING | - | - | - |
| 3.2 File URL functions | ⏳ PENDING | - | - | - |
| 3.3 urlToHttpOptions | ⏳ PENDING | - | - | - |

### Phase 4: Testing ⏳ NOT STARTED
| Task | Status | Start | Completion | Notes |
|------|--------|-------|------------|-------|
| 4.1 Edge cases | ⏳ PENDING | - | - | - |
| 4.2 ESM/CJS | ⏳ PENDING | - | - | - |
| 4.3 ASAN | ⏳ PENDING | - | - | - |
| 4.4 WPT | ⏳ PENDING | - | - | - |

### Phase 5: Polish ⏳ NOT STARTED
| Task | Status | Start | Completion | Notes |
|------|--------|-------|------------|-------|
| 5.1 Cleanup | ⏳ PENDING | - | - | - |
| 5.2 Documentation | ⏳ PENDING | - | - | - |

---

## 📊 Metrics

### Lines of Code Estimate
- **node_url.c**: ~800 lines
  - Module setup: ~100 lines
  - WHATWG exports: ~50 lines
  - Legacy API: ~400 lines (parse, format, resolve)
  - Utilities: ~200 lines (domain, file, http)
  - Module init: ~50 lines
- **Test files**: ~1200 lines (10 files × 120 lines)
- **Total new code**: ~2000 lines
- **Code reused**: ~5000 lines from src/url/

### API Coverage Progress
- **Target**: 10 API categories
- **Implemented**: 2/10 (20%) - WHATWG only
- **Remaining**: 8/10 (80%) - Legacy + Utilities

### Test Coverage Progress
- **Target**: 230+ tests
- **Written**: 0/230 (0%)
- **Passing**: 0/230 (0%)

---

## 🎉 Completion Criteria

This implementation will be considered **COMPLETE** when:

1. ✅ All 10 API categories implemented (100% coverage)
2. ✅ 230+ tests written and passing (100% pass rate)
3. ✅ Both CommonJS and ESM support verified
4. ✅ Zero memory leaks (ASAN validation)
5. ✅ WPT baseline maintained at 97%+ (29/32 tests)
6. ✅ Code properly formatted (`make format`)
7. ✅ All builds pass (`make test && make wpt && make clean && make`)
8. ✅ Documentation updated
9. ✅ Platform-specific tests pass (Unix/Windows)
10. ✅ Performance benchmarks meet requirements

---

## 📝 Implementation Notes

### Key Design Decisions

1. **WHATWG Priority**: Export existing URL/URLSearchParams classes directly (zero new code)
2. **Legacy Wrapper**: Adapt JSRT_ParseURL for legacy format (80% code reuse)
3. **Platform Handling**: Use #ifdef _WIN32 for Windows-specific file URL logic
4. **Memory Safety**: Follow QuickJS patterns (js_malloc, JS_FreeValue, ASAN testing)
5. **Error Handling**: Return empty/default values for invalid input (Node.js behavior)

### Common Pitfalls to Avoid

1. **Memory Leaks**: Always free C strings from JS_ToCString()
2. **NULL Checks**: Validate all malloc() returns
3. **Object Cleanup**: Free all JSValue objects with JS_FreeValue()
4. **Platform Assumptions**: Test on both Unix and Windows
5. **Edge Cases**: Handle empty strings, NULL inputs, invalid URLs gracefully

### Development Workflow

```bash
# 1. Create feature branch
git checkout -b feature/node-url-module

# 2. Development cycle
# Edit src/node/node_url.c
make format       # Format code
make test         # Run tests
make wpt          # Check WPT compliance

# 3. Memory safety check
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m test/node/url/test_*.js

# 4. Before commit (MANDATORY)
make format
make test         # Must pass 100%
make wpt          # Must maintain 97%+
make clean && make  # Verify release build

# 5. Commit with message
git add .
git commit -m "feat(node): implement node:url module with 100% API coverage"
```

---

**End of Plan Document**
