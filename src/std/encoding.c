#include "encoding.h"

#include <ctype.h>
#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/dbuf.h"
#include "../util/debug.h"

// Encoding labels table for WPT compatibility
// Based on https://encoding.spec.whatwg.org/encodings.json
static const struct {
  const char* name;
  const char* canonical;
} encodings_table[] = {
    // UTF-8 (The Encoding)
    {"unicode-1-1-utf-8", "utf-8"},
    {"unicode11utf8", "utf-8"},
    {"unicode20utf8", "utf-8"},
    {"utf-8", "utf-8"},
    {"utf8", "utf-8"},
    {"x-unicode20utf8", "utf-8"},

    // Legacy single-byte encodings
    {"866", "ibm866"},
    {"cp866", "ibm866"},
    {"csibm866", "ibm866"},
    {"ibm866", "ibm866"},

    {"csisolatin2", "iso-8859-2"},
    {"iso-8859-2", "iso-8859-2"},
    {"iso-ir-101", "iso-8859-2"},
    {"iso8859-2", "iso-8859-2"},
    {"iso88592", "iso-8859-2"},
    {"iso_8859-2", "iso-8859-2"},
    {"iso_8859-2:1987", "iso-8859-2"},
    {"l2", "iso-8859-2"},
    {"latin2", "iso-8859-2"},

    {"ansi_x3.4-1968", "windows-1252"},
    {"ascii", "windows-1252"},
    {"cp1252", "windows-1252"},
    {"cp819", "windows-1252"},
    {"csisolatin1", "windows-1252"},
    {"ibm819", "windows-1252"},
    {"iso-8859-1", "windows-1252"},
    {"iso-ir-100", "windows-1252"},
    {"iso8859-1", "windows-1252"},
    {"iso88591", "windows-1252"},
    {"iso_8859-1", "windows-1252"},
    {"iso_8859-1:1987", "windows-1252"},
    {"l1", "windows-1252"},
    {"latin1", "windows-1252"},
    {"us-ascii", "windows-1252"},
    {"windows-1252", "windows-1252"},
    {"x-cp1252", "windows-1252"},

    // Additional ISO-8859 encodings
    {"csisolatin3", "iso-8859-3"},
    {"iso-8859-3", "iso-8859-3"},
    {"iso-ir-109", "iso-8859-3"},
    {"iso8859-3", "iso-8859-3"},
    {"iso88593", "iso-8859-3"},
    {"iso_8859-3", "iso-8859-3"},
    {"iso_8859-3:1988", "iso-8859-3"},
    {"l3", "iso-8859-3"},
    {"latin3", "iso-8859-3"},

    {"csisolatin4", "iso-8859-4"},
    {"iso-8859-4", "iso-8859-4"},
    {"iso-ir-110", "iso-8859-4"},
    {"iso8859-4", "iso-8859-4"},
    {"iso88594", "iso-8859-4"},
    {"iso_8859-4", "iso-8859-4"},
    {"iso_8859-4:1988", "iso-8859-4"},
    {"l4", "iso-8859-4"},
    {"latin4", "iso-8859-4"},

    // Legacy UTF-16 encodings
    {"csunicode", "utf-16le"},
    {"iso-10646-ucs-2", "utf-16le"},
    {"ucs-2", "utf-16le"},
    {"unicode", "utf-16le"},
    {"unicodefeff", "utf-16le"},
    {"utf-16", "utf-16le"},
    {"utf-16le", "utf-16le"},

    {"unicodefffe", "utf-16be"},
    {"utf-16be", "utf-16be"},

    // Additional ISO-8859 encodings
    {"csisolatincyrillic", "iso-8859-5"},
    {"cyrillic", "iso-8859-5"},
    {"iso-8859-5", "iso-8859-5"},
    {"iso-ir-144", "iso-8859-5"},
    {"iso8859-5", "iso-8859-5"},
    {"iso88595", "iso-8859-5"},
    {"iso_8859-5", "iso-8859-5"},
    {"iso_8859-5:1988", "iso-8859-5"},

    // ISO-8859-6 (Arabic)
    {"arabic", "iso-8859-6"},
    {"asmo-708", "iso-8859-6"},
    {"csiso88596e", "iso-8859-6"},
    {"csiso88596i", "iso-8859-6"},
    {"csisolatinarabic", "iso-8859-6"},
    {"ecma-114", "iso-8859-6"},
    {"iso-8859-6", "iso-8859-6"},
    {"iso-8859-6-e", "iso-8859-6"},
    {"iso-8859-6-i", "iso-8859-6"},
    {"iso-ir-127", "iso-8859-6"},
    {"iso8859-6", "iso-8859-6"},
    {"iso88596", "iso-8859-6"},
    {"iso_8859-6", "iso-8859-6"},
    {"iso_8859-6:1987", "iso-8859-6"},

    // ISO-8859-7 (Greek)
    {"csisolatingreek", "iso-8859-7"},
    {"ecma-118", "iso-8859-7"},
    {"elot_928", "iso-8859-7"},
    {"greek", "iso-8859-7"},
    {"greek8", "iso-8859-7"},
    {"iso-8859-7", "iso-8859-7"},
    {"iso-ir-126", "iso-8859-7"},
    {"iso8859-7", "iso-8859-7"},
    {"iso88597", "iso-8859-7"},
    {"iso_8859-7", "iso-8859-7"},
    {"iso_8859-7:1987", "iso-8859-7"},
    {"sun_eu_greek", "iso-8859-7"},

    // ISO-8859-8 (Hebrew)
    {"csiso88598e", "iso-8859-8"},
    {"csiso88598i", "iso-8859-8-i"},
    {"csisolatinhebrew", "iso-8859-8"},
    {"hebrew", "iso-8859-8"},
    {"iso-8859-8", "iso-8859-8"},
    {"iso-8859-8-e", "iso-8859-8"},
    {"iso-8859-8-i", "iso-8859-8-i"},
    {"iso-ir-138", "iso-8859-8"},
    {"iso8859-8", "iso-8859-8"},
    {"iso88598", "iso-8859-8"},
    {"iso_8859-8", "iso-8859-8"},
    {"iso_8859-8:1988", "iso-8859-8"},
    {"logical", "iso-8859-8-i"},
    {"visual", "iso-8859-8"},

    // ISO-8859-10 (Latin-6)
    {"csisolatin6", "iso-8859-10"},
    {"iso-8859-10", "iso-8859-10"},
    {"iso-ir-157", "iso-8859-10"},
    {"iso8859-10", "iso-8859-10"},
    {"iso885910", "iso-8859-10"},
    {"l6", "iso-8859-10"},
    {"latin6", "iso-8859-10"},

    // ISO-8859-13 (Latin-7)
    {"iso-8859-13", "iso-8859-13"},
    {"iso-ir-179", "iso-8859-13"},
    {"iso8859-13", "iso-8859-13"},
    {"iso885913", "iso-8859-13"},
    {"l7", "iso-8859-13"},
    {"latin7", "iso-8859-13"},

    // ISO-8859-14 (Latin-8)
    {"iso-8859-14", "iso-8859-14"},
    {"iso-ir-199", "iso-8859-14"},
    {"iso8859-14", "iso-8859-14"},
    {"iso885914", "iso-8859-14"},
    {"l8", "iso-8859-14"},
    {"latin8", "iso-8859-14"},

    // ISO-8859-15 (Latin-9)
    {"csisolatin9", "iso-8859-15"},
    {"iso-8859-15", "iso-8859-15"},
    {"iso-ir-203", "iso-8859-15"},
    {"iso8859-15", "iso-8859-15"},
    {"iso885915", "iso-8859-15"},
    {"iso_8859-15", "iso-8859-15"},
    {"l9", "iso-8859-15"},
    {"latin9", "iso-8859-15"},

    // ISO-8859-16 (Latin-10)
    {"iso-8859-16", "iso-8859-16"},
    {"iso-ir-226", "iso-8859-16"},
    {"iso8859-16", "iso-8859-16"},
    {"iso885916", "iso-8859-16"},
    {"l10", "iso-8859-16"},
    {"latin10", "iso-8859-16"},

    // KOI8-R (Russian)
    {"cskoi8r", "koi8-r"},
    {"koi", "koi8-r"},
    {"koi8", "koi8-r"},
    {"koi8-r", "koi8-r"},
    {"koi8_r", "koi8-r"},

    // KOI8-U (Ukrainian)
    {"koi8-ru", "koi8-u"},
    {"koi8-u", "koi8-u"},

    // Macintosh (Mac OS Roman)
    {"csmacintosh", "macintosh"},
    {"mac", "macintosh"},
    {"macintosh", "macintosh"},
    {"macroman", "macintosh"},
    {"x-mac-roman", "macintosh"},

    // Mac Cyrillic
    {"x-mac-cyrillic", "x-mac-cyrillic"},
    {"x-mac-ukrainian", "x-mac-cyrillic"},

    // Windows-874 (Thai)
    {"dos-874", "windows-874"},
    {"iso-8859-11", "windows-874"},
    {"iso8859-11", "windows-874"},
    {"iso885911", "windows-874"},
    {"tis-620", "windows-874"},
    {"windows-874", "windows-874"},

    // Windows-1250 (Central European)
    {"cp1250", "windows-1250"},
    {"windows-1250", "windows-1250"},
    {"x-cp1250", "windows-1250"},

    // Windows-1251 (Cyrillic)
    {"cp1251", "windows-1251"},
    {"windows-1251", "windows-1251"},
    {"x-cp1251", "windows-1251"},

    // Windows-1252 (Western European)
    {"ansi_x3.4-1968", "windows-1252"},
    {"ascii", "windows-1252"},
    {"cp1252", "windows-1252"},
    {"cp819", "windows-1252"},
    {"csisolatin1", "windows-1252"},
    {"ibm819", "windows-1252"},
    {"iso-8859-1", "windows-1252"},
    {"iso-ir-100", "windows-1252"},
    {"iso8859-1", "windows-1252"},
    {"iso88591", "windows-1252"},
    {"iso_8859-1", "windows-1252"},
    {"iso_8859-1:1987", "windows-1252"},
    {"l1", "windows-1252"},
    {"latin1", "windows-1252"},
    {"us-ascii", "windows-1252"},
    {"windows-1252", "windows-1252"},
    {"x-cp1252", "windows-1252"},

    // Windows-1253 (Greek)
    {"cp1253", "windows-1253"},
    {"windows-1253", "windows-1253"},
    {"x-cp1253", "windows-1253"},

    // Windows-1254 (Turkish)
    {"cp1254", "windows-1254"},
    {"csisolatin5", "windows-1254"},
    {"iso-8859-9", "windows-1254"},
    {"iso-ir-148", "windows-1254"},
    {"iso8859-9", "windows-1254"},
    {"iso88599", "windows-1254"},
    {"iso_8859-9", "windows-1254"},
    {"iso_8859-9:1989", "windows-1254"},
    {"l5", "windows-1254"},
    {"latin5", "windows-1254"},
    {"windows-1254", "windows-1254"},
    {"x-cp1254", "windows-1254"},

    // Windows-1255 (Hebrew)
    {"cp1255", "windows-1255"},
    {"windows-1255", "windows-1255"},
    {"x-cp1255", "windows-1255"},

    // Windows-1256 (Arabic)
    {"cp1256", "windows-1256"},
    {"windows-1256", "windows-1256"},
    {"x-cp1256", "windows-1256"},

    // Windows-1257 (Baltic)
    {"cp1257", "windows-1257"},
    {"windows-1257", "windows-1257"},
    {"x-cp1257", "windows-1257"},

    // Windows-1258 (Vietnamese)
    {"cp1258", "windows-1258"},
    {"windows-1258", "windows-1258"},
    {"x-cp1258", "windows-1258"},

    // Legacy multi-byte Chinese (simplified) encodings - GBK
    {"chinese", "GBK"},
    {"csgb2312", "GBK"},
    {"csiso58gb231280", "GBK"},
    {"gb2312", "GBK"},
    {"gb_2312", "GBK"},
    {"gb_2312-80", "GBK"},
    {"gbk", "GBK"},
    {"iso-ir-58", "GBK"},
    {"x-gbk", "GBK"},

    // GB18030
    {"gb18030", "gb18030"},

    // Legacy multi-byte Chinese (traditional) encodings - Big5
    {"big5", "Big5"},
    {"big5-hkscs", "Big5"},
    {"cn-big5", "Big5"},
    {"csbig5", "Big5"},
    {"x-x-big5", "Big5"},

    // Legacy multi-byte Japanese encodings - EUC-JP
    {"cseucpkdfmtjapanese", "EUC-JP"},
    {"euc-jp", "EUC-JP"},
    {"x-euc-jp", "EUC-JP"},

    // ISO-2022-JP
    {"csiso2022jp", "ISO-2022-JP"},
    {"iso-2022-jp", "ISO-2022-JP"},

    // Shift_JIS
    {"csshiftjis", "Shift_JIS"},
    {"ms932", "Shift_JIS"},
    {"ms_kanji", "Shift_JIS"},
    {"shift-jis", "Shift_JIS"},
    {"shift_jis", "Shift_JIS"},
    {"sjis", "Shift_JIS"},
    {"windows-31j", "Shift_JIS"},
    {"x-sjis", "Shift_JIS"},

    // Legacy multi-byte Korean encodings - EUC-KR
    {"cseuckr", "EUC-KR"},
    {"csksc56011987", "EUC-KR"},
    {"euc-kr", "EUC-KR"},
    {"iso-ir-149", "EUC-KR"},
    {"korean", "EUC-KR"},
    {"ks_c_5601-1987", "EUC-KR"},
    {"ks_c_5601-1989", "EUC-KR"},
    {"ksc5601", "EUC-KR"},
    {"ksc_5601", "EUC-KR"},
    {"windows-949", "EUC-KR"},

    // Legacy miscellaneous encodings - Replacement encodings
    {"csiso2022kr", "replacement"},
    {"hz-gb-2312", "replacement"},
    {"iso-2022-cn", "replacement"},
    {"iso-2022-cn-ext", "replacement"},
    {"iso-2022-kr", "replacement"},
    {"replacement", "replacement"},

    // UTF-16BE
    {"unicodefffe", "UTF-16BE"},
    {"utf-16be", "UTF-16BE"},

    // UTF-16LE
    {"csunicode", "UTF-16LE"},
    {"iso-10646-ucs-2", "UTF-16LE"},
    {"ucs-2", "UTF-16LE"},
    {"unicode", "UTF-16LE"},
    {"unicodefeff", "UTF-16LE"},
    {"utf-16", "UTF-16LE"},
    {"utf-16le", "UTF-16LE"},

    // x-user-defined
    {"x-user-defined", "x-user-defined"},

    // Note: For server runtimes like jsrt, we only actually support UTF-8 decoding
    // but we accept the labels and normalize them to their canonical names
    // The actual decoding will fall back to UTF-8 replacement behavior for non-UTF-8 encodings
    // UTF-16LE/BE support is added for validation purposes (fatal mode error checking)
    {NULL, NULL}  // Sentinel
};

