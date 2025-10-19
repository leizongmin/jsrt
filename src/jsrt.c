#include "jsrt.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "build.h"
#include "runtime.h"
#include "std/module.h"
#include "util/file.h"
#include "util/http_client.h"

// External references to global variables (defined in node/process/module.c)
extern int jsrt_argc;
extern char** jsrt_argv;

// Forward declarations
static bool is_url(const char* str);
static JSRT_ReadFileResult download_url(const char* url);
static void print_module_not_found_error(const char* filename);
static const char* jsrt_get_version_string(void);
static char* jsrt_cli_resolve_path(const char* filename);
static char* jsrt_cli_normalize_path(const char* path);
static bool jsrt_cli_is_absolute_path(const char* path);

// Helper function to check if a string is a URL
static bool is_url(const char* str) {
  return (strncmp(str, "http://", 7) == 0 || strncmp(str, "https://", 8) == 0 || strncmp(str, "file://", 7) == 0);
}

// Helper function to download URL content using native HTTP client
static JSRT_ReadFileResult download_url(const char* url) {
  JSRT_ReadFileResult result = JSRT_ReadFileResultDefault();

  // Handle file:// URLs
  if (strncmp(url, "file://", 7) == 0) {
    const char* filepath = url + 7;
    return JSRT_ReadFile(filepath);
  }

  // Handle http:// URLs using direct HTTP client
  if (strncmp(url, "http://", 7) == 0) {
    JSRT_HttpResponse http_response = JSRT_HttpGet(url);

    if (http_response.error != JSRT_HTTP_OK) {
      result.error = JSRT_READ_FILE_ERROR_FILE_NOT_FOUND;
      JSRT_HttpResponseFree(&http_response);
      return result;
    }

    // Check HTTP status
    if (http_response.status < 200 || http_response.status >= 300) {
      result.error = JSRT_READ_FILE_ERROR_FILE_NOT_FOUND;
      JSRT_HttpResponseFree(&http_response);
      return result;
    }

    // Copy the response body
    if (http_response.body && http_response.body_size > 0) {
      result.data = malloc(http_response.body_size + 1);
      if (result.data) {
        memcpy(result.data, http_response.body, http_response.body_size);
        result.data[http_response.body_size] = '\0';
        result.size = http_response.body_size;
        result.error = JSRT_READ_FILE_OK;
      } else {
        result.error = JSRT_READ_FILE_ERROR_OUT_OF_MEMORY;
      }
    } else {
      // Empty response is still valid
      result.data = malloc(1);
      if (result.data) {
        result.data[0] = '\0';
        result.size = 0;
        result.error = JSRT_READ_FILE_OK;
      } else {
        result.error = JSRT_READ_FILE_ERROR_OUT_OF_MEMORY;
      }
    }

    JSRT_HttpResponseFree(&http_response);
    return result;
  }

  // HTTPS not supported by simple HTTP client - could be extended later
  if (strncmp(url, "https://", 8) == 0) {
    result.error = JSRT_READ_FILE_ERROR_FILE_NOT_FOUND;
    return result;
  }

  // Unsupported protocol
  result.error = JSRT_READ_FILE_ERROR_FILE_NOT_FOUND;
  return result;
}

