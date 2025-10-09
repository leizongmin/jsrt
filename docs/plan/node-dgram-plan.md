---
Created: 2025-10-09T00:00:00Z
Last Updated: 2025-10-09T00:00:00Z
Status: 📋 READY FOR IMPLEMENTATION
Overall Progress: 0/185 tasks (0%)
API Coverage: 0/30+ methods (0%)
---

# Node.js dgram Module Implementation Plan

## 📋 Executive Summary

### Objective
Implement a complete Node.js-compatible `node:dgram` module in jsrt that provides UDP/datagram socket functionality with full EventEmitter integration and support for unicast, broadcast, and multicast communications.

### Current Status
- ❌ **No dgram module** exists in jsrt
- ✅ **EventEmitter infrastructure** available in `src/node/events/` (8 files, full implementation)
- ✅ **libuv UDP API** available in `deps/libuv/` (comprehensive UDP support)
- ✅ **node:net module** as reference (97% complete, excellent architecture patterns)
- 🎯 **Target**: 30+ API methods with full Node.js compatibility

### Key Success Factors
1. **EventEmitter Integration**: Socket class MUST extend EventEmitter (Node.js requirement)
2. **Code Reuse**: Leverage node:net patterns (90%), EventEmitter (100%), libuv UDP (100%)
3. **Incremental Implementation**: Build from foundation → Basic UDP → Multicast → Advanced features
4. **Comprehensive Testing**: 50+ unit tests with ASAN validation

### Implementation Strategy
- **Phase 0**: Research & Architecture Setup (1-2 days, 15 tasks)
- **Phase 1**: Foundation & Socket Creation (2-3 days, 25 tasks)
- **Phase 2**: Basic Send/Receive Operations (3-4 days, 30 tasks)
- **Phase 3**: Events & EventEmitter Integration (2-3 days, 20 tasks)
- **Phase 4**: Address & Socket Options (2-3 days, 25 tasks)
- **Phase 5**: Multicast Support (3-4 days, 30 tasks)
- **Phase 6**: Broadcast & TTL (2-3 days, 20 tasks)
- **Phase 7**: Connected UDP (Optional) (1-2 days, 10 tasks)
- **Phase 8**: Buffer Size & Advanced Features (1-2 days, 10 tasks)
- **Total Estimated Time**: 17-26 days

---

## 🔍 Current State Analysis

### Existing Resources for Reuse

#### From `src/node/net/` (TCP Networking) - **90% Pattern Reuse**
**Excellent Architecture Patterns**:
```c
// Modular structure (6 files):
net_callbacks.c      // libuv callback handlers
net_finalizers.c     // Memory cleanup & deferred cleanup
net_socket.c         // Socket methods
net_server.c         // Server methods (NOT needed for dgram)
net_properties.c     // Property getters
net_module.c         // Module exports
net_internal.h       // Shared declarations

// Type safety system:
#define NET_TYPE_SOCKET 0x534F434B  // 'SOCK' in hex
#define NET_TYPE_SERVER 0x53525652  // 'SRVR' in hex

typedef struct {
  uint32_t type_tag;           // Must be first field
  JSContext* ctx;
  JSValue socket_obj;
  uv_tcp_t handle;             // Replace with uv_udp_t
  // ... state fields
} JSNetConnection;

// Deferred cleanup pattern (CRITICAL for memory safety):
- Type tags for safe struct identification
- close_count for handle lifecycle tracking
- Separate close and cleanup callbacks
- Zero use-after-free vulnerabilities
```

**Adaptation Strategy**:
```c
// Adapt net module patterns to dgram:
uv_tcp_t     → uv_udp_t              // Different handle type
uv_connect_t → uv_udp_send_t         // Different request type
on_connection → on_recv               // Different callback
TCP concepts  → UDP concepts          // Connectionless
```

#### From `src/node/events/` (EventEmitter) - **100% Reuse**
**Files Available** (8 files, ~2000 lines):
- `event_emitter_core.c` - Core emit/on/off
- `event_emitter_enhanced.c` - once, prependListener
- `event_helpers.c` - Helper functions
- `event_error_handling.c` - Error event special handling
- All stream classes MUST extend EventEmitter

**Integration Pattern** (from net module):
```c
typedef struct {
  uint32_t type_tag;
  JSContext* ctx;
  JSValue socket_obj;          // JavaScript Socket object
  JSValue event_emitter;       // NOT USED - socket_obj IS the EventEmitter
  uv_udp_t handle;
  // ... dgram-specific fields
} JSDgramSocket;

// EventEmitter methods added in module initialization:
void add_event_emitter_methods(JSContext* ctx, JSValue obj);
```

#### From `deps/libuv/` (UDP API) - **100% Direct Use**
**Available libuv UDP Functions** (from uv.h lines 737-783):
```c
// Handle initialization
UV_EXTERN int uv_udp_init(uv_loop_t*, uv_udp_t* handle);
UV_EXTERN int uv_udp_init_ex(uv_loop_t*, uv_udp_t* handle, unsigned int flags);

// Binding
UV_EXTERN int uv_udp_bind(uv_udp_t* handle, const struct sockaddr* addr, unsigned int flags);

// Send/Receive
UV_EXTERN int uv_udp_send(uv_udp_send_t* req, uv_udp_t* handle,
                          const uv_buf_t bufs[], unsigned int nbufs,
                          const struct sockaddr* addr, uv_udp_send_cb send_cb);
UV_EXTERN int uv_udp_recv_start(uv_udp_t* handle, uv_alloc_cb alloc_cb, uv_udp_recv_cb recv_cb);
UV_EXTERN int uv_udp_recv_stop(uv_udp_t* handle);

// Connected UDP
UV_EXTERN int uv_udp_connect(uv_udp_t* handle, const struct sockaddr* addr);
UV_EXTERN int uv_udp_getpeername(const uv_udp_t* handle, struct sockaddr* name, int* namelen);

// Address info
UV_EXTERN int uv_udp_getsockname(const uv_udp_t* handle, struct sockaddr* name, int* namelen);

// Multicast
UV_EXTERN int uv_udp_set_membership(uv_udp_t* handle, const char* multicast_addr,
                                    const char* interface_addr, uv_membership membership);
UV_EXTERN int uv_udp_set_source_membership(uv_udp_t* handle, const char* multicast_addr,
                                           const char* interface_addr, const char* source_addr,
                                           uv_membership membership);
UV_EXTERN int uv_udp_set_multicast_loop(uv_udp_t* handle, int on);
UV_EXTERN int uv_udp_set_multicast_ttl(uv_udp_t* handle, int ttl);
UV_EXTERN int uv_udp_set_multicast_interface(uv_udp_t* handle, const char* interface_addr);

// Broadcast & TTL
UV_EXTERN int uv_udp_set_broadcast(uv_udp_t* handle, int on);
UV_EXTERN int uv_udp_set_ttl(uv_udp_t* handle, int ttl);

// Buffer sizes (from uv.h lines 504-505)
UV_EXTERN int uv_send_buffer_size(uv_handle_t* handle, int* value);
UV_EXTERN int uv_recv_buffer_size(uv_handle_t* handle, int* value);

// Callbacks (from uv.h lines 706-711):
typedef void (*uv_udp_send_cb)(uv_udp_send_t* req, int status);
typedef void (*uv_udp_recv_cb)(uv_udp_t* handle, ssize_t nread,
                               const uv_buf_t* buf,
                               const struct sockaddr* addr,
                               unsigned flags);

// Structures (from uv.h lines 714-735):
struct uv_udp_s {
  UV_HANDLE_FIELDS
  size_t send_queue_size;        // Bytes queued for sending
  size_t send_queue_count;       // Number of send requests
  UV_UDP_PRIVATE_FIELDS
};

struct uv_udp_send_s {
  UV_REQ_FIELDS
  uv_udp_t* handle;
  uv_udp_send_cb cb;
  UV_UDP_SEND_PRIVATE_FIELDS
};

// Flags (from uv.h lines 649-704):
enum uv_udp_flags {
  UV_UDP_IPV6ONLY = 1,           // Disable dual stack
  UV_UDP_PARTIAL = 2,            // Message truncated
  UV_UDP_REUSEADDR = 4,          // SO_REUSEADDR
  UV_UDP_MMSG_CHUNK = 8,         // recvmmsg buffer
  UV_UDP_MMSG_FREE = 16,         // Free buffer
  UV_UDP_LINUX_RECVERR = 32,     // IP_RECVERR
  UV_UDP_REUSEPORT = 64,         // SO_REUSEPORT
  UV_UDP_RECVMMSG = 256          // Use recvmmsg
};

// Error codes (from uv.h lines 75-159):
UV_EADDRINUSE     // Address already in use
UV_EADDRNOTAVAIL  // Address not available
UV_EACCES         // Permission denied
UV_EMSGSIZE       // Message too long
UV_ENETDOWN       // Network is down
UV_ENETUNREACH    // Network unreachable
// ... (full error mapping available)
```

**Key Insights**:
- All UDP operations are available in libuv
- Callback-based async model (same as TCP)
- Error codes map to Node.js errors
- IPv4/IPv6 support built-in

### Missing Functionality (To Be Implemented)
```
❌ Module registration in node_modules.c
❌ dgram.createSocket() factory function
❌ dgram.Socket class with EventEmitter
❌ All socket methods (send, bind, close, etc.)
❌ All socket events (message, listening, close, error, connect)
❌ Address/port/family properties
❌ Multicast operations
❌ Broadcast operations
❌ Buffer size operations
❌ Test suite (50+ tests)
```

---

## 🏗️ Implementation Architecture