// Helper function to normalize encoding labels
static char* normalize_encoding_label(const char* label) {
  if (!label)
    return NULL;

  size_t len = strlen(label);
  char* normalized = malloc(len + 1);
  if (!normalized)
    return NULL;

  // Remove leading/trailing whitespace and convert to lowercase
  const char* start = label;
  const char* end = label + len - 1;

  // Skip leading whitespace
  while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\f' || *start == '\r')) {
    start++;
  }

  // Skip trailing whitespace
  while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\f' || *end == '\r')) {
    end--;
  }

  // Copy and convert to lowercase
  size_t normalized_len = end - start + 1;
  for (size_t i = 0; i < normalized_len; i++) {
    normalized[i] = tolower(start[i]);
  }
  normalized[normalized_len] = '\0';

  return normalized;
}

// Get canonical encoding name from label
static const char* get_canonical_encoding(const char* label) {
  if (!label)
    return "utf-8";

  char* normalized = normalize_encoding_label(label);
  if (!normalized)
    return "utf-8";

  for (size_t i = 0; encodings_table[i].name; i++) {
    if (strcmp(normalized, encodings_table[i].name) == 0) {
      free(normalized);
      return encodings_table[i].canonical;
    }
  }

  free(normalized);

  // For unknown encodings, we still return UTF-8 as the fallback
  // This is appropriate for server runtimes that primarily handle UTF-8
  return "utf-8";
}