int JSRT_CmdRunFile(const char* filename, bool compact_node, int argc, char** argv) {
  // Store command line arguments for process module
  jsrt_argc = argc;
  jsrt_argv = argv;
  int ret = 0;
  JSRT_Runtime* rt = JSRT_RuntimeNew();

  // Enable compact node mode if requested
  if (compact_node) {
    JSRT_RuntimeSetCompactNodeMode(rt, true);
  }

  JSRT_ReadFileResult file = JSRT_ReadFileResultDefault();
  JSRT_EvalResult res = JSRT_EvalResultDefault();
  JSRT_EvalResult res2 = JSRT_EvalResultDefault();
  char* entry_path = NULL;

  if (is_url(filename)) {
    JSRT_StdCommonJSSetEntryPath(NULL);
  } else {
    entry_path = jsrt_cli_resolve_path(filename);
    const char* path_to_set = entry_path ? entry_path : filename;
    JSRT_StdCommonJSSetEntryPath(path_to_set);
  }

  // Check if filename is a URL and handle accordingly
  if (is_url(filename)) {
    file = download_url(filename);
  } else {
    file = JSRT_ReadFile(filename);
  }

  if (file.error != JSRT_READ_FILE_OK) {
    if (file.error == JSRT_READ_FILE_ERROR_FILE_NOT_FOUND) {
      print_module_not_found_error(filename);
    } else {
      fprintf(stderr, "Error: %s\n", JSRT_ReadFileErrorToString(file.error));
    }
    ret = 1;
    goto end;
  }

  res = JSRT_RuntimeEval(rt, filename, file.data, file.size);
  if (res.is_error) {
    fprintf(stderr, "%s\n", res.error);
    ret = 1;
    goto end;
  }

  res2 = JSRT_RuntimeAwaitEvalResult(rt, &res);
  if (res2.is_error) {
    fprintf(stderr, "%s\n", res2.error);
    ret = 1;
    goto end;
  }

  JSRT_RuntimeRun(rt);

end:
  JSRT_EvalResultFree(&res2);
  JSRT_EvalResultFree(&res);
  JSRT_ReadFileResultFree(&file);
  if (entry_path) {
    free(entry_path);
  }
  JSRT_RuntimeFree(rt);
  return ret;
}

int JSRT_CmdRunStdin(bool compact_node, int argc, char** argv) {
  // Store command line arguments for process module
  jsrt_argc = argc;
  jsrt_argv = argv;
  int ret = 0;
  JSRT_Runtime* rt = JSRT_RuntimeNew();

  // Enable compact node mode if requested
  if (compact_node) {
    JSRT_RuntimeSetCompactNodeMode(rt, true);
  }

  JSRT_EvalResult res = JSRT_EvalResultDefault();
  JSRT_EvalResult res2 = JSRT_EvalResultDefault();

  // Read from stdin
  char* code = NULL;
  size_t code_size = 0;
  size_t code_capacity = 1024;

  code = malloc(code_capacity);
  if (!code) {
    fprintf(stderr, "Error: Failed to allocate memory for stdin input\n");
    ret = 1;
    goto end;
  }

  int c;
  while ((c = getchar()) != EOF) {
    if (code_size >= code_capacity - 1) {
      code_capacity *= 2;
      char* new_code = realloc(code, code_capacity);
      if (!new_code) {
        fprintf(stderr, "Error: Failed to reallocate memory for stdin input\n");
        ret = 1;
        goto end;
      }
      code = new_code;
    }
    code[code_size++] = c;
  }
  code[code_size] = '\0';

  if (code_size == 0) {
    fprintf(stderr, "Error: No input provided\n");
    ret = 1;
    goto end;
  }

  res = JSRT_RuntimeEval(rt, "<stdin>", code, code_size);
  if (res.is_error) {
    fprintf(stderr, "%s\n", res.error);
    ret = 1;
    goto end;
  }

  res2 = JSRT_RuntimeAwaitEvalResult(rt, &res);
  if (res2.is_error) {
    fprintf(stderr, "%s\n", res2.error);
    ret = 1;
    goto end;
  }

  JSRT_RuntimeRun(rt);

end:
  if (code)
    free(code);
  JSRT_EvalResultFree(&res2);
  JSRT_EvalResultFree(&res);
  JSRT_RuntimeFree(rt);
  return ret;
}

int JSRT_CmdRunEval(const char* code, int argc, char** argv) {
  // Store command line arguments for process module
  jsrt_argc = argc;
  jsrt_argv = argv;
  int ret = 0;
  JSRT_Runtime* rt = JSRT_RuntimeNew();

  JSRT_EvalResult res = JSRT_EvalResultDefault();
  JSRT_EvalResult res2 = JSRT_EvalResultDefault();

  res = JSRT_RuntimeEval(rt, "<eval>", code, strlen(code));
  if (res.is_error) {
    fprintf(stderr, "%s\n", res.error);
    ret = 1;
    goto end;
  }

  res2 = JSRT_RuntimeAwaitEvalResult(rt, &res);
  if (res2.is_error) {
    fprintf(stderr, "%s\n", res2.error);
    ret = 1;
    goto end;
  }

  JSRT_RuntimeRun(rt);

end:
  JSRT_EvalResultFree(&res2);
  JSRT_EvalResultFree(&res);
  JSRT_RuntimeFree(rt);
  return ret;
}

