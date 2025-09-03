#ifndef __JSRT_STD_EVENT_H__
#define __JSRT_STD_EVENT_H__

#include "../runtime.h"

// Export class IDs for use by other modules (like AbortController)
extern JSClassID JSRT_EventClassID;
extern JSClassID JSRT_EventTargetClassID;

void JSRT_RuntimeSetupStdEvent(JSRT_Runtime* rt);

#endif