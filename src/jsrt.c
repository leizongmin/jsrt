#include "jsrt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>

#include "runtime.h"
#include "util/file.h"
#include "util/http_client.h"

// External references to global variables (defined in node/process/module.c)
extern int jsrt_argc;
extern char** jsrt_argv;

// Forward declarations
static bool is_url(const char* str);
static JSRT_ReadFileResult download_url(const char* url);

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

  // Check if filename is a URL and handle accordingly
  if (is_url(filename)) {
    file = download_url(filename);
  } else {
    file = JSRT_ReadFile(filename);
  }

  if (file.error != JSRT_READ_FILE_OK) {
    fprintf(stderr, "Error: %s\n", JSRT_ReadFileErrorToString(file.error));
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