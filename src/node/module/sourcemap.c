#include "sourcemap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/debug.h"

// FNV-1a hash function for string keys
static uint32_t hash_string(const char* str) {
  uint32_t hash = 2166136261u;
  while (*str) {
    hash ^= (uint8_t)(*str++);
    hash *= 16777619u;
  }
  return hash;
}

// ============================================================================
// Source Map Lifecycle
// ============================================================================

JSRT_SourceMap* jsrt_source_map_new(JSContext* ctx) {
  JSRT_SourceMap* map = js_mallocz(ctx, sizeof(JSRT_SourceMap));
  if (!map) {
    return NULL;
  }

  // Initialize all fields to NULL/0
  map->version = NULL;
  map->file = NULL;
  map->source_root = NULL;
  map->sources = NULL;
  map->sources_count = 0;
  map->sources_content = NULL;
  map->sources_content_count = 0;
  map->names = NULL;
  map->names_count = 0;
  map->mappings = NULL;
  map->decoded_mappings = NULL;
  map->decoded_mappings_count = 0;
  map->payload = JS_UNDEFINED;

  return map;
}

void jsrt_source_map_free(JSRuntime* rt, JSRT_SourceMap* map) {
  if (!map) {
    return;
  }

  // Free string fields
  if (map->version) {
    js_free_rt(rt, map->version);
  }
  if (map->file) {
    js_free_rt(rt, map->file);
  }
  if (map->source_root) {
    js_free_rt(rt, map->source_root);
  }
  if (map->mappings) {
    js_free_rt(rt, map->mappings);
  }

  // Free sources array
  if (map->sources) {
    for (size_t i = 0; i < map->sources_count; i++) {
      if (map->sources[i]) {
        js_free_rt(rt, map->sources[i]);
      }
    }
    js_free_rt(rt, map->sources);
  }

  // Free sources_content array
  if (map->sources_content) {
    for (size_t i = 0; i < map->sources_content_count; i++) {
      if (map->sources_content[i]) {
        js_free_rt(rt, map->sources_content[i]);
      }
    }
    js_free_rt(rt, map->sources_content);
  }

  // Free names array
  if (map->names) {
    for (size_t i = 0; i < map->names_count; i++) {
      if (map->names[i]) {
        js_free_rt(rt, map->names[i]);
      }
    }
    js_free_rt(rt, map->names);
  }

  // Free decoded mappings
  if (map->decoded_mappings) {
    js_free_rt(rt, map->decoded_mappings);
  }

  // Free JS payload
  if (!JS_IsUndefined(map->payload)) {
    JS_FreeValueRT(rt, map->payload);
  }

  // Free the map itself
  js_free_rt(rt, map);
}

// ============================================================================
// Source Map Cache Management
// ============================================================================

JSRT_SourceMapCache* jsrt_source_map_cache_init(JSContext* ctx, size_t capacity) {
  if (capacity == 0) {
    capacity = 16;  // Default capacity
  }

  JSRT_SourceMapCache* cache = js_mallocz(ctx, sizeof(JSRT_SourceMapCache));
  if (!cache) {
    return NULL;
  }

  cache->buckets = js_mallocz(ctx, sizeof(JSRT_SourceMapCacheEntry*) * capacity);
  if (!cache->buckets) {
    js_free(ctx, cache);
    return NULL;
  }

  cache->capacity = capacity;
  cache->size = 0;
  cache->enabled = true;  // Enabled by default

  return cache;
}

void jsrt_source_map_cache_free(JSRuntime* rt, JSRT_SourceMapCache* cache) {
  if (!cache) {
    return;
  }

  // Free all entries
  for (size_t i = 0; i < cache->capacity; i++) {
    JSRT_SourceMapCacheEntry* entry = cache->buckets[i];
    while (entry) {
      JSRT_SourceMapCacheEntry* next = entry->next;

      // Free entry resources
      if (entry->path) {
        js_free_rt(rt, entry->path);
      }
      if (entry->source_map) {
        jsrt_source_map_free(rt, entry->source_map);
      }
      js_free_rt(rt, entry);

      entry = next;
    }
  }

  // Free buckets array
  js_free_rt(rt, cache->buckets);

  // Free cache itself
  js_free_rt(rt, cache);
}

