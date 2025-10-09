---
Created: 2025-10-09T00:00:00Z
Last Updated: 2025-10-09T00:00:00Z
Status: 📋 READY FOR IMPLEMENTATION
Overall Progress: 0/155 tasks (0%)
API Coverage: 5/30+ methods (17% stub only)
---

# Node.js DNS Module Implementation Plan

## 📋 Executive Summary

### Objective
Implement a complete Node.js-compatible `node:dns` module in jsrt that provides DNS resolution capabilities using libuv's asynchronous DNS functions and optional network DNS protocol queries.

### Current Status
- ⚠️ **Stub Implementation Exists**: `/Users/bytedance/github/leizongmin/jsrt/src/node/node_dns.c` (224 lines)
  - Returns mock/hardcoded data (127.0.0.1, ::1)
  - Only implements: `lookup`, `resolve`, `resolve4`, `resolve6`, `reverse` (stub)
  - Uses promises but no actual DNS queries
  - No callback support (Node.js uses callbacks primarily)
- ✅ **libuv DNS API Available**: `uv_getaddrinfo` and `uv_getnameinfo` (system resolver)
- ✅ **Architecture Patterns**: Excellent reference from `src/node/net/` module
- ❌ **c-ares Not Available**: No direct DNS protocol library (affects resolve* methods)
- 🎯 **Target**: 30+ API methods with full Node.js compatibility

### Key Success Factors
1. **Replace Mock Implementation**: Transform stub into real DNS resolution
2. **Dual API Support**: Both callback-based and promise-based APIs
3. **libuv Integration**: Use `uv_getaddrinfo` for `dns.lookup()` and `uv_getnameinfo` for `dns.lookupService()`
4. **Incremental Approach**: Start with system resolver, add network queries if feasible
5. **Cross-platform**: Handle platform-specific DNS behaviors

### Implementation Strategy
- **Phase 0**: Research & Architecture (1-2 days, 15 tasks)
- **Phase 1**: Foundation & Request Management (2-3 days, 25 tasks)
- **Phase 2**: System Resolver (dns.lookup) (3-4 days, 30 tasks)
- **Phase 3**: Reverse Lookup (dns.lookupService) (2-3 days, 20 tasks)
- **Phase 4**: Promises API (dns.promises) (2-3 days, 25 tasks)
- **Phase 5**: Network DNS Queries (Optional) (4-5 days, 30 tasks)
- **Phase 6**: Advanced Features & Resolver Class (2-3 days, 10 tasks)
- **Total Estimated Time**: 16-23 days (core features), +4-5 days (advanced features)

---

## 🔍 Current State Analysis

### Existing Implementation Assessment

#### Stub Code at `src/node/node_dns.c`
**Current Implementation** (224 lines):
```c
// Functions implemented (mock data only):
- js_dns_lookup()       // Returns 127.0.0.1 or ::1 (hardcoded)
- js_dns_resolve()      // Redirects to lookup
- js_dns_resolve4()     // Returns IPv4 mock
- js_dns_resolve6()     // Returns IPv6 mock
- js_dns_reverse()      // Returns ENOTIMPL error

// Module exports:
- JSRT_InitNodeDns()    // CommonJS initialization
- js_node_dns_init()    // ES module initialization
- RRTYPE constants      // A, AAAA, CNAME, MX, NS, PTR, SOA, TXT
```

**Critical Issues**:
1. ❌ No actual DNS resolution (returns hardcoded values)
2. ❌ No callback support (Node.js primary API)
3. ❌ Returns promises but Node.js uses callbacks for main API
4. ❌ No error handling for real DNS failures
5. ❌ Missing 25+ methods from Node.js API
6. ❌ No libuv integration
7. ❌ No memory management for async operations

**Reuse Strategy**:
- ✅ Keep module structure and exports
- ✅ Keep RRTYPE constants
- 🔄 Replace all function implementations
- ➕ Add callback support
- ➕ Add libuv request management
- ➕ Add proper error handling

### Available Resources for Reuse

#### From libuv DNS API - **100% Required**
**Functions Available**:
```c
// System DNS resolution (uses OS resolver):
int uv_getaddrinfo(
  uv_loop_t* loop,
  uv_getaddrinfo_t* req,
  uv_getaddrinfo_cb callback,
  const char* node,          // hostname
  const char* service,       // port or service name
  const struct addrinfo* hints
);

void uv_freeaddrinfo(struct addrinfo* ai);

// Reverse lookup (IP to hostname):
int uv_getnameinfo(
  uv_loop_t* loop,
  uv_getnameinfo_t* req,
  uv_getnameinfo_cb callback,
  const struct sockaddr* addr,
  int flags
);

// Callback types:
typedef void (*uv_getaddrinfo_cb)(
  uv_getaddrinfo_t* req,
  int status,
  struct addrinfo* res
);

typedef void (*uv_getnameinfo_cb)(
  uv_getnameinfo_t* req,
  int status,
  const char* hostname,
  const char* service
);
```

**Request Types**:
```c
typedef struct {
  UV_REQ_FIELDS
  uv_loop_t* loop;
  struct addrinfo* addrinfo;  // Result (must free)
  // Internal fields...
} uv_getaddrinfo_t;

typedef struct {
  UV_REQ_FIELDS
  uv_loop_t* loop;
  char host[NI_MAXHOST];      // Result hostname
  char service[NI_MAXSERV];   // Result service
  // Internal fields...
} uv_getnameinfo_t;
```

**Integration Pattern**:
```c
// Request state management (like net module):
typedef struct {
  uv_getaddrinfo_t req;      // libuv request
  JSContext* ctx;
  JSValue callback;           // JavaScript callback
  JSValue promise_funcs[2];   // For promises API
  bool use_promise;
  // Options...
} DNSLookupRequest;

// Usage:
DNSLookupRequest* req = js_malloc(ctx, sizeof(DNSLookupRequest));
req->ctx = ctx;
req->callback = JS_DupValue(ctx, callback_arg);
req->req.data = req;

uv_getaddrinfo(loop, &req->req, on_getaddrinfo_complete,
               hostname, NULL, &hints);
```

#### From `src/node/net/` Module - **90% Pattern Reuse**
**Excellent Architecture Patterns**:
```c
// Modular structure reference:
net_callbacks.c      → dns_callbacks.c     // libuv callback handlers
net_finalizers.c     → (not needed)        // Requests are short-lived
net_socket.c         → dns_lookup.c        // lookup/lookupService
net_module.c         → dns_module.c        // Module exports
net_internal.h       → dns_internal.h      // Shared declarations

// Request management pattern:
typedef struct {
  JSContext* ctx;
  JSValue callback;           // Store callback
  JSValue error_on_fail;      // Error handling
  // Request-specific data...
} RequestState;

// Error handling pattern (from net):
void emit_dns_error(JSContext* ctx, JSValue callback,
                    int status, const char* syscall) {
  JSValue error = node_make_system_error(ctx, status, syscall);
  JSValue args[] = {error};
  JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
  JS_FreeValue(ctx, error);
}
```

**Adaptation Strategy**:
```c
// net module → dns module mappings:
uv_tcp_t           → Not needed (stateless requests)
uv_connect_t       → uv_getaddrinfo_t / uv_getnameinfo_t
on_connection      → on_getaddrinfo / on_getnameinfo
Connection state   → Request state (short-lived)
EventEmitter       → Not needed (callback-based API)
```

### DNS API Categories

