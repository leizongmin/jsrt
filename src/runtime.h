#ifndef __JSRT_RUNTIME_H__
#define __JSRT_RUNTIME_H__

#include <quickjs.h>
#include <stdbool.h>
#include <uv.h>

#include "util/debug.h"
#include "util/list.h"

// Forward declaration for module loader
typedef struct JSRT_ModuleLoader JSRT_ModuleLoader;

// Forward declaration for source map cache
typedef struct JSRT_SourceMapCache JSRT_SourceMapCache;

// Forward declaration for compile cache
typedef struct JSRT_CompileCacheConfig JSRT_CompileCacheConfig;

// Forward declaration for hook registry
typedef struct JSRTHookRegistry JSRTHookRegistry;

typedef struct {
  JSRuntime* rt;
  JSContext* ctx;

  JSValue global;
  JSValue* dispose_values;
  size_t dispose_values_length;
  size_t dispose_values_capacity;

  JSValue* exception_values;
  size_t exception_values_length;
  size_t exception_values_capacity;

  uv_loop_t* uv_loop;
  bool compact_node_mode;

  // Module loader (new unified system)
  JSRT_ModuleLoader* module_loader;

  // Source map cache (for node:module source map support)
  JSRT_SourceMapCache* source_map_cache;

  // Compilation cache (for node:module bytecode cache)
  JSRT_CompileCacheConfig* compile_cache;

  // Module hook registry (for module.registerHooks())
  JSRTHookRegistry* hook_registry;
} JSRT_Runtime;

JSRT_Runtime* JSRT_RuntimeNew();

void JSRT_RuntimeFree(JSRT_Runtime* rt);

typedef struct {
  JSRT_Runtime* rt;
  bool is_error;
  JSValue value;
  char* error;
  size_t error_length;
} JSRT_EvalResult;

JSRT_EvalResult JSRT_EvalResultDefault();

JSRT_EvalResult JSRT_RuntimeEval(JSRT_Runtime* rt, const char* filename, const char* code, size_t length);

char* JSRT_RuntimeGetExceptionString(JSRT_Runtime* rt, JSValue e);

void JSRT_EvalResultFree(JSRT_EvalResult* result);

JSRT_EvalResult JSRT_RuntimeAwaitEvalResult(JSRT_Runtime* rt, JSRT_EvalResult* result);
bool JSRT_RuntimeRun(JSRT_Runtime* rt);
bool JSRT_RuntimeRunTicket(JSRT_Runtime* rt);

void JSRT_RuntimeAddDisposeValue(JSRT_Runtime* rt, JSValue value);
void JSRT_RuntimeFreeDisposeValues(JSRT_Runtime* rt);

#define JSRT_RuntimeFreeValue(runtime, value)    \
  {                                              \
    /* JSRT_Debug("free value %p", &(value)); */ \
    JS_FreeValueRT((runtime)->rt, value);        \
  }

void JSRT_RuntimeAddExceptionValue(JSRT_Runtime* rt, JSValue e);
void JSRT_RuntimeFreeExceptionValues(JSRT_Runtime* rt);
bool JSRT_RuntimeProcessUnhandledExceptionValues(JSRT_Runtime* rt);

void JSRT_RuntimeSetCompactNodeMode(JSRT_Runtime* rt, bool enabled);
void JSRT_RuntimeSetCompileCacheAllowed(JSRT_Runtime* rt, bool allowed);
void JSRT_RuntimeGetCompileCacheStats(JSRT_Runtime* rt, uint64_t* hits, uint64_t* misses, uint64_t* writes,
                                      uint64_t* errors, uint64_t* evictions, size_t* current_size, size_t* size_limit);
void JSRT_RuntimeSetModuleHookTrace(JSRT_Runtime* rt, bool enabled);

typedef struct {
  uint8_t* data;
  size_t size;
  char* error;
} JSRT_CompileResult;

JSRT_CompileResult JSRT_RuntimeCompileToBytecode(JSRT_Runtime* rt, const char* filename, const char* code,
                                                 size_t length);
void JSRT_CompileResultFree(JSRT_CompileResult* result);

#endif