// UTF-8 validation function with overlong sequence detection
static int validate_utf8_sequence(const uint8_t* data, size_t len, const uint8_t** next) {
  if (len == 0) {
    *next = data;
    return -1;
  }

  uint8_t c = data[0];

  if (c < 0x80) {
    // ASCII character
    *next = data + 1;
    return c;
  }

  // Invalid start bytes
  if (c == 0xFE || c == 0xFF || (c >= 0xF8 && c <= 0xFD)) {
    *next = data;
    return -1;
  }

  if ((c & 0xE0) == 0xC0) {
    // 2-byte sequence
    if (len < 2) {
      *next = data;
      return -1;
    }
    if ((data[1] & 0xC0) != 0x80) {
      *next = data;
      return -1;
    }

    uint32_t codepoint = ((c & 0x1F) << 6) | (data[1] & 0x3F);

    // Check for overlong encoding: 2-byte sequences must encode >= U+0080
    if (codepoint < 0x80) {
      *next = data;
      return -1;  // Overlong sequence
    }

    *next = data + 2;
    return codepoint;
  }

  if ((c & 0xF0) == 0xE0) {
    // 3-byte sequence
    if (len < 3) {
      *next = data;
      return -1;
    }
    if ((data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80) {
      *next = data;
      return -1;
    }

    uint32_t codepoint = ((c & 0x0F) << 12) | ((data[1] & 0x3F) << 6) | (data[2] & 0x3F);

    // Check for overlong encoding: 3-byte sequences must encode >= U+0800
    if (codepoint < 0x800) {
      *next = data;
      return -1;  // Overlong sequence
    }

    // Check for UTF-16 surrogates (U+D800 to U+DFFF)
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
      *next = data;
      return -1;  // Surrogates not allowed in UTF-8
    }

    *next = data + 3;
    return codepoint;
  }

  if ((c & 0xF8) == 0xF0) {
    // 4-byte sequence
    if (len < 4) {
      *next = data;
      return -1;
    }
    if ((data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80 || (data[3] & 0xC0) != 0x80) {
      *next = data;
      return -1;
    }

    uint32_t codepoint = ((c & 0x07) << 18) | ((data[1] & 0x3F) << 12) | ((data[2] & 0x3F) << 6) | (data[3] & 0x3F);

    // Check for overlong encoding: 4-byte sequences must encode >= U+10000
    if (codepoint < 0x10000) {
      *next = data;
      return -1;  // Overlong sequence
    }

    // Check for codepoints beyond Unicode range (> U+10FFFF)
    if (codepoint > 0x10FFFF) {
      *next = data;
      return -1;  // Beyond Unicode range
    }

    *next = data + 4;
    return codepoint;
  }

  *next = data;
  return -1;  // Invalid start byte
}

// Forward declare class IDs so they can be used in finalizers
static JSClassID JSRT_TextEncoderClassID;
static JSClassID JSRT_TextDecoderClassID;

// TextEncoder implementation
typedef struct {
  const char* encoding;
} JSRT_TextEncoder;

static void JSRT_TextEncoderFinalize(JSRuntime* rt, JSValue val) {
  JSRT_TextEncoder* encoder = JS_GetOpaque(val, JSRT_TextEncoderClassID);
  if (encoder) {
    free(encoder);
  }
}

static JSClassDef JSRT_TextEncoderClass = {
    .class_name = "TextEncoder",
    .finalizer = JSRT_TextEncoderFinalize,
};

static JSValue JSRT_TextEncoderConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj;
  JSRT_TextEncoder* encoder;

  encoder = malloc(sizeof(JSRT_TextEncoder));
  if (!encoder) {
    return JS_EXCEPTION;
  }

  encoder->encoding = "utf-8";  // Only UTF-8 supported for now

  obj = JS_NewObjectClass(ctx, JSRT_TextEncoderClassID);
  if (JS_IsException(obj)) {
    free(encoder);
    return JS_EXCEPTION;
  }

  JS_SetOpaque(obj, encoder);

  // Set encoding property
  JS_DefinePropertyValueStr(ctx, obj, "encoding", JS_NewString(ctx, "utf-8"), JS_PROP_C_W_E);

  return obj;
}