int JSRT_CmdRunEmbeddedBytecode(const char* executable_path, int argc, char** argv) {
  // Read the executable file to check for embedded bytecode
  FILE* exe_file = fopen(executable_path, "rb");
  if (!exe_file) {
    // Silently return non-zero to indicate no embedded bytecode
    return 1;
  }

  // Seek to end to check for footer signature
  fseek(exe_file, 0, SEEK_END);
  long exe_size = ftell(exe_file);

  // Check minimum size for boundary + 8-byte size
  const char* boundary = "JSRT_BYTECODE_BOUNDARY";
  size_t boundary_len = strlen(boundary);
  size_t min_size = boundary_len + 8;  // boundary + 8-byte size

  if (exe_size < min_size) {
    // No embedded bytecode, silently return
    fclose(exe_file);
    return 1;
  }

  // Read the 8-byte size from the end
  fseek(exe_file, exe_size - 8, SEEK_SET);
  uint64_t size_be = 0;
  if (fread(&size_be, 1, 8, exe_file) != 8) {
    // Failed to read size, silently return
    fclose(exe_file);
    return 1;
  }

  // Convert from big-endian
  long bytecode_size = 0;
  for (int i = 0; i < 8; i++) {
    bytecode_size = (bytecode_size << 8) | ((size_be >> (8 * (7 - i))) & 0xFF);
  }

  if (bytecode_size <= 0 || bytecode_size > exe_size) {
    // Invalid bytecode size, silently return
    fclose(exe_file);
    return 1;
  }

  // Check for boundary signature
  fseek(exe_file, exe_size - 8 - boundary_len, SEEK_SET);
  char boundary_check[64];
  if (fread(boundary_check, 1, boundary_len, exe_file) != boundary_len) {
    // Failed to read boundary, silently return
    fclose(exe_file);
    return 1;
  }
  boundary_check[boundary_len] = '\0';

  if (strcmp(boundary_check, boundary) != 0) {
    // No JSRT boundary found, silently return
    fclose(exe_file);
    return 1;
  }

  // Calculate bytecode start position
  long bytecode_start = exe_size - 8 - boundary_len - bytecode_size;

  // Allocate buffer for bytecode
  char* bytecode = malloc(bytecode_size);
  if (!bytecode) {
    fprintf(stderr, "Error: Memory allocation failed for bytecode\n");
    fclose(exe_file);
    return 1;
  }

  // Read bytecode
  fseek(exe_file, bytecode_start, SEEK_SET);
  size_t bytecode_read = fread(bytecode, 1, bytecode_size, exe_file);
  fclose(exe_file);

  if (bytecode_read != bytecode_size) {
    fprintf(stderr, "Error: Failed to read complete bytecode\n");
    free(bytecode);
    return 1;
  }

  // Now that we have the bytecode, initialize the runtime
  jsrt_argc = argc;
  jsrt_argv = argv;
  int ret = 0;
  JSRT_Runtime* rt = JSRT_RuntimeNew();
  if (!rt) {
    fprintf(stderr, "Error: Failed to create runtime\n");
    free(bytecode);
    return 1;
  }
  JSRT_EvalResult res = JSRT_EvalResultDefault();
  JSRT_EvalResult res2 = JSRT_EvalResultDefault();

  // Load and execute bytecode using QuickJS
  JSValue obj = JS_ReadObject(rt->ctx, (const uint8_t*)bytecode, bytecode_size, JS_READ_OBJ_BYTECODE);
  free(bytecode);

  if (JS_IsException(obj)) {
    res.is_error = true;
    JSValue exception = JS_GetException(rt->ctx);
    res.error = JSRT_RuntimeGetExceptionString(rt, exception);
    JS_FreeValue(rt->ctx, exception);
    fprintf(stderr, "Error loading bytecode: %s\n", res.error);
    JS_FreeValue(rt->ctx, obj);
    ret = 1;
    goto end;
  }

  // Execute the loaded bytecode
  JSValue result = JS_EvalFunction(rt->ctx, obj);
  if (JS_IsException(result)) {
    res.is_error = true;
    JSValue exception = JS_GetException(rt->ctx);
    res.error = JSRT_RuntimeGetExceptionString(rt, exception);
    JS_FreeValue(rt->ctx, exception);
    fprintf(stderr, "Error executing bytecode: %s\n", res.error);
    JS_FreeValue(rt->ctx, result);
    ret = 1;
    goto end;
  }

  JS_FreeValue(rt->ctx, result);

  JSRT_RuntimeRun(rt);

end:
  if (res.error) {
    free(res.error);
  }
  JSRT_EvalResultFree(&res2);
  JSRT_EvalResultFree(&res);
  JSRT_RuntimeFree(rt);
  return ret;
}

