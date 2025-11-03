/**
 * @file compile_cache.c
 * @brief Bytecode compilation cache implementation
 */

#include "compile_cache.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#define jsrt_getpid _getpid
#define jsrt_access _access
#else
#define jsrt_getpid getpid
#define jsrt_access access
#endif

#include "util/debug.h"

// Default cache directory
#define DEFAULT_CACHE_DIR ".jsrt/compile-cache"

#ifndef JSRT_VERSION
#define JSRT_VERSION "dev"
#endif

#ifndef QUICKJS_VERSION
#define QUICKJS_VERSION "unknown"
#endif

#define CACHE_META_SUFFIX ".meta"
#define CACHE_BYTECODE_SUFFIX ".jsc"

typedef struct {
  char* source_path;
  time_t mtime;
  bool portable;
  char jsrt_version[64];
  char quickjs_version[64];
  size_t bytecode_size;
} JSRT_CacheMetadata;

static void jsrt_cache_metadata_free(JSRT_CacheMetadata* meta) {
  if (!meta) {
    return;
  }
  if (meta->source_path) {
    free(meta->source_path);
    meta->source_path = NULL;
  }
}

static char* jsrt_cache_build_path(const char* directory, const char* key, const char* suffix) {
  if (!directory || !key || !suffix) {
    return NULL;
  }

  size_t dir_len = strlen(directory);
  bool needs_sep = dir_len > 0 && directory[dir_len - 1] != '/';
  size_t total_len = dir_len + (needs_sep ? 1 : 0) + strlen(key) + strlen(suffix) + 1;

  char* path = (char*)malloc(total_len);
  if (!path) {
    return NULL;
  }

  if (needs_sep) {
    snprintf(path, total_len, "%s/%s%s", directory, key, suffix);
  } else {
    snprintf(path, total_len, "%s%s%s", directory, key, suffix);
  }

  return path;
}

static bool jsrt_cache_write_atomic(const char* path, const uint8_t* data, size_t size) {
  if (!path || (!data && size > 0)) {
    return false;
  }

  size_t tmp_len = strlen(path) + 32;
  char* tmp_path = (char*)malloc(tmp_len);
  if (!tmp_path) {
    return false;
  }

  snprintf(tmp_path, tmp_len, "%s.tmp.%ld.%ld", path, (long)jsrt_getpid(), (long)time(NULL));

  FILE* f = fopen(tmp_path, "wb");
  if (!f) {
    free(tmp_path);
    return false;
  }

  bool success = true;
  if (size > 0) {
    if (fwrite(data, 1, size, f) != size) {
      success = false;
    }
  }

  if (success) {
    if (fflush(f) != 0) {
      success = false;
    }
#ifndef _WIN32
    if (success && fsync(fileno(f)) != 0) {
      success = false;
    }
#else
    if (success && _commit(_fileno(f)) != 0) {
      success = false;
    }
#endif
  }

  fclose(f);

  if (!success) {
    remove(tmp_path);
    free(tmp_path);
    return false;
  }

  // Remove existing file before rename to handle Windows semantics
  remove(path);
  if (rename(tmp_path, path) != 0) {
    remove(tmp_path);
    free(tmp_path);
    return false;
  }

  free(tmp_path);
  return true;
}

static bool jsrt_cache_read_file(const char* path, uint8_t** out_data, size_t* out_size) {
  if (!path || !out_data || !out_size) {
    return false;
  }

  FILE* f = fopen(path, "rb");
  if (!f) {
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }

  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return false;
  }

  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }

  uint8_t* buffer = (uint8_t*)malloc((size_t)size);
  if (!buffer) {
    fclose(f);
    return false;
  }

  size_t read_bytes = fread(buffer, 1, (size_t)size, f);
  fclose(f);

  if (read_bytes != (size_t)size) {
    free(buffer);
    return false;
  }

  *out_data = buffer;
  *out_size = (size_t)size;
  return true;
}

