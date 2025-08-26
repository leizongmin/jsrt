#include "blob.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"

// Forward declare class ID
JSClassID JSRT_BlobClassID;

// Blob implementation
typedef struct {
  uint8_t *data;
  size_t size;
  char *type;
} JSRT_Blob;

static void JSRT_BlobFinalize(JSRuntime *rt, JSValue val) {
  JSRT_Blob *blob = JS_GetOpaque(val, JSRT_BlobClassID);
  if (blob) {
    free(blob->data);
    free(blob->type);
    free(blob);
  }
}

static JSClassDef JSRT_BlobClass = {
    .class_name = "Blob",
    .finalizer = JSRT_BlobFinalize,
};

static JSValue JSRT_BlobConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  JSRT_Blob *blob = malloc(sizeof(JSRT_Blob));
  blob->data = NULL;
  blob->size = 0;
  blob->type = strdup("");  // Default empty type

  // Handle array parameter
  if (argc > 0 && JS_IsArray(ctx, argv[0])) {
    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    uint32_t length;
    JS_ToUint32(ctx, &length, length_val);
    JS_FreeValue(ctx, length_val);

    size_t total_size = 0;
    uint8_t **parts = NULL;
    size_t *part_sizes = NULL;

    if (length > 0) {
      parts = malloc(length * sizeof(uint8_t *));
      part_sizes = malloc(length * sizeof(size_t));

      // Process each array element
      for (uint32_t i = 0; i < length; i++) {
        JSValue element = JS_GetPropertyUint32(ctx, argv[0], i);

        if (JS_IsString(element)) {
          const char *str = JS_ToCString(ctx, element);
          size_t str_len = strlen(str);
          parts[i] = malloc(str_len);
          memcpy(parts[i], str, str_len);
          part_sizes[i] = str_len;
          total_size += str_len;
          JS_FreeCString(ctx, str);
        } else {
          // For now, skip non-string elements
          parts[i] = NULL;
          part_sizes[i] = 0;
        }

        JS_FreeValue(ctx, element);
      }

      // Combine all parts
      if (total_size > 0) {
        blob->data = malloc(total_size);
        size_t offset = 0;
        for (uint32_t i = 0; i < length; i++) {
          if (parts[i] && part_sizes[i] > 0) {
            memcpy(blob->data + offset, parts[i], part_sizes[i]);
            offset += part_sizes[i];
          }
          free(parts[i]);
        }
        blob->size = total_size;
      }

      free(parts);
      free(part_sizes);
    }
  }

  // Handle options parameter
  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue type_val = JS_GetPropertyStr(ctx, argv[1], "type");
    if (!JS_IsUndefined(type_val) && JS_IsString(type_val)) {
      const char *type_str = JS_ToCString(ctx, type_val);
      free(blob->type);
      blob->type = strdup(type_str);
      JS_FreeCString(ctx, type_str);
    }
    JS_FreeValue(ctx, type_val);
  }

  JSValue obj = JS_NewObjectClass(ctx, JSRT_BlobClassID);
  JS_SetOpaque(obj, blob);
  return obj;
}

static JSValue JSRT_BlobGetSize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Blob *blob = JS_GetOpaque(this_val, JSRT_BlobClassID);
  if (!blob) {
    return JS_EXCEPTION;
  }
  return JS_NewBigUint64(ctx, blob->size);
}

static JSValue JSRT_BlobGetType(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Blob *blob = JS_GetOpaque(this_val, JSRT_BlobClassID);
  if (!blob) {
    return JS_EXCEPTION;
  }
  return JS_NewString(ctx, blob->type);
}

