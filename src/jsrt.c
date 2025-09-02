#include "jsrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "runtime.h"
#include "std/process.h"
#include "util/file.h"

int JSRT_CmdRunFile(const char *filename, int argc, char **argv) {
  // Store command line arguments for process module
  g_jsrt_argc = argc;
  g_jsrt_argv = argv;
  int ret = 0;
  JSRT_Runtime *rt = JSRT_RuntimeNew();

  JSRT_ReadFileResult file = JSRT_ReadFileResultDefault();
  JSRT_EvalResult res = JSRT_EvalResultDefault();
  JSRT_EvalResult res2 = JSRT_EvalResultDefault();

  file = JSRT_ReadFile(filename);
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

int JSRT_CmdRunStdin(int argc, char **argv) {
  // Store command line arguments for process module
  g_jsrt_argc = argc;
  g_jsrt_argv = argv;
  int ret = 0;
  JSRT_Runtime *rt = JSRT_RuntimeNew();

  JSRT_EvalResult res = JSRT_EvalResultDefault();
  JSRT_EvalResult res2 = JSRT_EvalResultDefault();

  // Read from stdin
  char *code = NULL;
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
      char *new_code = realloc(code, code_capacity);
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
  if (code) {
    free(code);
  }
  JSRT_EvalResultFree(&res2);
  JSRT_EvalResultFree(&res);
  JSRT_RuntimeFree(rt);
  return ret;
}

int JSRT_CmdRunEmbeddedBytecode(const char *executable_path, int argc, char **argv) {
  // Store command line arguments for process module
  g_jsrt_argc = argc;
  g_jsrt_argv = argv;
  int ret = 0;
  JSRT_Runtime *rt = JSRT_RuntimeNew();
  JSRT_EvalResult res = JSRT_EvalResultDefault();
  JSRT_EvalResult res2 = JSRT_EvalResultDefault();

  // Read the executable file to extract embedded bytecode
  FILE *exe_file = fopen(executable_path, "rb");
  if (!exe_file) {
    fprintf(stderr, "Error: Cannot open executable file '%s'\n", executable_path);
    ret = 1;
    goto end;
  }

  // Seek to end to check for footer signature
  fseek(exe_file, 0, SEEK_END);
  long exe_size = ftell(exe_file);

  // Check minimum size for boundary + 8-byte size
  const char *boundary = "JSRT_BYTECODE_BOUNDARY";
  size_t boundary_len = strlen(boundary);
  size_t min_size = boundary_len + 8;  // boundary + 8-byte size

  if (exe_size < min_size) {
    fprintf(stderr, "Error: No embedded bytecode found\n");
    fclose(exe_file);
    ret = 1;
    goto end;
  }

  // Read the 8-byte size from the end
  fseek(exe_file, exe_size - 8, SEEK_SET);
  uint64_t size_be = 0;
  fread(&size_be, 1, 8, exe_file);

  // Convert from big-endian
  long bytecode_size = 0;
  for (int i = 0; i < 8; i++) {
    bytecode_size = (bytecode_size << 8) | ((size_be >> (8 * (7 - i))) & 0xFF);
  }

  if (bytecode_size <= 0 || bytecode_size > exe_size) {
    fprintf(stderr, "Error: Invalid bytecode size in executable\n");
    fclose(exe_file);
    ret = 1;
    goto end;
  }

  // Check for boundary signature
  fseek(exe_file, exe_size - 8 - boundary_len, SEEK_SET);
  char boundary_check[64];
  fread(boundary_check, 1, boundary_len, exe_file);
  boundary_check[boundary_len] = '\0';

  if (strcmp(boundary_check, boundary) != 0) {
    fprintf(stderr, "Error: No JSRT boundary found in executable\n");
    fclose(exe_file);
    ret = 1;
    goto end;
  }

  // Calculate bytecode start position
  long bytecode_start = exe_size - 8 - boundary_len - bytecode_size;

  // Allocate buffer for bytecode
  char *bytecode = malloc(bytecode_size);
  if (!bytecode) {
    fprintf(stderr, "Error: Memory allocation failed for bytecode\n");
    fclose(exe_file);
    ret = 1;
    goto end;
  }

  // Read bytecode
  fseek(exe_file, bytecode_start, SEEK_SET);
  size_t bytecode_read = fread(bytecode, 1, bytecode_size, exe_file);
  fclose(exe_file);

  if (bytecode_read != bytecode_size) {
    fprintf(stderr, "Error: Failed to read complete bytecode\n");
    free(bytecode);
    ret = 1;
    goto end;
  }

  // Load and execute bytecode using QuickJS
  JSValue obj = JS_ReadObject(rt->ctx, (const uint8_t *)bytecode, bytecode_size, JS_READ_OBJ_BYTECODE);
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