static bool jsrt_cache_write_metadata(const char* path, const JSRT_CacheMetadata* meta) {
  if (!path || !meta || !meta->source_path) {
    return false;
  }

  const char* source_path = meta->source_path;
  int needed = snprintf(NULL, 0,
                        "source_path=%s\n"
                        "mtime=%lld\n"
                        "portable=%d\n"
                        "jsrt_version=%s\n"
                        "quickjs_version=%s\n"
                        "bytecode_size=%zu\n",
                        source_path, (long long)meta->mtime, meta->portable ? 1 : 0, meta->jsrt_version,
                        meta->quickjs_version, meta->bytecode_size);
  if (needed < 0) {
    return false;
  }

  size_t buf_size = (size_t)needed + 1;
  char* buffer = (char*)malloc(buf_size);
  if (!buffer) {
    return false;
  }

  snprintf(buffer, buf_size,
           "source_path=%s\n"
           "mtime=%lld\n"
           "portable=%d\n"
           "jsrt_version=%s\n"
           "quickjs_version=%s\n"
           "bytecode_size=%zu\n",
           source_path, (long long)meta->mtime, meta->portable ? 1 : 0, meta->jsrt_version, meta->quickjs_version,
           meta->bytecode_size);

  bool ok = jsrt_cache_write_atomic(path, (const uint8_t*)buffer, strlen(buffer));
  free(buffer);
  return ok;
}

static bool jsrt_cache_read_metadata(const char* path, JSRT_CacheMetadata* meta) {
  if (!path || !meta) {
    return false;
  }

  memset(meta, 0, sizeof(*meta));

  FILE* f = fopen(path, "r");
  if (!f) {
    return false;
  }

  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    size_t len = strlen(line);
    if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
      line[len - 1] = '\0';
      len--;
      if (len > 0 && line[len - 1] == '\r') {
        line[len - 1] = '\0';
      }
    }

    if (strncmp(line, "source_path=", 12) == 0) {
      meta->source_path = strdup(line + 12);
      if (!meta->source_path) {
        fclose(f);
        return false;
      }
      continue;
    }

    long long mtime_val;
    if (sscanf(line, "mtime=%lld", &mtime_val) == 1) {
      meta->mtime = (time_t)mtime_val;
      continue;
    }

    int portable_flag;
    if (sscanf(line, "portable=%d", &portable_flag) == 1) {
      meta->portable = portable_flag != 0;
      continue;
    }

    if (strncmp(line, "jsrt_version=", 13) == 0) {
      strncpy(meta->jsrt_version, line + 13, sizeof(meta->jsrt_version) - 1);
      meta->jsrt_version[sizeof(meta->jsrt_version) - 1] = '\0';
      continue;
    }

    if (strncmp(line, "quickjs_version=", 16) == 0) {
      strncpy(meta->quickjs_version, line + 16, sizeof(meta->quickjs_version) - 1);
      meta->quickjs_version[sizeof(meta->quickjs_version) - 1] = '\0';
      continue;
    }

    unsigned long long bytecode_size_val;
    if (sscanf(line, "bytecode_size=%llu", &bytecode_size_val) == 1) {
      meta->bytecode_size = (size_t)bytecode_size_val;
      continue;
    }
  }

  fclose(f);

  if (!meta->source_path) {
    return false;
  }

  if (meta->jsrt_version[0] == '\0') {
    snprintf(meta->jsrt_version, sizeof(meta->jsrt_version), "%s", "unknown");
  }

  if (meta->quickjs_version[0] == '\0') {
    snprintf(meta->quickjs_version, sizeof(meta->quickjs_version), "%s", "unknown");
  }

  return true;
}

