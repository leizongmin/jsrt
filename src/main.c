#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jsrt.h"
#include "util/file.h"

void PrintHelp(bool is_error);
void PrintVersion(const char* executable_path);

int main(int argc, char** argv) {
  // Handle special stdin flag or piped input
  if ((argc == 2 && strcmp(argv[1], "-") == 0) || (argc == 1 && !isatty(STDIN_FILENO))) {
    // Run from stdin
    int ret = JSRT_CmdRunStdin(argc, argv);
    return ret;
  }

  if (argc < 2) {
    PrintHelp(true);
    return 1;
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

  // Handle regular file execution
  int ret = JSRT_CmdRunFile(command, argc, argv);

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

char* GetExecutableDir(const char* executable_path) {
  // Find the directory containing the executable
  char* path_copy = strdup(executable_path);
  char* dir = dirname(path_copy);
  char* result = strdup(dir);
  free(path_copy);
  return result;
}

void PrintVersion(const char* executable_path) {
  const char* version = "unknown";
  JSRT_ReadFileResult version_file = JSRT_ReadFileResultDefault();

  // Try to find VERSION file relative to executable
  char* exe_dir = GetExecutableDir(executable_path);

  // Try VERSION in the same directory as executable first
  char version_path[1024];
  snprintf(version_path, sizeof(version_path), "%s/VERSION", exe_dir);
  version_file = JSRT_ReadFile(version_path);

  // If not found, try one directory up (for bin/jsrt layout)
  if (version_file.error != JSRT_READ_FILE_OK) {
    snprintf(version_path, sizeof(version_path), "%s/../VERSION", exe_dir);
    version_file = JSRT_ReadFile(version_path);
  }

  // If still not found, try current directory as fallback
  if (version_file.error != JSRT_READ_FILE_OK) {
    version_file = JSRT_ReadFile("VERSION");
  }

  if (version_file.error == JSRT_READ_FILE_OK && version_file.data != NULL) {
    // Remove trailing newline if present
    char* version_str = version_file.data;
    size_t len = strlen(version_str);
    if (len > 0 && version_str[len - 1] == '\n') {
      version_str[len - 1] = '\0';
    }
    version = version_str;
  }

  printf("jsrt v%s\n", version);
  printf("A lightweight, fast JavaScript runtime built on QuickJS and libuv\n");
  printf("Copyright © 2024-2025 LEI Zongmin\n");
  printf("License: MIT\n");

  // Clean up
  if (version_file.error == JSRT_READ_FILE_OK) {
    JSRT_ReadFileResultFree(&version_file);
  }
  free(exe_dir);
}