#### Category 1: System Resolver (Phase 2-3) - **PRIORITY**
Uses libuv's threadpool, operates like `getaddrinfo(3)`:
```javascript
// dns.lookup(hostname[, options], callback)
dns.lookup('example.com', (err, address, family) => {
  // address: '93.184.216.34', family: 4
});

// dns.lookupService(address, port, callback)
dns.lookupService('127.0.0.1', 22, (err, hostname, service) => {
  // hostname: 'localhost', service: 'ssh'
});
```

**Implementation**: Use `uv_getaddrinfo` and `uv_getnameinfo`

#### Category 2: Network DNS Queries (Phase 5) - **OPTIONAL**
Direct DNS protocol queries (requires c-ares or custom implementation):
```javascript
// dns.resolve(hostname[, rrtype], callback)
dns.resolve4('example.com', (err, addresses) => {
  // addresses: ['93.184.216.34']
});

// Many record types:
dns.resolveMx('gmail.com', callback);   // MX records
dns.resolveTxt('example.com', callback); // TXT records
dns.resolveCname('www.example.com', callback);
// ... resolve6, resolveNs, resolveSoa, resolveSrv, resolveNaptr, etc.
```

**Implementation Options**:
1. **Option A**: Use `uv_getaddrinfo` with workarounds (limited functionality)
2. **Option B**: Integrate c-ares library (full functionality, adds dependency)
3. **Option C**: Stub with error codes (defer implementation)

**Recommendation**: Start with Option C (stubs), upgrade to A/B if needed

#### Category 3: Promises API (Phase 4)
Parallel API returning promises:
```javascript
import { promises as dnsPromises } from 'node:dns';
// or
import * as dnsPromises from 'node:dns/promises';

const addresses = await dnsPromises.lookup('example.com');
const mx = await dnsPromises.resolveMx('gmail.com');
```

**Implementation**: Wrapper around callback API

#### Category 4: Configuration & Resolver (Phase 6)
Server configuration and custom resolvers:
```javascript
dns.getServers();                     // Get DNS servers
dns.setServers(['8.8.8.8', '1.1.1.1']); // Set DNS servers
dns.setDefaultResultOrder('ipv4first'); // Address ordering

const resolver = new dns.Resolver();
resolver.setServers(['8.8.8.8']);
resolver.resolve4('example.com', callback);
```

**Implementation**: Platform-specific, may be limited without c-ares

---

## 📐 Architecture Design

### Module Structure

```
src/node/dns/
├── dns_internal.h          # Shared declarations, request structures
├── dns_callbacks.c         # libuv callback handlers
├── dns_lookup.c           # dns.lookup() implementation
├── dns_lookupservice.c    # dns.lookupService() implementation
├── dns_resolve.c          # dns.resolve*() methods (stubs/basic)
├── dns_promises.c         # Promises API wrapper
├── dns_module.c           # Module exports and initialization
└── dns_errors.c           # Error code mapping and handling
```

**Rationale**:
- Modular structure (like net module) for maintainability
- Separate files for different DNS operations
- Clear separation of concerns
- Future expansion friendly

### Request State Management

```c
// dns_internal.h

// Lookup request state
typedef struct {
  uv_getaddrinfo_t req;      // Must be first (for req.data access)
  JSContext* ctx;
  JSValue callback;           // JavaScript callback function
  JSValue promise_funcs[2];   // [resolve, reject] for promises
  bool use_promise;           // true if promises API
  bool all;                   // Return all addresses
  int family;                 // 0, 4, or 6
  int hints_flags;            // Additional getaddrinfo hints
  char* hostname;             // Saved for error messages
} DNSLookupRequest;

// Lookup service request state
typedef struct {
  uv_getnameinfo_t req;      // Must be first
  JSContext* ctx;
  JSValue callback;
  JSValue promise_funcs[2];
  bool use_promise;
  char* address;             // Saved for error messages
  int port;
} DNSLookupServiceRequest;

// Generic resolve request (for future network queries)
typedef struct {
  JSContext* ctx;
  JSValue callback;
  JSValue promise_funcs[2];
  bool use_promise;
  char* hostname;
  int rrtype;                // Record type
  // c-ares channel if implemented
} DNSResolveRequest;
```

### Error Handling Strategy

```c
// Map libuv error codes to Node.js DNS error codes
typedef struct {
  int uv_error;
  const char* node_code;
  const char* description;
} DNSErrorMapping;

// Common DNS errors:
static const DNSErrorMapping dns_errors[] = {
  {UV_EAI_ADDRFAMILY, "EADDRFAMILY", "Address family not supported"},
  {UV_EAI_AGAIN, "ENOTFOUND", "DNS server returned temporary failure"},
  {UV_EAI_BADFLAGS, "EBADFLAGS", "Bad hints flags value"},
  {UV_EAI_FAIL, "ENOTFOUND", "DNS server returned permanent failure"},
  {UV_EAI_FAMILY, "EADDRFAMILY", "Unsupported address family"},
  {UV_EAI_MEMORY, "ENOMEM", "Out of memory"},
  {UV_EAI_NODATA, "ENODATA", "No data returned"},
  {UV_EAI_NONAME, "ENOTFOUND", "Hostname not found"},
  {UV_EAI_SERVICE, "ESERVICE", "Service not available"},
  // ... more mappings
};

// Error creation function
JSValue create_dns_error(JSContext* ctx, int status,
                        const char* syscall, const char* hostname);
```

### Memory Management

**Critical Rules**:
1. Request structs allocated on heap (async operation)
2. Free request after callback completes
3. Free addrinfo with `uv_freeaddrinfo()`
4. Duplicate JS values that persist beyond function scope
5. Clean up on error paths

**Pattern**:
```c
void on_getaddrinfo_callback(uv_getaddrinfo_t* req,
                            int status, struct addrinfo* res) {
  DNSLookupRequest* dns_req = (DNSLookupRequest*)req;
  JSContext* ctx = dns_req->ctx;

  // Process result...

  // Cleanup (CRITICAL):
  if (res) uv_freeaddrinfo(res);
  JS_FreeValue(ctx, dns_req->callback);
  if (dns_req->use_promise) {
    JS_FreeValue(ctx, dns_req->promise_funcs[0]);
    JS_FreeValue(ctx, dns_req->promise_funcs[1]);
  }
  if (dns_req->hostname) js_free(ctx, dns_req->hostname);
  js_free(ctx, dns_req);
}
```

---

## 📝 Task Breakdown & Execution Tracker

### Phase 0: Research & Architecture Setup
**Goal**: Understand existing code, plan replacement strategy
**Duration**: 1-2 days