### File Structure
```
src/node/dgram/
├── dgram_socket.c           # Socket class & methods (~500 lines)
│   ├── createSocket factory
│   ├── bind(), close()
│   ├── address()
│   ├── ref(), unref()
├── dgram_callbacks.c        # libuv callbacks (~300 lines)
│   ├── on_send (uv_udp_send_cb)
│   ├── on_recv (uv_udp_recv_cb)
│   ├── on_alloc (uv_alloc_cb)
├── dgram_finalizers.c       # Memory cleanup (~200 lines)
│   ├── socket_close_callback
│   ├── js_dgram_socket_finalizer
│   ├── Deferred cleanup pattern
├── dgram_send.c             # Send operations (~300 lines)
│   ├── socket.send() method
│   ├── Send request management
│   ├── Buffer handling
├── dgram_multicast.c        # Multicast operations (~400 lines)
│   ├── addMembership()
│   ├── dropMembership()
│   ├── setMulticastTTL()
│   ├── setMulticastInterface()
│   ├── setMulticastLoopback()
│   ├── addSourceSpecificMembership()
│   ├── dropSourceSpecificMembership()
├── dgram_options.c          # Socket options (~300 lines)
│   ├── setBroadcast()
│   ├── setTTL()
│   ├── setSendBufferSize()
│   ├── setRecvBufferSize()
│   ├── getSendBufferSize()
│   ├── getRecvBufferSize()
├── dgram_module.c           # Module exports (~200 lines)
│   ├── Module initialization
│   ├── createSocket() export
│   ├── Socket class registration
├── dgram_internal.h         # Shared declarations (~150 lines)
│   ├── Data structures
│   ├── Function prototypes
│   ├── Constants & macros
└── dgram_connected.c        # Connected UDP (~200 lines) [OPTIONAL]
    ├── connect()
    ├── disconnect()
    ├── remoteAddress()

Total: ~2550 lines estimated

test/node/dgram/
├── test_dgram_basic.js            # Basic send/receive (10 tests)
├── test_dgram_bind.js             # Binding tests (8 tests)
├── test_dgram_events.js           # Event emission (10 tests)
├── test_dgram_errors.js           # Error handling (8 tests)
├── test_dgram_multicast.js        # Multicast functionality (10 tests)
├── test_dgram_broadcast.js        # Broadcast (6 tests)
├── test_dgram_options.js          # Socket options (8 tests)
├── test_dgram_connected.js        # Connected UDP (6 tests) [OPTIONAL]
└── test_dgram_esm.mjs             # ESM import (4 tests)

Total: 70+ test cases
```

### Module Registration
**File**: `src/node/node_modules.c`
```c
// Add to node modules array
extern JSModuleDef* js_init_module_node_dgram(JSContext* ctx, const char* module_name);

static const struct {
  const char* name;
  JSModuleDef* (*init)(JSContext* ctx, const char* module_name);
} node_modules[] = {
  // ... existing modules ...
  {"dgram", js_init_module_node_dgram},
  {NULL, NULL}
};
```

### Data Structures
**File**: `src/node/dgram/dgram_internal.h`
```c
#ifndef JSRT_NODE_DGRAM_INTERNAL_H
#define JSRT_NODE_DGRAM_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../runtime.h"
#include "../node_modules.h"

// Class ID for Socket
extern JSClassID js_dgram_socket_class_id;

// Type tag for cleanup callback identification
#define DGRAM_TYPE_SOCKET 0x44475241  // 'DGRA' in hex

// Socket state
typedef struct {
  uint32_t type_tag;           // Must be first field for cleanup
  JSContext* ctx;
  JSValue socket_obj;          // JavaScript Socket object (is EventEmitter)
  uv_udp_t handle;             // libuv UDP handle
  bool bound;                  // Socket is bound
  bool connected;              // Socket is connected (connected UDP)
  bool destroyed;              // Socket destroyed
  bool receiving;              // Currently receiving
  bool in_callback;            // Prevent finalization during callback
  int close_count;             // Handles that need to close before freeing
  char* multicast_interface;   // Current multicast interface
  // Statistics
  size_t bytes_sent;
  size_t bytes_received;
  size_t messages_sent;
  size_t messages_received;
} JSDgramSocket;

// Send request state
typedef struct {
  uv_udp_send_t req;           // libuv send request
  JSContext* ctx;
  JSValue socket_obj;          // Socket reference
  JSValue callback;            // Optional callback
  char* data;                  // Buffer copy
  size_t len;
} JSDgramSendReq;

// Socket methods (from dgram_socket.c)
JSValue js_dgram_create_socket(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_bind(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Send operations (from dgram_send.c)
JSValue js_dgram_socket_send(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Multicast operations (from dgram_multicast.c)
JSValue js_dgram_socket_add_membership(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_drop_membership(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_multicast_ttl(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_multicast_interface(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_multicast_loopback(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_add_source_specific_membership(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_drop_source_specific_membership(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Socket options (from dgram_options.c)
JSValue js_dgram_socket_set_broadcast(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_ttl(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_get_send_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_get_recv_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_send_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_recv_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Connected UDP (from dgram_connected.c) [OPTIONAL]
JSValue js_dgram_socket_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_disconnect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_remote_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Callbacks (from dgram_callbacks.c)
void on_dgram_send(uv_udp_send_t* req, int status);
void on_dgram_recv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
                   const struct sockaddr* addr, unsigned flags);
void on_dgram_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

// Finalizers (from dgram_finalizers.c)
void dgram_socket_close_callback(uv_handle_t* handle);
void js_dgram_socket_finalizer(JSRuntime* rt, JSValue val);

// Helper function (from node:events)
void add_event_emitter_methods(JSContext* ctx, JSValue obj);

#endif  // JSRT_NODE_DGRAM_INTERNAL_H
```

### Architecture Diagram
```
node:dgram (CommonJS/ESM)
├── Module Functions
│   ├── createSocket(type [, callback])
│   └── createSocket(options [, callback])
│
├── dgram.Socket Class (extends EventEmitter)
│   ├── Methods (20 methods)
│   │   ├── bind([port] [, address] [, callback])
│   │   ├── bind(options [, callback])
│   │   ├── send(msg, [offset, length,] port [, address] [, callback])
│   │   ├── close([callback])
│   │   ├── address()
│   │   ├── setBroadcast(flag)
│   │   ├── setTTL(ttl)
│   │   ├── setMulticastTTL(ttl)
│   │   ├── setMulticastInterface(multicastInterface)
│   │   ├── setMulticastLoopback(flag)
│   │   ├── addMembership(multicastAddress [, multicastInterface])
│   │   ├── dropMembership(multicastAddress [, multicastInterface])
│   │   ├── addSourceSpecificMembership(...)
│   │   ├── dropSourceSpecificMembership(...)
│   │   ├── ref()
│   │   ├── unref()
│   │   ├── getSendBufferSize()
│   │   ├── getRecvBufferSize()
│   │   ├── setSendBufferSize(size)
│   │   └── setRecvBufferSize(size)
│   │
│   ├── Connected UDP Methods (3 methods) [OPTIONAL]
│   │   ├── connect(port [, address] [, callback])
│   │   ├── disconnect()
│   │   └── remoteAddress()
│   │
│   └── Events (5 events)
│       ├── 'message' (msg, rinfo)
│       ├── 'listening'
│       ├── 'close'
│       ├── 'error' (err)
│       └── 'connect' (Node v12+) [OPTIONAL]
│
├── libuv Integration
│   ├── uv_udp_t handle (core UDP handle)
│   ├── uv_udp_send_t requests (async sends)
│   ├── uv_udp_send_cb callbacks
│   ├── uv_udp_recv_cb callbacks
│   └── EventEmitter event emission
│
└── Memory Management
    ├── Type tags (0x44475241 'DGRA')
    ├── Deferred cleanup pattern
    ├── Reference counting (close_count)
    └── QuickJS allocators (js_malloc/js_free)
```

---

## 📊 Overall Progress Tracking

**Total Tasks**: 185
**Completed**: 0
**In Progress**: 0
**Remaining**: 185

**Completion**: 0%

**Estimated Timeline**: 17-26 days

---

## 📝 Detailed Task Breakdown

### Phase 0: Research & Architecture Setup (15 tasks)

**Goal**: Understand dgram API, setup file structure, establish patterns
**Duration**: 1-2 days
**Dependencies**: None (initial phase)

#### Task 0.1: [S][R:LOW][C:SIMPLE] Research & Documentation (5 tasks)
**Duration**: 0.5 days
**Dependencies**: None

**Subtasks**:
- [x] **0.1.1** [2h] Read Node.js dgram documentation thoroughly
  - Study all methods, events, and options
  - Understand UDP vs TCP differences
  - Document API surface area
  - **Reference**: https://nodejs.org/api/dgram.html

- [x] **0.1.2** [2h] Study libuv UDP API in detail
  - Read deps/libuv/docs/src/udp.rst
  - Understand uv_udp_* functions
  - Map libuv to Node.js APIs
  - **Reference**: deps/libuv/include/uv.h lines 737-783

- [x] **0.1.3** [1h] Analyze node:net module patterns
  - Study file structure (6 files)
  - Review deferred cleanup pattern
  - Understand type tag system
  - **Reference**: src/node/net/net_internal.h

- [x] **0.1.4** [1h] Review EventEmitter integration
  - Study add_event_emitter_methods()
  - Understand event emission patterns
  - **Reference**: src/node/events/

- [x] **0.1.5** [1h] Document UDP-specific challenges
  - Connectionless protocol implications
  - Multicast/broadcast complexities
  - Error handling differences from TCP

**Acceptance Criteria**:
- Complete understanding of dgram API
- Clear mapping of libuv to Node.js
- Architecture decisions documented

**Parallel Execution**: All tasks can run in parallel

#### Task 0.2: [S][R:LOW][C:SIMPLE] File Structure Setup (6 tasks)
**Duration**: 0.5 days
**Dependencies**: [D:0.1] Research complete

**Subtasks**:
- [ ] **0.2.1** [1h] Create directory structure
  ```bash
  mkdir -p src/node/dgram
  mkdir -p test/node/dgram
  ```

- [ ] **0.2.2** [1h] Create header file `dgram_internal.h`
  - Add type definitions
  - Add function prototypes
  - Add constants & macros
  - **Test**: File compiles without errors

- [ ] **0.2.3** [1h] Create stub implementation files
  - dgram_socket.c (empty functions)
  - dgram_callbacks.c (empty callbacks)
  - dgram_finalizers.c (cleanup stubs)
  - **Test**: All files compile

- [ ] **0.2.4** [1h] Register module in node_modules.c
  - Add js_init_module_node_dgram declaration
  - Add to node_modules array
  - **Test**: Module loads without error

- [ ] **0.2.5** [1h] Create basic test infrastructure
  - test_dgram_basic.js with first test
  - **Test**: Test runs (even if fails)

- [ ] **0.2.6** [0.5h] Run baseline tests
  ```bash
  make clean && make jsrt_g
  make test
  make wpt
  ```
  - Document current pass/fail counts
  - Establish baseline

**Acceptance Criteria**:
```javascript
// test_dgram_basic.js should run
const dgram = require('dgram');
console.log('dgram module loaded:', typeof dgram.createSocket);
// Expected: "dgram module loaded: function"
```

**Parallel Execution**: 0.2.1 → 0.2.2-0.2.5 (parallel) → 0.2.6

#### Task 0.3: [S][R:MEDIUM][C:MEDIUM] Memory Management Setup (4 tasks)
**Duration**: 0.5 days
**Dependencies**: [D:0.2] File structure ready