static JSValue JSRT_TextEncoderEncode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_TextEncoder* encoder = JS_GetOpaque2(ctx, this_val, JSRT_TextEncoderClassID);
  if (!encoder) {
    return JS_EXCEPTION;
  }

  const char* input = "";
  size_t input_len = 0;

  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    input = JS_ToCStringLen(ctx, &input_len, argv[0]);
    if (!input) {
      return JS_EXCEPTION;
    }
  }

  // Create Uint8Array with the UTF-8 bytes
  JSValue array_buffer = JS_NewArrayBufferCopy(ctx, (const uint8_t*)input, input_len);
  if (JS_IsException(array_buffer)) {
    JS_FreeCString(ctx, input);
    return JS_EXCEPTION;
  }

  JSValue global = JS_GetGlobalObject(ctx);
  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  JSValue uint8_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);
  JS_FreeValue(ctx, uint8_array_ctor);
  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, array_buffer);
  JS_FreeCString(ctx, input);

  return uint8_array;
}

static JSValue JSRT_TextEncoderEncodeInto(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_TextEncoder* encoder = JS_GetOpaque2(ctx, this_val, JSRT_TextEncoderClassID);
  if (!encoder) {
    return JS_EXCEPTION;
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "encodeInto requires 2 arguments");
  }

  const char* input = "";
  size_t input_len = 0;

  if (!JS_IsUndefined(argv[0])) {
    input = JS_ToCStringLen(ctx, &input_len, argv[0]);
    if (!input) {
      return JS_EXCEPTION;
    }
  }

  // Get destination Uint8Array
  JSValue destination = argv[1];
  size_t byte_offset, byte_length;
  JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, destination, &byte_offset, &byte_length, NULL);
  if (JS_IsException(array_buffer)) {
    JS_FreeCString(ctx, input);
    return JS_ThrowTypeError(ctx, "destination must be a Uint8Array");
  }

  uint8_t* buffer = JS_GetArrayBuffer(ctx, &byte_length, array_buffer);
  if (!buffer) {
    JS_FreeCString(ctx, input);
    JS_FreeValue(ctx, array_buffer);
    return JS_EXCEPTION;
  }

  // Copy UTF-8 bytes into destination buffer
  size_t bytes_written = input_len < byte_length ? input_len : byte_length;
  memcpy(buffer + byte_offset, input, bytes_written);

  JS_FreeCString(ctx, input);
  JS_FreeValue(ctx, array_buffer);

  // Return result object with read and written properties
  JSValue result = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, result, "read", JS_NewUint32(ctx, input_len));
  JS_SetPropertyStr(ctx, result, "written", JS_NewUint32(ctx, bytes_written));

  return result;
}

