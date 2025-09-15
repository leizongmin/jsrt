#include <stdlib.h>
#include <string.h>
#include "process.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Process environment variables getter
JSValue js_process_get_env(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue env_obj = JS_NewObject(ctx);

  if (JS_IsException(env_obj)) {
    return JS_EXCEPTION;
  }

#ifdef _WIN32
  // Windows: Use GetEnvironmentStrings()
  LPCH env_strings = GetEnvironmentStrings();
  if (env_strings != NULL) {
    LPCH current = env_strings;
    while (*current) {
      char* env_var = current;
      char* equals = strchr(env_var, '=');
      if (equals != NULL) {
        // Create a copy to safely modify
        size_t var_len = strlen(env_var);
        char* var_copy = malloc(var_len + 1);
        if (var_copy) {
          strncpy(var_copy, env_var, var_len);
          var_copy[var_len] = '\0';
          char* equals_copy = var_copy + (equals - env_var);
          *equals_copy = '\0';  // Split key and value
          char* key = var_copy;
          char* value = equals_copy + 1;

          JSValue key_val = JS_NewString(ctx, key);
          JSValue value_val = JS_NewString(ctx, value);

          if (!JS_IsException(key_val) && !JS_IsException(value_val)) {
            JS_SetPropertyStr(ctx, env_obj, key, value_val);
          } else {
            JS_FreeValue(ctx, key_val);
            JS_FreeValue(ctx, value_val);
          }

          free(var_copy);
        }
      }
      current += strlen(current) + 1;
    }
    FreeEnvironmentStrings(env_strings);
  }
#else
  // Unix/Linux: Use extern char **environ
  extern char** environ;
  if (environ != NULL) {
    for (char** env = environ; *env != NULL; env++) {
      char* env_var = strdup(*env);
      if (env_var != NULL) {
        char* equals = strchr(env_var, '=');
        if (equals != NULL) {
          *equals = '\0';  // Split key and value
          char* key = env_var;
          char* value = equals + 1;

          JSValue key_val = JS_NewString(ctx, key);
          JSValue value_val = JS_NewString(ctx, value);

          if (!JS_IsException(key_val) && !JS_IsException(value_val)) {
            JS_SetPropertyStr(ctx, env_obj, key, value_val);
          } else {
            JS_FreeValue(ctx, key_val);
            JS_FreeValue(ctx, value_val);
          }
        }
        free(env_var);
      }
    }
  }
#endif

  return env_obj;
}

void jsrt_process_init_env(void) {
  // Environment initialization if needed
  // Currently no initialization required
}