| ID | Task | Exec Mode | Status | Dependencies | Risk | Complexity |
|----|------|-----------|--------|--------------|------|------------|
| 0.1 | Analyze existing stub implementation | [S] | ⏳ PENDING | None | LOW | SIMPLE |
| 0.2 | Study libuv DNS API documentation | [P] | ⏳ PENDING | None | LOW | SIMPLE |
| 0.3 | Study Node.js dns module API specification | [P] | ⏳ PENDING | None | LOW | MEDIUM |
| 0.4 | Review net module architecture patterns | [S] | ⏳ PENDING | 0.1 | LOW | MEDIUM |
| 0.5 | Design request state structures | [S] | ⏳ PENDING | 0.2,0.3,0.4 | MED | MEDIUM |
| 0.6 | Plan module directory structure | [S] | ⏳ PENDING | 0.5 | LOW | SIMPLE |
| 0.7 | Create dns_internal.h header | [S] | ⏳ PENDING | 0.5,0.6 | LOW | MEDIUM |
| 0.8 | Map DNS error codes (libuv → Node.js) | [P] | ⏳ PENDING | 0.2,0.3 | LOW | MEDIUM |
| 0.9 | Design memory management strategy | [S] | ⏳ PENDING | 0.5 | HIGH | MEDIUM |
| 0.10 | Create test plan for dns.lookup | [P] | ⏳ PENDING | 0.3 | LOW | SIMPLE |
| 0.11 | Create test plan for dns.lookupService | [P] | ⏳ PENDING | 0.3 | LOW | SIMPLE |
| 0.12 | Document migration plan from stub | [S] | ⏳ PENDING | 0.1-0.9 | LOW | SIMPLE |
| 0.13 | Set up build integration | [S] | ⏳ PENDING | 0.6 | LOW | TRIVIAL |
| 0.14 | Run baseline tests (make test) | [S] | ⏳ PENDING | 0.13 | LOW | TRIVIAL |
| 0.15 | Create Phase 0 completion checklist | [S] | ⏳ PENDING | 0.1-0.14 | LOW | TRIVIAL |

**Success Criteria**:
- [ ] All existing stub code analyzed and documented
- [ ] Request state structures defined in dns_internal.h
- [ ] Error mapping table created
- [ ] Memory management patterns documented
- [ ] Test plans created
- [ ] Baseline tests pass

---

### Phase 1: Foundation & Request Management
**Goal**: Create infrastructure for async DNS operations
**Duration**: 2-3 days

| ID | Task | Exec Mode | Status | Dependencies | Risk | Complexity |
|----|------|-----------|--------|--------------|------|------------|
| 1.1 | Create src/node/dns/ directory | [S] | ⏳ PENDING | 0.15 | LOW | TRIVIAL |
| 1.2 | Create dns_internal.h header file | [S] | ⏳ PENDING | 1.1 | LOW | MEDIUM |
| 1.3 | Define DNSLookupRequest structure | [S] | ⏳ PENDING | 1.2 | LOW | MEDIUM |
| 1.4 | Define DNSLookupServiceRequest structure | [S] | ⏳ PENDING | 1.2 | LOW | MEDIUM |
| 1.5 | Create dns_errors.c for error handling | [S] | ⏳ PENDING | 1.2 | LOW | MEDIUM |
| 1.6 | Implement create_dns_error() function | [S] | ⏳ PENDING | 1.5 | MED | MEDIUM |
| 1.7 | Implement get_dns_error_code() mapping | [S] | ⏳ PENDING | 1.5 | LOW | MEDIUM |
| 1.8 | Create dns_callbacks.c file | [S] | ⏳ PENDING | 1.2 | LOW | SIMPLE |
| 1.9 | Implement request allocation helper | [S] | ⏳ PENDING | 1.3,1.4 | MED | MEDIUM |
| 1.10 | Implement request cleanup helper | [S] | ⏳ PENDING | 1.3,1.4 | HIGH | MEDIUM |
| 1.11 | Add memory leak detection test | [S] | ⏳ PENDING | 1.10 | HIGH | MEDIUM |
| 1.12 | Test error creation functions | [S] | ⏳ PENDING | 1.6,1.7 | LOW | SIMPLE |
| 1.13 | Create dns_module.c skeleton | [S] | ⏳ PENDING | 1.2 | LOW | SIMPLE |
| 1.14 | Add module to build system (Makefile) | [S] | ⏳ PENDING | 1.13 | MED | MEDIUM |
| 1.15 | Verify build succeeds | [S] | ⏳ PENDING | 1.14 | LOW | TRIVIAL |
| 1.16 | Implement helper: parse_lookup_options() | [S] | ⏳ PENDING | 1.3 | MED | MEDIUM |
| 1.17 | Implement helper: convert_addrinfo_to_js() | [S] | ⏳ PENDING | 1.3 | MED | COMPLEX |
| 1.18 | Test option parsing with various inputs | [S] | ⏳ PENDING | 1.16 | LOW | SIMPLE |
| 1.19 | Test addrinfo conversion logic | [S] | ⏳ PENDING | 1.17 | MED | MEDIUM |
| 1.20 | Document request lifecycle | [P] | ⏳ PENDING | 1.9,1.10 | LOW | SIMPLE |
| 1.21 | Run make test (verify no regressions) | [S] | ⏳ PENDING | 1.15 | LOW | TRIVIAL |
| 1.22 | Run make format | [S] | ⏳ PENDING | 1.21 | LOW | TRIVIAL |
| 1.23 | ASAN validation (make jsrt_m) | [S] | ⏳ PENDING | 1.11 | HIGH | MEDIUM |
| 1.24 | Code review checklist completion | [S] | ⏳ PENDING | 1.1-1.23 | LOW | SIMPLE |
| 1.25 | Phase 1 completion validation | [S] | ⏳ PENDING | 1.24 | LOW | TRIVIAL |

**Success Criteria**:
- [ ] Directory structure created
- [ ] Request structures defined and documented
- [ ] Error handling infrastructure complete
- [ ] Helper functions tested
- [ ] No memory leaks (ASAN clean)
- [ ] Build system integrated
- [ ] All tests pass

---

### Phase 2: System Resolver - dns.lookup()
**Goal**: Implement core DNS hostname resolution
**Duration**: 3-4 days

| ID | Task | Exec Mode | Status | Dependencies | Risk | Complexity |
|----|------|-----------|--------|--------------|------|------------|
| 2.1 | Create dns_lookup.c file | [S] | ⏳ PENDING | 1.25 | LOW | TRIVIAL |
| 2.2 | Implement js_dns_lookup() entry point | [S] | ⏳ PENDING | 2.1 | MED | MEDIUM |
| 2.3 | Add argument parsing (hostname, options, callback) | [S] | ⏳ PENDING | 2.2 | MED | MEDIUM |
| 2.4 | Validate hostname argument (type, non-empty) | [S] | ⏳ PENDING | 2.3 | LOW | SIMPLE |
| 2.5 | Parse options object (family, hints, all, verbatim) | [S] | ⏳ PENDING | 2.3 | MED | COMPLEX |
| 2.6 | Allocate DNSLookupRequest structure | [S] | ⏳ PENDING | 2.5 | MED | MEDIUM |
| 2.7 | Store callback and context in request | [S] | ⏳ PENDING | 2.6 | MED | MEDIUM |
| 2.8 | Build addrinfo hints from options | [S] | ⏳ PENDING | 2.5 | MED | COMPLEX |
| 2.9 | Handle family option (0, 4, 6) | [S] | ⏳ PENDING | 2.8 | LOW | MEDIUM |
| 2.10 | Handle hints option (flags) | [S] | ⏳ PENDING | 2.8 | MED | MEDIUM |
| 2.11 | Call uv_getaddrinfo() with parameters | [S] | ⏳ PENDING | 2.8 | HIGH | MEDIUM |
| 2.12 | Implement error path cleanup | [S] | ⏳ PENDING | 2.6,2.11 | HIGH | MEDIUM |
| 2.13 | Implement on_getaddrinfo_callback() | [S] | ⏳ PENDING | 2.11 | HIGH | COMPLEX |
| 2.14 | Handle callback status errors | [S] | ⏳ PENDING | 2.13 | MED | MEDIUM |
| 2.15 | Convert addrinfo to JS (single address) | [S] | ⏳ PENDING | 2.13 | MED | COMPLEX |
| 2.16 | Convert addrinfo to JS (all addresses) | [S] | ⏳ PENDING | 2.13 | MED | COMPLEX |
| 2.17 | Call JavaScript callback with results | [S] | ⏳ PENDING | 2.15,2.16 | MED | MEDIUM |
| 2.18 | Free addrinfo with uv_freeaddrinfo() | [S] | ⏳ PENDING | 2.17 | HIGH | SIMPLE |
| 2.19 | Free request structure and JS values | [S] | ⏳ PENDING | 2.17 | HIGH | MEDIUM |
| 2.20 | Write test: lookup IPv4 address | [S] | ⏳ PENDING | 2.19 | LOW | SIMPLE |
| 2.21 | Write test: lookup IPv6 address | [S] | ⏳ PENDING | 2.19 | LOW | SIMPLE |
| 2.22 | Write test: lookup with family option | [S] | ⏳ PENDING | 2.19 | LOW | SIMPLE |
| 2.23 | Write test: lookup with all option | [S] | ⏳ PENDING | 2.19 | LOW | MEDIUM |
| 2.24 | Write test: lookup error handling | [S] | ⏳ PENDING | 2.19 | MED | MEDIUM |
| 2.25 | Write test: lookup localhost | [S] | ⏳ PENDING | 2.19 | LOW | SIMPLE |
| 2.26 | Update dns_module.c exports | [S] | ⏳ PENDING | 2.2 | LOW | SIMPLE |
| 2.27 | Run all tests (make test) | [S] | ⏳ PENDING | 2.20-2.25 | MED | TRIVIAL |
| 2.28 | ASAN validation (no leaks) | [S] | ⏳ PENDING | 2.27 | HIGH | MEDIUM |
| 2.29 | Test with real hostnames (example.com, google.com) | [S] | ⏳ PENDING | 2.27 | MED | SIMPLE |
| 2.30 | Phase 2 completion validation | [S] | ⏳ PENDING | 2.1-2.29 | LOW | TRIVIAL |