// TextDecoder implementation
typedef struct {
  const char* encoding;
  bool fatal;
  bool ignore_bom;
} JSRT_TextDecoder;

static void JSRT_TextDecoderFinalize(JSRuntime* rt, JSValue val) {
  JSRT_TextDecoder* decoder = JS_GetOpaque(val, JSRT_TextDecoderClassID);
  if (decoder) {
    free(decoder);
  }
}

static JSClassDef JSRT_TextDecoderClass = {
    .class_name = "TextDecoder",
    .finalizer = JSRT_TextDecoderFinalize,
};

static JSValue JSRT_TextDecoderConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj;
  JSRT_TextDecoder* decoder;

  decoder = malloc(sizeof(JSRT_TextDecoder));
  if (!decoder) {
    return JS_EXCEPTION;
  }

  // Parse encoding label if provided
  const char* encoding_label = "utf-8";
  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    encoding_label = JS_ToCString(ctx, argv[0]);
    if (!encoding_label) {
      free(decoder);
      return JS_EXCEPTION;
    }
  }

  // Get canonical encoding name
  const char* canonical = get_canonical_encoding(encoding_label);
  decoder->encoding = canonical;
  decoder->fatal = false;
  decoder->ignore_bom = false;

  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    JS_FreeCString(ctx, encoding_label);
  }

  // Parse options if provided
  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue fatal_val = JS_GetPropertyStr(ctx, argv[1], "fatal");
    if (!JS_IsUndefined(fatal_val)) {
      decoder->fatal = JS_ToBool(ctx, fatal_val);
    }
    JS_FreeValue(ctx, fatal_val);

    JSValue ignore_bom_val = JS_GetPropertyStr(ctx, argv[1], "ignoreBOM");
    if (!JS_IsUndefined(ignore_bom_val)) {
      decoder->ignore_bom = JS_ToBool(ctx, ignore_bom_val);
    }
    JS_FreeValue(ctx, ignore_bom_val);
  }

  obj = JS_NewObjectClass(ctx, JSRT_TextDecoderClassID);
  if (JS_IsException(obj)) {
    free(decoder);
    return JS_EXCEPTION;
  }

  JS_SetOpaque(obj, decoder);

  // Set properties - encoding property should be lowercase per WPT tests
  char* lowercase_encoding = strdup(decoder->encoding);
  if (lowercase_encoding) {
    for (char* p = lowercase_encoding; *p; p++) {
      *p = tolower(*p);
    }
    JS_DefinePropertyValueStr(ctx, obj, "encoding", JS_NewString(ctx, lowercase_encoding), JS_PROP_C_W_E);
    free(lowercase_encoding);
  } else {
    JS_DefinePropertyValueStr(ctx, obj, "encoding", JS_NewString(ctx, decoder->encoding), JS_PROP_C_W_E);
  }
  JS_DefinePropertyValueStr(ctx, obj, "fatal", JS_NewBool(ctx, decoder->fatal), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, obj, "ignoreBOM", JS_NewBool(ctx, decoder->ignore_bom), JS_PROP_C_W_E);

  return obj;
}

