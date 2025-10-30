#include <quickjs.h>
#include <stdlib.h>
#include <string.h>
#include "../../util/debug.h"
#include "process.h"

#ifdef _WIN32
#include <psapi.h>
#include <windows.h>
#else
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

// Convert timeval to microseconds
static int64_t timeval_to_microseconds(struct timeval tv) {
  return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
}

// process.cpuUsage([previousValue])
JSValue js_process_cpu_usage(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  // Windows implementation
  FILETIME creation_time, exit_time, kernel_time, user_time;
  if (!GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time)) {
    return JS_ThrowInternalError(ctx, "Failed to get process times");
  }

  // Convert FILETIME to microseconds (FILETIME is in 100-nanosecond intervals)
  ULARGE_INTEGER kernel, user;
  kernel.LowPart = kernel_time.dwLowDateTime;
  kernel.HighPart = kernel_time.dwHighDateTime;
  user.LowPart = user_time.dwLowDateTime;
  user.HighPart = user_time.dwHighDateTime;

  int64_t system_micros = kernel.QuadPart / 10;  // Convert to microseconds
  int64_t user_micros = user.QuadPart / 10;

#else
  // Unix implementation
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    return JS_ThrowInternalError(ctx, "Failed to get resource usage");
  }

  int64_t user_micros = timeval_to_microseconds(usage.ru_utime);
  int64_t system_micros = timeval_to_microseconds(usage.ru_stime);
#endif

  // If previousValue provided, calculate delta
  if (argc > 0 && JS_IsObject(argv[0])) {
    JSValue prev_user = JS_GetPropertyStr(ctx, argv[0], "user");
    JSValue prev_system = JS_GetPropertyStr(ctx, argv[0], "system");

    int64_t prev_user_val, prev_system_val;
    if (JS_ToInt64(ctx, &prev_user_val, prev_user) == 0 && JS_ToInt64(ctx, &prev_system_val, prev_system) == 0) {
      user_micros -= prev_user_val;
      system_micros -= prev_system_val;
    }

    JS_FreeValue(ctx, prev_user);
    JS_FreeValue(ctx, prev_system);
  }

  // Return { user: micros, system: micros }
  JSValue result = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, result, "user", JS_NewInt64(ctx, user_micros));
  JS_SetPropertyStr(ctx, result, "system", JS_NewInt64(ctx, system_micros));

  return result;
}

// process.resourceUsage()
JSValue js_process_resource_usage(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue result = JS_NewObject(ctx);

#ifdef _WIN32
  // Windows: Limited resource information
  FILETIME creation_time, exit_time, kernel_time, user_time;
  if (GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time)) {
    ULARGE_INTEGER kernel, user;
    kernel.LowPart = kernel_time.dwLowDateTime;
    kernel.HighPart = kernel_time.dwHighDateTime;
    user.LowPart = user_time.dwLowDateTime;
    user.HighPart = user_time.dwHighDateTime;

    JS_SetPropertyStr(ctx, result, "userCPUTime", JS_NewInt64(ctx, user.QuadPart / 10));
    JS_SetPropertyStr(ctx, result, "systemCPUTime", JS_NewInt64(ctx, kernel.QuadPart / 10));
  }

  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    JS_SetPropertyStr(ctx, result, "maxRSS", JS_NewInt64(ctx, pmc.PeakWorkingSetSize / 1024));
  }

  // Set other fields to 0 for Windows
  JS_SetPropertyStr(ctx, result, "sharedMemorySize", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "unsharedDataSize", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "unsharedStackSize", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "minorPageFault", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "majorPageFault", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "swappedOut", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "fsRead", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "fsWrite", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "ipcSent", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "ipcReceived", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "signalsCount", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "voluntaryContextSwitches", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, result, "involuntaryContextSwitches", JS_NewInt32(ctx, 0));