**Subtasks**:
- [ ] **0.3.1** [1h] Implement type tag system
  - Define DGRAM_TYPE_SOCKET (0x44475241)
  - Add to JSDgramSocket struct
  - **Test**: Type tag is first field

- [ ] **0.3.2** [2h] Implement deferred cleanup pattern
  - Study net module's cleanup
  - Adapt for dgram (no server needed)
  - Add close_count tracking
  - **Test**: Cleanup works correctly

- [ ] **0.3.3** [1h] Create finalizer skeleton
  - js_dgram_socket_finalizer()
  - dgram_socket_close_callback()
  - **Test**: No crashes on cleanup

- [ ] **0.3.4** [1h] Test with ASAN
  ```bash
  make jsrt_m
  ./target/debug/jsrt_m test/node/dgram/test_dgram_basic.js
  ```
  - Verify zero leaks
  - Fix any issues found

**Acceptance Criteria**:
- Type tags implemented correctly
- Deferred cleanup pattern working
- Zero memory leaks in ASAN

**Parallel Execution**: 0.3.1 → 0.3.2-0.3.3 (parallel) → 0.3.4

---

### Phase 1: Foundation & Socket Creation (25 tasks)

**Goal**: Implement createSocket() and basic socket infrastructure
**Duration**: 2-3 days
**Dependencies**: [D:0] Phase 0 complete

#### Task 1.1: [S][R:MEDIUM][C:MEDIUM] Socket Class Infrastructure (8 tasks)
**Duration**: 1 day
**Dependencies**: [D:0.3] Memory management ready

**Subtasks**:
- [ ] **1.1.1** [2h] Register Socket class with QuickJS
  - Create js_dgram_socket_class_id
  - Define class definition
  - Register class in module init
  - **Test**: Class registered successfully

- [ ] **1.1.2** [2h] Implement JSDgramSocket structure
  - Add all required fields
  - Initialize with defaults
  - **Test**: Structure allocates correctly

- [ ] **1.1.3** [2h] Add EventEmitter integration
  - Call add_event_emitter_methods()
  - Verify emit/on/once work
  - **Test**: socket.on('message', ...) works

- [ ] **1.1.4** [1h] Implement socket property getters
  - Use JS_DefinePropertyGetSet
  - Add 'destroyed' property
  - **Test**: Properties accessible

- [ ] **1.1.5** [2h] Initialize libuv UDP handle
  - Call uv_udp_init()
  - Set handle->data to socket
  - **Test**: Handle initialized

- [ ] **1.1.6** [1h] Add error handling helpers
  - Create create_dgram_error() function
  - Map libuv errors to Node.js codes
  - **Test**: Errors created correctly

- [ ] **1.1.7** [1h] Implement statistics tracking
  - bytes_sent, bytes_received
  - messages_sent, messages_received
  - **Test**: Counters increment

- [ ] **1.1.8** [1h] Add debug logging
  - Use JSRT_DEBUG macros
  - Log state transitions
  - **Test**: Logs appear in debug mode

**Acceptance Criteria**:
```javascript
const dgram = require('dgram');
const socket = new dgram.Socket('udp4');
console.log(socket.destroyed);  // false
socket.on('message', (msg) => console.log(msg));
```

**Parallel Execution**: 1.1.1 → 1.1.2 → 1.1.3-1.1.7 (parallel) → 1.1.8

#### Task 1.2: [S][R:MEDIUM][C:MEDIUM] createSocket() Factory (9 tasks)
**Duration**: 1 day
**Dependencies**: [D:1.1] Socket class ready

**Subtasks**:
- [ ] **1.2.1** [2h] Implement createSocket(type) overload
  - Parse 'udp4' or 'udp6'
  - Create socket instance
  - Initialize UV handle
  - **Test**: Both types work

- [ ] **1.2.2** [2h] Implement createSocket(options) overload
  - Parse options object
  - Support 'type', 'reuseAddr', 'ipv6Only', 'recvBufferSize', 'sendBufferSize'
  - **Test**: Options applied correctly

- [ ] **1.2.3** [1h] Add optional callback parameter
  - Register as 'message' listener if provided
  - **Test**: Callback fires on message

- [ ] **1.2.4** [2h] Implement IPv4 socket creation
  - Use uv_udp_init() without flags
  - **Test**: IPv4 socket works

- [ ] **1.2.5** [2h] Implement IPv6 socket creation
  - Use uv_udp_init_ex() with UV_UDP_IPV6ONLY if needed
  - **Test**: IPv6 socket works

- [ ] **1.2.6** [1h] Add reuseAddr option handling
  - Set UV_UDP_REUSEADDR flag on bind
  - **Test**: Option works

- [ ] **1.2.7** [1h] Add buffer size options
  - Set sendBufferSize/recvBufferSize
  - Call uv_send_buffer_size/uv_recv_buffer_size
  - **Test**: Sizes set correctly

- [ ] **1.2.8** [1h] Implement error handling
  - Handle invalid type
  - Handle invalid options
  - Throw appropriate errors
  - **Test**: Errors thrown correctly

- [ ] **1.2.9** [1h] Write comprehensive tests
  - Test all overloads
  - Test all options
  - **Test**: All pass

**Acceptance Criteria**:
```javascript
// Type string
const socket1 = dgram.createSocket('udp4');
const socket2 = dgram.createSocket('udp6');

// Options object
const socket3 = dgram.createSocket({
  type: 'udp4',
  reuseAddr: true,
  recvBufferSize: 16384,
  sendBufferSize: 16384
});

// With callback
const socket4 = dgram.createSocket('udp4', (msg, rinfo) => {
  console.log('Received:', msg, rinfo);
});
```

**Parallel Execution**: 1.2.1-1.2.2 (sequential) → 1.2.3-1.2.8 (parallel) → 1.2.9

#### Task 1.3: [S][R:MEDIUM][C:SIMPLE] Module Exports (8 tasks)
**Duration**: 0.5 days
**Dependencies**: [D:1.2] createSocket ready

**Subtasks**:
- [ ] **1.3.1** [1h] Export createSocket function
  - Add to module exports
  - **Test**: dgram.createSocket exists

- [ ] **1.3.2** [1h] Export Socket constructor
  - Make Socket accessible directly
  - **Test**: new dgram.Socket() works

- [ ] **1.3.3** [0.5h] Add CommonJS support
  - Test require('dgram')
  - **Test**: CommonJS works

- [ ] **1.3.4** [0.5h] Add ESM support
  - Test import dgram from 'dgram'
  - **Test**: ESM works

- [ ] **1.3.5** [1h] Implement module metadata
  - Add version, description
  - **Test**: Metadata accessible

- [ ] **1.3.6** [0.5h] Create test_dgram_basic.js
  - Test socket creation
  - **Test**: Test passes

- [ ] **1.3.7** [0.5h] Create test_dgram_esm.mjs
  - Test ESM import
  - **Test**: ESM test passes

- [ ] **1.3.8** [1h] Run all tests
  ```bash
  make test
  make wpt
  ```
  - Verify no regressions
  - Document results

**Acceptance Criteria**:
```javascript
// CommonJS
const dgram = require('dgram');
const socket1 = dgram.createSocket('udp4');

// ESM
import dgram from 'dgram';
const socket2 = dgram.createSocket('udp6');
```

**Parallel Execution**: 1.3.1-1.3.5 (parallel) → 1.3.6-1.3.7 (parallel) → 1.3.8

---

### Phase 2: Basic Send/Receive Operations (30 tasks)

**Goal**: Implement bind(), send(), and message reception
**Duration**: 3-4 days
**Dependencies**: [D:1] Phase 1 complete

#### Task 2.1: [S][R:HIGH][C:COMPLEX] Bind Implementation (10 tasks)
**Duration**: 1.5 days
**Dependencies**: [D:1.3] Module exports ready

**Subtasks**:
- [ ] **2.1.1** [2h] Implement bind([port] [, address] [, callback])
  - Parse variable arguments
  - Default port to random
  - Default address to '0.0.0.0' or '::'
  - **Test**: Basic bind works

- [ ] **2.1.2** [2h] Implement bind(options [, callback])
  - Parse options: port, address, exclusive
  - **Test**: Options bind works

- [ ] **2.1.3** [2h] IPv4 address binding
  - Use uv_ip4_addr() to parse
  - Call uv_udp_bind() with IPv4 addr
  - **Test**: IPv4 bind works

- [ ] **2.1.4** [2h] IPv6 address binding
  - Use uv_ip6_addr() to parse
  - Call uv_udp_bind() with IPv6 addr
  - **Test**: IPv6 bind works

- [ ] **2.1.5** [2h] Implement exclusive option
  - Use UV_UDP_REUSEADDR flag if not exclusive
  - **Test**: Exclusive works

- [ ] **2.1.6** [1h] Add bound state tracking
  - Set socket->bound = true
  - Prevent double bind
  - **Test**: Error on double bind

- [ ] **2.1.7** [1h] Implement 'listening' event
  - Emit after successful bind
  - Call callback if provided
  - **Test**: Event fires

- [ ] **2.1.8** [2h] Add error handling
  - Handle EADDRINUSE
  - Handle EADDRNOTAVAIL
  - Handle invalid address
  - **Test**: Errors handled correctly

- [ ] **2.1.9** [1h] Implement address() method
  - Use uv_udp_getsockname()
  - Return {address, family, port}
  - **Test**: Returns correct info

- [ ] **2.1.10** [1h] Write bind tests
  - test_dgram_bind.js (8 tests)
  - **Test**: All pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');

// Simple bind
socket.bind(41234, () => {
  console.log('Listening on', socket.address());
  // { address: '0.0.0.0', family: 'IPv4', port: 41234 }
});

