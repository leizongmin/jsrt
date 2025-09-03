#ifndef __JSRT_JSRT_H__
#define __JSRT_JSRT_H__

#include <stdio.h>

int JSRT_CmdRunFile(const char *filename, int argc, char **argv);
int JSRT_CmdRunStdin(int argc, char **argv);
int JSRT_CmdRunEmbeddedBytecode(const char *executable_path, int argc, char **argv);
int JSRT_CmdRunREPL(int argc, char **argv);

#endif
