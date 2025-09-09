#ifndef __JSRT_STD_CRYPTO_H__
#define __JSRT_STD_CRYPTO_H__

#include <quickjs.h>

#include "../runtime.h"

void JSRT_RuntimeSetupStdCrypto(JSRT_Runtime* rt);
const char* JSRT_GetOpenSSLVersion();

// Unified crypto functions that work for both static and dynamic OpenSSL
JSValue jsrt_crypto_getRandomValues(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue jsrt_crypto_randomUUID(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Expose openssl_handle for other crypto modules (only for dynamic loading)
#ifndef JSRT_STATIC_OPENSSL
#ifdef _WIN32
#include <windows.h>
extern HMODULE openssl_handle;
#else
extern void* openssl_handle;
#endif
#endif

#endif