// Options bind
socket.bind({
  port: 41235,
  address: '127.0.0.1',
  exclusive: true
}, () => {
  console.log('Bound exclusively');
});
```

**Parallel Execution**: 2.1.1-2.1.2 (sequential) → 2.1.3-2.1.9 (parallel) → 2.1.10

#### Task 2.2: [S][R:HIGH][C:COMPLEX] Send Implementation (12 tasks)
**Duration**: 1.5 days
**Dependencies**: [D:2.1] Bind complete

**Subtasks**:
- [ ] **2.2.1** [2h] Implement send(msg, port [, address] [, callback])
  - Parse arguments
  - Default address to '127.0.0.1' or '::1'
  - **Test**: Basic send works

- [ ] **2.2.2** [2h] Implement send(msg, offset, length, port, address, callback)
  - Handle Buffer slice
  - **Test**: Offset/length work

- [ ] **2.2.3** [2h] Create JSDgramSendReq structure
  - Allocate with js_malloc
  - Store callback reference
  - **Test**: Request allocated

- [ ] **2.2.4** [2h] Prepare uv_buf_t for sending
  - Copy buffer data
  - Create uv_buf_t
  - **Test**: Buffer prepared

- [ ] **2.2.5** [3h] Call uv_udp_send()
  - Parse destination address
  - Call uv_udp_send with callback
  - **Test**: Send initiated

- [ ] **2.2.6** [2h] Implement on_dgram_send callback
  - Handle success/error
  - Call JS callback if provided
  - Cleanup send request
  - **Test**: Callback fires

- [ ] **2.2.7** [1h] Update statistics
  - Increment messages_sent
  - Increment bytes_sent
  - **Test**: Stats updated

- [ ] **2.2.8** [1h] Handle EMSGSIZE error
  - Message too long
  - **Test**: Error emitted

- [ ] **2.2.9** [1h] Handle ENETDOWN error
  - Network is down
  - **Test**: Error emitted

- [ ] **2.2.10** [1h] Implement automatic binding
  - Bind to random port if not bound
  - **Test**: Auto-bind works

- [ ] **2.2.11** [1h] Add send queue management
  - Track pending sends
  - **Test**: Queue works

- [ ] **2.2.12** [1h] Write send tests
  - Basic send/receive
  - **Test**: Tests pass

**Acceptance Criteria**:
```javascript
const sender = dgram.createSocket('udp4');
const receiver = dgram.createSocket('udp4');

receiver.bind(41234, () => {
  sender.send('Hello UDP', 41234, 'localhost', (err) => {
    if (err) throw err;
    console.log('Message sent');
  });
});
```

**Parallel Execution**: 2.2.1-2.2.2 (sequential) → 2.2.3-2.2.11 (parallel) → 2.2.12

#### Task 2.3: [S][R:HIGH][C:COMPLEX] Receive Implementation (8 tasks)
**Duration**: 1 day
**Dependencies**: [D:2.2] Send complete

**Subtasks**:
- [ ] **2.3.1** [2h] Implement on_dgram_recv callback
  - Handle uv_udp_recv_cb signature
  - Parse sender address
  - **Test**: Callback receives data

- [ ] **2.3.2** [2h] Implement on_dgram_alloc callback
  - Allocate buffer for incoming data
  - Use uv_alloc_cb signature
  - **Test**: Buffer allocated

- [ ] **2.3.3** [2h] Create rinfo object
  - {address, family, port, size}
  - Parse from struct sockaddr
  - **Test**: rinfo correct

- [ ] **2.3.4** [2h] Emit 'message' event
  - Create Buffer from received data
  - Emit with (msg, rinfo)
  - **Test**: Event fires

- [ ] **2.3.5** [1h] Handle UV_UDP_PARTIAL flag
  - Detect truncated messages
  - Include in rinfo or error
  - **Test**: Partial detected

- [ ] **2.3.6** [1h] Update statistics
  - Increment messages_received
  - Increment bytes_received
  - **Test**: Stats updated

- [ ] **2.3.7** [1h] Start receiving on bind
  - Call uv_udp_recv_start()
  - Set receiving flag
  - **Test**: Receiving starts

- [ ] **2.3.8** [1h] Write receive tests
  - Send/receive test
  - **Test**: Tests pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');

socket.on('message', (msg, rinfo) => {
  console.log('Received:', msg.toString());
  console.log('From:', rinfo.address, rinfo.port);
  // Received: Hello UDP
  // From: 127.0.0.1 54321
});

socket.bind(41234);
```

**Parallel Execution**: 2.3.1-2.3.2 (parallel) → 2.3.3-2.3.7 (parallel) → 2.3.8

---

### Phase 3: Events & EventEmitter Integration (20 tasks)

**Goal**: Implement all socket events with proper EventEmitter integration
**Duration**: 2-3 days
**Dependencies**: [D:2] Phase 2 complete

#### Task 3.1: [S][R:MEDIUM][C:MEDIUM] Core Events (8 tasks)
**Duration**: 1 day
**Dependencies**: [D:2.3] Receive complete

**Subtasks**:
- [ ] **3.1.1** [2h] Implement 'error' event
  - Emit on all errors
  - Include error object
  - **Test**: Error event fires

- [ ] **3.1.2** [2h] Implement 'close' event
  - Emit after socket closed
  - **Test**: Close event fires

- [ ] **3.1.3** [1h] Implement 'listening' event
  - Already done in 2.1.7
  - Verify it works
  - **Test**: Listening event fires

- [ ] **3.1.4** [1h] Implement 'message' event
  - Already done in 2.3.4
  - Verify it works
  - **Test**: Message event fires

- [ ] **3.1.5** [1h] Add error event special handling
  - Throw if no listeners
  - **Test**: Unhandled error throws

- [ ] **3.1.6** [1h] Test event ordering
  - Verify correct sequence
  - **Test**: Order correct

- [ ] **3.1.7** [1h] Test event listener management
  - Add/remove listeners
  - **Test**: Management works

- [ ] **3.1.8** [1h] Write event tests
  - test_dgram_events.js (10 tests)
  - **Test**: All pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');

socket.on('listening', () => {
  console.log('Socket is listening');
});

socket.on('message', (msg, rinfo) => {
  console.log('Message:', msg.toString());
});

socket.on('error', (err) => {
  console.error('Error:', err.message);
});

socket.on('close', () => {
  console.log('Socket closed');
});

socket.bind(41234);
```

**Parallel Execution**: 3.1.1-3.1.7 (parallel) → 3.1.8

#### Task 3.2: [S][R:MEDIUM][C:MEDIUM] Close Implementation (7 tasks)
**Duration**: 1 day
**Dependencies**: [D:3.1] Core events ready

**Subtasks**:
- [ ] **3.2.1** [2h] Implement close([callback]) method
  - Stop receiving
  - Close UV handle
  - **Test**: Close works

- [ ] **3.2.2** [2h] Implement uv_udp_recv_stop()
  - Stop receiving messages
  - Clear receiving flag
  - **Test**: Receiving stops

- [ ] **3.2.3** [2h] Implement handle close callback
  - Use dgram_socket_close_callback
  - Emit 'close' event
  - **Test**: Callback fires

- [ ] **3.2.4** [1h] Add close callback handling
  - Call user callback if provided
  - **Test**: Callback called

- [ ] **3.2.5** [1h] Set destroyed flag
  - Mark socket as destroyed
  - Prevent further operations
  - **Test**: Destroyed flag set

- [ ] **3.2.6** [1h] Cleanup resources
  - Free multicast_interface
  - Cancel pending sends
  - **Test**: Resources freed

- [ ] **3.2.7** [1h] Write close tests
  - Normal close
  - Close with callback
  - **Test**: Tests pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');

socket.on('close', () => {
  console.log('Closed');
});

socket.bind(41234, () => {
  socket.close(() => {
    console.log('Close callback');
  });
});
```

**Parallel Execution**: 3.2.1-3.2.6 (parallel) → 3.2.7

#### Task 3.3: [P][R:LOW][C:SIMPLE] Ref/Unref (5 tasks)
**Duration**: 0.5 days
**Dependencies**: [D:3.2] Close ready

**Subtasks**:
- [ ] **3.3.1** [1h] Implement ref() method
  - Call uv_ref()
  - Keep event loop alive
  - **Test**: Ref works

- [ ] **3.3.2** [1h] Implement unref() method
  - Call uv_unref()
  - Allow process exit
  - **Test**: Unref works

- [ ] **3.3.3** [1h] Test ref behavior
  - Socket keeps process alive
  - **Test**: Process doesn't exit

- [ ] **3.3.4** [1h] Test unref behavior
  - Socket doesn't keep process alive
  - **Test**: Process can exit

- [ ] **3.3.5** [0.5h] Write ref/unref tests
  - Test both methods
  - **Test**: Tests pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');
socket.bind(41234);
socket.unref();  // Process can exit even with open socket
```

**Parallel Execution**: All tasks can run in parallel

---

### Phase 4: Address & Socket Options (25 tasks)

**Goal**: Implement address methods and basic socket options
**Duration**: 2-3 days
**Dependencies**: [D:3] Phase 3 complete

#### Task 4.1: [S][R:MEDIUM][C:MEDIUM] Address Methods (8 tasks)
**Duration**: 1 day
**Dependencies**: [D:3.3] Ref/unref ready

**Subtasks**:
- [ ] **4.1.1** [2h] Enhance address() method
  - Already implemented in 2.1.9
  - Add error handling
  - **Test**: Handles edge cases

- [ ] **4.1.2** [1h] Handle unbound socket
  - Return null or throw
  - **Test**: Correct behavior

- [ ] **4.1.3** [2h] IPv4 address formatting
  - Use uv_ip4_name()
  - Format correctly
  - **Test**: IPv4 correct

- [ ] **4.1.4** [2h] IPv6 address formatting
  - Use uv_ip6_name()
  - Format correctly
  - **Test**: IPv6 correct

- [ ] **4.1.5** [1h] Family detection
  - Detect IPv4 vs IPv6
  - Return correct family string
  - **Test**: Family correct

- [ ] **4.1.6** [1h] Port extraction
  - Extract port from sockaddr
  - Convert from network byte order
  - **Test**: Port correct

- [ ] **4.1.7** [1h] Test with different addresses
  - 0.0.0.0, 127.0.0.1, ::1, etc.
  - **Test**: All work

- [ ] **4.1.8** [1h] Write address tests
  - Test address() method
  - **Test**: Tests pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');
socket.bind(41234, '127.0.0.1', () => {
  const addr = socket.address();
  console.log(addr);
  // { address: '127.0.0.1', family: 'IPv4', port: 41234 }
});
```

**Parallel Execution**: 4.1.1-4.1.7 (parallel) → 4.1.8

#### Task 4.2: [S][R:MEDIUM][C:MEDIUM] Basic Socket Options (10 tasks)
**Duration**: 1 day
**Dependencies**: [D:4.1] Address methods ready

**Subtasks**:
- [ ] **4.2.1** [2h] Implement setBroadcast(flag)
  - Call uv_udp_set_broadcast()
  - Enable/disable broadcast
  - **Test**: Broadcast flag set

- [ ] **4.2.2** [2h] Implement setTTL(ttl)
  - Call uv_udp_set_ttl()
  - Validate TTL range (1-255)
  - **Test**: TTL set