static void jsrt_cache_remove_entry(const char* directory, const char* key) {
  if (!directory || !key) {
    return;
  }

  char* meta_path = jsrt_cache_build_path(directory, key, CACHE_META_SUFFIX);
  char* code_path = jsrt_cache_build_path(directory, key, CACHE_BYTECODE_SUFFIX);

  if (meta_path) {
    remove(meta_path);
    free(meta_path);
  }

  if (code_path) {
    remove(code_path);
    free(code_path);
  }
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get user's home directory
 * @return Home directory path or NULL on error
 */
static const char* get_home_directory(void) {
  const char* home = getenv("HOME");
  if (!home) {
    home = getenv("USERPROFILE");  // Windows fallback
  }
  return home;
}

/**
 * Join path components
 * @param base Base directory
 * @param path Path component
 * @return Joined path (caller must free) or NULL on error
 */
static char* path_join(const char* base, const char* path) {
  if (!base || !path) {
    return NULL;
  }

  size_t base_len = strlen(base);
  size_t path_len = strlen(path);

  // Check if base ends with separator
  bool needs_sep = (base_len > 0 && base[base_len - 1] != '/');
  size_t total_len = base_len + (needs_sep ? 1 : 0) + path_len + 1;

  char* result = (char*)malloc(total_len);
  if (!result) {
    return NULL;
  }

  if (needs_sep) {
    snprintf(result, total_len, "%s/%s", base, path);
  } else {
    snprintf(result, total_len, "%s%s", base, path);
  }

  return result;
}

/**
 * Create directory recursively
 * @param path Directory path
 * @return true on success, false on error
 */
static bool mkdir_recursive(const char* path) {
  if (!path) {
    return false;
  }

  // Check if directory already exists
  struct stat st;
  if (stat(path, &st) == 0) {
    return S_ISDIR(st.st_mode);
  }

  // Create parent directory first
  char* path_copy = strdup(path);
  if (!path_copy) {
    return false;
  }

  char* last_sep = strrchr(path_copy, '/');
  if (last_sep && last_sep != path_copy) {
    *last_sep = '\0';
    if (!mkdir_recursive(path_copy)) {
      free(path_copy);
      return false;
    }
  }

  free(path_copy);

  // Create the directory
  if (mkdir(path, 0755) == 0) {
    return true;
  }

  // Check if another process created it
  if (errno == EEXIST && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
    return true;
  }

  return false;
}

// ============================================================================
// Cache Lifecycle
// ============================================================================

JSRT_CompileCacheConfig* jsrt_compile_cache_init(JSContext* ctx) {
  JSRT_CompileCacheConfig* config = (JSRT_CompileCacheConfig*)malloc(sizeof(JSRT_CompileCacheConfig));
  if (!config) {
    return NULL;
  }

  config->directory = NULL;
  config->portable = false;
  config->allow_enable = true;
  config->enabled = false;
  config->hits = 0;
  config->misses = 0;
  config->writes = 0;
  config->errors = 0;
  config->evictions = 0;
  config->size_limit = DEFAULT_CACHE_SIZE_LIMIT;
  config->current_size = 0;
  config->lru_head = NULL;
  config->lru_tail = NULL;

  JSRT_Debug("Compile cache initialized (disabled by default)");

  return config;
}

void jsrt_compile_cache_free(JSRT_CompileCacheConfig* config) {
  if (!config) {
    return;
  }

  // Clean up LRU list
  JSRT_CacheLRUEntry* current = config->lru_head;
  while (current) {
    JSRT_CacheLRUEntry* next = current->next;
    if (current->key) {
      free(current->key);
    }
    free(current);
    current = next;
  }

  if (config->directory) {
    free(config->directory);
  }

  JSRT_Debug("Compile cache freed (hits: %lu, misses: %lu, writes: %lu, errors: %lu, evictions: %lu)", config->hits,
             config->misses, config->writes, config->errors, config->evictions);

  free(config);
}

// ============================================================================
// Cache Configuration
// ============================================================================

JSRT_CompileCacheStatus jsrt_compile_cache_enable(JSContext* ctx, JSRT_CompileCacheConfig* config,
                                                  const char* directory, bool portable) {
  if (!config) {
    return JSRT_COMPILE_CACHE_FAILED;
  }

  if (!config->allow_enable) {
    JSRT_Debug("Compile cache enable request ignored (disabled by runtime settings)");
    return JSRT_COMPILE_CACHE_DISABLED;
  }

  // Check if already enabled
  if (config->enabled) {
    JSRT_Debug("Compile cache already enabled at: %s", config->directory);
    return JSRT_COMPILE_CACHE_ALREADY_ENABLED;
  }

  // Determine cache directory
  const char* cache_dir = directory;
  char* default_dir = NULL;

  if (!cache_dir) {
    // Use default: ~/.jsrt/compile-cache/
    const char* home = get_home_directory();
    if (!home) {
      JSRT_Debug("Failed to get home directory");
      return JSRT_COMPILE_CACHE_FAILED;
    }

    default_dir = path_join(home, DEFAULT_CACHE_DIR);
    if (!default_dir) {
      JSRT_Debug("Failed to construct default cache directory");
      return JSRT_COMPILE_CACHE_FAILED;
    }
    cache_dir = default_dir;
  }

  // Create cache directory
  if (!jsrt_compile_cache_create_directory(cache_dir)) {
    JSRT_Debug("Failed to create cache directory: %s", cache_dir);
    if (default_dir) {
      free(default_dir);
    }
    return JSRT_COMPILE_CACHE_FAILED;
  }

  // Write version file
  if (!jsrt_compile_cache_write_version(cache_dir)) {
    JSRT_Debug("Failed to write version file");
    if (default_dir) {
      free(default_dir);
    }
    return JSRT_COMPILE_CACHE_FAILED;
  }

  // Store configuration
  config->directory = strdup(cache_dir);
  if (!config->directory) {
    JSRT_Debug("Failed to allocate cache directory string");
    if (default_dir) {
      free(default_dir);
    }
    return JSRT_COMPILE_CACHE_FAILED;
  }
  config->portable = portable;
  config->enabled = true;

  // Perform startup cleanup and calculate current cache size
  config->current_size = jsrt_compile_cache_get_disk_size(cache_dir);
  int cleanup_count = jsrt_compile_cache_startup_cleanup(config);
  if (cleanup_count > 0) {
    JSRT_Debug("Startup cleanup removed %d stale entries", cleanup_count);
    // Recalculate size after cleanup
    config->current_size = jsrt_compile_cache_get_disk_size(cache_dir);
  }

  if (default_dir) {
    free(default_dir);
  }

  JSRT_Debug("Compile cache enabled: %s (portable: %d, size: %zu bytes, limit: %zu bytes)", config->directory, portable,
             config->current_size, config->size_limit);

  return JSRT_COMPILE_CACHE_ENABLED;
}

void jsrt_compile_cache_disable(JSRT_CompileCacheConfig* config) {
  if (!config) {
    return;
  }

  if (config->directory) {
    free(config->directory);
    config->directory = NULL;
  }

  config->enabled = false;

  JSRT_Debug("Compile cache disabled");
}

void jsrt_compile_cache_set_allowed(JSRT_CompileCacheConfig* config, bool allowed) {
  if (!config) {
    return;
  }

  config->allow_enable = allowed;
  JSRT_Debug("Compile cache allow_enable set to: %s", allowed ? "true" : "false");
}

const char* jsrt_compile_cache_get_directory(JSRT_CompileCacheConfig* config) {
  if (!config || !config->enabled || !config->allow_enable) {
    return NULL;
  }
  return config->directory;
}

bool jsrt_compile_cache_is_enabled(JSRT_CompileCacheConfig* config) {
  return config && config->enabled && config->allow_enable;
}

// ============================================================================
// Cache Directory Management
// ============================================================================

bool jsrt_compile_cache_create_directory(const char* directory) {
  if (!directory) {
    return false;
  }

  if (!mkdir_recursive(directory)) {
    JSRT_Debug("Failed to create directory: %s (errno: %d)", directory, errno);
    return false;
  }

  JSRT_Debug("Cache directory created: %s", directory);
  return true;
}

bool jsrt_compile_cache_write_version(const char* directory) {
  if (!directory) {
    return false;
  }

  char* version_file = path_join(directory, "version.txt");
  if (!version_file) {
    return false;
  }

  FILE* f = fopen(version_file, "w");
  if (!f) {
    JSRT_Debug("Failed to open version file: %s (errno: %d)", version_file, errno);
    free(version_file);
    return false;
  }

  fprintf(f, "jsrt_version=%s\n", JSRT_VERSION);
  fprintf(f, "quickjs_version=%s\n", QUICKJS_VERSION);

  fclose(f);
  free(version_file);

  JSRT_Debug("Version file written (jsrt: %s, quickjs: %s)", JSRT_VERSION, QUICKJS_VERSION);

  return true;
}

bool jsrt_compile_cache_validate_version(const char* directory) {
  if (!directory) {
    return false;
  }

  char* version_file = path_join(directory, "version.txt");
  if (!version_file) {
    return false;
  }

  FILE* f = fopen(version_file, "r");
  if (!f) {
    free(version_file);
    return false;
  }

  char line[256];
  char jsrt_ver[64] = {0};
  char quickjs_ver[64] = {0};

  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "jsrt_version=%63s", jsrt_ver) == 1) {
      continue;
    }
    if (sscanf(line, "quickjs_version=%63s", quickjs_ver) == 1) {
      continue;
    }
  }

  fclose(f);
  free(version_file);

  // Validate versions
  bool valid = (strcmp(jsrt_ver, JSRT_VERSION) == 0) && (strcmp(quickjs_ver, QUICKJS_VERSION) == 0);

  if (!valid) {
    JSRT_Debug("Version mismatch - Cache: jsrt=%s quickjs=%s, Runtime: jsrt=%s quickjs=%s", jsrt_ver, quickjs_ver,
               JSRT_VERSION, QUICKJS_VERSION);
  }

  return valid;
}