#else
  // Unix: Full resource information from getrusage()
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    JS_FreeValue(ctx, result);
    return JS_ThrowInternalError(ctx, "Failed to get resource usage");
  }

  // CPU times in microseconds
  JS_SetPropertyStr(ctx, result, "userCPUTime", JS_NewInt64(ctx, timeval_to_microseconds(usage.ru_utime)));
  JS_SetPropertyStr(ctx, result, "systemCPUTime", JS_NewInt64(ctx, timeval_to_microseconds(usage.ru_stime)));

  // Memory in kilobytes
  JS_SetPropertyStr(ctx, result, "maxRSS", JS_NewInt64(ctx, usage.ru_maxrss));
  JS_SetPropertyStr(ctx, result, "sharedMemorySize", JS_NewInt64(ctx, usage.ru_ixrss));
  JS_SetPropertyStr(ctx, result, "unsharedDataSize", JS_NewInt64(ctx, usage.ru_idrss));
  JS_SetPropertyStr(ctx, result, "unsharedStackSize", JS_NewInt64(ctx, usage.ru_isrss));

  // Page faults
  JS_SetPropertyStr(ctx, result, "minorPageFault", JS_NewInt64(ctx, usage.ru_minflt));
  JS_SetPropertyStr(ctx, result, "majorPageFault", JS_NewInt64(ctx, usage.ru_majflt));

  // Swapping
  JS_SetPropertyStr(ctx, result, "swappedOut", JS_NewInt64(ctx, usage.ru_nswap));

  // File system I/O
  JS_SetPropertyStr(ctx, result, "fsRead", JS_NewInt64(ctx, usage.ru_inblock));
  JS_SetPropertyStr(ctx, result, "fsWrite", JS_NewInt64(ctx, usage.ru_oublock));

  // IPC
  JS_SetPropertyStr(ctx, result, "ipcSent", JS_NewInt64(ctx, usage.ru_msgsnd));
  JS_SetPropertyStr(ctx, result, "ipcReceived", JS_NewInt64(ctx, usage.ru_msgrcv));

  // Signals
  JS_SetPropertyStr(ctx, result, "signalsCount", JS_NewInt64(ctx, usage.ru_nsignals));

  // Context switches
  JS_SetPropertyStr(ctx, result, "voluntaryContextSwitches", JS_NewInt64(ctx, usage.ru_nvcsw));
  JS_SetPropertyStr(ctx, result, "involuntaryContextSwitches", JS_NewInt64(ctx, usage.ru_nivcsw));
#endif

  return result;
}

// process.memoryUsage.rss()
JSValue js_process_memory_usage_rss(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    return JS_NewInt64(ctx, pmc.WorkingSetSize);
  }
  return JS_NewInt32(ctx, 0);
#else
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
    long rss_bytes = usage.ru_maxrss;
#ifdef __APPLE__
    // macOS reports RSS in bytes
    return JS_NewInt64(ctx, rss_bytes);
#else
    // Linux reports RSS in kilobytes
    return JS_NewInt64(ctx, rss_bytes * 1024);
#endif
  }
  return JS_NewInt32(ctx, 0);
#endif
}

// process.availableMemory()
JSValue js_process_available_memory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef __linux__
  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    uint64_t available = (uint64_t)info.freeram * (uint64_t)info.mem_unit;
    return JS_NewInt64(ctx, available);
  }
#elif defined(__APPLE__)
  vm_statistics64_data_t vm_stats;
  mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
  if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS) {
    uint64_t free_pages = vm_stats.free_count;
    uint64_t page_size;
    size_t len = sizeof(page_size);
    if (sysctlbyname("hw.pagesize", &page_size, &len, NULL, 0) == 0) {
      return JS_NewInt64(ctx, free_pages * page_size);
    }
  }
#elif defined(_WIN32)
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (GlobalMemoryStatusEx(&mem_status)) {
    return JS_NewInt64(ctx, mem_status.ullAvailPhys);
  }
#endif

  return JS_ThrowInternalError(ctx, "Failed to get available memory");
}

// process.constrainedMemory()
JSValue js_process_constrained_memory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // For now, return undefined as most systems don't have memory constraints
  // This would be used in containerized environments (Docker, Kubernetes)
  // or systems with cgroups memory limits

#ifdef __linux__
  // Try to read cgroup memory limit
  FILE* f = fopen("/sys/fs/cgroup/memory/memory.limit_in_bytes", "r");
  if (f) {
    uint64_t limit;
    if (fscanf(f, "%llu", &limit) == 1) {
      fclose(f);
      // Check if it's a real limit (not max value)
      if (limit < UINT64_MAX / 2) {
        return JS_NewInt64(ctx, limit);
      }
    }
    fclose(f);
  }
#endif

  // No constraint found
  return JS_UNDEFINED;
}

// Initialize resources module
void jsrt_process_init_resources(void) {
  JSRT_Debug("Process resources module initialized");
}