- [ ] **4.2.3** [1h] Validate broadcast flag
  - Check boolean value
  - **Test**: Validation works

- [ ] **4.2.4** [1h] Validate TTL value
  - Range 1-255
  - Throw on invalid
  - **Test**: Validation works

- [ ] **4.2.5** [1h] Handle libuv errors
  - Map to Node.js errors
  - **Test**: Errors handled

- [ ] **4.2.6** [1h] Test broadcast send
  - Send to 255.255.255.255
  - **Test**: Broadcast works

- [ ] **4.2.7** [1h] Test TTL behavior
  - Set different TTLs
  - **Test**: TTL affects routing

- [ ] **4.2.8** [1h] Test option persistence
  - Options persist across operations
  - **Test**: Persistence works

- [ ] **4.2.9** [1h] Test error cases
  - Invalid values
  - **Test**: Errors thrown

- [ ] **4.2.10** [1h] Write options tests
  - test_dgram_options.js (8 tests)
  - **Test**: All pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');
socket.bind(41234, () => {
  socket.setBroadcast(true);
  socket.setTTL(64);
  socket.send('Broadcast', 41234, '255.255.255.255');
});
```

**Parallel Execution**: 4.2.1-4.2.9 (parallel) → 4.2.10

#### Task 4.3: [P][R:LOW][C:SIMPLE] Error Handling (7 tasks)
**Duration**: 0.5 days
**Dependencies**: [D:4.2] Options ready

**Subtasks**:
- [ ] **4.3.1** [1h] Create error mapping table
  - libuv to Node.js codes
  - **Test**: Mapping correct

- [ ] **4.3.2** [1h] Implement create_dgram_error()
  - Create Error with properties
  - code, errno, syscall
  - **Test**: Error created correctly

- [ ] **4.3.3** [1h] Add error emission helper
  - emit_dgram_error()
  - **Test**: Errors emitted

- [ ] **4.3.4** [1h] Test EADDRINUSE
  - Address already in use
  - **Test**: Correct error

- [ ] **4.3.5** [1h] Test EADDRNOTAVAIL
  - Address not available
  - **Test**: Correct error

- [ ] **4.3.6** [1h] Test EACCES
  - Permission denied
  - **Test**: Correct error

- [ ] **4.3.7** [1h] Write error tests
  - test_dgram_errors.js (8 tests)
  - **Test**: All pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');
socket.on('error', (err) => {
  console.log(err.code);      // 'EADDRINUSE'
  console.log(err.errno);     // -48 (or similar)
  console.log(err.syscall);   // 'bind'
});
socket.bind(80);  // Likely fails with permission error
```

**Parallel Execution**: All tasks can run in parallel

---

### Phase 5: Multicast Support (30 tasks)

**Goal**: Implement full multicast functionality
**Duration**: 3-4 days
**Dependencies**: [D:4] Phase 4 complete

#### Task 5.1: [S][R:HIGH][C:COMPLEX] Basic Multicast (10 tasks)
**Duration**: 1.5 days
**Dependencies**: [D:4.3] Error handling ready

**Subtasks**:
- [ ] **5.1.1** [2h] Implement addMembership(multicastAddress [, multicastInterface])
  - Parse arguments
  - Call uv_udp_set_membership(UV_JOIN_GROUP)
  - **Test**: Join works

- [ ] **5.1.2** [2h] Implement dropMembership(multicastAddress [, multicastInterface])
  - Parse arguments
  - Call uv_udp_set_membership(UV_LEAVE_GROUP)
  - **Test**: Leave works

- [ ] **5.1.3** [2h] Parse multicast address
  - Validate IP format
  - Ensure it's multicast range
  - **Test**: Validation works

- [ ] **5.1.4** [2h] Parse interface address
  - Default to null (auto)
  - Validate if provided
  - **Test**: Interface works

- [ ] **5.1.5** [1h] Store multicast state
  - Track joined groups
  - **Test**: State tracked

- [ ] **5.1.6** [1h] Handle duplicate joins
  - Prevent double join
  - Or allow (match Node.js)
  - **Test**: Behavior correct

- [ ] **5.1.7** [1h] Handle leave without join
  - Error or silent
  - **Test**: Behavior correct

- [ ] **5.1.8** [1h] Test IPv4 multicast
  - 224.0.0.0 - 239.255.255.255
  - **Test**: IPv4 works

- [ ] **5.1.9** [1h] Test IPv6 multicast
  - ff00::/8
  - **Test**: IPv6 works

- [ ] **5.1.10** [1h] Write basic multicast tests
  - Join/leave tests
  - **Test**: Tests pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');
socket.bind(41234, () => {
  socket.addMembership('224.0.0.114');

  socket.on('message', (msg, rinfo) => {
    console.log('Multicast message:', msg.toString());
  });
});
```

**Parallel Execution**: 5.1.1-5.1.9 (parallel) → 5.1.10

#### Task 5.2: [S][R:MEDIUM][C:COMPLEX] Multicast Options (10 tasks)
**Duration**: 1 day
**Dependencies**: [D:5.1] Basic multicast ready

**Subtasks**:
- [ ] **5.2.1** [2h] Implement setMulticastTTL(ttl)
  - Call uv_udp_set_multicast_ttl()
  - Validate range 0-255
  - **Test**: TTL set

- [ ] **5.2.2** [2h] Implement setMulticastInterface(multicastInterface)
  - Call uv_udp_set_multicast_interface()
  - Parse interface address
  - **Test**: Interface set

- [ ] **5.2.3** [2h] Implement setMulticastLoopback(flag)
  - Call uv_udp_set_multicast_loop()
  - Enable/disable loopback
  - **Test**: Loopback set

- [ ] **5.2.4** [1h] Validate multicast TTL
  - Range 0-255
  - **Test**: Validation works

- [ ] **5.2.5** [1h] Validate loopback flag
  - Boolean value
  - **Test**: Validation works

- [ ] **5.2.6** [1h] Test multicast TTL behavior
  - Set different TTLs
  - **Test**: TTL affects routing

- [ ] **5.2.7** [1h] Test interface selection
  - Bind to specific interface
  - **Test**: Interface works

- [ ] **5.2.8** [1h] Test loopback mode
  - Receive own multicast
  - **Test**: Loopback works

- [ ] **5.2.9** [1h] Test option combinations
  - Multiple options together
  - **Test**: All work together

- [ ] **5.2.10** [1h] Write multicast option tests
  - TTL, interface, loopback tests
  - **Test**: Tests pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');
socket.bind(41234, () => {
  socket.setMulticastTTL(128);
  socket.setMulticastLoopback(true);
  socket.setMulticastInterface('192.168.1.100');
  socket.addMembership('224.0.0.114');
});
```

**Parallel Execution**: 5.2.1-5.2.9 (parallel) → 5.2.10

#### Task 5.3: [S][R:HIGH][C:COMPLEX] Source-Specific Multicast (10 tasks)
**Duration**: 1 day
**Dependencies**: [D:5.2] Multicast options ready

**Subtasks**:
- [ ] **5.3.1** [2h] Implement addSourceSpecificMembership(sourceAddress, groupAddress [, multicastInterface])
  - Parse 3 arguments
  - Call uv_udp_set_source_membership(UV_JOIN_GROUP)
  - **Test**: SSM join works

- [ ] **5.3.2** [2h] Implement dropSourceSpecificMembership(sourceAddress, groupAddress [, multicastInterface])
  - Parse 3 arguments
  - Call uv_udp_set_source_membership(UV_LEAVE_GROUP)
  - **Test**: SSM leave works

- [ ] **5.3.3** [2h] Parse source address
  - Validate unicast address
  - **Test**: Source valid

- [ ] **5.3.4** [2h] Parse group address
  - Validate multicast address
  - **Test**: Group valid

- [ ] **5.3.5** [1h] Validate SSM arguments
  - All 3 addresses correct
  - **Test**: Validation works

- [ ] **5.3.6** [1h] Test SSM with IPv4
  - Source-specific IPv4 multicast
  - **Test**: IPv4 SSM works

- [ ] **5.3.7** [1h] Test SSM with IPv6
  - Source-specific IPv6 multicast
  - **Test**: IPv6 SSM works

- [ ] **5.3.8** [1h] Test SSM filtering
  - Receive only from specific source
  - **Test**: Filtering works

- [ ] **5.3.9** [1h] Handle SSM errors
  - Invalid addresses
  - Platform not supported
  - **Test**: Errors handled

- [ ] **5.3.10** [1h] Write SSM tests
  - test_dgram_multicast.js updated
  - **Test**: Tests pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');
socket.bind(41234, () => {
  // Only receive from specific source
  socket.addSourceSpecificMembership(
    '192.168.1.50',      // source
    '232.0.0.114',       // group
    '192.168.1.100'      // interface
  );
});
```

**Parallel Execution**: 5.3.1-5.3.9 (parallel) → 5.3.10

---

### Phase 6: Broadcast & TTL (20 tasks)

**Goal**: Complete broadcast support and TTL management
**Duration**: 2-3 days
**Dependencies**: [D:5] Phase 5 complete

#### Task 6.1: [S][R:MEDIUM][C:MEDIUM] Broadcast Implementation (10 tasks)
**Duration**: 1 day
**Dependencies**: [D:5.3] SSM ready

**Subtasks**:
- [ ] **6.1.1** [2h] Enhance setBroadcast() method
  - Already implemented in 4.2.1
  - Add comprehensive tests
  - **Test**: Broadcast fully tested

- [ ] **6.1.2** [2h] Test broadcast send
  - Send to 255.255.255.255
  - **Test**: Message sent

- [ ] **6.1.3** [2h] Test broadcast receive
  - Receive broadcast messages
  - **Test**: Message received

- [ ] **6.1.4** [1h] Test subnet broadcast
  - Send to subnet broadcast address
  - **Test**: Subnet broadcast works

- [ ] **6.1.5** [1h] Test broadcast without setBroadcast
  - Should fail with error
  - **Test**: Error thrown

- [ ] **6.1.6** [1h] Test broadcast flag persistence
  - Flag persists across operations
  - **Test**: Persistence works

- [ ] **6.1.7** [1h] Handle broadcast errors
  - EACCES if not enabled
  - **Test**: Errors handled

- [ ] **6.1.8** [1h] Test IPv4 broadcast
  - IPv4 broadcast works
  - **Test**: IPv4 works

- [ ] **6.1.9** [1h] Test IPv6 broadcast
  - IPv6 has no broadcast (multicast instead)
  - **Test**: Behaves correctly

- [ ] **6.1.10** [1h] Write broadcast tests
  - test_dgram_broadcast.js (6 tests)
  - **Test**: All pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');
socket.bind(41234, () => {
  socket.setBroadcast(true);
  socket.send('Broadcast message', 41234, '255.255.255.255', (err) => {
    if (err) throw err;
    console.log('Broadcast sent');
  });
});
```