static JSValue JSRT_TextDecoderDecode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_TextDecoder* decoder = JS_GetOpaque2(ctx, this_val, JSRT_TextDecoderClassID);
  if (!decoder) {
    return JS_EXCEPTION;
  }

  if (argc == 0 || JS_IsUndefined(argv[0])) {
    return JS_NewString(ctx, "");
  }

  uint8_t* input_data = NULL;
  size_t input_len = 0;

  // Try to get as ArrayBuffer first
  input_data = JS_GetArrayBuffer(ctx, &input_len, argv[0]);

  if (input_data) {
    // ArrayBuffer path
    if (input_len == 0) {
      return JS_NewString(ctx, "");
    }

    // Skip BOM if not ignoring it and present
    size_t start_pos = 0;
    if (!decoder->ignore_bom && input_len >= 3 && input_data[0] == 0xEF && input_data[1] == 0xBB &&
        input_data[2] == 0xBF) {
      start_pos = 3;
    }

    // Validate input if fatal mode is enabled
    if (decoder->fatal) {
      if (strcmp(decoder->encoding, "utf-8") == 0) {
        // Validate UTF-8 sequence
        const uint8_t* p = input_data + start_pos;
        const uint8_t* end = input_data + input_len;
        while (p < end) {
          const uint8_t* p_next;
          int c = validate_utf8_sequence(p, end - p, &p_next);
          if (c < 0) {
            return JS_ThrowTypeError(ctx, "Invalid UTF-8 sequence");
          }
          p = p_next;
        }
      } else if (strcmp(decoder->encoding, "utf-16le") == 0) {
        // Validate UTF-16LE: must have even number of bytes for complete code units
        size_t remaining_len = input_len - start_pos;
        if (remaining_len % 2 != 0) {
          return JS_ThrowTypeError(ctx, "Incomplete UTF-16LE code unit");
        }
        // Additional UTF-16LE validation could be added here for surrogate pairs, etc.
      } else if (strcmp(decoder->encoding, "utf-16be") == 0) {
        // Validate UTF-16BE: must have even number of bytes for complete code units
        size_t remaining_len = input_len - start_pos;
        if (remaining_len % 2 != 0) {
          return JS_ThrowTypeError(ctx, "Incomplete UTF-16BE code unit");
        }
      }
      // For other encodings, we don't implement strict validation in this server runtime
    }

    return JS_NewStringLen(ctx, (const char*)(input_data + start_pos), input_len - start_pos);
  }

  // Try to get as typed array (including DataView)
  size_t byte_offset, bytes_per_element;
  JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[0], &byte_offset, &input_len, &bytes_per_element);
  if (JS_IsException(array_buffer)) {
    // Clear the exception and try DataView
    JS_FreeValue(ctx, array_buffer);

    // Check if it's a DataView
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue dataview_ctor = JS_GetPropertyStr(ctx, global, "DataView");

    if (!JS_IsException(dataview_ctor) && !JS_IsUndefined(dataview_ctor)) {
      int is_dataview = JS_IsInstanceOf(ctx, argv[0], dataview_ctor);
      JS_FreeValue(ctx, dataview_ctor);

      if (is_dataview > 0) {
        // Handle DataView: get its buffer, byteOffset, and byteLength
        JSValue buffer_prop = JS_GetPropertyStr(ctx, argv[0], "buffer");
        JSValue offset_prop = JS_GetPropertyStr(ctx, argv[0], "byteOffset");
        JSValue length_prop = JS_GetPropertyStr(ctx, argv[0], "byteLength");

        if (!JS_IsException(buffer_prop) && !JS_IsException(offset_prop) && !JS_IsException(length_prop)) {
          array_buffer = buffer_prop;

          if (JS_ToIndex(ctx, &byte_offset, offset_prop) == 0) {
            if (JS_ToIndex(ctx, &input_len, length_prop) != 0) {
              input_len = 0;
            }
          }

          JS_FreeValue(ctx, offset_prop);
          JS_FreeValue(ctx, length_prop);
        } else {
          JS_FreeValue(ctx, global);
          JS_FreeValue(ctx, buffer_prop);
          JS_FreeValue(ctx, offset_prop);
          JS_FreeValue(ctx, length_prop);
          return JS_ThrowTypeError(ctx, "input must be an ArrayBuffer, typed array, or DataView");
        }
      } else {
        JS_FreeValue(ctx, global);
        return JS_ThrowTypeError(ctx, "input must be an ArrayBuffer, typed array, or DataView");
      }
    } else {
      JS_FreeValue(ctx, global);
      JS_FreeValue(ctx, dataview_ctor);
      return JS_ThrowTypeError(ctx, "input must be an ArrayBuffer, typed array, or DataView");
    }
    JS_FreeValue(ctx, global);
  }

  if (input_len == 0) {
    JS_FreeValue(ctx, array_buffer);
    return JS_NewString(ctx, "");
  }

  size_t buffer_size;
  uint8_t* buffer = JS_GetArrayBuffer(ctx, &buffer_size, array_buffer);
  if (!buffer) {
    JS_FreeValue(ctx, array_buffer);
    return JS_ThrowTypeError(ctx, "Failed to get buffer from typed array");
  }

  input_data = buffer + byte_offset;

  // Skip BOM if not ignoring it and present
  size_t start_pos = 0;
  if (!decoder->ignore_bom && input_len >= 3 && input_data[0] == 0xEF && input_data[1] == 0xBB &&
      input_data[2] == 0xBF) {
    start_pos = 3;
  }

  // Validate input if fatal mode is enabled
  if (decoder->fatal) {
    if (strcmp(decoder->encoding, "utf-8") == 0) {
      // Validate UTF-8 sequence
      const uint8_t* p = input_data + start_pos;
      const uint8_t* end = input_data + input_len;
      while (p < end) {
        const uint8_t* p_next;
        int c = validate_utf8_sequence(p, end - p, &p_next);
        if (c < 0) {
          JS_FreeValue(ctx, array_buffer);
          return JS_ThrowTypeError(ctx, "Invalid UTF-8 sequence");
        }
        p = p_next;
      }
    } else if (strcmp(decoder->encoding, "utf-16le") == 0) {
      // Validate UTF-16LE: must have even number of bytes for complete code units
      size_t remaining_len = input_len - start_pos;
      if (remaining_len % 2 != 0) {
        JS_FreeValue(ctx, array_buffer);
        return JS_ThrowTypeError(ctx, "Incomplete UTF-16LE code unit");
      }
    } else if (strcmp(decoder->encoding, "utf-16be") == 0) {
      // Validate UTF-16BE: must have even number of bytes for complete code units
      size_t remaining_len = input_len - start_pos;
      if (remaining_len % 2 != 0) {
        JS_FreeValue(ctx, array_buffer);
        return JS_ThrowTypeError(ctx, "Incomplete UTF-16BE code unit");
      }
    }
  }

  // Create string from the buffer
  JSValue result = JS_NewStringLen(ctx, (const char*)(input_data + start_pos), input_len - start_pos);
  JS_FreeValue(ctx, array_buffer);
  return result;
}

