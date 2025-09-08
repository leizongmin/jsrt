// Static OpenSSL build - minimal HTTP fetch implementation
#include "../runtime.h"

// Minimal implementation for static builds
void JSRT_RuntimeSetupHttpFetch(JSRT_Runtime* rt) {
  // Do nothing for now - fetch disabled in static builds
  (void)rt;
}