**Parallel Execution**: 6.1.1-6.1.9 (parallel) → 6.1.10

#### Task 6.2: [P][R:MEDIUM][C:SIMPLE] TTL Management (10 tasks)
**Duration**: 1 day
**Dependencies**: [D:6.1] Broadcast ready

**Subtasks**:
- [ ] **6.2.1** [2h] Enhance setTTL() method
  - Already implemented in 4.2.2
  - Add comprehensive tests
  - **Test**: TTL fully tested

- [ ] **6.2.2** [1h] Test default TTL
  - Default should be 64
  - **Test**: Default correct

- [ ] **6.2.3** [1h] Test TTL range validation
  - 1-255 valid
  - 0 or >255 invalid
  - **Test**: Validation works

- [ ] **6.2.4** [1h] Test TTL = 1 (local)
  - Packets don't leave subnet
  - **Test**: Local only

- [ ] **6.2.5** [1h] Test TTL = 64 (typical)
  - Normal routing
  - **Test**: Normal routing works

- [ ] **6.2.6** [1h] Test TTL = 255 (max)
  - Maximum hops
  - **Test**: Max hops work

- [ ] **6.2.7** [1h] Compare unicast vs multicast TTL
  - Different methods
  - **Test**: Both work independently

- [ ] **6.2.8** [1h] Test TTL persistence
  - TTL persists across sends
  - **Test**: Persistence works

- [ ] **6.2.9** [1h] Handle TTL errors
  - Invalid values
  - **Test**: Errors handled

- [ ] **6.2.10** [1h] Update options tests
  - Add TTL tests to test_dgram_options.js
  - **Test**: Tests pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');
socket.bind(41234, () => {
  socket.setTTL(1);  // Local subnet only
  socket.send('Local message', 41234, '192.168.1.255');

  socket.setMulticastTTL(32);  // Multicast TTL separate
  socket.addMembership('224.0.0.114');
});
```

**Parallel Execution**: All tasks can run in parallel

---

### Phase 7: Connected UDP (Optional) (10 tasks)

**Goal**: Implement connected UDP socket support (Node.js v12+)
**Duration**: 1-2 days
**Dependencies**: [D:6] Phase 6 complete
**Priority**: OPTIONAL - Can be deferred

#### Task 7.1: [S][R:MEDIUM][C:MEDIUM] Connected UDP (10 tasks)
**Duration**: 1-2 days
**Dependencies**: [D:6.2] TTL management ready

**Subtasks**:
- [ ] **7.1.1** [2h] Implement connect(port [, address] [, callback])
  - Parse arguments
  - Call uv_udp_connect()
  - **Test**: Connect works

- [ ] **7.1.2** [2h] Implement disconnect() method
  - Call uv_udp_connect(NULL)
  - Reset connected state
  - **Test**: Disconnect works

- [ ] **7.1.3** [2h] Implement remoteAddress() method
  - Call uv_udp_getpeername()
  - Return {address, family, port}
  - **Test**: Remote address correct

- [ ] **7.1.4** [1h] Emit 'connect' event
  - After successful connect
  - **Test**: Event fires

- [ ] **7.1.5** [1h] Update send() for connected sockets
  - Allow send without address
  - Use connected address
  - **Test**: Send works

- [ ] **7.1.6** [1h] Prevent double connect
  - Error on already connected
  - **Test**: Error thrown

- [ ] **7.1.7** [1h] Test connected send/receive
  - Full duplex communication
  - **Test**: Works correctly

- [ ] **7.1.8** [1h] Test disconnect and reconnect
  - Can reconnect to different address
  - **Test**: Reconnect works

- [ ] **7.1.9** [1h] Handle connect errors
  - Invalid address
  - **Test**: Errors handled

- [ ] **7.1.10** [1h] Write connected UDP tests
  - test_dgram_connected.js (6 tests)
  - **Test**: All pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');

socket.on('connect', () => {
  console.log('Connected to', socket.remoteAddress());
  socket.send('Hello', (err) => {
    if (err) throw err;
  });
});

socket.connect(41234, 'localhost');
```

**Parallel Execution**: 7.1.1-7.1.9 (parallel) → 7.1.10

---

### Phase 8: Buffer Size & Advanced Features (10 tasks)

**Goal**: Implement buffer size operations and final features
**Duration**: 1-2 days
**Dependencies**: [D:7] Phase 7 complete (or D:6 if Phase 7 skipped)

#### Task 8.1: [P][R:MEDIUM][C:SIMPLE] Buffer Size Operations (10 tasks)
**Duration**: 1 day
**Dependencies**: [D:7.1] Connected UDP ready (or [D:6.2])

**Subtasks**:
- [ ] **8.1.1** [1h] Implement getSendBufferSize()
  - Call uv_send_buffer_size() with NULL
  - Return current size
  - **Test**: Returns correct size

- [ ] **8.1.2** [1h] Implement getRecvBufferSize()
  - Call uv_recv_buffer_size() with NULL
  - Return current size
  - **Test**: Returns correct size

- [ ] **8.1.3** [1h] Implement setSendBufferSize(size)
  - Call uv_send_buffer_size() with size
  - Validate size > 0
  - **Test**: Size set correctly

- [ ] **8.1.4** [1h] Implement setRecvBufferSize(size)
  - Call uv_recv_buffer_size() with size
  - Validate size > 0
  - **Test**: Size set correctly

- [ ] **8.1.5** [1h] Test buffer size validation
  - Invalid sizes rejected
  - **Test**: Validation works

- [ ] **8.1.6** [1h] Test default buffer sizes
  - Check platform defaults
  - **Test**: Defaults correct

- [ ] **8.1.7** [1h] Test buffer size changes
  - Change sizes dynamically
  - **Test**: Changes apply

- [ ] **8.1.8** [1h] Test large buffer sizes
  - Set very large buffers
  - **Test**: Large sizes work

- [ ] **8.1.9** [1h] Handle buffer size errors
  - OS limitations
  - **Test**: Errors handled

- [ ] **8.1.10** [1h] Update options tests
  - Add buffer size tests
  - **Test**: Tests pass

**Acceptance Criteria**:
```javascript
const socket = dgram.createSocket('udp4');

console.log('Send buffer:', socket.getSendBufferSize());
console.log('Recv buffer:', socket.getRecvBufferSize());

socket.setSendBufferSize(65536);
socket.setRecvBufferSize(65536);

console.log('New send buffer:', socket.getSendBufferSize());
console.log('New recv buffer:', socket.getRecvBufferSize());
```

**Parallel Execution**: All tasks can run in parallel

---

## 🧪 Testing Strategy

### Test Coverage Requirements

#### Unit Tests (70+ test cases)
**Coverage**: 100% of all dgram methods and events
**Location**: `test/node/dgram/`
**Execution**: `make test`

**Test Files Structure**:
```
test/node/dgram/
├── test_dgram_basic.js           [10 tests] Socket creation, basic ops
├── test_dgram_bind.js             [8 tests]  Binding scenarios
├── test_dgram_events.js           [10 tests] Event emission
├── test_dgram_errors.js           [8 tests]  Error handling
├── test_dgram_multicast.js        [10 tests] Multicast functionality
├── test_dgram_broadcast.js        [6 tests]  Broadcast operations
├── test_dgram_options.js          [8 tests]  Socket options
├── test_dgram_connected.js        [6 tests]  Connected UDP [OPTIONAL]
└── test_dgram_esm.mjs             [4 tests]  ESM imports

Total: 70 test cases
```

#### Test Categories

**1. Socket Creation & Lifecycle** (10 tests)
```javascript
// test_dgram_basic.js
test('createSocket with type string', () => {
  const socket = dgram.createSocket('udp4');
  assert.ok(socket);
});

test('createSocket with options', () => {
  const socket = dgram.createSocket({
    type: 'udp4',
    reuseAddr: true
  });
  assert.ok(socket);
});

test('createSocket with callback', (done) => {
  const socket = dgram.createSocket('udp4', (msg, rinfo) => {
    assert.ok(msg);
    done();
  });
  // ... send test message
});
```

**2. Binding** (8 tests)
```javascript
// test_dgram_bind.js
test('bind to specific port', (done) => {
  const socket = dgram.createSocket('udp4');
  socket.bind(41234, () => {
    const addr = socket.address();
    assert.strictEqual(addr.port, 41234);
    socket.close(done);
  });
});

test('bind to random port', (done) => {
  const socket = dgram.createSocket('udp4');
  socket.bind(0, () => {
    const addr = socket.address();
    assert.ok(addr.port > 0);
    socket.close(done);
  });
});

test('bind error EADDRINUSE', (done) => {
  const socket1 = dgram.createSocket('udp4');
  const socket2 = dgram.createSocket('udp4');

  socket1.bind(41234, () => {
    socket2.on('error', (err) => {
      assert.strictEqual(err.code, 'EADDRINUSE');
      socket1.close();
      socket2.close();
      done();
    });
    socket2.bind(41234);
  });
});
```

**3. Send/Receive** (included in other tests)
```javascript
test('send and receive message', (done) => {
  const sender = dgram.createSocket('udp4');
  const receiver = dgram.createSocket('udp4');

  receiver.on('message', (msg, rinfo) => {
    assert.strictEqual(msg.toString(), 'Hello UDP');
    assert.strictEqual(rinfo.port, sender.address().port);
    sender.close();
    receiver.close(done);
  });

  receiver.bind(41234, () => {
    sender.send('Hello UDP', 41234, 'localhost');
  });
});
```

**4. Events** (10 tests)
```javascript
// test_dgram_events.js
test('listening event fires', (done) => {
  const socket = dgram.createSocket('udp4');
  socket.on('listening', () => {
    assert.ok(true);
    socket.close(done);
  });
  socket.bind(41234);
});

test('close event fires', (done) => {
  const socket = dgram.createSocket('udp4');
  socket.on('close', () => {
    assert.ok(true);
    done();
  });
  socket.bind(41234, () => {
    socket.close();
  });
});

test('error event with no listener throws', () => {
  const socket = dgram.createSocket('udp4');
  assert.throws(() => {
    // Simulate error emission
    socket.emit('error', new Error('test'));
  });
});
```

