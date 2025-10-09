// This file has been replaced by the modular implementation in src/node/dns/
// The actual implementation is now split across multiple files for better maintainability:
// - dns/dns_internal.h      - Request structures and internal declarations
// - dns/dns_errors.c        - Error handling and error code mapping
// - dns/dns_callbacks.c     - libuv callback handlers
// - dns/dns_lookup.c        - dns.lookup() implementation
// - dns/dns_lookupservice.c - dns.lookupService() implementation
// - dns/dns_module.c        - Module exports and initialization

// Include the module initialization from the new location
#include "dns/dns_module.c"