// ============================================================================
// Cache Key Generation
// ============================================================================

/**
 * Simple FNV-1a hash for cache keys
 * @param data Data to hash
 * @param len Length of data
 * @return Hash value
 */
static uint64_t fnv1a_hash(const uint8_t* data, size_t len) {
  uint64_t hash = 14695981039346656037ULL;  // FNV offset basis
  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
    hash *= 1099511628211ULL;  // FNV prime
  }
  return hash;
}

char* jsrt_compile_cache_generate_key(const char* source_path, bool portable) {
  if (!source_path) {
    return NULL;
  }

  uint64_t hash = 0;

  if (portable) {
    // Portable mode: hash file content
    FILE* f = fopen(source_path, "rb");
    if (!f) {
      return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0 || size > 100 * 1024 * 1024) {  // 100MB limit
      fclose(f);
      return NULL;
    }

    uint8_t* content = (uint8_t*)malloc(size);
    if (!content) {
      fclose(f);
      return NULL;
    }

    if (fread(content, 1, size, f) != (size_t)size) {
      free(content);
      fclose(f);
      return NULL;
    }

    hash = fnv1a_hash(content, size);
    free(content);
    fclose(f);
  } else {
    // Non-portable mode: hash path + mtime
    struct stat st;
    if (stat(source_path, &st) != 0) {
      return NULL;
    }

    // Combine path and mtime
    size_t path_len = strlen(source_path);
    size_t total_len = path_len + sizeof(time_t);
    uint8_t* data = (uint8_t*)malloc(total_len);
    if (!data) {
      return NULL;
    }

    memcpy(data, source_path, path_len);
    memcpy(data + path_len, &st.st_mtime, sizeof(time_t));

    hash = fnv1a_hash(data, total_len);
    free(data);
  }

  // Convert hash to hex string
  char* key = (char*)malloc(17);  // 16 hex digits + null terminator
  if (!key) {
    return NULL;
  }

  snprintf(key, 17, "%016lx", hash);

  return key;
}

