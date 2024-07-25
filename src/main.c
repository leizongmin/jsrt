#include <stdbool.h>
#include <stdio.h>

#include "jsrt.h"

void PrintHelp(bool is_error);

int main(int argc, char **argv) {
  if (argc < 2) {
    PrintHelp(true);
    return 1;
  }

  const char *filename = argv[1];
  int ret = JSRT_CmdRunFile(filename);

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
          "\n");
}