JSRT_SourceMap* jsrt_source_map_cache_lookup(JSRT_SourceMapCache* cache, const char* path) {
  if (!cache || !path || !cache->enabled) {
    return NULL;
  }

  uint32_t hash = hash_string(path);
  size_t bucket_index = hash % cache->capacity;

  JSRT_SourceMapCacheEntry* entry = cache->buckets[bucket_index];
  while (entry) {
    if (strcmp(entry->path, path) == 0) {
      return entry->source_map;
    }
    entry = entry->next;
  }

  return NULL;
}

bool jsrt_source_map_cache_store(JSContext* ctx, JSRT_SourceMapCache* cache, const char* path, JSRT_SourceMap* map) {
  if (!cache || !path || !map) {
    return false;
  }

  // Check if already exists (update case)
  uint32_t hash = hash_string(path);
  size_t bucket_index = hash % cache->capacity;

  JSRT_SourceMapCacheEntry* entry = cache->buckets[bucket_index];
  while (entry) {
    if (strcmp(entry->path, path) == 0) {
      // Update existing entry
      jsrt_source_map_free(JS_GetRuntime(ctx), entry->source_map);
      entry->source_map = map;
      return true;
    }
    entry = entry->next;
  }

  // Create new entry
  JSRT_SourceMapCacheEntry* new_entry = js_mallocz(ctx, sizeof(JSRT_SourceMapCacheEntry));
  if (!new_entry) {
    return false;
  }

  new_entry->path = js_strdup(ctx, path);
  if (!new_entry->path) {
    js_free(ctx, new_entry);
    return false;
  }

  new_entry->source_map = map;
  new_entry->next = cache->buckets[bucket_index];
  cache->buckets[bucket_index] = new_entry;
  cache->size++;

  return true;
}

void jsrt_source_map_cache_set_enabled(JSRT_SourceMapCache* cache, bool enabled) {
  if (cache) {
    cache->enabled = enabled;
  }
}

bool jsrt_source_map_cache_is_enabled(JSRT_SourceMapCache* cache) {
  return cache ? cache->enabled : false;
}

// ============================================================================
// Base64 Decoder for VLQ (Source Map v3)
// ============================================================================

// Base64 character mapping for Source Map VLQ
// Valid characters: A-Z, a-z, 0-9, +, /
static const int8_t base64_decode_table[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0-15
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 16-31
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,  // 32-47 ('+' = 62, '/' = 63)
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,  // 48-63 ('0'-'9' = 52-61)
    -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,  // 64-79 ('A'-'O' = 0-14)
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,  // 80-95 ('P'-'Z' = 15-25)
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,  // 96-111 ('a'-'o' = 26-40)
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1   // 112-127 ('p'-'z' = 41-51)
};

/**
 * Decode a single Base64 character to 6-bit value
 * @param c Character to decode
 * @return 6-bit value (0-63) or -1 if invalid
 */
static inline int8_t decode_base64_char(char c) {
  if (c < 0 || c >= 128) {
    return -1;
  }
  return base64_decode_table[(unsigned char)c];
}

// ============================================================================
// VLQ Decoder (Variable-Length Quantity) for Source Map v3
// ============================================================================

// VLQ Constants
#define VLQ_BASE_SHIFT 5                            // 5 bits per character
#define VLQ_BASE (1 << VLQ_BASE_SHIFT)              // 32
#define VLQ_BASE_MASK (VLQ_BASE - 1)                // 31 (0b11111)
#define VLQ_CONTINUATION_BIT (1 << VLQ_BASE_SHIFT)  // 32 (0b100000)

