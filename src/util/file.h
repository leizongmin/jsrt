#ifndef __JSRT_UTIL_FILE_H__
#define __JSRT_UTIL_FILE_H__

#include <stddef.h>

typedef enum {
  JSRT_READ_FILE_OK = 0,
  JSRT_READ_FILE_ERROR_FILE_NOT_FOUND,
  JSRT_READ_FILE_ERROR_OUT_OF_MEMORY,
  JSRT_READ_FILE_ERROR_READ_ERROR,
} JSRT_ReadFileError;

typedef struct {
  JSRT_ReadFileError error;
  char* data;
  size_t size;
} JSRT_ReadFileResult;

JSRT_ReadFileResult JSRT_ReadFileResultDefault();

JSRT_ReadFileResult JSRT_ReadFile(const char* path);

void JSRT_ReadFileResultFree(JSRT_ReadFileResult* result);

const char* JSRT_ReadFileErrorToString(JSRT_ReadFileError error);

#endif