**Success Criteria**:
- [ ] dns.lookup() works with callbacks
- [ ] Supports all options (family, hints, all, verbatim)
- [ ] Correctly resolves localhost and real hostnames
- [ ] Error handling works (invalid hostnames)
- [ ] No memory leaks (ASAN clean)
- [ ] 6+ tests passing

---

### Phase 3: Reverse Lookup - dns.lookupService()
**Goal**: Implement IP address to hostname/service resolution
**Duration**: 2-3 days

| ID | Task | Exec Mode | Status | Dependencies | Risk | Complexity |
|----|------|-----------|--------|--------------|------|------------|
| 3.1 | Create dns_lookupservice.c file | [S] | ⏳ PENDING | 2.30 | LOW | TRIVIAL |
| 3.2 | Implement js_dns_lookupservice() entry point | [S] | ⏳ PENDING | 3.1 | MED | MEDIUM |
| 3.3 | Parse address and port arguments | [S] | ⏳ PENDING | 3.2 | MED | MEDIUM |
| 3.4 | Validate address format (IPv4/IPv6) | [S] | ⏳ PENDING | 3.3 | MED | MEDIUM |
| 3.5 | Validate port number (0-65535) | [S] | ⏳ PENDING | 3.3 | LOW | SIMPLE |
| 3.6 | Allocate DNSLookupServiceRequest | [S] | ⏳ PENDING | 3.5 | MED | MEDIUM |
| 3.7 | Convert address+port to sockaddr struct | [S] | ⏳ PENDING | 3.4,3.5 | HIGH | COMPLEX |
| 3.8 | Handle IPv4 address (sockaddr_in) | [S] | ⏳ PENDING | 3.7 | MED | MEDIUM |
| 3.9 | Handle IPv6 address (sockaddr_in6) | [S] | ⏳ PENDING | 3.7 | MED | MEDIUM |
| 3.10 | Call uv_getnameinfo() with sockaddr | [S] | ⏳ PENDING | 3.7 | HIGH | MEDIUM |
| 3.11 | Implement error path cleanup | [S] | ⏳ PENDING | 3.6,3.10 | HIGH | MEDIUM |
| 3.12 | Implement on_getnameinfo_callback() | [S] | ⏳ PENDING | 3.10 | HIGH | MEDIUM |
| 3.13 | Handle callback status errors | [S] | ⏳ PENDING | 3.12 | MED | MEDIUM |
| 3.14 | Extract hostname from result | [S] | ⏳ PENDING | 3.12 | LOW | SIMPLE |
| 3.15 | Extract service from result | [S] | ⏳ PENDING | 3.12 | LOW | SIMPLE |
| 3.16 | Call JavaScript callback with results | [S] | ⏳ PENDING | 3.14,3.15 | MED | MEDIUM |
| 3.17 | Free request structure | [S] | ⏳ PENDING | 3.16 | HIGH | MEDIUM |
| 3.18 | Write test: lookupService for 127.0.0.1:22 | [S] | ⏳ PENDING | 3.17 | LOW | SIMPLE |
| 3.19 | Write test: lookupService for ::1:22 | [S] | ⏳ PENDING | 3.17 | LOW | SIMPLE |
| 3.20 | Write test: lookupService error handling | [S] | ⏳ PENDING | 3.17 | MED | MEDIUM |
| 3.21 | Update dns_module.c exports | [S] | ⏳ PENDING | 3.2 | LOW | SIMPLE |
| 3.22 | Run all tests (make test) | [S] | ⏳ PENDING | 3.18-3.20 | MED | TRIVIAL |
| 3.23 | ASAN validation | [S] | ⏳ PENDING | 3.22 | HIGH | MEDIUM |
| 3.24 | Phase 3 completion validation | [S] | ⏳ PENDING | 3.1-3.23 | LOW | TRIVIAL |

**Success Criteria**:
- [ ] dns.lookupService() works with callbacks
- [ ] Correctly resolves IPv4 and IPv6 addresses
- [ ] Returns hostname and service name
- [ ] Error handling works (invalid addresses)
- [ ] No memory leaks (ASAN clean)
- [ ] 3+ tests passing

---

### Phase 4: Promises API (dns.promises)
**Goal**: Add promise-based API as wrapper around callbacks
**Duration**: 2-3 days

