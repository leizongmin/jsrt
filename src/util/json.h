#ifndef __JSRT_UTIL_JSON_H__
#define __JSRT_UTIL_JSON_H__

#include <quickjs.h>

// Simple JSON parser for package.json files
JSValue JSRT_ParseJSON(JSContext* ctx, const char* json_str);

// Extract specific fields from package.json object
const char* JSRT_GetPackageMain(JSContext* ctx, JSValue package_json);
const char* JSRT_GetPackageModule(JSContext* ctx, JSValue package_json);
const char* JSRT_GetPackageName(JSContext* ctx, JSValue package_json);
const char* JSRT_GetPackageType(JSContext* ctx, JSValue package_json);

#endif