/**
 * Decode a single VLQ value from a Base64-encoded string
 *
 * VLQ encoding (Source Map v3 spec):
 * - Each Base64 character represents 6 bits
 * - Bit 5 (0x20): continuation flag (1 = more characters follow, 0 = last)
 * - Bits 0-4: data bits
 * - For the first character of a value: bit 0 is the sign bit
 *
 * @param str Pointer to Base64 string (will be advanced)
 * @param value Output: decoded integer value
 * @return true on success, false on error
 */
static bool decode_vlq_value(const char** str, int32_t* value) {
  if (!str || !*str || !value) {
    return false;
  }

  int32_t result = 0;
  int32_t shift = 0;
  bool continuation = true;
  bool first = true;

  while (continuation) {
    char c = **str;
    if (c == '\0') {
      // Unexpected end of string
      return false;
    }

    int8_t digit = decode_base64_char(c);
    if (digit < 0) {
      // Invalid Base64 character
      return false;
    }

    (*str)++;  // Advance pointer

    // Check continuation bit (bit 5)
    continuation = (digit & VLQ_CONTINUATION_BIT) != 0;

    // Extract data bits (bits 0-4)
    int32_t data_bits = digit & VLQ_BASE_MASK;

    if (first) {
      // First character: bit 0 is sign, bits 1-4 are data
      first = false;
      bool negative = (data_bits & 1) != 0;
      data_bits >>= 1;  // Remove sign bit
      result = data_bits;
      shift = 4;  // Already consumed 4 data bits

      // Apply sign if negative
      if (negative) {
        result = -result;
      }
    } else {
      // Subsequent characters: all 5 bits are data
      result += data_bits << shift;
      shift += 5;
    }

    // Prevent overflow (reasonable limit: 32 bits)
    if (shift > 31) {
      return false;
    }
  }

  *value = result;
  return true;
}

/**
 * Decode VLQ-encoded mappings string into array of integers
 *
 * @param ctx QuickJS context
 * @param mappings VLQ-encoded mappings string
 * @param values Output array of decoded integers
 * @param count Output count of decoded integers
 * @return true on success, false on error
 */
static bool decode_vlq_mappings(JSContext* ctx, const char* mappings, int32_t** values, size_t* count) {
  if (!ctx || !mappings || !values || !count) {
    return false;
  }

  // Estimate size (rough: average 2 chars per value)
  size_t estimated_size = strlen(mappings) / 2 + 100;
  int32_t* result = js_malloc(ctx, estimated_size * sizeof(int32_t));
  if (!result) {
    return false;
  }

  size_t result_count = 0;
  size_t result_capacity = estimated_size;
  const char* ptr = mappings;

  while (*ptr != '\0') {
    // Skip separators (';' for line, ',' for segment)
    if (*ptr == ';' || *ptr == ',') {
      ptr++;
      continue;
    }

    // Decode one VLQ value
    int32_t value;
    if (!decode_vlq_value(&ptr, &value)) {
      JSRT_Debug("Failed to decode VLQ value at position %ld", ptr - mappings);
      js_free(ctx, result);
      return false;
    }

    // Grow array if needed
    if (result_count >= result_capacity) {
      result_capacity *= 2;
      int32_t* new_result = js_realloc(ctx, result, result_capacity * sizeof(int32_t));
      if (!new_result) {
        js_free(ctx, result);
        return false;
      }
      result = new_result;
    }

    result[result_count++] = value;
  }

  *values = result;
  *count = result_count;
  return true;
}

// ============================================================================
// Source Map Mappings Builder (Task 2.4)
// ============================================================================

/**
 * Build decoded mappings array from VLQ-encoded mappings string
 * Converts relative VLQ values into absolute JSRT_SourceMapping structs
 *
 * Source Map v3 format:
 * - Semicolons (;) separate lines
 * - Commas (,) separate segments within a line
 * - Each segment has 1, 4, or 5 VLQ values:
 *   1. Generated column (delta from previous column or 0 for first in line)
 *   2. Source file index (delta, optional)
 *   3. Original line (delta, optional)
 *   4. Original column (delta, optional)
 *   5. Name index (delta, optional)
 */
