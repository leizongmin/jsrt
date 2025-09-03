#include "path.h"

bool JSRT_PathHasSuffix(const char* str, const char* suffix) {
  size_t len = strlen(str);
  size_t slen = strlen(suffix);
  return (len >= slen && !memcmp(str + len - slen, suffix, slen));
}
