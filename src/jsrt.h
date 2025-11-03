#ifndef __JSRT_JSRT_H__
#define __JSRT_JSRT_H__

#include <stdbool.h>
#include <stdio.h>

int JSRT_CmdRunFile(const char* filename, bool compact_node, bool compile_cache_allowed, bool module_hook_trace,
                    int argc, char** argv);
int JSRT_CmdRunStdin(bool compact_node, bool compile_cache_allowed, bool module_hook_trace, int argc, char** argv);
int JSRT_CmdRunEmbeddedBytecode(const char* executable_path, int argc, char** argv);

#endif
