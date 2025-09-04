#include "json.h"

#include <string.h>
#include <stdlib.h>

JSValue JSRT_ParseJSON(JSContext* ctx, const char* json_str) {
  if (!json_str) {
    return JS_NULL;
  }
  
  return JS_ParseJSON(ctx, json_str, strlen(json_str), "<package.json>");
}

const char* JSRT_GetPackageMain(JSContext* ctx, JSValue package_json) {
  if (JS_IsNull(package_json) || JS_IsUndefined(package_json)) {
    return NULL;
  }
  
  JSValue main_val = JS_GetPropertyStr(ctx, package_json, "main");
  if (JS_IsUndefined(main_val) || JS_IsNull(main_val)) {
    JS_FreeValue(ctx, main_val);
    return "index.js";  // Default main entry point
  }
  
  const char* main_str = JS_ToCString(ctx, main_val);
  JS_FreeValue(ctx, main_val);
  return main_str;
}

const char* JSRT_GetPackageModule(JSContext* ctx, JSValue package_json) {
  if (JS_IsNull(package_json) || JS_IsUndefined(package_json)) {
    return NULL;
  }
  
  JSValue module_val = JS_GetPropertyStr(ctx, package_json, "module");
  if (JS_IsUndefined(module_val) || JS_IsNull(module_val)) {
    JS_FreeValue(ctx, module_val);
    return NULL;
  }
  
  const char* module_str = JS_ToCString(ctx, module_val);
  JS_FreeValue(ctx, module_val);
  return module_str;
}

const char* JSRT_GetPackageName(JSContext* ctx, JSValue package_json) {
  if (JS_IsNull(package_json) || JS_IsUndefined(package_json)) {
    return NULL;
  }
  
  JSValue name_val = JS_GetPropertyStr(ctx, package_json, "name");
  if (JS_IsUndefined(name_val) || JS_IsNull(name_val)) {
    JS_FreeValue(ctx, name_val);
    return NULL;
  }
  
  const char* name_str = JS_ToCString(ctx, name_val);
  JS_FreeValue(ctx, name_val);
  return name_str;
}