static bool jsrt_source_map_build_mappings(JSContext* ctx, JSRT_SourceMap* map) {
  if (!map || !map->mappings) {
    return false;
  }

  // Estimate capacity (rough: one mapping per 5 characters)
  size_t estimated_capacity = strlen(map->mappings) / 5 + 100;
  JSRT_SourceMapping* mappings = js_malloc(ctx, estimated_capacity * sizeof(JSRT_SourceMapping));
  if (!mappings) {
    return false;
  }

  size_t mappings_count = 0;
  size_t mappings_capacity = estimated_capacity;

  // State tracking for delta decoding
  int32_t generated_line = 0;
  int32_t generated_column = 0;
  int32_t source_index = 0;
  int32_t original_line = 0;
  int32_t original_column = 0;
  int32_t name_index = 0;

  const char* ptr = map->mappings;

  while (*ptr != '\0') {
    // Handle line separator
    if (*ptr == ';') {
      generated_line++;
      generated_column = 0;  // Reset column at start of new line
      ptr++;
      continue;
    }

    // Handle segment separator
    if (*ptr == ',') {
      ptr++;
      continue;
    }

    // Decode VLQ segment (1, 4, or 5 values)
    int32_t values[5];
    int value_count = 0;

    // Decode up to 5 values
    while (*ptr != '\0' && *ptr != ',' && *ptr != ';' && value_count < 5) {
      if (!decode_vlq_value(&ptr, &values[value_count])) {
        JSRT_Debug("Failed to decode VLQ value at position %ld", ptr - map->mappings);
        js_free(ctx, mappings);
        return false;
      }
      value_count++;
    }

    // Must have at least 1 value (generated column)
    if (value_count < 1) {
      continue;
    }

    // Apply deltas
    generated_column += values[0];

    // Create mapping
    JSRT_SourceMapping mapping;
    mapping.generated_line = generated_line;
    mapping.generated_column = generated_column;
    mapping.source_index = -1;
    mapping.original_line = -1;
    mapping.original_column = -1;
    mapping.name_index = -1;

    // If we have source information (4 or 5 values)
    if (value_count >= 4) {
      source_index += values[1];
      original_line += values[2];
      original_column += values[3];

      mapping.source_index = source_index;
      mapping.original_line = original_line;
      mapping.original_column = original_column;

      // Optional name index
      if (value_count >= 5) {
        name_index += values[4];
        mapping.name_index = name_index;
      }
    }

    // Grow array if needed
    if (mappings_count >= mappings_capacity) {
      mappings_capacity *= 2;
      JSRT_SourceMapping* new_mappings = js_realloc(ctx, mappings, mappings_capacity * sizeof(JSRT_SourceMapping));
      if (!new_mappings) {
        js_free(ctx, mappings);
        return false;
      }
      mappings = new_mappings;
    }

    mappings[mappings_count++] = mapping;
  }

  // Store in map
  map->decoded_mappings = mappings;
  map->decoded_mappings_count = mappings_count;

  JSRT_Debug("Built %zu mappings from VLQ string", mappings_count);
  return true;
}

// ============================================================================
// Source Map Parsing (Task 2.2)
// ============================================================================

/**
 * Parse Source Map v3 JSON payload
 * Implements Task 2.2: Source Map Parsing
 */
