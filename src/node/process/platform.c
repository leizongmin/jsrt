#include <stdlib.h>
#include <string.h>
#include "process.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define PATH_MAX 260
#else
#include <errno.h>
#include <limits.h>
#endif

// Platform-specific implementations

#ifdef _WIN32
// Windows implementation of getppid()
static int jsrt_getppid_win32(void) {
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE) {
    return 0;
  }

  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);
  DWORD currentPID = GetCurrentProcessId();
  DWORD parentPID = 0;

  if (Process32First(hSnapshot, &pe32)) {
    do {
      if (pe32.th32ProcessID == currentPID) {
        parentPID = pe32.th32ParentProcessID;
        break;
      }
    } while (Process32Next(hSnapshot, &pe32));
  }

  CloseHandle(hSnapshot);
  return (int)parentPID;
}

// Windows implementation of gettimeofday()
static int jsrt_gettimeofday_win32(struct timeval* tv, void* tz) {
  if (!tv)
    return -1;

  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);

  unsigned __int64 time = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
  time /= 10;                    // Convert to microseconds
  time -= 11644473600000000ULL;  // Convert to Unix epoch

  tv->tv_sec = (long)(time / 1000000UL);
  tv->tv_usec = (long)(time % 1000000UL);
  return 0;
}

// Windows implementation of getcwd()
static char* jsrt_getcwd_win32(char* buf, size_t size) {
  if (!buf || size == 0) {
    return NULL;
  }

  char* result = _getcwd(buf, (int)size);
  if (result) {
    // Convert backslashes to forward slashes for consistency
    for (char* p = result; *p; p++) {
      if (*p == '\\') {
        *p = '/';
      }
    }
  }
  return result;
}

// Windows implementation of chdir()
static int jsrt_chdir_win32(const char* path) {
  return _chdir(path);
}

#endif  // _WIN32

// Cross-platform wrapper functions

int jsrt_process_getpid(void) {
#ifdef _WIN32
  return (int)GetCurrentProcessId();
#else
  return getpid();
#endif
}

int jsrt_process_getppid(void) {
#ifdef _WIN32
  return jsrt_getppid_win32();
#else
  return getppid();
#endif
}

int jsrt_process_gettimeofday(struct timeval* tv, void* tz) {
#ifdef _WIN32
  return jsrt_gettimeofday_win32(tv, tz);
#else
  return gettimeofday(tv, tz);
#endif
}

char* jsrt_process_getcwd(char* buf, size_t size) {
#ifdef _WIN32
  return jsrt_getcwd_win32(buf, size);
#else
  return getcwd(buf, size);
#endif
}

int jsrt_process_chdir(const char* path) {
#ifdef _WIN32
  return jsrt_chdir_win32(path);
#else
  return chdir(path);
#endif
}

const char* jsrt_process_platform(void) {
#ifdef _WIN32
  return "win32";
#elif defined(__APPLE__)
  return "darwin";
#elif defined(__linux__)
  return "linux";
#elif defined(__FreeBSD__)
  return "freebsd";
#elif defined(__OpenBSD__)
  return "openbsd";
#elif defined(__NetBSD__)
  return "netbsd";
#elif defined(__sun)
  return "sunos";
#else
  return "unknown";
#endif
}

const char* jsrt_process_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
  return "x64";
#elif defined(__i386__) || defined(_M_IX86)
  return "ia32";
#elif defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
  return "arm";
#elif defined(__mips__)
  return "mips";
#elif defined(__mips64__)
  return "mips64";
#elif defined(__powerpc64__)
  return "ppc64";
#elif defined(__powerpc__)
  return "ppc";
#elif defined(__s390x__)
  return "s390x";
#elif defined(__s390__)
  return "s390";
#else
  return "unknown";
#endif
}

void jsrt_process_init_platform(void) {
  // Platform-specific initialization if needed
  // Currently no initialization required
}