#ifdef _WIN32
#define JSRT_CLI_PATH_SEPARATOR_STR "\\"
#define JSRT_CLI_IS_PATH_SEPARATOR(c) ((c) == '\\' || (c) == '/')
#else
#define JSRT_CLI_PATH_SEPARATOR_STR "/"
#define JSRT_CLI_IS_PATH_SEPARATOR(c) ((c) == '/')
#endif

#define JSRT_CLI_MAX_PATH_SEGMENTS 256

static const char* jsrt_get_version_string(void) {
#ifdef JSRT_VERSION
  return JSRT_VERSION;
#else
  return "unknown";
#endif
}

static char* jsrt_cli_get_cwd(void) {
  size_t size = PATH_MAX;
  char* buffer = (char*)malloc(size + 1);
  if (!buffer) {
    return NULL;
  }

  int rc = uv_cwd(buffer, &size);
  if (rc == UV_ENOBUFS) {
    char* resized = (char*)realloc(buffer, size + 1);
    if (!resized) {
      free(buffer);
      return NULL;
    }
    buffer = resized;
    rc = uv_cwd(buffer, &size);
  }

  if (rc != 0) {
    free(buffer);
    return NULL;
  }

  buffer[size] = '\0';
  return buffer;
}

static bool jsrt_cli_is_absolute_path(const char* path) {
  if (!path || !*path) {
    return false;
  }

#ifdef _WIN32
  if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) {
    return path[1] == ':' && JSRT_CLI_IS_PATH_SEPARATOR(path[2]);
  }
  return JSRT_CLI_IS_PATH_SEPARATOR(path[0]);
#else
  return path[0] == '/';
#endif
}