// ============================================================================
// Cache Lookup
// ============================================================================

JSValue jsrt_compile_cache_lookup(JSContext* ctx, JSRT_CompileCacheConfig* config, const char* source_path) {
  if (!config || !config->enabled || !source_path) {
    return JS_UNDEFINED;
  }

  JSValue result = JS_UNDEFINED;
  bool meta_loaded = false;
  JSRT_CacheMetadata meta;
  memset(&meta, 0, sizeof(meta));

  char* key = jsrt_compile_cache_generate_key(source_path, config->portable);
  if (!key) {
    config->errors++;
    return JS_UNDEFINED;
  }

  char* meta_path = jsrt_cache_build_path(config->directory, key, CACHE_META_SUFFIX);
  char* code_path = jsrt_cache_build_path(config->directory, key, CACHE_BYTECODE_SUFFIX);
  if (!meta_path || !code_path) {
    config->errors++;
    goto cleanup;
  }

  if (jsrt_access(meta_path, F_OK) != 0 || jsrt_access(code_path, F_OK) != 0) {
    goto miss;
  }

  if (!jsrt_cache_read_metadata(meta_path, &meta)) {
    JSRT_Debug("Failed to parse cache metadata: %s", meta_path);
    goto invalidate;
  }
  meta_loaded = true;

  bool version_match =
      (strcmp(meta.jsrt_version, JSRT_VERSION) == 0) && (strcmp(meta.quickjs_version, QUICKJS_VERSION) == 0);
  if (!version_match) {
    JSRT_Debug("Compile cache version mismatch for %s", source_path);
    goto invalidate;
  }

  if (meta.portable != config->portable) {
    JSRT_Debug("Compile cache portable flag mismatch for %s", source_path);
    goto invalidate;
  }

  if (!config->portable) {
    struct stat st;
    if (stat(source_path, &st) != 0) {
      JSRT_Debug("Compile cache stat failed for %s (errno=%d)", source_path, errno);
      goto invalidate;
    }
    if (st.st_mtime != meta.mtime) {
      JSRT_Debug("Compile cache mtime mismatch for %s", source_path);
      goto invalidate;
    }
  }

  uint8_t* data = NULL;
  size_t data_size = 0;
  if (!jsrt_cache_read_file(code_path, &data, &data_size)) {
    JSRT_Debug("Failed to read cached bytecode: %s", code_path);
    config->errors++;
    goto invalidate;
  }

  if (data_size != meta.bytecode_size) {
    JSRT_Debug("Cached bytecode size mismatch for %s", source_path);
    free(data);
    goto invalidate;
  }

  JSValue obj = JS_ReadObject(ctx, data, data_size, JS_READ_OBJ_BYTECODE);
  free(data);

  if (JS_IsException(obj)) {
    JSRT_Debug("JS_ReadObject failed for cached module %s", source_path);
    jsrt_cache_remove_entry(config->directory, key);
    config->errors++;
    goto cleanup;
  }

  config->hits++;
  result = obj;

  // Update LRU tracking for successful cache hit
  jsrt_compile_cache_update_lru(config, key, data_size);

  goto cleanup;

invalidate:
  jsrt_cache_remove_entry(config->directory, key);
  jsrt_compile_cache_remove_lru(config, key);
  config->errors++;

miss:
  config->misses++;

cleanup:
  if (meta_loaded) {
    jsrt_cache_metadata_free(&meta);
  }
  if (meta_path) {
    free(meta_path);
  }
  if (code_path) {
    free(code_path);
  }
  free(key);
  return result;
}