static JSValue JSRT_BlobSlice(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Blob *blob = JS_GetOpaque(this_val, JSRT_BlobClassID);
  if (!blob) {
    return JS_EXCEPTION;
  }

  int64_t start = 0, end = blob->size;

  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    JS_ToInt64(ctx, &start, argv[0]);
  }

  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    JS_ToInt64(ctx, &end, argv[1]);
  }

  // Normalize indices
  if (start < 0) start = blob->size + start;
  if (end < 0) end = blob->size + end;
  if (start < 0) start = 0;
  if (end < 0) end = 0;
  if (start > (int64_t)blob->size) start = blob->size;
  if (end > (int64_t)blob->size) end = blob->size;
  if (start > end) start = end;

  // Create new blob with sliced data
  JSRT_Blob *new_blob = malloc(sizeof(JSRT_Blob));
  new_blob->size = end - start;
  new_blob->type = strdup(blob->type);

  if (new_blob->size > 0) {
    new_blob->data = malloc(new_blob->size);
    memcpy(new_blob->data, blob->data + start, new_blob->size);
  } else {
    new_blob->data = NULL;
  }

  JSValue new_obj = JS_NewObjectClass(ctx, JSRT_BlobClassID);
  JS_SetOpaque(new_obj, new_blob);
  return new_obj;
}

static JSValue JSRT_BlobText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Blob *blob = JS_GetOpaque(this_val, JSRT_BlobClassID);
  if (!blob) {
    return JS_EXCEPTION;
  }

  // Create text content
  char *text = malloc(blob->size + 1);
  if (blob->data && blob->size > 0) {
    memcpy(text, blob->data, blob->size);
  }
  text[blob->size] = '\0';

  JSValue result = JS_NewString(ctx, text);
  free(text);

  // For now, return the string directly instead of a promise
  return result;
}

static JSValue JSRT_BlobArrayBuffer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Blob *blob = JS_GetOpaque(this_val, JSRT_BlobClassID);
  if (!blob) {
    return JS_EXCEPTION;
  }

  // Create ArrayBuffer with the blob data
  JSValue arraybuffer = JS_NewArrayBuffer(ctx, blob->data, blob->size, NULL, NULL, false);

  // For now, return the ArrayBuffer directly instead of a promise
  return arraybuffer;
}

static JSValue JSRT_BlobStream(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Blob *blob = JS_GetOpaque(this_val, JSRT_BlobClassID);
  if (!blob) {
    return JS_EXCEPTION;
  }

  // Create a basic ReadableStream (requires Streams API)
  JSValue readable_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "ReadableStream");
  if (JS_IsUndefined(readable_ctor)) {
    JS_FreeValue(ctx, readable_ctor);
    return JS_ThrowReferenceError(ctx, "ReadableStream is not available");
  }

  JSValue stream = JS_CallConstructor(ctx, readable_ctor, 0, NULL);
  JS_FreeValue(ctx, readable_ctor);

  return stream;
}

void JSRT_RuntimeSetupStdBlob(JSRT_Runtime *rt) {
  JSRT_Debug("JSRT_RuntimeSetupStdBlob: initializing Blob API");

  JSContext *ctx = rt->ctx;

  // Register Blob class
  JS_NewClassID(&JSRT_BlobClassID);
  JS_NewClass(rt->rt, JSRT_BlobClassID, &JSRT_BlobClass);

  JSValue blob_proto = JS_NewObject(ctx);

  // Properties
  JSValue get_size = JS_NewCFunction(ctx, JSRT_BlobGetSize, "get size", 0);
  JSValue get_type = JS_NewCFunction(ctx, JSRT_BlobGetType, "get type", 0);
  JSAtom size_atom = JS_NewAtom(ctx, "size");
  JSAtom type_atom = JS_NewAtom(ctx, "type");
  JS_DefinePropertyGetSet(ctx, blob_proto, size_atom, get_size, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, blob_proto, type_atom, get_type, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, size_atom);
  JS_FreeAtom(ctx, type_atom);

  // Methods
  JS_SetPropertyStr(ctx, blob_proto, "slice", JS_NewCFunction(ctx, JSRT_BlobSlice, "slice", 2));
  JS_SetPropertyStr(ctx, blob_proto, "text", JS_NewCFunction(ctx, JSRT_BlobText, "text", 0));
  JS_SetPropertyStr(ctx, blob_proto, "arrayBuffer", JS_NewCFunction(ctx, JSRT_BlobArrayBuffer, "arrayBuffer", 0));
  JS_SetPropertyStr(ctx, blob_proto, "stream", JS_NewCFunction(ctx, JSRT_BlobStream, "stream", 0));

  JS_SetClassProto(ctx, JSRT_BlobClassID, blob_proto);

  JSValue blob_ctor = JS_NewCFunction2(ctx, JSRT_BlobConstructor, "Blob", 2, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "Blob", blob_ctor);
}