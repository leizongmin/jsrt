#ifndef __JSRT_RUNTIME_H__
#define __JSRT_RUNTIME_H__

#include <quickjs.h>
#include <stdbool.h>
#include <uv.h>

#include "util/debug.h"
#include "util/list.h"

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

typedef struct {
  uint8_t* data;
  size_t size;
  char* error;
} JSRT_CompileResult;

JSRT_CompileResult JSRT_RuntimeCompileToBytecode(JSRT_Runtime* rt, const char* filename, const char* code,
                                                 size_t length);
void JSRT_CompileResultFree(JSRT_CompileResult* result);

#endif
