#include "formdata.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"

// Forward declare class ID
JSClassID JSRT_FormDataClassID;

// FormData entry
typedef struct JSRT_FormDataEntry {
  char *name;
  JSValue value;
  char *filename;  // For File entries
  struct JSRT_FormDataEntry *next;
} JSRT_FormDataEntry;

// FormData implementation
typedef struct {
  JSRT_FormDataEntry *entries;
} JSRT_FormData;

static void JSRT_FormDataEntryFree(JSRuntime *rt, JSRT_FormDataEntry *entry) {
  if (entry) {
    free(entry->name);
    free(entry->filename);
    JS_FreeValueRT(rt, entry->value);
    free(entry);
  }
}

static void JSRT_FormDataFinalize(JSRuntime *rt, JSValue val) {
  JSRT_FormData *formdata = JS_GetOpaque(val, JSRT_FormDataClassID);
  if (formdata) {
    JSRT_FormDataEntry *entry = formdata->entries;
    while (entry) {
      JSRT_FormDataEntry *next = entry->next;
      JSRT_FormDataEntryFree(rt, entry);
      entry = next;
    }
    free(formdata);
  }
}

static JSClassDef JSRT_FormDataClass = {
    .class_name = "FormData",
    .finalizer = JSRT_FormDataFinalize,
};

static JSValue JSRT_FormDataConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  JSRT_FormData *formdata = malloc(sizeof(JSRT_FormData));
  formdata->entries = NULL;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_FormDataClassID);
  JS_SetOpaque(obj, formdata);
  return obj;
}

static JSRT_FormDataEntry *JSRT_FormDataFindEntry(JSRT_FormData *formdata, const char *name) {
  JSRT_FormDataEntry *entry = formdata->entries;
  while (entry) {
    if (strcmp(entry->name, name) == 0) {
      return entry;
    }
    entry = entry->next;
  }
  return NULL;
}

static void JSRT_FormDataAddEntry(JSContext *ctx, JSRT_FormData *formdata, const char *name, JSValue value,
                                  const char *filename) {
  JSRT_FormDataEntry *entry = malloc(sizeof(JSRT_FormDataEntry));
  entry->name = strdup(name);
  entry->value = JS_DupValue(ctx, value);
  entry->filename = filename ? strdup(filename) : NULL;
  entry->next = formdata->entries;
  formdata->entries = entry;
}

static void JSRT_FormDataRemoveEntry(JSRuntime *rt, JSRT_FormData *formdata, const char *name) {
  JSRT_FormDataEntry **entry = &formdata->entries;
  while (*entry) {
    if (strcmp((*entry)->name, name) == 0) {
      JSRT_FormDataEntry *to_remove = *entry;
      *entry = (*entry)->next;
      JSRT_FormDataEntryFree(rt, to_remove);
      return;
    }
    entry = &(*entry)->next;
  }
}

static JSValue JSRT_FormDataAppend(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_FormData *formdata = JS_GetOpaque(this_val, JSRT_FormDataClassID);
  if (!formdata) {
    return JS_EXCEPTION;
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "FormData.append requires at least 2 arguments");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  const char *filename = (argc > 2 && JS_IsString(argv[2])) ? JS_ToCString(ctx, argv[2]) : NULL;

  JSRT_FormDataAddEntry(ctx, formdata, name, argv[1], filename);

  JS_FreeCString(ctx, name);
  if (filename) JS_FreeCString(ctx, filename);

  return JS_UNDEFINED;
}

static JSValue JSRT_FormDataDelete(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_FormData *formdata = JS_GetOpaque(this_val, JSRT_FormDataClassID);
  if (!formdata) {
    return JS_EXCEPTION;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "FormData.delete requires 1 argument");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  JSRT_FormDataRemoveEntry(JS_GetRuntime(ctx), formdata, name);
  JS_FreeCString(ctx, name);

  return JS_UNDEFINED;
}

static JSValue JSRT_FormDataGet(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_FormData *formdata = JS_GetOpaque(this_val, JSRT_FormDataClassID);
  if (!formdata) {
    return JS_EXCEPTION;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "FormData.get requires 1 argument");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  JSRT_FormDataEntry *entry = JSRT_FormDataFindEntry(formdata, name);
  JS_FreeCString(ctx, name);

  if (entry) {
    return JS_DupValue(ctx, entry->value);
  }

  return JS_NULL;
}