JSRT_SourceMap* jsrt_source_map_parse(JSContext* ctx, JSValue payload) {
  if (!ctx || JS_IsUndefined(payload) || JS_IsNull(payload)) {
    return NULL;
  }

  // Validate payload is an object
  if (!JS_IsObject(payload)) {
    JSRT_Debug("Source map payload is not an object");
    return NULL;
  }

  // Create new source map
  JSRT_SourceMap* map = jsrt_source_map_new(ctx);
  if (!map) {
    return NULL;
  }

  // Keep payload alive
  map->payload = JS_DupValue(ctx, payload);

  // Extract 'version' field (required, must be 3)
  JSValue version_val = JS_GetPropertyStr(ctx, payload, "version");
  if (JS_IsException(version_val) || JS_IsUndefined(version_val)) {
    JSRT_Debug("Source map missing 'version' field");
    goto error;
  }

  int32_t version;
  if (JS_ToInt32(ctx, &version, version_val) < 0 || version != 3) {
    JSRT_Debug("Source map version must be 3, got: %d", version);
    JS_FreeValue(ctx, version_val);
    goto error;
  }
  JS_FreeValue(ctx, version_val);

  map->version = js_strdup(ctx, "3");
  if (!map->version) {
    goto error;
  }

  // Extract 'file' field (optional)
  JSValue file_val = JS_GetPropertyStr(ctx, payload, "file");
  if (!JS_IsUndefined(file_val) && !JS_IsNull(file_val)) {
    const char* file_str = JS_ToCString(ctx, file_val);
    if (file_str) {
      map->file = js_strdup(ctx, file_str);
      JS_FreeCString(ctx, file_str);
    }
  }
  JS_FreeValue(ctx, file_val);

  // Extract 'sourceRoot' field (optional)
  JSValue source_root_val = JS_GetPropertyStr(ctx, payload, "sourceRoot");
  if (!JS_IsUndefined(source_root_val) && !JS_IsNull(source_root_val)) {
    const char* source_root_str = JS_ToCString(ctx, source_root_val);
    if (source_root_str) {
      map->source_root = js_strdup(ctx, source_root_str);
      JS_FreeCString(ctx, source_root_str);
    }
  }
  JS_FreeValue(ctx, source_root_val);

  // Extract 'sources' array (required)
  JSValue sources_val = JS_GetPropertyStr(ctx, payload, "sources");
  if (JS_IsException(sources_val) || !JS_IsArray(ctx, sources_val)) {
    JSRT_Debug("Source map missing or invalid 'sources' array");
    JS_FreeValue(ctx, sources_val);
    goto error;
  }

  JSValue sources_len_val = JS_GetPropertyStr(ctx, sources_val, "length");
  uint32_t sources_len;
  if (JS_ToUint32(ctx, &sources_len, sources_len_val) < 0) {
    JS_FreeValue(ctx, sources_len_val);
    JS_FreeValue(ctx, sources_val);
    goto error;
  }
  JS_FreeValue(ctx, sources_len_val);

  if (sources_len > 0) {
    map->sources = js_mallocz(ctx, sources_len * sizeof(char*));
    if (!map->sources) {
      JS_FreeValue(ctx, sources_val);
      goto error;
    }
    map->sources_count = sources_len;

    for (uint32_t i = 0; i < sources_len; i++) {
      JSValue source_val = JS_GetPropertyUint32(ctx, sources_val, i);
      const char* source_str = JS_ToCString(ctx, source_val);
      if (source_str) {
        map->sources[i] = js_strdup(ctx, source_str);
        JS_FreeCString(ctx, source_str);
      }
      JS_FreeValue(ctx, source_val);
    }
  }
  JS_FreeValue(ctx, sources_val);

  // Extract 'names' array (optional)
  JSValue names_val = JS_GetPropertyStr(ctx, payload, "names");
  if (!JS_IsUndefined(names_val) && !JS_IsNull(names_val) && JS_IsArray(ctx, names_val)) {
    JSValue names_len_val = JS_GetPropertyStr(ctx, names_val, "length");
    uint32_t names_len;
    if (JS_ToUint32(ctx, &names_len, names_len_val) == 0 && names_len > 0) {
      map->names = js_mallocz(ctx, names_len * sizeof(char*));
      if (map->names) {
        map->names_count = names_len;
        for (uint32_t i = 0; i < names_len; i++) {
          JSValue name_val = JS_GetPropertyUint32(ctx, names_val, i);
          const char* name_str = JS_ToCString(ctx, name_val);
          if (name_str) {
            map->names[i] = js_strdup(ctx, name_str);
            JS_FreeCString(ctx, name_str);
          }
          JS_FreeValue(ctx, name_val);
        }
      }
    }
    JS_FreeValue(ctx, names_len_val);
  }
  JS_FreeValue(ctx, names_val);

  // Extract 'mappings' string (required)
  JSValue mappings_val = JS_GetPropertyStr(ctx, payload, "mappings");
  if (JS_IsException(mappings_val) || JS_IsUndefined(mappings_val)) {
    JSRT_Debug("Source map missing 'mappings' field");
    JS_FreeValue(ctx, mappings_val);
    goto error;
  }

  const char* mappings_str = JS_ToCString(ctx, mappings_val);
  if (!mappings_str) {
    JS_FreeValue(ctx, mappings_val);
    goto error;
  }

  // Store original mappings string
  map->mappings = js_strdup(ctx, mappings_str);
  JS_FreeCString(ctx, mappings_str);
  JS_FreeValue(ctx, mappings_val);

  if (!map->mappings) {
    goto error;
  }

  // Decode VLQ mappings and build mapping structs
  if (!jsrt_source_map_build_mappings(ctx, map)) {
    JSRT_Debug("Failed to build source map mappings");
    goto error;
  }

  JSRT_Debug("Source map parsed successfully: version=%s, sources=%zu, names=%zu, mappings=%zu", map->version,
             map->sources_count, map->names_count, map->decoded_mappings_count);

  return map;

error:
  jsrt_source_map_free(JS_GetRuntime(ctx), map);
  return NULL;
}