| ID | Task | Exec Mode | Status | Dependencies | Risk | Complexity |
|----|------|-----------|--------|--------------|------|------------|
| 4.1 | Create dns_promises.c file | [S] | ⏳ PENDING | 3.24 | LOW | TRIVIAL |
| 4.2 | Design promise wrapper pattern | [S] | ⏳ PENDING | 4.1 | MED | MEDIUM |
| 4.3 | Implement js_dns_lookup_promise() | [S] | ⏳ PENDING | 4.2 | MED | MEDIUM |
| 4.4 | Create promise capability (resolve/reject) | [S] | ⏳ PENDING | 4.3 | MED | MEDIUM |
| 4.5 | Store promise_funcs in request | [S] | ⏳ PENDING | 4.4 | MED | MEDIUM |
| 4.6 | Set use_promise flag | [S] | ⏳ PENDING | 4.5 | LOW | SIMPLE |
| 4.7 | Modify on_getaddrinfo_callback for promises | [S] | ⏳ PENDING | 4.6 | HIGH | MEDIUM |
| 4.8 | Call resolve() on success | [S] | ⏳ PENDING | 4.7 | MED | MEDIUM |
| 4.9 | Call reject() on error | [S] | ⏳ PENDING | 4.7 | MED | MEDIUM |
| 4.10 | Implement js_dns_lookupservice_promise() | [S] | ⏳ PENDING | 4.3-4.9 | MED | MEDIUM |
| 4.11 | Modify on_getnameinfo_callback for promises | [S] | ⏳ PENDING | 4.10 | HIGH | MEDIUM |
| 4.12 | Create dns.promises namespace object | [S] | ⏳ PENDING | 4.11 | LOW | MEDIUM |
| 4.13 | Export promises.lookup() | [S] | ⏳ PENDING | 4.12 | LOW | SIMPLE |
| 4.14 | Export promises.lookupService() | [S] | ⏳ PENDING | 4.12 | LOW | SIMPLE |
| 4.15 | Write test: promises.lookup() with await | [S] | ⏳ PENDING | 4.13 | LOW | SIMPLE |
| 4.16 | Write test: promises.lookupService() with await | [S] | ⏳ PENDING | 4.14 | LOW | SIMPLE |
| 4.17 | Write test: promise rejection on error | [S] | ⏳ PENDING | 4.15 | MED | MEDIUM |
| 4.18 | Update dns_module.c exports | [S] | ⏳ PENDING | 4.12 | LOW | MEDIUM |
| 4.19 | Support require('node:dns/promises') | [S] | ⏳ PENDING | 4.18 | MED | COMPLEX |
| 4.20 | Run all tests (make test) | [S] | ⏳ PENDING | 4.15-4.17 | MED | TRIVIAL |
| 4.21 | ASAN validation | [S] | ⏳ PENDING | 4.20 | HIGH | MEDIUM |
| 4.22 | Test async/await syntax | [S] | ⏳ PENDING | 4.20 | LOW | SIMPLE |
| 4.23 | Test .then()/.catch() syntax | [S] | ⏳ PENDING | 4.20 | LOW | SIMPLE |
| 4.24 | Document promises API usage | [P] | ⏳ PENDING | 4.1-4.23 | LOW | SIMPLE |
| 4.25 | Phase 4 completion validation | [S] | ⏳ PENDING | 4.24 | LOW | TRIVIAL |

**Success Criteria**:
- [ ] dns.promises.lookup() works
- [ ] dns.promises.lookupService() works
- [ ] require('node:dns/promises') works
- [ ] Promises resolve/reject correctly
- [ ] async/await and .then() both work
- [ ] No memory leaks
- [ ] 5+ tests passing

---

### Phase 5: Network DNS Queries (Optional/Stub)
**Goal**: Implement resolve*() methods (stub or basic implementation)
**Duration**: 4-5 days (if implemented), 1 day (if stubbed)

#### Option A: Stub Implementation (Recommended Initial Approach)

| ID | Task | Exec Mode | Status | Dependencies | Risk | Complexity |
|----|------|-----------|--------|--------------|------|------------|
| 5.1 | Create dns_resolve.c file | [S] | ⏳ PENDING | 4.25 | LOW | TRIVIAL |
| 5.2 | Implement js_dns_resolve() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.3 | Implement js_dns_resolve4() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.4 | Implement js_dns_resolve6() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.5 | Implement js_dns_resolveMx() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.6 | Implement js_dns_resolveTxt() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.7 | Implement js_dns_resolveCname() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.8 | Implement js_dns_resolveNs() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.9 | Implement js_dns_resolveSoa() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.10 | Implement js_dns_resolveSrv() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.11 | Implement js_dns_resolveNaptr() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.12 | Implement js_dns_resolvePtr() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.13 | Implement js_dns_resolveCaa() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.14 | Implement js_dns_reverse() (stub) | [S] | ⏳ PENDING | 5.1 | LOW | SIMPLE |
| 5.15 | All stubs return ENOTIMPL error | [S] | ⏳ PENDING | 5.2-5.14 | LOW | SIMPLE |
| 5.16 | Update dns_module.c exports | [S] | ⏳ PENDING | 5.15 | LOW | MEDIUM |
| 5.17 | Add promises wrappers for resolve methods | [S] | ⏳ PENDING | 5.16 | LOW | MEDIUM |
| 5.18 | Write tests verifying ENOTIMPL errors | [S] | ⏳ PENDING | 5.17 | LOW | SIMPLE |
| 5.19 | Document stub status in comments | [P] | ⏳ PENDING | 5.1-5.18 | LOW | TRIVIAL |
| 5.20 | Phase 5A completion validation | [S] | ⏳ PENDING | 5.19 | LOW | TRIVIAL |

#### Option B: Basic Implementation using getaddrinfo workarounds (Future)

| ID | Task | Exec Mode | Status | Dependencies | Risk | Complexity |
|----|------|-----------|--------|--------------|------|------------|
| 5.21 | Research getaddrinfo record type support | [S] | ⏳ PENDING | 5.20 | MED | MEDIUM |
| 5.22 | Implement resolve4 using getaddrinfo | [S] | ⏳ PENDING | 5.21 | MED | COMPLEX |
| 5.23 | Implement resolve6 using getaddrinfo | [S] | ⏳ PENDING | 5.21 | MED | COMPLEX |
| 5.24 | Research reverse DNS with getnameinfo | [S] | ⏳ PENDING | 5.21 | MED | MEDIUM |
| 5.25 | Implement reverse() using getnameinfo | [S] | ⏳ PENDING | 5.24 | MED | COMPLEX |
| 5.26 | Test basic resolve implementations | [S] | ⏳ PENDING | 5.22,5.23,5.25 | MED | MEDIUM |
| 5.27 | Document limitations vs full DNS queries | [P] | ⏳ PENDING | 5.26 | LOW | SIMPLE |
| 5.28 | Phase 5B completion validation | [S] | ⏳ PENDING | 5.27 | LOW | TRIVIAL |

#### Option C: Full c-ares Integration (Advanced, Future)

| ID | Task | Exec Mode | Status | Dependencies | Risk | Complexity |
|----|------|-----------|--------|--------------|------|------------|
| 5.29 | Evaluate c-ares library integration | [S] | ⏳ PENDING | 5.20 | HIGH | COMPLEX |
| 5.30 | Add c-ares to dependencies | [S] | ⏳ PENDING | 5.29 | HIGH | COMPLEX |
| 5.31 | Integrate c-ares with libuv event loop | [S] | ⏳ PENDING | 5.30 | HIGH | COMPLEX |
| 5.32 | (Deferred - out of initial scope) | [-] | ⏳ PENDING | - | - | - |

**Success Criteria (Option A - Recommended)**:
- [ ] All resolve*() methods defined
- [ ] All return ENOTIMPL error
- [ ] Promises API wrappers exist
- [ ] Tests verify stub behavior
- [ ] No memory leaks

---

### Phase 6: Advanced Features & Configuration
**Goal**: Server configuration, Resolver class, additional features
**Duration**: 2-3 days

