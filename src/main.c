#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "build.h"
#include "jsrt.h"
#include "repl.h"
#include "runtime.h"
#include "util/file.h"

void PrintHelp(bool is_error);
void PrintVersion(const char* executable_path);

int main(int argc, char** argv) {
  // Always check if this executable contains embedded bytecode first
  // This handles self-contained executables with or without arguments
  int ret = JSRT_CmdRunEmbeddedBytecode(argv[0], argc, argv);
  if (ret == 0) {
    return ret;  // Successfully executed embedded bytecode
  }

  // Handle explicit stdin flag
  if (argc == 2 && strcmp(argv[1], "-") == 0) {
    ret = JSRT_CmdRunStdin(false, true, argc, argv);
    return ret;
  }

  // Check for compact-node flag (enabled by default)
  bool compact_node = true;
  bool compile_cache_allowed = true;
  int script_arg_start = 1;

  // Check for JSRT_NO_COMPILE_CACHE environment variable
  const char* no_cache_env = getenv("JSRT_NO_COMPILE_CACHE");
  if (no_cache_env &&
      (strcmp(no_cache_env, "1") == 0 || strcmp(no_cache_env, "true") == 0 || strcmp(no_cache_env, "yes") == 0)) {
    compile_cache_allowed = false;
  }

  // Process command line flags
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--compact-node") == 0) {
      compact_node = true;
      script_arg_start++;
    } else if (strcmp(argv[i], "--no-compact-node") == 0) {
      compact_node = false;
      script_arg_start++;
    } else if (strcmp(argv[i], "--no-compile-cache") == 0) {
      compile_cache_allowed = false;
      script_arg_start++;
    } else {
      // Stop processing flags when we hit a non-flag
      break;
    }
  }

  if (argc < script_arg_start + 1) {
    // If no embedded bytecode and no arguments, check for piped stdin input
    if (!isatty(STDIN_FILENO)) {
      ret = JSRT_CmdRunStdin(compact_node, compile_cache_allowed, argc, argv);
      return ret;
    }

    // No embedded bytecode and no piped input - start REPL
    return JSRT_CmdRunREPL(argc, argv);
  }

  const char* command = argv[script_arg_start];

  // Handle special commands
  if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
    PrintHelp(false);
    return 0;
  }

  if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
    PrintVersion(argv[0]);
    return 0;
  }

  if (strcmp(command, "build") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: build command requires a filename\n");
      fprintf(stderr, "Usage: jsrt build <filename> [target]\n");
      return 1;
    }
    const char* filename = argv[2];
    const char* target = argc >= 4 ? argv[3] : NULL;
    return BuildExecutable(argv[0], filename, target);
  }

  if (strcmp(command, "repl") == 0) {
    return JSRT_CmdRunREPL(argc - script_arg_start, argv + script_arg_start);
  }

  // If no embedded bytecode, handle regular file execution
  // In compact-node mode, prepend argv[0] (executable path) to match Node.js behavior
  // where process.argv[0] is the Node executable and process.argv[1] is the script
  if (compact_node) {
    // Create new argv array with executable path prepended
    int new_argc = argc - script_arg_start + 1;
    char** new_argv = (char**)malloc((new_argc + 1) * sizeof(char*));
    new_argv[0] = argv[0];  // Executable path
    for (int i = 0; i < argc - script_arg_start; i++) {
      new_argv[i + 1] = argv[script_arg_start + i];
    }
    new_argv[new_argc] = NULL;

    ret = JSRT_CmdRunFile(command, compact_node, compile_cache_allowed, new_argc, new_argv);
    free(new_argv);
  } else {
    ret =
        JSRT_CmdRunFile(command, compact_node, compile_cache_allowed, argc - script_arg_start, argv + script_arg_start);
  }

  return ret;
}

void PrintHelp(bool is_error) {
  fprintf(is_error ? stderr : stdout,
          "Welcome to jsrt, a small JavaScript runtime.\n"
          "Author:   LEI Zongmin <leizongmin@gmail.com>\n"
          "Homepage: https://github.com/leizongmin/jsrt\n"
          "License:  MIT\n"
          "\n"
          "Usage: jsrt <filename> [args]            Run script file\n"
          "       jsrt --no-compact-node <file> [args] Disable Node.js compact mode\n"
          "       jsrt <url> [args]                 Run script from URL\n"
          "       jsrt build <filename> [target]    Create self-contained binary file\n"
          "       jsrt repl                         Run REPL\n"
          "       jsrt version                      Print version\n"
          "       jsrt help                         Print this help message\n"
          "       jsrt -                            Read JavaScript code from stdin\n"
          "       echo 'code' | jsrt                Pipe JavaScript code from stdin\n"
          "\n"
          "Node.js Compatibility:\n"
          "  Enabled by default                   Compact Node.js mode is always on:\n"
          "                                        - Load modules without 'node:' prefix\n"
          "                                        - Provide __dirname and __filename\n"
          "                                        - Expose global process object\n"
          "  --no-compact-node                     Disable compact Node.js mode\n"
          "\n");
}

void PrintVersion(const char* executable_path) {
#ifdef JSRT_VERSION
  const char* version = JSRT_VERSION;
#else
  const char* version = "unknown";
#endif

  printf("jsrt v%s\n", version);
  printf("A lightweight, fast JavaScript runtime built on QuickJS and libuv\n");
  printf("Copyright © 2024-2025 LEI Zongmin\n");
  printf("License: MIT\n");
}
