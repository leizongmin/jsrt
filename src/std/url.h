#ifndef __JSRT_STD_URL_H__
#define __JSRT_STD_URL_H__

// This header provides backward compatibility for the old url.c location
// The URL implementation has been moved to src/url/ directory

#include "../runtime.h"
#include "../url/url.h"

// Re-export the main setup function for compatibility
void JSRT_RuntimeSetupStdURL(JSRT_Runtime* rt);

#endif