| ID | Task | Exec Mode | Status | Dependencies | Risk | Complexity |
|----|------|-----------|--------|--------------|------|------------|
| 6.1 | Implement dns.getServers() | [S] | ⏳ PENDING | 5.20 | MED | MEDIUM |
| 6.2 | Implement dns.setServers() | [S] | ⏳ PENDING | 6.1 | HIGH | COMPLEX |
| 6.3 | Store server list globally | [S] | ⏳ PENDING | 6.2 | MED | MEDIUM |
| 6.4 | Implement dns.setDefaultResultOrder() | [S] | ⏳ PENDING | 6.3 | MED | MEDIUM |
| 6.5 | Implement dns.getDefaultResultOrder() | [S] | ⏳ PENDING | 6.4 | LOW | SIMPLE |
| 6.6 | Create Resolver class skeleton | [S] | ⏳ PENDING | 6.5 | MED | COMPLEX |
| 6.7 | Implement Resolver constructor | [S] | ⏳ PENDING | 6.6 | MED | MEDIUM |
| 6.8 | Implement Resolver.setServers() | [S] | ⏳ PENDING | 6.7 | MED | MEDIUM |
| 6.9 | Implement Resolver.cancel() | [S] | ⏳ PENDING | 6.7 | HIGH | COMPLEX |
| 6.10 | Add Resolver methods (mirror main API) | [S] | ⏳ PENDING | 6.7 | MED | MEDIUM |
| 6.11 | Write tests for getServers/setServers | [S] | ⏳ PENDING | 6.2 | LOW | MEDIUM |
| 6.12 | Write tests for Resolver class | [S] | ⏳ PENDING | 6.10 | MED | MEDIUM |
| 6.13 | Update dns_module.c exports | [S] | ⏳ PENDING | 6.5,6.10 | LOW | MEDIUM |
| 6.14 | Run all tests (make test) | [S] | ⏳ PENDING | 6.11,6.12 | MED | TRIVIAL |
| 6.15 | ASAN validation | [S] | ⏳ PENDING | 6.14 | HIGH | MEDIUM |
| 6.16 | Phase 6 completion validation | [S] | ⏳ PENDING | 6.1-6.15 | LOW | TRIVIAL |

**Success Criteria**:
- [ ] getServers/setServers work
- [ ] setDefaultResultOrder implemented
- [ ] Resolver class exists and is usable
- [ ] Tests pass
- [ ] No memory leaks

---

## 🚀 Live Execution Dashboard

### Current Phase: Phase 0 - Research & Architecture Setup
**Overall Progress:** 0/155 tasks completed (0%)

**Complexity Status:** Planning phase

**Status:** READY TO START

### Parallel Execution Opportunities
**Can Run Now (No Dependencies):**
- Task 0.1: Analyze existing stub implementation
- Task 0.2: Study libuv DNS API documentation
- Task 0.3: Study Node.js dns module API specification

**Can Run in Parallel After Phase 0:**
- Phase 1 tasks are mostly sequential (infrastructure building)
- Tests in later phases can run in parallel with documentation

### Next Steps
1. Start with Phase 0 research tasks (0.1, 0.2, 0.3 in parallel)
2. Complete architecture design (0.4-0.9)
3. Create test plans and baselines (0.10-0.15)
4. Begin Phase 1 implementation

### Critical Path
Phase 0 → Phase 1 → Phase 2 → Phase 3 → Phase 4 → (Optional: Phase 5, 6)

**Minimum Viable Product (MVP)**:
- Phase 0-4 complete = Core DNS functionality working
- Estimated time: 10-15 days

**Full Implementation**:
- All phases complete = Complete Node.js compatibility
- Estimated time: 16-23 days (with stubs), 20-30 days (with c-ares)

---

## 📊 Risk Assessment & Mitigation

### High-Risk Areas

#### 1. Memory Management in Async Callbacks
**Risk**: Memory leaks or use-after-free in async operations
**Probability**: HIGH
**Impact**: CRITICAL
**Mitigation**:
- Follow net module patterns exactly
- Use ASAN in every test iteration
- Create dedicated leak detection tests
- Document cleanup paths clearly
- Code review all allocation/free pairs

#### 2. sockaddr Structure Handling
**Risk**: Incorrect address conversion, platform differences
**Probability**: MEDIUM
**Impact**: HIGH
**Mitigation**:
- Test on Linux, macOS, Windows
- Use standard POSIX functions
- Validate address formats before conversion
- Test both IPv4 and IPv6 thoroughly

#### 3. Error Code Mapping
**Risk**: Incorrect libuv → Node.js error code translation
**Probability**: MEDIUM
**Impact**: MEDIUM
**Mitigation**:
- Create comprehensive error mapping table
- Test all error paths
- Compare with Node.js behavior
- Document differences

#### 4. libuv Event Loop Integration
**Risk**: Blocking event loop, deadlocks
**Probability**: LOW
**Impact**: HIGH
**Mitigation**:
- All DNS operations are async by design
- Use libuv's threadpool (automatic for getaddrinfo)
- Never call synchronous DNS functions
- Monitor event loop health in tests

#### 5. Promise Memory Management
**Risk**: Leaked promise capabilities, dangling references
**Probability**: MEDIUM
**Impact**: HIGH
**Mitigation**:
- Always free promise_funcs[0] and [1]
- Test promise rejection paths
- ASAN validation on all promise tests
- Follow QuickJS promise documentation

### Medium-Risk Areas

#### 6. Cross-Platform DNS Behavior
**Risk**: Different results on different platforms
**Probability**: MEDIUM
**Impact**: MEDIUM
**Mitigation**:
- Document platform-specific behavior
- Test on all major platforms
- Use conditional compilation if needed
- Follow libuv's platform abstractions

#### 7. Network DNS Queries without c-ares
**Risk**: Limited functionality for resolve*() methods
**Probability**: HIGH (if not using c-ares)
**Impact**: MEDIUM
**Mitigation**:
- Start with stub implementations
- Document limitations clearly
- Provide upgrade path to c-ares
- Consider getaddrinfo workarounds

### Low-Risk Areas

#### 8. Module Export Structure
**Risk**: Incorrect CommonJS/ESM exports
**Probability**: LOW
**Impact**: LOW
**Mitigation**:
- Follow existing module patterns
- Test both require() and import
- Verify export names match Node.js

---

## ✅ Success Criteria & Validation

### Phase-Level Success Criteria

**Phase 0 Complete When**:
- [ ] All documentation reviewed and understood
- [ ] Architecture designed and documented
- [ ] Request structures defined
- [ ] Error mapping complete
- [ ] Test plans created
- [ ] Baseline tests pass

**Phase 1 Complete When**:
- [ ] Directory structure created
- [ ] All helper functions implemented and tested
- [ ] Error handling infrastructure working
- [ ] Build system integrated
- [ ] No memory leaks (ASAN clean)
- [ ] make test && make format pass

**Phase 2 Complete When**:
- [ ] dns.lookup() fully functional
- [ ] All options supported (family, hints, all, verbatim)
- [ ] Real hostname resolution works
- [ ] 6+ tests passing
- [ ] No memory leaks
- [ ] Error handling correct

**Phase 3 Complete When**:
- [ ] dns.lookupService() fully functional
- [ ] IPv4 and IPv6 supported
- [ ] 3+ tests passing
- [ ] No memory leaks
- [ ] Error handling correct

**Phase 4 Complete When**:
- [ ] Promises API works for lookup and lookupService
- [ ] require('node:dns/promises') works
- [ ] 5+ tests passing
- [ ] async/await and .then() both work
- [ ] No memory leaks

**Phase 5 Complete When** (Option A - Stubs):
- [ ] All resolve*() methods defined
- [ ] All return ENOTIMPL
- [ ] Promises wrappers exist
- [ ] Tests verify stub behavior

**Phase 6 Complete When**:
- [ ] getServers/setServers work
- [ ] Resolver class usable
- [ ] Tests pass
- [ ] No memory leaks

### Overall Project Success Criteria

#### Minimum Viable Product (MVP)
**Definition**: Core DNS functionality sufficient for basic Node.js compatibility

**Requirements**:
- ✅ Phases 0-4 complete
- ✅ dns.lookup() works with callbacks and promises
- ✅ dns.lookupService() works with callbacks and promises
- ✅ All tests pass (make test)
- ✅ No memory leaks (ASAN clean)
- ✅ Code formatted (make format)
- ✅ API coverage: 4 core methods (lookup, lookupService + promises)
- ✅ Test coverage: 15+ tests passing
- ✅ Documentation complete

