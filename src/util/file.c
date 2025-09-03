#include "file.h"

#include <stdio.h>
#include <stdlib.h>

#include "debug.h"

JSRT_ReadFileResult JSRT_ReadFileResultDefault() {
  JSRT_ReadFileResult result;
  result.data = NULL;
  result.size = 0;
  result.error = JSRT_READ_FILE_OK;
  return result;
}

JSRT_ReadFileResult JSRT_ReadFile(const char* path) {
  JSRT_ReadFileResult result = JSRT_ReadFileResultDefault();

  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    result.error = JSRT_READ_FILE_ERROR_FILE_NOT_FOUND;
    goto fail;
  }

  fseek(file, 0, SEEK_END);
  result.size = ftell(file);
  fseek(file, 0, SEEK_SET);

  result.data = malloc(result.size + 1);
  if (result.data == NULL) {
    result.error = JSRT_READ_FILE_ERROR_OUT_OF_MEMORY;
    fclose(file);
    goto fail;
  }

  size_t bytes_read = fread(result.data, 1, result.size, file);
  if (bytes_read != result.size) {
    result.error = JSRT_READ_FILE_ERROR_READ_ERROR;
    free(result.data);
    fclose(file);
    goto fail;
  }

  result.data[result.size] = '\0';
  fclose(file);
  // JSRT_Debug("read file: path=%s size=%zu", path, result.size);
  return result;

fail:
  // JSRT_Debug("read file: path=%s error=%d", path, result.error);
  return result;
}

void JSRT_ReadFileResultFree(JSRT_ReadFileResult* result) {
  free(result->data);
  result->data = NULL;
  result->size = 0;
  result->error = JSRT_READ_FILE_OK;
}

const char* JSRT_ReadFileErrorToString(JSRT_ReadFileError error) {
  switch (error) {
    case JSRT_READ_FILE_OK:
      return "JSRT_READ_FILE_OK";
    case JSRT_READ_FILE_ERROR_FILE_NOT_FOUND:
      return "JSRT_READ_FILE_ERROR_FILE_NOT_FOUND";
    case JSRT_READ_FILE_ERROR_OUT_OF_MEMORY:
      return "JSRT_READ_FILE_ERROR_OUT_OF_MEMORY";
    case JSRT_READ_FILE_ERROR_READ_ERROR:
      return "JSRT_READ_FILE_ERROR_READ_ERROR";
    default:
      return "Unknown error";
  }
}
