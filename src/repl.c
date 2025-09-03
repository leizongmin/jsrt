#include "repl.h"

#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jsrt.h"
#include "runtime.h"
#include "std/console.h"
#include "util/dbuf.h"

// Global variable to track CTRL-C count for REPL exit
static volatile int g_ctrl_c_count = 0;

// Signal handler for CTRL-C in REPL
static void jsrt_repl_sigint_handler(int sig) {
  (void)sig;  // Suppress unused parameter warning
  g_ctrl_c_count++;
  if (g_ctrl_c_count >= 2) {
    printf("\n(To exit, press ^C again or ^D or type /exit)\n");
    rl_on_new_line();
    // Clear the line buffer manually
    rl_line_buffer[0] = '\0';
    rl_point = rl_end = 0;
    rl_redisplay();
    exit(0);
  } else {
    printf("\n(To exit, press ^C again or ^D or type /exit)\n");
    rl_on_new_line();
    // Clear the line buffer manually
    rl_line_buffer[0] = '\0';
    rl_point = rl_end = 0;
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
  extern int g_jsrt_argc;
  extern char **g_jsrt_argv;
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