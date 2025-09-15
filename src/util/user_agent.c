#include "user_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to get version from process.versions
static char* get_version_from_process(JSContext* ctx, const char* key) {
  if (!ctx) {
    return strdup("unknown");
  }

  JSValue global = JS_GetGlobalObject(ctx);
  JSValue process = JS_GetPropertyStr(ctx, global, "process");
  JS_FreeValue(ctx, global);

  if (JS_IsUndefined(process)) {
    JS_FreeValue(ctx, process);
    return strdup("unknown");
  }

  JSValue versions = JS_GetPropertyStr(ctx, process, "versions");
  JS_FreeValue(ctx, process);

  if (JS_IsUndefined(versions)) {
    JS_FreeValue(ctx, versions);
    return strdup("unknown");
  }

  JSValue version_val = JS_GetPropertyStr(ctx, versions, key);
  JS_FreeValue(ctx, versions);

  if (JS_IsUndefined(version_val)) {
    JS_FreeValue(ctx, version_val);
    return strdup("unknown");
  }

  const char* version_str = JS_ToCString(ctx, version_val);
  JS_FreeValue(ctx, version_val);

  if (!version_str) {
    return strdup("unknown");
  }

  char* result = strdup(version_str);
  JS_FreeCString(ctx, version_str);
  return result;
}

char* jsrt_generate_user_agent(JSContext* ctx) {
  // Get dynamic versions from process.versions
  char* jsrt_version = get_version_from_process(ctx, "jsrt");

  // Use fallback if version is unknown
  if (strcmp(jsrt_version, "unknown") == 0) {
    free(jsrt_version);
    jsrt_version = strdup("1.0");
  }

  // Build user agent string: jsrt/version
  size_t ua_len = strlen("jsrt/") + strlen(jsrt_version) + 1;
  char* user_agent = malloc(ua_len);
  if (!user_agent) {
    free(jsrt_version);
    return strdup("jsrt/1.0");  // Emergency fallback
  }

  snprintf(user_agent, ua_len, "jsrt/%s", jsrt_version);
  free(jsrt_version);

  return user_agent;
}

const char* jsrt_get_static_user_agent(void) {
  // Static fallback when no JavaScript context is available
  return "jsrt/1.0";
}