// ============================================================================
// Cache Storage
// ============================================================================

bool jsrt_compile_cache_store(JSContext* ctx, JSRT_CompileCacheConfig* config, const char* source_path,
                              JSValue bytecode) {
  if (!config || !config->enabled || !source_path) {
    return false;
  }

  bool success = false;
  char* key = jsrt_compile_cache_generate_key(source_path, config->portable);
  if (!key) {
    config->errors++;
    return false;
  }

  size_t bytecode_size = 0;
  uint8_t* bytecode_data = JS_WriteObject(ctx, &bytecode_size, bytecode, JS_WRITE_OBJ_BYTECODE);
  if (!bytecode_data) {
    JSRT_Debug("JS_WriteObject failed for %s", source_path);
    config->errors++;
    free(key);
    return false;
  }

  // Check if we need to evict entries to stay within size limit
  if (jsrt_compile_cache_maybe_evict(config, bytecode_size)) {
    JSRT_Debug("LRU eviction performed before storing %s (size: %zu bytes)", source_path, bytecode_size);
  }

  char* code_path = jsrt_cache_build_path(config->directory, key, CACHE_BYTECODE_SUFFIX);
  char* meta_path = jsrt_cache_build_path(config->directory, key, CACHE_META_SUFFIX);
  if (!code_path || !meta_path) {
    config->errors++;
    goto cleanup;
  }

  if (!jsrt_cache_write_atomic(code_path, bytecode_data, bytecode_size)) {
    JSRT_Debug("Failed to write bytecode cache file: %s", code_path);
    config->errors++;
    goto cleanup;
  }

  JSRT_CacheMetadata meta = {0};
  meta.source_path = strdup(source_path);
  if (!meta.source_path) {
    config->errors++;
    remove(code_path);
    goto cleanup;
  }

  struct stat st;
  if (stat(source_path, &st) == 0) {
    meta.mtime = st.st_mtime;
  } else {
    meta.mtime = 0;
  }
  meta.portable = config->portable;
  snprintf(meta.jsrt_version, sizeof(meta.jsrt_version), "%s", JSRT_VERSION);
  snprintf(meta.quickjs_version, sizeof(meta.quickjs_version), "%s", QUICKJS_VERSION);
  meta.bytecode_size = bytecode_size;

  if (!jsrt_cache_write_metadata(meta_path, &meta)) {
    JSRT_Debug("Failed to write cache metadata: %s", meta_path);
    jsrt_cache_metadata_free(&meta);
    remove(code_path);
    config->errors++;
    goto cleanup;
  }

  jsrt_cache_metadata_free(&meta);
  config->writes++;
  success = true;

  // Update LRU tracking and cache size for successful cache write
  jsrt_compile_cache_update_lru(config, key, bytecode_size);

cleanup:
  if (!success) {
    if (meta_path) {
      remove(meta_path);
    }
  }
  if (meta_path) {
    free(meta_path);
  }
  if (code_path) {
    free(code_path);
  }
  if (bytecode_data) {
    free(bytecode_data);
  }
  free(key);
  return success;
}

// ============================================================================
// Cache Statistics
// ============================================================================

void jsrt_compile_cache_get_stats(JSRT_CompileCacheConfig* config, uint64_t* hits, uint64_t* misses, uint64_t* writes,
                                  uint64_t* errors, uint64_t* evictions, size_t* current_size, size_t* size_limit) {
  if (!config) {
    return;
  }

  if (hits)
    *hits = config->hits;
  if (misses)
    *misses = config->misses;
  if (writes)
    *writes = config->writes;
  if (errors)
    *errors = config->errors;
  if (evictions)
    *evictions = config->evictions;
  if (current_size)
    *current_size = config->current_size;
  if (size_limit)
    *size_limit = config->size_limit;
}

int jsrt_compile_cache_flush(JSRT_CompileCacheConfig* config) {
  if (!config || !config->enabled) {
    return 0;
  }

  // TODO: Implement pending writes flush in Task 3.4
  JSRT_Debug("Flushing compile cache (no-op for now)");
  return 0;
}

