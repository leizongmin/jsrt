#include <stdbool.h>
#include <stdio.h>

#include "../src/jsrt.h"
#include "../src/util/debug.h"

void PrintHelp(bool is_error);

int main(int argc, char** argv) {
  for (int i = 0; i < argc; i++) {
    JSRT_Debug("argv[%d] = %s", i, argv[i]);
  }

  if (argc < 2) {
    PrintHelp(true);
    return 1;
  }

  int failed_count = 0;
  for (int i = 1; i < argc; i++) {
    const char* filename = argv[i];
    printf("\033[32mRun file: %s\033[0m\n", filename);
    int ret = JSRT_CmdRunFile(filename, argc, argv);
    if (ret != 0) {
      printf("\033[31m>> Error: %d\033[0m\n", ret);
      failed_count++;
      // Continue with other tests instead of stopping
    } else {
      printf("\033[32m>> OK\033[0m\n");
    }
  }

  if (failed_count > 0) {
    printf("\033[31m%d test(s) failed\033[0m\n", failed_count);
    return 1;
  }
  printf("\033[32mDone.\033[0m\n");
  return 0;
}

void PrintHelp(bool is_error) {
  fprintf(is_error ? stderr : stdout,
          "jsrt unit test tool for js files\n"
          "Usage: jsrt_test_js <filename...>\n"
          "\n");
}
