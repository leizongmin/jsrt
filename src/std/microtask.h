#ifndef JSRT_STD_MICROTASK_H
#define JSRT_STD_MICROTASK_H

#include "../runtime.h"

/**
 * Initialize queueMicrotask global function
 * Implements WinterCG Minimum Common API requirement
 */
void JSRT_RuntimeSetupStdMicrotask(JSRT_Runtime* rt);

#endif  // JSRT_STD_MICROTASK_H