// ============================================================================
// Cache Maintenance
// ============================================================================

#include <dirent.h>

int jsrt_compile_cache_clear(JSRT_CompileCacheConfig* config) {
  if (!config || !config->enabled || !config->directory) {
    return 0;
  }

  int removed_count = 0;
  DIR* dir = opendir(config->directory);
  if (!dir) {
    JSRT_Debug("Failed to open cache directory for clearing: %s", config->directory);
    return 0;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    // Skip . and .. directories
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Remove files matching our cache patterns (.meta, .jsc)
    size_t name_len = strlen(entry->d_name);
    if ((name_len > 5 && strcmp(entry->d_name + name_len - 5, CACHE_META_SUFFIX) == 0) ||
        (name_len > 4 && strcmp(entry->d_name + name_len - 4, CACHE_BYTECODE_SUFFIX) == 0)) {
      char* full_path = jsrt_cache_build_path(config->directory, entry->d_name, "");
      if (full_path) {
        if (remove(full_path) == 0) {
          removed_count++;
          JSRT_Debug("Removed cache file: %s", entry->d_name);
        }
        free(full_path);
      }
    }
  }

  closedir(dir);

  // Clear LRU tracking
  JSRT_CacheLRUEntry* current = config->lru_head;
  while (current) {
    JSRT_CacheLRUEntry* next = current->next;
    if (current->key) {
      free(current->key);
    }
    free(current);
    current = next;
  }
  config->lru_head = NULL;
  config->lru_tail = NULL;
  config->current_size = 0;

  JSRT_Debug("Cleared compile cache: removed %d files", removed_count);
  return removed_count;
}

// Forward declaration for metadata structure
static void jsrt_cache_metadata_free(JSRT_CacheMetadata* meta);

int jsrt_compile_cache_startup_cleanup(JSRT_CompileCacheConfig* config) {
  if (!config || !config->enabled || !config->directory) {
    return 0;
  }

  int removed_count = 0;
  DIR* dir = opendir(config->directory);
  if (!dir) {
    return 0;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    // Skip . and .. directories
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    size_t name_len = strlen(entry->d_name);

    // Look for .meta files to validate
    if (name_len > 5 && strcmp(entry->d_name + name_len - 5, CACHE_META_SUFFIX) == 0) {
      // Extract key (remove .meta suffix)
      char* key = strndup(entry->d_name, name_len - 5);
      if (!key) {
        continue;
      }

      // Validate cache entry
      bool valid = false;
      char* meta_path = jsrt_cache_build_path(config->directory, key, CACHE_META_SUFFIX);
      char* code_path = jsrt_cache_build_path(config->directory, key, CACHE_BYTECODE_SUFFIX);

      if (meta_path && code_path) {
        JSRT_CacheMetadata meta;
        if (jsrt_cache_read_metadata(meta_path, &meta)) {
          // Check version compatibility
          bool version_match =
              (strcmp(meta.jsrt_version, JSRT_VERSION) == 0) && (strcmp(meta.quickjs_version, QUICKJS_VERSION) == 0);

          if (version_match && access(code_path, F_OK) == 0) {
            // Check if source file still exists and hasn't been modified (non-portable mode)
            if (config->portable || access(meta.source_path, F_OK) == 0) {
              if (config->portable) {
                valid = true;  // Portable mode: don't check source mtime
              } else {
                struct stat st;
                if (stat(meta.source_path, &st) == 0 && st.st_mtime == meta.mtime) {
                  valid = true;
                }
              }
            }
          }

          jsrt_cache_metadata_free(&meta);
        }
      }

      // Remove invalid entries
      if (!valid) {
        JSRT_Debug("Removing stale cache entry: %s", key);
        jsrt_cache_remove_entry(config->directory, key);
        removed_count++;
      }

      if (meta_path) {
        free(meta_path);
      }
      if (code_path) {
        free(code_path);
      }
      free(key);
    }
  }

  closedir(dir);

  if (removed_count > 0) {
    JSRT_Debug("Startup cleanup removed %d stale entries", removed_count);
  }

  return removed_count;
}

