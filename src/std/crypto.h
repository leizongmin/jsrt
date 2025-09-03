#ifndef __JSRT_STD_CRYPTO_H__
#define __JSRT_STD_CRYPTO_H__

#include <quickjs.h>

#include "../runtime.h"

void JSRT_RuntimeSetupStdCrypto(JSRT_Runtime* rt);
const char* JSRT_GetOpenSSLVersion();

// Expose openssl_handle for other crypto modules
#ifdef _WIN32
#include <windows.h>
extern HMODULE openssl_handle;
#else
extern void* openssl_handle;
#endif

#endif