#### Full Implementation
**Definition**: Complete Node.js dns module compatibility

**Requirements**:
- ✅ All MVP requirements
- ✅ Phases 5-6 complete
- ✅ All resolve*() methods defined (stub or implemented)
- ✅ getServers/setServers work
- ✅ Resolver class available
- ✅ API coverage: 30+ methods
- ✅ Test coverage: 30+ tests passing
- ✅ Cross-platform validated
- ✅ Performance benchmarked

### Quality Gates

**Before Each Commit**:
```bash
# MANDATORY (per jsrt guidelines)
make format                  # Code formatting
make test                    # Unit tests (100% pass)
make jsrt_m                  # Build with ASAN
./target/debug/jsrt_m test/node/test_node_dns*.js  # ASAN validation
```

**Before Phase Completion**:
```bash
# Full validation suite
make clean && make           # Clean build
make test                    # All tests
make wpt                     # WPT compliance (if applicable)
make jsrt_g                  # Debug build
make jsrt_m                  # ASAN build
```

**Performance Benchmarks** (Optional but Recommended):
- Measure DNS lookup time (should be < 100ms for cached)
- Measure memory usage (should not grow unbounded)
- Compare with Node.js performance (within 2x is acceptable)

---

## 📚 Implementation Notes & Patterns

### Key Implementation Patterns

#### 1. Request Lifecycle Pattern
```c
// 1. Create request
DNSLookupRequest* req = js_malloc(ctx, sizeof(DNSLookupRequest));
req->ctx = ctx;
req->callback = JS_DupValue(ctx, callback_arg);
req->req.data = req;  // Link libuv req to our request

// 2. Start async operation
int r = uv_getaddrinfo(loop, &req->req, on_callback, ...);
if (r < 0) {
  // Error path: clean up immediately
  JS_FreeValue(ctx, req->callback);
  js_free(ctx, req);
  return create_dns_error(ctx, r, "getaddrinfo", hostname);
}

// 3. Success path: callback will handle cleanup
return JS_UNDEFINED;

// 4. In callback: process result and clean up
void on_callback(uv_getaddrinfo_t* uv_req, int status, struct addrinfo* res) {
  DNSLookupRequest* req = (DNSLookupRequest*)uv_req;

  // Process result...

  // CRITICAL: Always clean up
  if (res) uv_freeaddrinfo(res);
  JS_FreeValue(req->ctx, req->callback);
  js_free(req->ctx, req);
}
```

#### 2. Error Handling Pattern
```c
JSValue create_dns_error(JSContext* ctx, int status,
                        const char* syscall, const char* hostname) {
  JSValue error = JS_NewError(ctx);

  // Map libuv error to Node.js code
  const char* code = get_dns_error_code(status);
  const char* message = uv_strerror(status);

  JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, code));
  JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, syscall));
  if (hostname) {
    JS_SetPropertyStr(ctx, error, "hostname", JS_NewString(ctx, hostname));
  }
  JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, message));
  JS_SetPropertyStr(ctx, error, "errno", JS_NewInt32(ctx, status));

  return error;
}
```

#### 3. Promise Wrapper Pattern
```c
JSValue js_dns_lookup_promise(JSContext* ctx, JSValueConst this_val,
                              int argc, JSValueConst* argv) {
  // Create promise
  JSValue promise_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, promise_funcs);
  if (JS_IsException(promise)) return promise;

  // Prepare request
  DNSLookupRequest* req = ...;
  req->use_promise = true;
  req->promise_funcs[0] = JS_DupValue(ctx, promise_funcs[0]);
  req->promise_funcs[1] = JS_DupValue(ctx, promise_funcs[1]);

  // Start async operation (same as callback version)
  int r = uv_getaddrinfo(...);
  if (r < 0) {
    // Reject promise
    JSValue error = create_dns_error(...);
    JSValue args[] = {error};
    JS_Call(ctx, req->promise_funcs[1], JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, error);

    // Clean up
    JS_FreeValue(ctx, req->promise_funcs[0]);
    JS_FreeValue(ctx, req->promise_funcs[1]);
    js_free(ctx, req);
  }

  // Free temporary promise capability references
  JS_FreeValue(ctx, promise_funcs[0]);
  JS_FreeValue(ctx, promise_funcs[1]);

  return promise;
}

// In callback:
void on_callback(...) {
  if (req->use_promise) {
    if (status == 0) {
      // Resolve
      JSValue result = convert_result(...);
      JSValue args[] = {result};
      JS_Call(ctx, req->promise_funcs[0], JS_UNDEFINED, 1, args);
      JS_FreeValue(ctx, result);
    } else {
      // Reject
      JSValue error = create_dns_error(...);
      JSValue args[] = {error};
      JS_Call(ctx, req->promise_funcs[1], JS_UNDEFINED, 1, args);
      JS_FreeValue(ctx, error);
    }
    JS_FreeValue(ctx, req->promise_funcs[0]);
    JS_FreeValue(ctx, req->promise_funcs[1]);
  } else {
    // Call callback...
  }
}
```

#### 4. addrinfo Conversion Pattern
```c
JSValue convert_addrinfo_to_js(JSContext* ctx, struct addrinfo* res,
                              bool all, int* family_out) {
  if (all) {
    // Return array of address objects
    JSValue array = JS_NewArray(ctx);
    uint32_t index = 0;

    for (struct addrinfo* ai = res; ai != NULL; ai = ai->ai_next) {
      JSValue addr_obj = JS_NewObject(ctx);
      char addr_str[INET6_ADDRSTRLEN];
      int family = 0;

      if (ai->ai_family == AF_INET) {
        struct sockaddr_in* sa = (struct sockaddr_in*)ai->ai_addr;
        inet_ntop(AF_INET, &sa->sin_addr, addr_str, sizeof(addr_str));
        family = 4;
      } else if (ai->ai_family == AF_INET6) {
        struct sockaddr_in6* sa = (struct sockaddr_in6*)ai->ai_addr;
        inet_ntop(AF_INET6, &sa->sin6_addr, addr_str, sizeof(addr_str));
        family = 6;
      }

      JS_SetPropertyStr(ctx, addr_obj, "address", JS_NewString(ctx, addr_str));
      JS_SetPropertyStr(ctx, addr_obj, "family", JS_NewInt32(ctx, family));
      JS_SetPropertyUint32(ctx, array, index++, addr_obj);
    }

    return array;
  } else {
    // Return first address as string
    char addr_str[INET6_ADDRSTRLEN];

    if (res->ai_family == AF_INET) {
      struct sockaddr_in* sa = (struct sockaddr_in*)res->ai_addr;
      inet_ntop(AF_INET, &sa->sin_addr, addr_str, sizeof(addr_str));
      *family_out = 4;
    } else if (res->ai_family == AF_INET6) {
      struct sockaddr_in6* sa = (struct sockaddr_in6*)res->ai_addr;
      inet_ntop(AF_INET6, &sa->sin6_addr, addr_str, sizeof(addr_str));
      *family_out = 6;
    }

    return JS_NewString(ctx, addr_str);
  }
}
```

### Testing Strategy

