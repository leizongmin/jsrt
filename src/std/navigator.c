#include "navigator.h"
#include <quickjs.h>
#include <stdlib.h>
#include <string.h>

// Navigator implementation for WinterTC compliance
// Per WinterTC spec: globalThis.navigator.userAgent must be provided
// and conform to RFC 7231 structure

static JSClassID JSRT_NavigatorClassID;

typedef struct {
    // Empty structure - userAgent is generated dynamically
    int _placeholder;
} JSRT_Navigator;

// Helper function to get version from process.versions
static char* get_version_from_process(JSContext *ctx, const char* key) {
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

// userAgent getter
static JSValue JSRT_NavigatorGetUserAgent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRT_Navigator *nav = JS_GetOpaque2(ctx, this_val, JSRT_NavigatorClassID);
    if (!nav) return JS_EXCEPTION;
    
    // Get dynamic versions from process.versions
    char* jsrt_version = get_version_from_process(ctx, "jsrt");
    char* quickjs_version = get_version_from_process(ctx, "quickjs");
    
    // If quickjs version is not in process.versions, use a fallback
    if (strcmp(quickjs_version, "unknown") == 0) {
        free(quickjs_version);
        quickjs_version = strdup("2024-01-13");  // QuickJS release date as version
    }
    
    // Build user agent string with actual versions
    // Format: jsrt/version (JavaScript Runtime) QuickJS/version
    size_t ua_len = strlen("jsrt/") + strlen(jsrt_version) + 
                   strlen(" (JavaScript Runtime) QuickJS/") + strlen(quickjs_version) + 1;
    char* user_agent = malloc(ua_len);
    if (!user_agent) {
        free(jsrt_version);
        free(quickjs_version);
        return JS_EXCEPTION;
    }
    
    snprintf(user_agent, ua_len, "jsrt/%s (JavaScript Runtime) QuickJS/%s", 
             jsrt_version, quickjs_version);
    
    JSValue result = JS_NewString(ctx, user_agent);
    
    free(jsrt_version);
    free(quickjs_version);
    free(user_agent);
    
    return result;
}

// Navigator finalizer
static void JSRT_NavigatorFinalizer(JSRuntime *rt, JSValue obj) {
    JSRT_Navigator *nav = JS_GetOpaque(obj, JSRT_NavigatorClassID);
    if (nav) {
        free(nav);
    }
}

// Navigator class definition
static JSClassDef JSRT_NavigatorClass = {
    "Navigator",
    .finalizer = JSRT_NavigatorFinalizer,
};

JSValue jsrt_init_module_navigator(JSContext *ctx) {
    // Register Navigator class
    JS_NewClassID(&JSRT_NavigatorClassID);
    JS_NewClass(JS_GetRuntime(ctx), JSRT_NavigatorClassID, &JSRT_NavigatorClass);

    // Create Navigator instance
    JSRT_Navigator *nav = malloc(sizeof(JSRT_Navigator));
    if (!nav) {
        return JS_EXCEPTION;
    }

    // Navigator structure is minimal - userAgent is generated dynamically
    nav->_placeholder = 0;

    JSValue navigator_obj = JS_NewObjectClass(ctx, JSRT_NavigatorClassID);
    JS_SetOpaque(navigator_obj, nav);

    // Add userAgent property
    JS_DefinePropertyGetSet(ctx, navigator_obj,
                           JS_NewAtom(ctx, "userAgent"),
                           JS_NewCFunction(ctx, JSRT_NavigatorGetUserAgent, "get userAgent", 0),
                           JS_UNDEFINED,
                           JS_PROP_CONFIGURABLE);

    return navigator_obj;
}

// Runtime setup function
void JSRT_RuntimeSetupNavigator(JSRT_Runtime *rt) {
    JSValue navigator = jsrt_init_module_navigator(rt->ctx);
    if (JS_IsException(navigator)) {
        JS_ThrowInternalError(rt->ctx, "Failed to initialize navigator");
        return;
    }
    
    // Set as global navigator property
    JS_SetPropertyStr(rt->ctx, rt->global, "navigator", navigator);
}