// ============================================================================
// SourceMap JavaScript Class (Task 2.3)
// ============================================================================

// SourceMap class ID
static JSClassID jsrt_source_map_class_id;

// SourceMap finalizer - called when JS object is garbage collected
static void jsrt_source_map_finalizer(JSRuntime* rt, JSValue val) {
  JSRT_SourceMap* map = JS_GetOpaque(val, jsrt_source_map_class_id);
  if (map) {
    jsrt_source_map_free(rt, map);
  }
}

// SourceMap class definition
static JSClassDef jsrt_source_map_class = {
    .class_name = "SourceMap",
    .finalizer = jsrt_source_map_finalizer,
};

/**
 * SourceMap constructor - not exposed publicly
 * Internal use only for creating instances from parsed source maps
 */
static JSValue jsrt_source_map_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  // This constructor is not meant to be called directly from JavaScript
  // Source maps are created via module.findSourceMap() or new SourceMap(payload)
  return JS_ThrowTypeError(ctx, "SourceMap constructor is not exposed");
}

/**
 * sourceMap.payload getter
 * Returns the original JSON payload
 */
static JSValue jsrt_source_map_get_payload(JSContext* ctx, JSValueConst this_val) {
  JSRT_SourceMap* map = JS_GetOpaque(this_val, jsrt_source_map_class_id);
  if (!map) {
    return JS_ThrowTypeError(ctx, "not a SourceMap instance");
  }

  // Return duplicated payload value
  return JS_DupValue(ctx, map->payload);
}

/**
 * sourceMap.findEntry(lineOffset, columnOffset)
 * Find source mapping for zero-indexed position
 *
 * @param lineOffset Zero-indexed line number in generated file
 * @param columnOffset Zero-indexed column number in generated file
 * @return Mapping object or empty object {} if not found
 */