**5. Multicast** (10 tests)
```javascript
// test_dgram_multicast.js
test('addMembership and receive multicast', (done) => {
  const socket1 = dgram.createSocket('udp4');
  const socket2 = dgram.createSocket('udp4');

  socket1.on('message', (msg, rinfo) => {
    assert.strictEqual(msg.toString(), 'Multicast');
    socket1.close();
    socket2.close(done);
  });

  socket1.bind(41234, () => {
    socket1.addMembership('224.0.0.114');
    socket2.send('Multicast', 41234, '224.0.0.114');
  });
});

test('setMulticastTTL', () => {
  const socket = dgram.createSocket('udp4');
  socket.bind(41234, () => {
    socket.setMulticastTTL(128);
    // Verify TTL set (no direct way to read, but no error)
    socket.close();
  });
});
```

**6. Broadcast** (6 tests)
```javascript
// test_dgram_broadcast.js
test('setBroadcast and send broadcast', (done) => {
  const socket = dgram.createSocket('udp4');
  socket.bind(41234, () => {
    socket.setBroadcast(true);
    socket.send('Broadcast', 41234, '255.255.255.255', (err) => {
      assert.ifError(err);
      socket.close(done);
    });
  });
});

test('broadcast without setBroadcast fails', (done) => {
  const socket = dgram.createSocket('udp4');
  socket.bind(41234, () => {
    socket.send('Broadcast', 41234, '255.255.255.255', (err) => {
      assert.ok(err);
      assert.strictEqual(err.code, 'EACCES');
      socket.close(done);
    });
  });
});
```

**7. Error Handling** (8 tests)
```javascript
// test_dgram_errors.js
test('bind EADDRINUSE', (done) => {
  // Already shown above
});

test('send EMSGSIZE', (done) => {
  const socket = dgram.createSocket('udp4');
  const largeMsg = Buffer.alloc(65536);  // Larger than MTU
  socket.send(largeMsg, 41234, 'localhost', (err) => {
    assert.ok(err);
    assert.strictEqual(err.code, 'EMSGSIZE');
    socket.close(done);
  });
});
```

#### Memory Safety Tests
**Tool**: AddressSanitizer (ASAN)
**Command**:
```bash
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m test/node/dgram/test_*.js
```

**Success Criteria**:
- Zero memory leaks
- Zero buffer overflows
- Zero use-after-free errors
- Clean ASAN reports

#### Integration Tests
- Send between multiple sockets
- Multicast with multiple receivers
- Broadcast with multiple listeners
- Mix of IPv4 and IPv6 sockets
- Stress test with many messages

#### Compatibility Tests
**Reference**: Node.js v18+ dgram behavior
- Event timing matches Node.js
- Error messages match Node.js format
- Edge case behavior matches exactly
- Options validation matches Node.js

### Test Execution Strategy
```bash
# Run all dgram tests
make test FILTER=dgram

# Run specific test file
./target/release/jsrt test/node/dgram/test_dgram_basic.js

# Run with ASAN for memory checks
./target/debug/jsrt_m test/node/dgram/test_dgram_multicast.js

# Run ESM tests
./target/release/jsrt test/node/dgram/test_dgram_esm.mjs

# Debug mode with detailed output
SHOW_ALL_FAILURES=1 make test FILTER=dgram

# Full validation
make format
make test
make wpt
```

---

## 📈 Task Summary & Execution Plan

### Task Count by Phase

| Phase | Name | Tasks | Complexity | Priority | Duration |
|-------|------|-------|------------|----------|----------|
| 0 | Research & Setup | 15 | SIMPLE | HIGH | 1-2 days |
| 1 | Foundation & Socket | 25 | MEDIUM | HIGH | 2-3 days |
| 2 | Send/Receive | 30 | COMPLEX | HIGH | 3-4 days |
| 3 | Events & Integration | 20 | MEDIUM | HIGH | 2-3 days |
| 4 | Address & Options | 25 | MEDIUM | HIGH | 2-3 days |
| 5 | Multicast | 30 | COMPLEX | HIGH | 3-4 days |
| 6 | Broadcast & TTL | 20 | MEDIUM | MEDIUM | 2-3 days |
| 7 | Connected UDP | 10 | MEDIUM | LOW (OPTIONAL) | 1-2 days |
| 8 | Buffer Size | 10 | SIMPLE | MEDIUM | 1-2 days |
| **TOTAL** | **All Phases** | **185** | **MIXED** | **MIXED** | **17-26 days** |

### Critical Path (Must Complete for Core Functionality)

**Critical Phases**: 0 → 1 → 2 → 3 → 4 → 5
- Phase 0: Research & Setup (15 tasks)
- Phase 1: Foundation (25 tasks)
- Phase 2: Send/Receive (30 tasks)
- Phase 3: Events (20 tasks)
- Phase 4: Address & Options (25 tasks)
- Phase 5: Multicast (30 tasks)
- **Total Critical Path**: 145 tasks (~14-20 days)

### Parallel Work Opportunities

**After Phase 0 completes:**
- Phase 1 (Foundation) can start

**After Phase 3 completes:**
- Phase 4 (Address) and Phase 6 (Broadcast) can run in parallel
- Both only depend on Phase 3

**After Phase 5 completes:**
- Phase 7 (Connected UDP) and Phase 8 (Buffer Size) can run in parallel
- Phase 7 is OPTIONAL, can be skipped

### Dependency Graph

```
Phase 0 (Research & Setup)
    ↓
Phase 1 (Foundation & Socket Creation)
    ↓
Phase 2 (Send/Receive Operations)
    ↓
Phase 3 (Events & EventEmitter)
    ↓
    ├──→ Phase 4 (Address & Options)
    │        ↓
    │    Phase 5 (Multicast Support)
    │        ↓
    └──→ Phase 6 (Broadcast & TTL)
             ↓
             ├──→ Phase 7 (Connected UDP) [OPTIONAL]
             │
             └──→ Phase 8 (Buffer Size Operations)
                      ↓
                 100% Complete
```

### Recommended Execution Order

1. **Week 1**: Phase 0, Phase 1 (Research + Foundation)
2. **Week 2**: Phase 2, Phase 3 (Send/Receive + Events)
3. **Week 3**: Phase 4, Phase 5 (Address + Multicast)
4. **Week 4**: Phase 6, Phase 8 (Broadcast + Buffer Size)
5. **Optional Week**: Phase 7 (Connected UDP)

**Total Estimated Duration**:
- Without Phase 7: 17-22 days (4 weeks)
- With Phase 7: 18-26 days (4-5 weeks)

**Critical Path Duration**: 14-20 days for 80% coverage

---

## 🎯 Success Criteria

### Functional Requirements
- ✅ **Module Functions** (2 functions)
  - [ ] dgram.createSocket(type [, callback])
  - [ ] dgram.createSocket(options [, callback])

- ✅ **Socket Methods** (20+ methods)
  - [ ] bind([port] [, address] [, callback])
  - [ ] bind(options [, callback])
  - [ ] send(msg, [offset, length,] port [, address] [, callback])
  - [ ] close([callback])
  - [ ] address()
  - [ ] setBroadcast(flag)
  - [ ] setTTL(ttl)
  - [ ] setMulticastTTL(ttl)
  - [ ] setMulticastInterface(multicastInterface)
  - [ ] setMulticastLoopback(flag)
  - [ ] addMembership(multicastAddress [, multicastInterface])
  - [ ] dropMembership(multicastAddress [, multicastInterface])
  - [ ] addSourceSpecificMembership(...)
  - [ ] dropSourceSpecificMembership(...)
  - [ ] ref()
  - [ ] unref()
  - [ ] getSendBufferSize()
  - [ ] getRecvBufferSize()
  - [ ] setSendBufferSize(size)
  - [ ] setRecvBufferSize(size)
  - [ ] connect(port [, address] [, callback]) [OPTIONAL]
  - [ ] disconnect() [OPTIONAL]
  - [ ] remoteAddress() [OPTIONAL]

- ✅ **Socket Events** (5 events)
  - [ ] 'message' (msg, rinfo)
  - [ ] 'listening'
  - [ ] 'close'
  - [ ] 'error' (err)
  - [ ] 'connect' [OPTIONAL]

- ✅ **Core Features**
  - [ ] All core methods implemented (20+ APIs)
  - [ ] All events fire correctly
  - [ ] EventEmitter integration (100%)
  - [ ] IPv4 and IPv6 support
  - [ ] Multicast (join, leave, options)
  - [ ] Broadcast support
  - [ ] Buffer size management

### Quality Requirements
- ✅ **Testing** (100% coverage)
  - [ ] 70+ unit tests (100% pass rate)
  - [ ] Edge case testing (errors, edge conditions)
  - [ ] Integration tests (multicast, broadcast)
  - [ ] CommonJS and ESM tests
  - [ ] Cross-platform tests

- ✅ **Memory Safety**
  - [ ] Zero memory leaks (ASAN validation)
  - [ ] Proper cleanup on close
  - [ ] No buffer overflows
  - [ ] No use-after-free errors
  - [ ] Deferred cleanup pattern working

- ✅ **Code Quality**
  - [ ] Code formatted (`make format`)
  - [ ] WPT baseline maintained
  - [ ] Project tests pass (`make test`)
  - [ ] All functions documented
  - [ ] Debug logging for state changes

### Performance Requirements
- ⚡ **Throughput**
  - [ ] Handle 1000+ messages/second
  - [ ] Support large datagrams (up to 65507 bytes)
  - [ ] Efficient multicast handling

- ⚡ **Memory**
  - [ ] Memory usage proportional to buffer sizes
  - [ ] No memory leaks under load
  - [ ] Efficient send queue management

### Compatibility Requirements
- ✅ **Node.js Compatibility** (v18+)
  - [ ] API matches Node.js exactly
  - [ ] Event timing matches Node.js
  - [ ] Error messages match Node.js
  - [ ] Behavior matches Node.js test suite
  - [ ] All options supported

---

## ⚠️ Risk Assessment

### High Risk Items

**1. UDP Connectionless Complexity** [R:HIGH][Phases 2-3]
- **Risk**: Connectionless protocol has different error handling than TCP
- **Mitigation**: Study Node.js dgram behavior carefully, extensive testing
- **Fallback**: Implement basic features first, add advanced later

**2. Multicast Platform Differences** [R:HIGH][Phase 5]
- **Risk**: Multicast behavior varies by OS and network configuration
- **Mitigation**: Test on all platforms (macOS, Linux, Windows), skip unsupported features
- **Fallback**: Mark platform-specific features as such

**3. Memory Management in Async Send** [R:MEDIUM][Phase 2]
- **Risk**: Send requests must outlive the send operation
- **Mitigation**: Copy buffer data, use proper request cleanup
- **Fallback**: Use deferred cleanup pattern from net module