// Setup functions
void JSRT_RuntimeSetupStdEncoding(JSRT_Runtime* rt) {
  JSContext* ctx = rt->ctx;

  // Register TextEncoder class
  JS_NewClassID(&JSRT_TextEncoderClassID);
  JS_NewClass(rt->rt, JSRT_TextEncoderClassID, &JSRT_TextEncoderClass);

  JSValue text_encoder_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, text_encoder_proto, "encode", JS_NewCFunction(ctx, JSRT_TextEncoderEncode, "encode", 1));
  JS_SetPropertyStr(ctx, text_encoder_proto, "encodeInto",
                    JS_NewCFunction(ctx, JSRT_TextEncoderEncodeInto, "encodeInto", 2));
  JS_SetClassProto(ctx, JSRT_TextEncoderClassID, text_encoder_proto);

  JSValue text_encoder_ctor =
      JS_NewCFunction2(ctx, JSRT_TextEncoderConstructor, "TextEncoder", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "TextEncoder", text_encoder_ctor);

  // Register TextDecoder class
  JS_NewClassID(&JSRT_TextDecoderClassID);
  JS_NewClass(rt->rt, JSRT_TextDecoderClassID, &JSRT_TextDecoderClass);

  JSValue text_decoder_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, text_decoder_proto, "decode", JS_NewCFunction(ctx, JSRT_TextDecoderDecode, "decode", 1));
  JS_SetClassProto(ctx, JSRT_TextDecoderClassID, text_decoder_proto);

  JSValue text_decoder_ctor =
      JS_NewCFunction2(ctx, JSRT_TextDecoderConstructor, "TextDecoder", 2, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "TextDecoder", text_decoder_ctor);

  JSRT_Debug("Encoding API setup completed");
}