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
    ret = JSRT_CmdRunStdin(argc, argv);
    return ret;
  }

  if (argc < 2) {
    // If no embedded bytecode and no arguments, check for piped stdin input
    if (!isatty(STDIN_FILENO)) {
      ret = JSRT_CmdRunStdin(argc, argv);
      return ret;
    }

    // No embedded bytecode and no piped input - start REPL
    return JSRT_CmdRunREPL(argc, argv);
  }

  const char* command = argv[1];

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
    return JSRT_CmdRunREPL(argc, argv);
  }

  // If no embedded bytecode, handle regular file execution
  ret = JSRT_CmdRunFile(command, argc, argv);

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
          "       jsrt <url> [args]                 Run script from URL\n"
          "       jsrt build <filename> [target]    Create self-contained binary file\n"
          "       jsrt repl                         Run REPL\n"
          "       jsrt version                      Print version\n"
          "       jsrt help                         Print this help message\n"
          "       jsrt -                            Read JavaScript code from stdin\n"
          "       echo 'code' | jsrt                Pipe JavaScript code from stdin\n"
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