static char* jsrt_cli_normalize_path(const char* path) {
  if (!path || !*path) {
    return NULL;
  }

  size_t len = strlen(path);
  char* normalized = (char*)malloc(len + 1);
  if (!normalized) {
    return NULL;
  }

  for (size_t i = 0; i < len; i++) {
#ifdef _WIN32
    if (path[i] == '/') {
      normalized[i] = '\\';
    } else {
      normalized[i] = path[i];
    }
#else
    if (path[i] == '\\') {
      normalized[i] = '/';
    } else {
      normalized[i] = path[i];
    }
#endif
  }
  normalized[len] = '\0';

  bool is_absolute = jsrt_cli_is_absolute_path(normalized);
  char* result = (char*)malloc(len + 3);
  if (!result) {
    free(normalized);
    return NULL;
  }

  char* segments[JSRT_CLI_MAX_PATH_SEGMENTS];
  size_t segment_count = 0;

#ifdef _WIN32
  bool is_drive_path = false;
  char drive_prefix[4] = {0};
  if (is_absolute && len >= 2 &&
      ((normalized[0] >= 'A' && normalized[0] <= 'Z') || (normalized[0] >= 'a' && normalized[0] <= 'z')) &&
      normalized[1] == ':') {
    is_drive_path = true;
    drive_prefix[0] = normalized[0];
    drive_prefix[1] = ':';
    drive_prefix[2] = '\0';
  }
#endif

  char* token = strtok(normalized, JSRT_CLI_PATH_SEPARATOR_STR);
  while (token && segment_count < JSRT_CLI_MAX_PATH_SEGMENTS) {
    if (strcmp(token, ".") == 0) {
      token = strtok(NULL, JSRT_CLI_PATH_SEPARATOR_STR);
      continue;
    }

    if (strcmp(token, "..") == 0) {
      if (segment_count > 0 && strcmp(segments[segment_count - 1], "..") != 0) {
        segment_count--;
      } else if (!is_absolute && segment_count < JSRT_CLI_MAX_PATH_SEGMENTS) {
        segments[segment_count++] = token;
      }
    } else if (segment_count < JSRT_CLI_MAX_PATH_SEGMENTS) {
      segments[segment_count++] = token;
    }

    token = strtok(NULL, JSRT_CLI_PATH_SEPARATOR_STR);
  }

  result[0] = '\0';

#ifdef _WIN32
  if (is_drive_path) {
    strcpy(result, drive_prefix);
  } else if (is_absolute) {
    strcpy(result, JSRT_CLI_PATH_SEPARATOR_STR);
  }
#else
  if (is_absolute) {
    strcpy(result, JSRT_CLI_PATH_SEPARATOR_STR);
  }
#endif

  for (size_t i = 0; i < segment_count; i++) {
#ifdef _WIN32
    if (is_drive_path) {
      strcat(result, JSRT_CLI_PATH_SEPARATOR_STR);
      strcat(result, segments[i]);
    } else {
#endif
      if (i > 0) {
        strcat(result, JSRT_CLI_PATH_SEPARATOR_STR);
      }
      strcat(result, segments[i]);
#ifdef _WIN32
    }
#endif
  }

  if (result[0] == '\0') {
#ifdef _WIN32
    if (is_drive_path) {
      strcat(result, JSRT_CLI_PATH_SEPARATOR_STR);
    } else {
      strcpy(result, ".");
    }
#else
    strcpy(result, is_absolute ? JSRT_CLI_PATH_SEPARATOR_STR : ".");
#endif
  }

  free(normalized);
  return result;
}

static char* jsrt_cli_resolve_path(const char* filename) {
  if (!filename) {
    return NULL;
  }

  if (jsrt_cli_is_absolute_path(filename)) {
    return jsrt_cli_normalize_path(filename);
  }

  char* cwd = jsrt_cli_get_cwd();
  if (!cwd) {
    return NULL;
  }

  size_t cwd_len = strlen(cwd);
  size_t filename_len = strlen(filename);
  bool needs_separator = cwd_len > 0 && !JSRT_CLI_IS_PATH_SEPARATOR(cwd[cwd_len - 1]);

  size_t combined_len = cwd_len + (needs_separator ? 1 : 0) + filename_len + 1;
  char* combined = (char*)malloc(combined_len);
  if (!combined) {
    free(cwd);
    return NULL;
  }

  if (needs_separator) {
    snprintf(combined, combined_len, "%s%s%s", cwd, JSRT_CLI_PATH_SEPARATOR_STR, filename);
  } else {
    snprintf(combined, combined_len, "%s%s", cwd, filename);
  }

  free(cwd);
  char* normalized = jsrt_cli_normalize_path(combined);
  free(combined);
  return normalized;
}

static void print_module_not_found_error(const char* filename) {
  char* resolved = NULL;
  if (!is_url(filename)) {
    resolved = jsrt_cli_resolve_path(filename);
  }

  const char* display = resolved ? resolved : filename;

  char* message = NULL;
  char* stack = NULL;
  JSRT_StdModuleBuildNotFoundStrings(display, NULL, false, &message, &stack);

  if (stack) {
    fprintf(stderr, "%s\n\n", stack);
  } else {
    fprintf(stderr, "Error: Cannot find module '%s'\n\n", display);
  }

  fprintf(stderr, "jsrt v%s\n", jsrt_get_version_string());

  free(message);
  free(stack);
  free(resolved);
}
