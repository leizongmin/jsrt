#include "jsrt.h"

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