#### Unit Test Structure
```javascript
// test/node/test_node_dns_basic.js
const dns = require('node:dns');

// Test 1: Basic lookup with callback
dns.lookup('localhost', (err, address, family) => {
  if (err) throw err;
  console.log('Lookup localhost:', address, family);
  assert(address === '127.0.0.1' || address === '::1');
});

// Test 2: Lookup with family option
dns.lookup('localhost', { family: 4 }, (err, address, family) => {
  if (err) throw err;
  assert(address === '127.0.0.1');
  assert(family === 4);
});

// Test 3: Lookup with all option
dns.lookup('localhost', { all: true }, (err, addresses) => {
  if (err) throw err;
  assert(Array.isArray(addresses));
  assert(addresses.length > 0);
  assert(addresses[0].address);
  assert(addresses[0].family);
});

// Test 4: Error handling
dns.lookup('invalid.hostname.that.does.not.exist.xyz', (err, address) => {
  assert(err);
  assert(err.code === 'ENOTFOUND');
  assert(err.syscall === 'getaddrinfo');
});
```

#### Memory Leak Test
```javascript
// test/node/test_node_dns_memory.js
const dns = require('node:dns');

// Perform many lookups to detect leaks
let completed = 0;
const total = 100;

for (let i = 0; i < total; i++) {
  dns.lookup('localhost', (err, address) => {
    if (err) throw err;
    completed++;
    if (completed === total) {
      console.log('All lookups completed without leaks');
    }
  });
}
```

#### ASAN Validation
```bash
# Build with AddressSanitizer
make jsrt_m

# Run tests with ASAN
ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m test/node/test_node_dns_basic.js
ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m test/node/test_node_dns_memory.js

# Should report: "No leaks detected"
```

---

## 🔄 Migration from Stub Implementation

### Current Stub Code Analysis
**File**: `/Users/bytedance/github/leizongmin/jsrt/src/node/node_dns.c`

**Keep**:
- ✅ Module structure (JSRT_InitNodeDns, js_node_dns_init)
- ✅ RRTYPE constants
- ✅ Function signatures (matching Node.js API)

**Replace**:
- ❌ All function implementations (currently return mock data)
- ❌ Promise-only approach (Node.js uses callbacks)
- ❌ Hardcoded return values

**Add**:
- ➕ libuv integration
- ➕ Callback support
- ➕ Request state management
- ➕ Error handling
- ➕ Memory management
- ➕ Promises API (as separate namespace)

### Migration Steps

**Step 1: Backup Current Implementation**
```bash
cp src/node/node_dns.c src/node/node_dns.c.backup
```

**Step 2: Create New Directory Structure**
```bash
mkdir -p src/node/dns
```

**Step 3: Move Module Initialization**
```bash
# Extract JSRT_InitNodeDns and js_node_dns_init to dns_module.c
# Keep RRTYPE constants
```

**Step 4: Implement New Functions**
```bash
# Create new implementations in dns/ subdirectory
# Link from dns_module.c
```

**Step 5: Update Build System**
```makefile
# Update Makefile to include dns/*.c files
```

**Step 6: Deprecate Old File**
```bash
# Remove or rename node_dns.c after migration complete
```

---

## 📖 References & Resources

### Node.js Documentation
- Official API: https://nodejs.org/docs/latest/api/dns.html
- Error codes: https://nodejs.org/api/errors.html#dns-errors
- Promises API: https://nodejs.org/api/dns.html#dns-promises-api

### libuv Documentation
- DNS utilities: https://docs.libuv.org/en/v1.x/dns.html
- getaddrinfo: http://man7.org/linux/man-pages/man3/getaddrinfo.3.html
- getnameinfo: http://man7.org/linux/man-pages/man3/getnameinfo.3.html

### c-ares (Optional, for network DNS)
- Project: https://c-ares.org/
- Documentation: https://c-ares.org/docs.html
- Integration: (defer to Phase 5C if needed)

### jsrt Resources
- Architecture: `.claude/docs/architecture.md`
- Testing guide: `.claude/docs/testing.md`
- Net module reference: `src/node/net/`
- EventEmitter: `src/node/events/`

### Platform-Specific
- POSIX DNS: https://pubs.opengroup.org/onlinepubs/9699919799/
- Windows DNS: https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/
- macOS DNS: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/getaddrinfo.3.html

---

## 📅 Timeline & Milestones

### Recommended Approach: Incremental MVP

**Week 1**:
- Days 1-2: Phase 0 (Research & Architecture)
- Days 3-5: Phase 1 (Foundation & Request Management)

**Week 2**:
- Days 1-4: Phase 2 (dns.lookup implementation)
- Day 5: Phase 3 start (dns.lookupService)

**Week 3**:
- Days 1-2: Phase 3 completion
- Days 3-5: Phase 4 (Promises API)

**MVP Complete**: End of Week 3
- Core DNS functionality working
- Tests passing
- Ready for use in basic applications

**Week 4** (Optional):
- Days 1-2: Phase 5 (Stub implementations)
- Days 3-5: Phase 6 (Advanced features)

**Full Implementation**: End of Week 4
- Complete Node.js API surface
- All tests passing
- Production-ready

### Milestones

**M1: Architecture Complete** (End of Phase 0)
- [ ] Design approved
- [ ] Test plans created
- [ ] Baseline tests pass

**M2: Infrastructure Ready** (End of Phase 1)
- [ ] Request management working
- [ ] Error handling in place
- [ ] Build system integrated

**M3: MVP Feature Complete** (End of Phase 4)
- [ ] lookup() and lookupService() working
- [ ] Promises API available
- [ ] 20+ tests passing

**M4: Full API Surface** (End of Phase 6)
- [ ] All methods defined
- [ ] Configuration working
- [ ] 30+ tests passing
- [ ] Production ready

---

## ✨ Completion Checklist

### Before Marking Project Complete

**Code Quality**:
- [ ] All code formatted (make format)
- [ ] No compiler warnings
- [ ] ASAN clean (no leaks, no errors)
- [ ] Code follows jsrt patterns
- [ ] Comments and documentation complete

**Functionality**:
- [ ] dns.lookup() works with all options
- [ ] dns.lookupService() works
- [ ] Promises API (dns.promises) works
- [ ] Error handling comprehensive
- [ ] Cross-platform tested (if possible)

**Testing**:
- [ ] Unit tests pass (make test)
- [ ] WPT tests pass (if applicable)
- [ ] Memory leak tests pass
- [ ] Error path tests pass
- [ ] Integration tests pass

**Documentation**:
- [ ] API documented in code comments
- [ ] Usage examples provided
- [ ] Limitations documented
- [ ] Migration notes from stub

**Integration**:
- [ ] Module exports correct (CommonJS)
- [ ] Module exports correct (ESM)
- [ ] require('node:dns') works
- [ ] require('node:dns/promises') works
- [ ] No conflicts with existing modules

**Performance**:
- [ ] DNS lookups complete in reasonable time
- [ ] No event loop blocking
- [ ] Memory usage reasonable
- [ ] Concurrent requests handled correctly

---

## 🎯 Next Actions

To begin implementation:

1. **Read this plan thoroughly**
2. **Run baseline tests**: `make test && make wpt`
3. **Start Phase 0**: Begin with tasks 0.1, 0.2, 0.3 in parallel
4. **Update this document**: Track progress in the execution dashboard
5. **Follow jsrt workflow**: format → test → commit

**First Command to Run**:
```bash
# Verify current state
make clean && make test
```

**First Task to Start**:
```
Task 0.1: Analyze existing stub implementation at src/node/node_dns.c
```

---

**Document Status**: 📋 READY FOR IMPLEMENTATION
**Last Updated**: 2025-10-09T00:00:00Z
**Plan Author**: AI Assistant
**Target Completion**: 3-4 weeks (MVP: 2-3 weeks)
