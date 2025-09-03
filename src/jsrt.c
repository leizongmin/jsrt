#include "jsrt.h"

#include <history.h>
#include <readline.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>

#include "runtime.h"
#include "std/console.h"
#include "std/process.h"
#include "util/dbuf.h"
#include "util/file.h"
#include "util/http_client.h"

// Forward declarations
static bool is_url(const char *str);
static JSRT_ReadFileResult download_url(const char *url);

// Helper function to check if a string is a URL
static bool is_url(const char *str) {
  return (strncmp(str, "http://", 7) == 0 || strncmp(str, "https://", 8) == 0 || strncmp(str, "file://", 7) == 0);
}

// Helper function to download URL content using native HTTP client
static JSRT_ReadFileResult download_url(const char *url) {
  JSRT_ReadFileResult result = JSRT_ReadFileResultDefault();

  // Handle file:// URLs
  if (strncmp(url, "file://", 7) == 0) {
    const char *filepath = url + 7;
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

int JSRT_CmdRunFile(const char *filename, int argc, char **argv) {
  // Store command line arguments for process module
  g_jsrt_argc = argc;
  g_jsrt_argv = argv;
  int ret = 0;
  JSRT_Runtime *rt = JSRT_RuntimeNew();

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
  if (code) free(code);
  JSRT_EvalResultFree(&res2);
  JSRT_EvalResultFree(&res);
  JSRT_RuntimeFree(rt);
  return ret;
}

int JSRT_CmdRunEval(const char *code, int argc, char **argv) {
  // Store command line arguments for process module
  g_jsrt_argc = argc;
  g_jsrt_argv = argv;
  int ret = 0;
  JSRT_Runtime *rt = JSRT_RuntimeNew();

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

int JSRT_CmdRunEmbeddedBytecode(const char *executable_path, int argc, char **argv) {
  // Read the executable file to check for embedded bytecode
  FILE *exe_file = fopen(executable_path, "rb");
  if (!exe_file) {
    // Silently return non-zero to indicate no embedded bytecode
    return 1;
  }

  // Seek to end to check for footer signature
  fseek(exe_file, 0, SEEK_END);
  long exe_size = ftell(exe_file);

  // Check minimum size for boundary + 8-byte size
  const char *boundary = "JSRT_BYTECODE_BOUNDARY";
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
  fread(&size_be, 1, 8, exe_file);

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
  fread(boundary_check, 1, boundary_len, exe_file);
  boundary_check[boundary_len] = '\0';

  if (strcmp(boundary_check, boundary) != 0) {
    // No JSRT boundary found, silently return
    fclose(exe_file);
    return 1;
  }

  // Calculate bytecode start position
  long bytecode_start = exe_size - 8 - boundary_len - bytecode_size;

  // Allocate buffer for bytecode
  char *bytecode = malloc(bytecode_size);
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
  g_jsrt_argc = argc;
  g_jsrt_argv = argv;
  int ret = 0;
  JSRT_Runtime *rt = JSRT_RuntimeNew();
  if (!rt) {
    fprintf(stderr, "Error: Failed to create runtime\n");
    free(bytecode);
    return 1;
  }
  JSRT_EvalResult res = JSRT_EvalResultDefault();
  JSRT_EvalResult res2 = JSRT_EvalResultDefault();

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
// Global variable to track CTRL-C count for REPL exit
static volatile int g_ctrl_c_count = 0;

// Signal handler for CTRL-C in REPL
static void jsrt_repl_sigint_handler(int sig) {
  (void)sig;  // Suppress unused parameter warning
  g_ctrl_c_count++;
  if (g_ctrl_c_count >= 2) {
    printf("\n(To exit, press ^C again or ^D or type /exit)\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
    exit(0);
  } else {
    printf("\n(To exit, press ^C again or ^D or type /exit)\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
  }
}

// Get REPL history file path
static char *jsrt_get_repl_history_path() {
  char *history_path = getenv("JSRT_REPL_HISTORY");
  if (history_path && strlen(history_path) > 0) {
    return strdup(history_path);
  }

  char *home = getenv("HOME");
  if (!home) {
    return strdup(".jsrt_repl");
  }

  size_t len = strlen(home) + strlen("/.jsrt_repl") + 1;
  char *path = malloc(len);
  snprintf(path, len, "%s/.jsrt_repl", home);
  return path;
}

// Check if JavaScript code is syntactically complete
static bool jsrt_is_code_complete(JSContext *ctx, const char *code, size_t code_len) {
  // First, try to compile the code without executing it
  JSValue result = JS_Eval(ctx, code, code_len, "<repl-check>", JS_EVAL_FLAG_COMPILE_ONLY);

  if (JS_IsException(result)) {
    JS_FreeValue(ctx, result);

    // If compilation failed, do a simple brace/bracket/paren counting check
    // to see if the code is obviously incomplete
    int braces = 0, brackets = 0, parens = 0;
    bool in_string = false, in_comment = false;
    char string_char = 0;

    for (size_t i = 0; i < code_len; i++) {
      char c = code[i];

      if (in_comment) {
        // Handle single-line comments
        if (c == '\n') {
          in_comment = false;
        }
        continue;
      }

      if (in_string) {
        // Handle string escaping
        if (c == '\\' && i + 1 < code_len) {
          i++;  // Skip next character
          continue;
        }
        if (c == string_char) {
          in_string = false;
          string_char = 0;
        }
        continue;
      }

      // Check for comment start
      if (c == '/' && i + 1 < code_len && code[i + 1] == '/') {
        in_comment = true;
        i++;  // Skip next '/'
        continue;
      }

      // Check for string start
      if (c == '"' || c == '\'' || c == '`') {
        in_string = true;
        string_char = c;
        continue;
      }

      // Count brackets and braces
      switch (c) {
        case '{':
          braces++;
          break;
        case '}':
          braces--;
          break;
        case '[':
          brackets++;
          break;
        case ']':
          brackets--;
          break;
        case '(':
          parens++;
          break;
        case ')':
          parens--;
          break;
      }
    }

    // If we're in a string or have unmatched brackets, the code is incomplete
    if (in_string || braces > 0 || brackets > 0 || parens > 0) {
      return false;  // Incomplete
    }

    // If brackets are balanced but compilation failed, try some specific error patterns
    JSValue exception = JS_GetException(ctx);
    const char *exception_str = JS_ToCString(ctx, exception);
    bool is_incomplete = false;

    if (exception_str) {
      // Only check for very specific incomplete patterns to avoid false positives
      if (strstr(exception_str, "unexpected token in expression: ''") ||
          strstr(exception_str, "unexpected end of input") || strstr(exception_str, "unterminated string literal") ||
          strstr(exception_str, "unterminated comment")) {
        is_incomplete = true;
      }

      JS_FreeCString(ctx, exception_str);
    }

    JS_FreeValue(ctx, exception);
    return !is_incomplete;
  }

  // Code compiles successfully - it's complete
  JS_FreeValue(ctx, result);
  return true;
}

// Process REPL shortcuts
static bool jsrt_process_repl_shortcut(const char *input) {
  if (strcmp(input, "/exit") == 0 || strcmp(input, "/quit") == 0) {
    printf("Goodbye!\n");
    return true;  // Exit REPL
  }

  if (strcmp(input, "/help") == 0) {
    printf("JSRT REPL Commands:\n");
    printf("  /help     - Show this help message\n");
    printf("  /exit     - Exit REPL (also Ctrl+C twice or Ctrl+D)\n");
    printf("  /quit     - Exit REPL (same as /exit)\n");
    printf("  /clear    - Clear screen\n");
    printf("\nEnvironment Variables:\n");
    printf("  JSRT_REPL_HISTORY - Custom path for history file (default: ~/.jsrt_repl)\n");
    printf("\nKeyboard shortcuts:\n");
    printf("  Ctrl+C    - Interrupt current operation (twice to exit)\n");
    printf("  Ctrl+D    - Exit REPL\n");
    printf("  Up/Down   - Navigate command history\n");
    printf("  Left/Right- Navigate within current line\n");
    return false;  // Don't exit REPL
  }

  if (strcmp(input, "/clear") == 0) {
    printf("\033[2J\033[H");  // Clear screen and move cursor to top
    return false;             // Don't exit REPL
  }

  // If not a recognized shortcut, return false to process as JavaScript
  return false;
}

int JSRT_CmdRunREPL(int argc, char **argv) {
  // Store command line arguments for process module
  g_jsrt_argc = argc;
  g_jsrt_argv = argv;

  // Initialize runtime
  JSRT_Runtime *rt = JSRT_RuntimeNew();

  // Setup signal handler for CTRL-C
  signal(SIGINT, jsrt_repl_sigint_handler);

  // Get history file path
  char *history_path = jsrt_get_repl_history_path();

  // Load history
  read_history(history_path);

  printf("Welcome to jsrt REPL!\n");
  printf("Type JavaScript code or use shortcuts like /help, /exit\n");
  printf("Press Ctrl+C twice or Ctrl+D to exit\n");
  printf("\n");

  char *input_line;
  char *accumulated_input = NULL;
  size_t accumulated_size = 0;
  int line_number = 1;
  bool is_continuation = false;

  while (true) {
    // Reset CTRL-C counter at each prompt
    g_ctrl_c_count = 0;

    // Create prompt (main or continuation)
    char prompt[32];
    if (is_continuation) {
      snprintf(prompt, sizeof(prompt), "...   ");
    } else {
      snprintf(prompt, sizeof(prompt), "jsrt:%d> ", line_number);
    }

    // Read input with readline (handles history, arrow keys, etc.)
    input_line = readline(prompt);

    // Handle EOF (Ctrl+D)
    if (!input_line) {
      // If we have accumulated input, try to evaluate it before exiting
      if (accumulated_input && accumulated_size > 1) {
        char filename[32];
        snprintf(filename, sizeof(filename), "<repl:%d>", line_number);

        JSRT_EvalResult res = JSRT_RuntimeEval(rt, filename, accumulated_input, accumulated_size - 1);
        if (res.is_error) {
          fprintf(stderr, "Error: %s\n", res.error);
        } else {
          // Await result if it's a promise
          JSRT_EvalResult res2 = JSRT_RuntimeAwaitEvalResult(rt, &res);
          if (res2.is_error) {
            fprintf(stderr, "Error: %s\n", res2.error);
          } else {
            // Print the result like Node.js REPL
            JSValue result_val = res2.value.tag != JS_TAG_UNDEFINED ? res2.value : res.value;
            if (!JS_IsUndefined(result_val) && !JS_IsNull(result_val)) {
              DynBuf dbuf;
              dbuf_init(&dbuf);
              JSRT_GetJSValuePrettyString(&dbuf, rt->ctx, result_val, NULL, true);
              if (dbuf.buf && dbuf.size > 0) {
                // Remove trailing newline if present
                char *output = (char *)dbuf.buf;
                if (output[dbuf.size - 1] == '\n') {
                  output[dbuf.size - 1] = '\0';
                }
                printf("%s\n", output);
              }
              dbuf_free(&dbuf);
            }
          }
          JSRT_EvalResultFree(&res2);
        }
        JSRT_EvalResultFree(&res);
        JSRT_RuntimeRunTicket(rt);
      }

      if (accumulated_input) {
        free(accumulated_input);
        accumulated_input = NULL;
      }
      printf("\nGoodbye!\n");
      break;
    }

    // If this is first line and it's empty, skip
    if (!is_continuation && strlen(input_line) == 0) {
      free(input_line);
      continue;
    }

    // If not in continuation mode, check for shortcuts first
    if (!is_continuation) {
      if (jsrt_process_repl_shortcut(input_line)) {
        free(input_line);
        break;  // Exit REPL
      }

      // Skip shortcut lines that didn't exit
      if (input_line[0] == '/') {
        free(input_line);
        continue;
      }
    }

    // Accumulate input (first line or continuation)
    if (!accumulated_input) {
      // First line
      accumulated_size = strlen(input_line) + 1;
      accumulated_input = malloc(accumulated_size);
      strcpy(accumulated_input, input_line);
    } else {
      // Continuation line - append with newline
      size_t line_len = strlen(input_line);
      size_t new_size = accumulated_size + line_len + 1;  // +1 for newline (null terminator reused)
      accumulated_input = realloc(accumulated_input, new_size);
      accumulated_input[accumulated_size - 1] = '\n';  // Replace null terminator with newline
      strcpy(accumulated_input + accumulated_size, input_line);
      accumulated_size = new_size;
    }

    free(input_line);

    // Check if the accumulated code is complete
    if (jsrt_is_code_complete(rt->ctx, accumulated_input, accumulated_size - 1)) {
      // Code is complete - add to history and evaluate
      add_history(accumulated_input);
      is_continuation = false;

      // Evaluate JavaScript code
      char filename[32];
      snprintf(filename, sizeof(filename), "<repl:%d>", line_number);

      JSRT_EvalResult res = JSRT_RuntimeEval(rt, filename, accumulated_input, accumulated_size - 1);
      if (res.is_error) {
        fprintf(stderr, "Error: %s\n", res.error);
      } else {
        // Await result if it's a promise
        JSRT_EvalResult res2 = JSRT_RuntimeAwaitEvalResult(rt, &res);
        if (res2.is_error) {
          fprintf(stderr, "Error: %s\n", res2.error);
        } else {
          // Print the result like Node.js REPL
          JSValue result_val = res2.value.tag != JS_TAG_UNDEFINED ? res2.value : res.value;
          if (!JS_IsUndefined(result_val) && !JS_IsNull(result_val)) {
            DynBuf dbuf;
            dbuf_init(&dbuf);
            JSRT_GetJSValuePrettyString(&dbuf, rt->ctx, result_val, NULL, true);
            if (dbuf.buf && dbuf.size > 0) {
              // Remove trailing newline if present
              char *output = (char *)dbuf.buf;
              if (output[dbuf.size - 1] == '\n') {
                output[dbuf.size - 1] = '\0';
              }
              printf("%s\n", output);
            }
            dbuf_free(&dbuf);
          }
        }
        JSRT_EvalResultFree(&res2);
      }
      JSRT_EvalResultFree(&res);

      // Run pending async operations
      JSRT_RuntimeRunTicket(rt);

      // Clean up and advance to next line
      free(accumulated_input);
      accumulated_input = NULL;
      accumulated_size = 0;
      line_number++;
    } else {
      // Code is incomplete - continue to next line
      is_continuation = true;
    }
  }

  // Clean up any remaining accumulated input
  if (accumulated_input) {
    free(accumulated_input);
  }

  // Save history
  write_history(history_path);

  // Cleanup
  free(history_path);
  JSRT_RuntimeFree(rt);
  return 0;
}