**4. EventEmitter Integration** [R:MEDIUM][Phase 3]
- **Risk**: Events must fire in correct order with correct timing
- **Mitigation**: Follow net module patterns exactly
- **Fallback**: Extensive event timing tests

**5. libuv UDP API Limitations** [R:MEDIUM][Phases 5-8]
- **Risk**: libuv may not support all Node.js dgram features
- **Mitigation**: Research libuv docs thoroughly before implementing
- **Fallback**: Document limitations, partial implementation acceptable

### Medium Risk Items

**6. IPv6 Testing** [R:MEDIUM][All Phases]
- **Risk**: May not have IPv6 available in all test environments
- **Mitigation**: Make IPv6 tests conditional, skip if unavailable
- **Fallback**: Test basic IPv6 parsing without actual network

**7. Broadcast Platform Differences** [R:MEDIUM][Phase 6]
- **Risk**: Broadcast may require admin/root on some platforms
- **Mitigation**: Document requirements, skip tests if insufficient permissions
- **Fallback**: Test with regular permissions where possible

**8. Buffer Size Limits** [R:MEDIUM][Phase 8]
- **Risk**: OS may have limits on buffer sizes
- **Mitigation**: Query system limits, validate against them
- **Fallback**: Document platform-specific limits

### Low Risk Items

**9. Error Code Mapping** [R:LOW][Phase 4]
- **Risk**: libuv error codes don't map 1:1 to Node.js
- **Mitigation**: Create comprehensive mapping table from net module
- **Fallback**: Use generic error codes where no mapping exists

**10. Connected UDP** [R:LOW][Phase 7]
- **Risk**: Optional feature, low priority
- **Mitigation**: Implement last, can defer if needed
- **Fallback**: Skip entirely, document as unsupported

---

## 📚 References

### Node.js Documentation
- **Official Docs**: https://nodejs.org/api/dgram.html
- **Stability**: 2 - Stable
- **Version Target**: Node.js v18+ compatibility

### libuv Documentation
- **UDP API**: http://docs.libuv.org/en/v1.x/udp.html
- **Header File**: deps/libuv/include/uv.h (lines 737-783)
- **Error Codes**: deps/libuv/include/uv.h (lines 75-159)
- **Multicast**: uv_udp_set_membership, uv_udp_set_source_membership
- **Broadcast**: uv_udp_set_broadcast
- **TTL**: uv_udp_set_ttl, uv_udp_set_multicast_ttl

### Existing jsrt Code References
- **node:net module**: `src/node/net/` (97% complete, excellent patterns)
  - net_internal.h (data structures, type tags)
  - net_callbacks.c (libuv callback patterns)
  - net_finalizers.c (deferred cleanup pattern)
  - net_socket.c (socket method implementations)
  - net_module.c (module exports)
- **EventEmitter**: `src/node/events/` (8 files, full implementation)
- **Module registration**: `src/node/node_modules.c`

### Testing Resources
- Node.js dgram tests: https://github.com/nodejs/node/tree/main/test/parallel (test-dgram-*.js files)
- jsrt test patterns: `test/node/net/test_*.js`

---

## 💡 Implementation Notes

### Memory Management Patterns
```c
// Always use QuickJS allocators
JSDgramSocket* socket = js_malloc(ctx, sizeof(JSDgramSocket));
js_free(ctx, socket);

// JSValue lifetime
JSValue val = JS_NewString(ctx, "test");
// Use val...
JS_FreeValue(ctx, val);

// Duplicate when storing
socket->socket_obj = JS_DupValue(ctx, socket_obj);
// Later...
JS_FreeValue(ctx, socket->socket_obj);
```

### libuv Callback Pattern (from net module)
```c
static void on_dgram_send(uv_udp_send_t* req, int status) {
  JSDgramSendReq* send_req = (JSDgramSendReq*)req;
  if (!send_req || !send_req->ctx) return;

  JSContext* ctx = send_req->ctx;

  // Handle error
  if (status < 0) {
    JSValue error = create_dgram_error(ctx, status, "send");
    // Emit 'error' event or call callback with error
  }

  // Call callback if provided
  if (!JS_IsUndefined(send_req->callback)) {
    JSValue result = JS_Call(ctx, send_req->callback, ...);
    JS_FreeValue(ctx, result);
  }

  // Cleanup
  JS_FreeValue(ctx, send_req->socket_obj);
  JS_FreeValue(ctx, send_req->callback);
  js_free(ctx, send_req->data);
  js_free(ctx, send_req);
}
```

### Error Creation Pattern (from net module)
```c
JSValue create_dgram_error(JSContext* ctx, int uv_err, const char* syscall) {
  JSValue error = JS_NewError(ctx);

  // Map libuv error to Node.js error code
  const char* code = uv_err_name(uv_err);  // e.g., "EADDRINUSE"

  JS_SetPropertyStr(ctx, error, "message",
                    JS_NewString(ctx, uv_strerror(uv_err)));
  JS_SetPropertyStr(ctx, error, "code",
                    JS_NewString(ctx, code));
  JS_SetPropertyStr(ctx, error, "errno",
                    JS_NewInt32(ctx, uv_err));
  JS_SetPropertyStr(ctx, error, "syscall",
                    JS_NewString(ctx, syscall));

  return error;
}
```

### Deferred Cleanup Pattern (from net module)
```c
// In runtime cleanup callback (runtime.c):
static void JSRT_RuntimeCleanupWalkCallback(uv_handle_t* handle, void* arg) {
  if (handle->data) {
    // Check type tag
    uint32_t* type_ptr = (uint32_t*)handle->data;
    if (*type_ptr == DGRAM_TYPE_SOCKET) {
      JSDgramSocket* socket = (JSDgramSocket*)handle->data;
      // Cleanup socket struct
      js_free(socket->ctx, socket);
      handle->data = NULL;
    }
  }
}

// In socket finalizer:
void js_dgram_socket_finalizer(JSRuntime* rt, JSValue val) {
  JSDgramSocket* socket = JS_GetOpaque(val, js_dgram_socket_class_id);
  if (!socket) return;

  // Mark for cleanup but don't free yet
  socket->destroyed = true;

  // Close handle (will trigger callback later)
  if (!uv_is_closing((uv_handle_t*)&socket->handle)) {
    uv_close((uv_handle_t*)&socket->handle, dgram_socket_close_callback);
  }
}
```

---

## 🔄 Change Log

| Date | Change | Reason |
|------|--------|--------|
| 2025-10-09 | Initial plan created | Project kickoff |

---

## 📋 Next Steps

### Immediate Actions (Phase 0)

1. **Run Baseline Tests**
   ```bash
   cd /Users/bytedance/github/leizongmin/jsrt
   make clean
   make jsrt_g
   make test > baseline_tests.log 2>&1
   make wpt > baseline_wpt.log 2>&1
   ```

2. **Document Baseline**
   - Count passing/failing tests
   - Note any existing test failures
   - Establish success criteria

3. **Set Up Development Environment**
   ```bash
   # Create feature branch
   git checkout -b feature/dgram-module

   # Build debug version
   make jsrt_g

   # Build ASAN version for memory testing
   make jsrt_m
   ```

4. **Create File Structure**
   ```bash
   mkdir -p src/node/dgram
   mkdir -p test/node/dgram
   ```

5. **Start Phase 0 Implementation**
   - Begin with task 0.1.1 (Read Node.js dgram docs)
   - Follow task breakdown sequentially

---

## 🎉 Completion Criteria

This implementation will be considered **COMPLETE** when:

1. ✅ All 30+ API methods implemented and working
2. ✅ 70+ tests written and passing (100% pass rate)
3. ✅ EventEmitter fully integrated into Socket class
4. ✅ All 5 socket events fire correctly (message, listening, close, error, connect)
5. ✅ IPv4 and IPv6 support verified
6. ✅ Multicast operations working (join, leave, SSM)
7. ✅ Broadcast support implemented and tested
8. ✅ Both CommonJS and ESM support verified
9. ✅ Zero memory leaks (ASAN validation)
10. ✅ WPT baseline maintained (no regressions)
11. ✅ Code properly formatted (`make format`)
12. ✅ All builds pass (`make test && make wpt && make clean && make`)
13. ✅ Documentation updated (if applicable)

### Optional Completion (100% API Coverage)
14. ⏳ Connected UDP implemented (Phase 7)
15. ⏳ All advanced features working

---

## 📊 API Coverage Tracking

### dgram.Socket Methods (23 total)
- [ ] bind([port] [, address] [, callback])
- [ ] bind(options [, callback])
- [ ] send(msg, [offset, length,] port [, address] [, callback])
- [ ] close([callback])
- [ ] address()
- [ ] setBroadcast(flag)
- [ ] setTTL(ttl)
- [ ] setMulticastTTL(ttl)
- [ ] setMulticastInterface(multicastInterface)
- [ ] setMulticastLoopback(flag)
- [ ] addMembership(multicastAddress [, multicastInterface])
- [ ] dropMembership(multicastAddress [, multicastInterface])
- [ ] addSourceSpecificMembership(sourceAddress, groupAddress [, multicastInterface])
- [ ] dropSourceSpecificMembership(sourceAddress, groupAddress [, multicastInterface])
- [ ] ref()
- [ ] unref()
- [ ] getSendBufferSize()
- [ ] getRecvBufferSize()
- [ ] setSendBufferSize(size)
- [ ] setRecvBufferSize(size)
- [ ] connect(port [, address] [, callback]) [OPTIONAL]
- [ ] disconnect() [OPTIONAL]
- [ ] remoteAddress() [OPTIONAL]

### dgram.Socket Events (5 total)
- [ ] 'message' (msg, rinfo)
- [ ] 'listening'
- [ ] 'close'
- [ ] 'error' (err)
- [ ] 'connect' [OPTIONAL]

### Module Functions (2 total)
- [ ] dgram.createSocket(type [, callback])
- [ ] dgram.createSocket(options [, callback])

**Total APIs**: 30+ (23 methods + 5 events + 2 functions)
**Optional APIs**: 3 (connected UDP features)

---

**Plan Status**: 📋 READY FOR IMPLEMENTATION

**Total Estimated Tasks**: 185
**Total Estimated Time**: 17-26 days (4-5 weeks)
**Estimated Lines of Code**: ~2550 (implementation) + ~1000 (tests) = ~3550 total

**Critical Path**: 14-20 days for 80% coverage
**Full Coverage**: 17-26 days for 100% coverage (including optional features)

**End of Plan Document**