bool jsrt_compile_cache_maybe_evict(JSRT_CompileCacheConfig* config, size_t added_size) {
  if (!config || !config->enabled || added_size == 0) {
    return false;
  }

  // Check if adding this entry would exceed the size limit
  if (config->current_size + added_size <= config->size_limit) {
    return false;  // No eviction needed
  }

  size_t target_size = config->size_limit * 0.8;  // Target 80% of limit
  size_t bytes_to_free = (config->current_size + added_size) - target_size;
  size_t bytes_freed = 0;
  int entries_evicted = 0;

  JSRT_Debug("Cache eviction needed: current=%zu, adding=%zu, limit=%zu, target=%zu", config->current_size, added_size,
             config->size_limit, target_size);

  // Evict least recently used entries
  while (config->lru_tail && bytes_freed < bytes_to_free) {
    JSRT_CacheLRUEntry* lru_entry = config->lru_tail;

    // Remove the cache entry from disk
    jsrt_cache_remove_entry(config->directory, lru_entry->key);
    bytes_freed += lru_entry->size;
    entries_evicted++;

    JSRT_Debug("Evicted LRU entry: %s (size: %zu bytes)", lru_entry->key, lru_entry->size);

    // Remove from LRU list
    jsrt_compile_cache_remove_lru(config, lru_entry->key);
  }

  if (entries_evicted > 0) {
    config->evictions += entries_evicted;
    config->current_size -= bytes_freed;
    JSRT_Debug("Cache eviction completed: evicted %d entries, freed %zu bytes", entries_evicted, bytes_freed);
    return true;
  }

  return false;
}

void jsrt_compile_cache_update_lru(JSRT_CompileCacheConfig* config, const char* key, size_t size) {
  if (!config || !key) {
    return;
  }

  time_t now = time(NULL);

  // Check if entry already exists in LRU list
  JSRT_CacheLRUEntry* current = config->lru_head;
  while (current) {
    if (strcmp(current->key, key) == 0) {
      // Entry exists - move to front and update
      current->access_time = now;
      current->size = size;

      // Remove from current position
      if (current->prev) {
        current->prev->next = current->next;
      } else {
        // Already at head
        config->lru_head = current->next;
      }

      if (current->next) {
        current->next->prev = current->prev;
      } else {
        // Currently at tail
        config->lru_tail = current->prev;
      }

      // Move to front
      current->prev = NULL;
      current->next = config->lru_head;
      if (config->lru_head) {
        config->lru_head->prev = current;
      }
      config->lru_head = current;

      if (!config->lru_tail) {
        config->lru_tail = current;
      }

      return;
    }
    current = current->next;
  }

  // Entry doesn't exist - create new LRU entry
  JSRT_CacheLRUEntry* new_entry = malloc(sizeof(JSRT_CacheLRUEntry));
  if (!new_entry) {
    return;
  }

  new_entry->key = strdup(key);
  if (!new_entry->key) {
    free(new_entry);
    return;
  }

  new_entry->access_time = now;
  new_entry->size = size;
  new_entry->prev = NULL;
  new_entry->next = config->lru_head;

  if (config->lru_head) {
    config->lru_head->prev = new_entry;
  }
  config->lru_head = new_entry;

  if (!config->lru_tail) {
    config->lru_tail = new_entry;
  }

  // Update cache size (only for new entries)
  config->current_size += size;
}

void jsrt_compile_cache_remove_lru(JSRT_CompileCacheConfig* config, const char* key) {
  if (!config || !key) {
    return;
  }

  JSRT_CacheLRUEntry* current = config->lru_head;
  while (current) {
    if (strcmp(current->key, key) == 0) {
      // Update cache size
      config->current_size -= current->size;

      // Remove from LRU list
      if (current->prev) {
        current->prev->next = current->next;
      } else {
        config->lru_head = current->next;
      }

      if (current->next) {
        current->next->prev = current->prev;
      } else {
        config->lru_tail = current->prev;
      }

      // Free the entry
      if (current->key) {
        free(current->key);
      }
      free(current);

      return;
    }
    current = current->next;
  }
}

size_t jsrt_compile_cache_get_disk_size(const char* directory) {
  if (!directory) {
    return 0;
  }

  size_t total_size = 0;
  DIR* dir = opendir(directory);
  if (!dir) {
    return 0;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    // Skip . and .. directories
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Check for cache files
    size_t name_len = strlen(entry->d_name);
    if ((name_len > 5 && strcmp(entry->d_name + name_len - 5, CACHE_META_SUFFIX) == 0) ||
        (name_len > 4 && strcmp(entry->d_name + name_len - 4, CACHE_BYTECODE_SUFFIX) == 0)) {
      char* full_path = jsrt_cache_build_path(directory, entry->d_name, "");
      if (full_path) {
        struct stat st;
        if (stat(full_path, &st) == 0) {
          total_size += st.st_size;
        }
        free(full_path);
      }
    }
  }

  closedir(dir);
  return total_size;
}