static JSValue JSRT_FormDataGetAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_FormData *formdata = JS_GetOpaque(this_val, JSRT_FormDataClassID);
  if (!formdata) {
    return JS_EXCEPTION;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "FormData.getAll requires 1 argument");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  JSValue result = JS_NewArray(ctx);
  uint32_t index = 0;

  JSRT_FormDataEntry *entry = formdata->entries;
  while (entry) {
    if (strcmp(entry->name, name) == 0) {
      JS_SetPropertyUint32(ctx, result, index++, JS_DupValue(ctx, entry->value));
    }
    entry = entry->next;
  }

  JS_FreeCString(ctx, name);
  return result;
}

static JSValue JSRT_FormDataHas(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_FormData *formdata = JS_GetOpaque(this_val, JSRT_FormDataClassID);
  if (!formdata) {
    return JS_EXCEPTION;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "FormData.has requires 1 argument");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  JSRT_FormDataEntry *entry = JSRT_FormDataFindEntry(formdata, name);
  JS_FreeCString(ctx, name);

  return JS_NewBool(ctx, entry != NULL);
}

static JSValue JSRT_FormDataSet(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_FormData *formdata = JS_GetOpaque(this_val, JSRT_FormDataClassID);
  if (!formdata) {
    return JS_EXCEPTION;
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "FormData.set requires at least 2 arguments");
  }

  const char *name = JS_ToCString(ctx, argv[0]);

  // Remove existing entry
  JSRT_FormDataRemoveEntry(JS_GetRuntime(ctx), formdata, name);

  // Add new entry
  const char *filename = (argc > 2 && JS_IsString(argv[2])) ? JS_ToCString(ctx, argv[2]) : NULL;
  JSRT_FormDataAddEntry(ctx, formdata, name, argv[1], filename);

  JS_FreeCString(ctx, name);
  if (filename) JS_FreeCString(ctx, filename);

  return JS_UNDEFINED;
}

static JSValue JSRT_FormDataForEach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_FormData *formdata = JS_GetOpaque(this_val, JSRT_FormDataClassID);
  if (!formdata) {
    return JS_EXCEPTION;
  }

  if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "FormData.forEach requires a function argument");
  }

  JSValue callback = argv[0];
  JSValue thisArg = (argc > 1) ? argv[1] : JS_UNDEFINED;

  JSRT_FormDataEntry *entry = formdata->entries;
  while (entry) {
    JSValue args[] = {JS_DupValue(ctx, entry->value), JS_NewString(ctx, entry->name), JS_DupValue(ctx, this_val)};
    JSValue result = JS_Call(ctx, callback, thisArg, 3, args);

    // Free the arguments
    for (int i = 0; i < 3; i++) {
      JS_FreeValue(ctx, args[i]);
    }

    if (JS_IsException(result)) {
      return result;
    }
    JS_FreeValue(ctx, result);

    entry = entry->next;
  }

  return JS_UNDEFINED;
}

void JSRT_RuntimeSetupStdFormData(JSRT_Runtime *rt) {
  JSRT_Debug("JSRT_RuntimeSetupStdFormData: initializing FormData API");

  JSContext *ctx = rt->ctx;

  // Register FormData class
  JS_NewClassID(&JSRT_FormDataClassID);
  JS_NewClass(rt->rt, JSRT_FormDataClassID, &JSRT_FormDataClass);

  JSValue formdata_proto = JS_NewObject(ctx);

  // Methods
  JS_SetPropertyStr(ctx, formdata_proto, "append", JS_NewCFunction(ctx, JSRT_FormDataAppend, "append", 2));
  JS_SetPropertyStr(ctx, formdata_proto, "delete", JS_NewCFunction(ctx, JSRT_FormDataDelete, "delete", 1));
  JS_SetPropertyStr(ctx, formdata_proto, "get", JS_NewCFunction(ctx, JSRT_FormDataGet, "get", 1));
  JS_SetPropertyStr(ctx, formdata_proto, "getAll", JS_NewCFunction(ctx, JSRT_FormDataGetAll, "getAll", 1));
  JS_SetPropertyStr(ctx, formdata_proto, "has", JS_NewCFunction(ctx, JSRT_FormDataHas, "has", 1));
  JS_SetPropertyStr(ctx, formdata_proto, "set", JS_NewCFunction(ctx, JSRT_FormDataSet, "set", 2));
  JS_SetPropertyStr(ctx, formdata_proto, "forEach", JS_NewCFunction(ctx, JSRT_FormDataForEach, "forEach", 1));

  JS_SetClassProto(ctx, JSRT_FormDataClassID, formdata_proto);

  JSValue formdata_ctor = JS_NewCFunction2(ctx, JSRT_FormDataConstructor, "FormData", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "FormData", formdata_ctor);
}