#include "process_platform.h"

// Platform-specific includes and implementations
#ifdef _WIN32
// clang-format off
#include <windows.h>   // Must come first to define basic Windows types
#include <process.h>
#include <tlhelp32.h>
#include <winsock2.h>  // For struct timeval
#include <direct.h>    // For _getcwd, _chdir
// clang-format on

// Windows implementation of missing functions
int jsrt_getppid(void) {
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE)
    return 0;

  PROCESSENTRY32 pe32 = {.dwSize = sizeof(PROCESSENTRY32)};
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

int jsrt_gettimeofday(struct timeval* tv, void* tz) {
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

char* jsrt_getcwd(char* buf, size_t size) {
  return _getcwd(buf, (int)size);
}

int jsrt_chdir(const char* path) {
  return _chdir(path);
}

#else
#include <limits.h>  // For PATH_MAX
#include <sys/time.h>
#include <unistd.h>

// Unix-like systems use native functions
int jsrt_getppid(void) {
  return getppid();
}

int jsrt_gettimeofday(struct timeval* tv, void* tz) {
  return gettimeofday(tv, tz);
}

char* jsrt_getcwd(char* buf, size_t size) {
  return getcwd(buf, size);
}

int jsrt_chdir(const char* path) {
  return chdir(path);
}

#endif

// Cross-platform process ID function
int jsrt_getpid(void) {
  return getpid();
}

// Get platform name
const char* jsrt_get_platform(void) {
#ifdef __APPLE__
  return "darwin";
#elif __linux__
  return "linux";
#elif _WIN32
  return "win32";
#elif __FreeBSD__
  return "freebsd";
#elif __OpenBSD__
  return "openbsd";
#elif __NetBSD__
  return "netbsd";
#else
  return "unknown";
#endif
}

// Get architecture name
const char* jsrt_get_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
  return "x64";
#elif defined(__i386__) || defined(_M_IX86)
  return "x32";
#elif defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
  return "arm";
#else
  return "unknown";
#endif
}

// Get path maximum length
size_t jsrt_get_path_max(void) {
#ifdef _WIN32
  return 260;  // Windows path length limit
#else
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
  return PATH_MAX;
#endif
}