static JSValue jsrt_source_map_find_entry(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_SourceMap* map = JS_GetOpaque(this_val, jsrt_source_map_class_id);
  if (!map) {
    return JS_ThrowTypeError(ctx, "not a SourceMap instance");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "findEntry requires 2 arguments: lineOffset and columnOffset");
  }

  int32_t line_offset, column_offset;
  if (JS_ToInt32(ctx, &line_offset, argv[0]) < 0) {
    return JS_EXCEPTION;
  }
  if (JS_ToInt32(ctx, &column_offset, argv[1]) < 0) {
    return JS_EXCEPTION;
  }

  // Validate parameters
  if (line_offset < 0 || column_offset < 0) {
    return JS_ThrowRangeError(ctx, "lineOffset and columnOffset must be non-negative");
  }

  // If no mappings, return empty object
  if (!map->decoded_mappings || map->decoded_mappings_count == 0) {
    return JS_NewObject(ctx);
  }

  // Binary search for line, then linear search for column
  // Mappings are sorted by generated_line, then generated_column

  // Find first mapping for the target line
  size_t left = 0;
  size_t right = map->decoded_mappings_count;
  size_t line_start = SIZE_MAX;

  // Binary search to find any mapping on the target line
  while (left < right) {
    size_t mid = left + (right - left) / 2;
    if (map->decoded_mappings[mid].generated_line < line_offset) {
      left = mid + 1;
    } else if (map->decoded_mappings[mid].generated_line > line_offset) {
      right = mid;
    } else {
      // Found a mapping on target line, but need to find the first one
      line_start = mid;
      // Continue searching left to find the first mapping on this line
      right = mid;
    }
  }

  // If we found a mapping on the target line, find the first one
  if (line_start != SIZE_MAX) {
    while (line_start > 0 && map->decoded_mappings[line_start - 1].generated_line == line_offset) {
      line_start--;
    }
  } else if (left < map->decoded_mappings_count && map->decoded_mappings[left].generated_line == line_offset) {
    line_start = left;
  } else {
    // No mapping found for this line
    return JS_NewObject(ctx);
  }

  // Linear search within the line for the closest column <= target column
  JSRT_SourceMapping* best_mapping = NULL;
  for (size_t i = line_start; i < map->decoded_mappings_count; i++) {
    JSRT_SourceMapping* mapping = &map->decoded_mappings[i];

    // Stop if we've moved to a different line
    if (mapping->generated_line != line_offset) {
      break;
    }

    // Find the mapping with largest column <= target column
    if (mapping->generated_column <= column_offset) {
      best_mapping = mapping;
    } else {
      // Columns are sorted, so we can stop
      break;
    }
  }

  // If no suitable mapping found, return empty object
  if (!best_mapping) {
    return JS_NewObject(ctx);
  }

  // Build result object
  JSValue result = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, result, "generatedLine", JS_NewInt32(ctx, best_mapping->generated_line));
  JS_SetPropertyStr(ctx, result, "generatedColumn", JS_NewInt32(ctx, best_mapping->generated_column));

  // Add source information if available
  if (best_mapping->source_index >= 0 && (size_t)best_mapping->source_index < map->sources_count) {
    JS_SetPropertyStr(ctx, result, "originalSource", JS_NewString(ctx, map->sources[best_mapping->source_index]));
    JS_SetPropertyStr(ctx, result, "originalLine", JS_NewInt32(ctx, best_mapping->original_line));
    JS_SetPropertyStr(ctx, result, "originalColumn", JS_NewInt32(ctx, best_mapping->original_column));

    // Add name if available
    if (best_mapping->name_index >= 0 && (size_t)best_mapping->name_index < map->names_count) {
      JS_SetPropertyStr(ctx, result, "name", JS_NewString(ctx, map->names[best_mapping->name_index]));
    }
  }

  return result;
}

/**
 * sourceMap.findOrigin(lineNumber, columnNumber)
 * Find original position for one-indexed position (for Error stacks)
 *
 * @param lineNumber One-indexed line number in generated file
 * @param columnNumber One-indexed column number in generated file
 * @return Object with {fileName, lineNumber, columnNumber, name} or empty object
 */
