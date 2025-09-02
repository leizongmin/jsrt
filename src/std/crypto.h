#ifndef __JSRT_STD_CRYPTO_H__
#define __JSRT_STD_CRYPTO_H__

#include <quickjs.h>

#include "../runtime.h"

void JSRT_RuntimeSetupStdCrypto(JSRT_Runtime *rt);
const char *JSRT_GetOpenSSLVersion();

#endif