// Static OpenSSL build - HTTP fetch implementation using static OpenSSL
#include "../runtime.h"
#include "../std/fetch.h"

// Use the standard fetch implementation for static builds with OpenSSL
void JSRT_RuntimeSetupHttpFetch(JSRT_Runtime* rt) {
  // Call the standard fetch setup function - it works with static OpenSSL too
  JSRT_RuntimeSetupStdFetch(rt);
}