static JSValue jsrt_source_map_find_origin(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_SourceMap* map = JS_GetOpaque(this_val, jsrt_source_map_class_id);
  if (!map) {
    return JS_ThrowTypeError(ctx, "not a SourceMap instance");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "findOrigin requires 2 arguments: lineNumber and columnNumber");
  }

  int32_t line_number, column_number;
  if (JS_ToInt32(ctx, &line_number, argv[0]) < 0) {
    return JS_EXCEPTION;
  }
  if (JS_ToInt32(ctx, &column_number, argv[1]) < 0) {
    return JS_EXCEPTION;
  }

  // Validate parameters
  if (line_number < 1 || column_number < 1) {
    return JS_ThrowRangeError(ctx, "lineNumber and columnNumber must be >= 1");
  }

  // Convert one-indexed to zero-indexed
  int32_t line_offset = line_number - 1;
  int32_t column_offset = column_number - 1;

  // TODO: Task 2.5 will implement by calling findEntry internally
  JSRT_Debug("findOrigin called with line=%d, column=%d (not yet implemented)", line_number, column_number);

  JSValue result = JS_NewObject(ctx);
  return result;
}

// SourceMap prototype methods
static const JSCFunctionListEntry jsrt_source_map_proto[] = {
    JS_CGETSET_DEF("payload", jsrt_source_map_get_payload, NULL),
    JS_CFUNC_DEF("findEntry", 2, jsrt_source_map_find_entry),
    JS_CFUNC_DEF("findOrigin", 2, jsrt_source_map_find_origin),
};

/**
 * Initialize SourceMap class in module context
 * Registers the SourceMap class with QuickJS
 */
bool jsrt_source_map_class_init(JSContext* ctx, JSValue module_obj) {
  JSRuntime* rt = JS_GetRuntime(ctx);

  // Register SourceMap class
  JS_NewClassID(&jsrt_source_map_class_id);
  if (JS_NewClass(rt, jsrt_source_map_class_id, &jsrt_source_map_class) < 0) {
    JSRT_Debug("Failed to register SourceMap class");
    return false;
  }

  // Create SourceMap constructor
  JSValue source_map_ctor = JS_NewCFunction2(ctx, jsrt_source_map_constructor, "SourceMap", 1, JS_CFUNC_constructor, 0);

  // Create prototype object
  JSValue proto = JS_NewObject(ctx);
  JS_SetPropertyFunctionList(ctx, proto, jsrt_source_map_proto,
                             sizeof(jsrt_source_map_proto) / sizeof(jsrt_source_map_proto[0]));

  // Set prototype on constructor
  JS_SetConstructorBit(ctx, source_map_ctor, true);
  JS_SetPropertyStr(ctx, source_map_ctor, "prototype", proto);

  // Add SourceMap to module exports (for internal use)
  // Note: Not exposed in public node:module API, only used internally
  JS_SetPropertyStr(ctx, module_obj, "SourceMap", source_map_ctor);

  JSRT_Debug("SourceMap class registered successfully");
  return true;
}

/**
 * Create SourceMap instance from parsed data
 * Used internally by module.findSourceMap()
 */
JSValue jsrt_source_map_create_instance(JSContext* ctx, JSRT_SourceMap* map) {
  if (!ctx || !map) {
    return JS_UNDEFINED;
  }

  // Create new SourceMap instance
  JSValue obj = JS_NewObjectClass(ctx, jsrt_source_map_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  // Set the opaque pointer (takes ownership)
  JS_SetOpaque(obj, map);

  JSRT_Debug("Created SourceMap instance: sources=%zu, names=%zu", map->sources_count, map->names_count);

  return obj;
}

// ============================================================================
// Source Map Lookup (Stub - to be implemented in Task 2.6)
// ============================================================================

JSValue jsrt_find_source_map(JSContext* ctx, JSRT_SourceMapCache* cache, const char* path) {
  // TODO: Implement in Task 2.6 (module.findSourceMap)
  // This will:
  // 1. Check cache
  // 2. Look for inline source maps
  // 3. Look for external .map files
  // 4. Parse and cache if found
  JSRT_Debug("jsrt_find_source_map: TODO - implement in Task 2.6");
  return JS_UNDEFINED;
}
