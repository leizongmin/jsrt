#ifndef __JSRT_STD_ENCODING_H__
#define __JSRT_STD_ENCODING_H__

#include "../runtime.h"

void JSRT_RuntimeSetupStdEncoding(JSRT_Runtime* rt);

// Helper function for surrogate handling - converts JSValue string to UTF-8 with surrogate replacement
char* JSRT_StringToUTF8WithSurrogateReplacement(JSContext* ctx, JSValue string_val, size_t* output_len);

#endif