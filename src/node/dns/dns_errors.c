#include "dns_internal.h"

// Map libuv error codes to Node.js DNS error codes
const char* get_dns_error_code(int status) {
  switch (status) {
    case UV_EAI_ADDRFAMILY:
      return "EADDRFAMILY";
    case UV_EAI_AGAIN:
      return "ENOTFOUND";
    case UV_EAI_BADFLAGS:
      return "EBADFLAGS";
    case UV_EAI_FAIL:
      return "ENOTFOUND";
    case UV_EAI_FAMILY:
      return "EADDRFAMILY";
    case UV_EAI_MEMORY:
      return "ENOMEM";
    case UV_EAI_NODATA:
      return "ENODATA";
    case UV_EAI_NONAME:
      return "ENOTFOUND";
    case UV_EAI_SERVICE:
      return "ESERVICE";
    case UV_EAI_SOCKTYPE:
      return "ESOCKTYPE";
    default:
      return "EUNKNOWN";
  }
}

// Create a DNS error object
JSValue create_dns_error(JSContext* ctx, int status, const char* syscall, const char* hostname) {
  JSValue error = JS_NewError(ctx);

  // Map libuv error to Node.js code
  const char* code = get_dns_error_code(status);
  const char* message = uv_strerror(status);

  JS_DefinePropertyValueStr(ctx, error, "code", JS_NewString(ctx, code), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValueStr(ctx, error, "syscall", JS_NewString(ctx, syscall), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  if (hostname) {
    JS_DefinePropertyValueStr(ctx, error, "hostname", JS_NewString(ctx, hostname),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  }
  JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, message), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValueStr(ctx, error, "errno", JS_NewInt32(ctx, status), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  return error;
}
