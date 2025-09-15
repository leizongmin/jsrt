#ifndef JSRT_PROCESS_PLATFORM_H
#define JSRT_PROCESS_PLATFORM_H

#include <stddef.h>

// Platform detection
#ifdef _WIN32
#include <winsock2.h>  // For struct timeval on Windows
#else
#include <sys/time.h>  // For struct timeval on Unix-like systems
#endif

// Cross-platform function declarations
int jsrt_getpid(void);
int jsrt_getppid(void);
int jsrt_gettimeofday(struct timeval* tv, void* tz);
char* jsrt_getcwd(char* buf, size_t size);
int jsrt_chdir(const char* path);

// Platform information functions
const char* jsrt_get_platform(void);
const char* jsrt_get_arch(void);
size_t jsrt_get_path_max(void);

// Platform-specific macros for consistency
#define JSRT_GETPID() jsrt_getpid()
#define JSRT_GETPPID() jsrt_getppid()
#define JSRT_GETTIMEOFDAY(tv, tz) jsrt_gettimeofday(tv, tz)
#define JSRT_GETCWD(buf, size) jsrt_getcwd(buf, size)
#define JSRT_CHDIR(path) jsrt_chdir(path)

#endif  // JSRT_PROCESS_PLATFORM_H