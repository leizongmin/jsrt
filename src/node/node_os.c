#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "node_modules.h"

#ifdef _WIN32
#include <lmcons.h>
#include <process.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define JSRT_GETPID() _getpid()
#define JSRT_GETPPID() 0  // Not available on Windows
#define JSRT_GETHOSTNAME(buf, size) (GetComputerNameA(buf, &size) ? 0 : -1)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>
#define JSRT_GETPID() getpid()
#define JSRT_GETPPID() getppid()
#define JSRT_GETHOSTNAME(buf, size) gethostname(buf, size)
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#endif
#endif

// node:os.arch implementation
static JSValue js_os_arch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
#ifdef _WIN64
  return JS_NewString(ctx, "x64");
#else
  return JS_NewString(ctx, "ia32");
#endif
#else
  struct utsname uts;
  if (uname(&uts) == 0) {
    if (strcmp(uts.machine, "x86_64") == 0 || strcmp(uts.machine, "amd64") == 0) {
      return JS_NewString(ctx, "x64");
    } else if (strncmp(uts.machine, "arm", 3) == 0) {
      if (strstr(uts.machine, "64")) {
        return JS_NewString(ctx, "arm64");
      } else {
        return JS_NewString(ctx, "arm");
      }
    } else if (strncmp(uts.machine, "aarch64", 7) == 0) {
      return JS_NewString(ctx, "arm64");
    } else if (strcmp(uts.machine, "i386") == 0 || strcmp(uts.machine, "i686") == 0) {
      return JS_NewString(ctx, "ia32");
    }
    return JS_NewString(ctx, uts.machine);
  }
  return JS_NewString(ctx, "unknown");
#endif
}

// node:os.platform implementation
static JSValue js_os_platform(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  return JS_NewString(ctx, "win32");
#elif defined(__APPLE__)
  return JS_NewString(ctx, "darwin");
#elif defined(__linux__)
  return JS_NewString(ctx, "linux");
#elif defined(__FreeBSD__)
  return JS_NewString(ctx, "freebsd");
#elif defined(__OpenBSD__)
  return JS_NewString(ctx, "openbsd");
#elif defined(__NetBSD__)
  return JS_NewString(ctx, "netbsd");
#elif defined(__sun)
  return JS_NewString(ctx, "sunos");
#else
  return JS_NewString(ctx, "unknown");
#endif
}

// node:os.type implementation
static JSValue js_os_type(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  return JS_NewString(ctx, "Windows_NT");
#else
  struct utsname uts;
  if (uname(&uts) == 0) {
    return JS_NewString(ctx, uts.sysname);
  }
  return JS_NewString(ctx, "Unknown");
#endif
}

// node:os.release implementation
static JSValue js_os_release(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  OSVERSIONINFOA osvi;
  ZeroMemory(&osvi, sizeof(OSVERSIONINFOA));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
  if (GetVersionExA(&osvi)) {
    char version[64];
    snprintf(version, sizeof(version), "%d.%d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    return JS_NewString(ctx, version);
  }
  return JS_NewString(ctx, "Unknown");
#else
  struct utsname uts;
  if (uname(&uts) == 0) {
    return JS_NewString(ctx, uts.release);
  }
  return JS_NewString(ctx, "Unknown");
#endif
}

// node:os.hostname implementation
static JSValue js_os_hostname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  char hostname[256];
#ifdef _WIN32
  DWORD size = sizeof(hostname);
  if (JSRT_GETHOSTNAME(hostname, size) == 0) {
#else
  if (JSRT_GETHOSTNAME(hostname, sizeof(hostname)) == 0) {
#endif
    return JS_NewString(ctx, hostname);
  }
  return JS_NewString(ctx, "localhost");
}

// node:os.tmpdir implementation
static JSValue js_os_tmpdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  char temp_path[MAX_PATH];
  DWORD length = GetTempPathA(MAX_PATH, temp_path);
  if (length > 0 && length < MAX_PATH) {
    // Remove trailing backslash if present
    if (temp_path[length - 1] == '\\') {
      temp_path[length - 1] = '\0';
    }
    return JS_NewString(ctx, temp_path);
  }
  return JS_NewString(ctx, "C:\\Windows\\Temp");
#else
  const char* tmpdir = getenv("TMPDIR");
  if (!tmpdir)
    tmpdir = getenv("TMP");
  if (!tmpdir)
    tmpdir = getenv("TEMP");
  if (!tmpdir)
    tmpdir = "/tmp";
  return JS_NewString(ctx, tmpdir);
#endif
}

// node:os.homedir implementation
static JSValue js_os_homedir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  const char* home = getenv("USERPROFILE");
  if (!home) {
    const char* drive = getenv("HOMEDRIVE");
    const char* path = getenv("HOMEPATH");
    if (drive && path) {
      char full_path[MAX_PATH];
      snprintf(full_path, sizeof(full_path), "%s%s", drive, path);
      return JS_NewString(ctx, full_path);
    }
    return JS_NewString(ctx, "C:\\");
  }
  return JS_NewString(ctx, home);
#else
  const char* home = getenv("HOME");
  if (!home) {
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
      home = pw->pw_dir;
    } else {
      home = "/";
    }
  }
  return JS_NewString(ctx, home);
#endif
}

// node:os.userInfo implementation
static JSValue js_os_userInfo(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue user_obj = JS_NewObject(ctx);

#ifdef _WIN32
  char username[UNLEN + 1];
  DWORD username_len = UNLEN + 1;
  if (GetUserNameA(username, &username_len)) {
    JS_SetPropertyStr(ctx, user_obj, "username", JS_NewString(ctx, username));
  } else {
    JS_SetPropertyStr(ctx, user_obj, "username", JS_NewString(ctx, "unknown"));
  }

  // Windows doesn't have uid/gid concepts like Unix
  JS_SetPropertyStr(ctx, user_obj, "uid", JS_NewInt32(ctx, -1));
  JS_SetPropertyStr(ctx, user_obj, "gid", JS_NewInt32(ctx, -1));
  JS_SetPropertyStr(ctx, user_obj, "shell", JS_NewString(ctx, getenv("COMSPEC") ? getenv("COMSPEC") : "cmd.exe"));
#else
  struct passwd* pw = getpwuid(getuid());
  if (pw) {
    JS_SetPropertyStr(ctx, user_obj, "username", JS_NewString(ctx, pw->pw_name));
    JS_SetPropertyStr(ctx, user_obj, "uid", JS_NewInt32(ctx, pw->pw_uid));
    JS_SetPropertyStr(ctx, user_obj, "gid", JS_NewInt32(ctx, pw->pw_gid));
    JS_SetPropertyStr(ctx, user_obj, "shell", JS_NewString(ctx, pw->pw_shell ? pw->pw_shell : "/bin/sh"));
    if (pw->pw_dir) {
      JS_SetPropertyStr(ctx, user_obj, "homedir", JS_NewString(ctx, pw->pw_dir));
    }
  } else {
    JS_SetPropertyStr(ctx, user_obj, "username", JS_NewString(ctx, "unknown"));
    JS_SetPropertyStr(ctx, user_obj, "uid", JS_NewInt32(ctx, getuid()));
    JS_SetPropertyStr(ctx, user_obj, "gid", JS_NewInt32(ctx, getgid()));
    JS_SetPropertyStr(ctx, user_obj, "shell", JS_NewString(ctx, "/bin/sh"));
  }
#endif

  // Add homedir using our homedir function
  JSValue homedir_val = js_os_homedir(ctx, JS_UNDEFINED, 0, NULL);
  JS_SetPropertyStr(ctx, user_obj, "homedir", homedir_val);

  return user_obj;
}

// node:os.endianness implementation
static JSValue js_os_endianness(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  union {
    uint32_t i;
    char c[4];
  } test = {0x01020304};

  if (test.c[0] == 1) {
    return JS_NewString(ctx, "BE");  // Big Endian
  } else {
    return JS_NewString(ctx, "LE");  // Little Endian
  }
}

// Helper function to read CPU model name from /proc/cpuinfo on Linux
#if defined(__linux__)
static void get_cpu_model_linux(char* model, size_t model_size) {
  FILE* f = fopen("/proc/cpuinfo", "r");
  if (!f) {
    snprintf(model, model_size, "Unknown CPU");
    return;
  }

  char line[256];
  char model_name[256] = {0};
  char hardware[256] = {0};
  char processor[256] = {0};

  while (fgets(line, sizeof(line), f)) {
    // Try "model name" field (x86/x86_64)
    if (strncmp(line, "model name", 10) == 0) {
      char* colon = strchr(line, ':');
      if (colon) {
        colon++;
        while (*colon == ' ' || *colon == '\t')
          colon++;
        char* newline = strchr(colon, '\n');
        if (newline)
          *newline = '\0';
        snprintf(model_name, sizeof(model_name), "%s", colon);
        break;  // model name is preferred, stop here
      }
    }
    // Try "Hardware" field (ARM)
    else if (strncmp(line, "Hardware", 8) == 0) {
      char* colon = strchr(line, ':');
      if (colon) {
        colon++;
        while (*colon == ' ' || *colon == '\t')
          colon++;
        char* newline = strchr(colon, '\n');
        if (newline)
          *newline = '\0';
        snprintf(hardware, sizeof(hardware), "%s", colon);
      }
    }
    // Try "Processor" field (some ARM)
    else if (strncmp(line, "Processor", 9) == 0 && processor[0] == '\0') {
      char* colon = strchr(line, ':');
      if (colon) {
        colon++;
        while (*colon == ' ' || *colon == '\t')
          colon++;
        char* newline = strchr(colon, '\n');
        if (newline)
          *newline = '\0';
        snprintf(processor, sizeof(processor), "%s", colon);
      }
    }
  }

  // Prefer model name, then hardware, then processor, then unknown
  if (model_name[0] != '\0') {
    snprintf(model, model_size, "%s", model_name);
  } else if (hardware[0] != '\0') {
    snprintf(model, model_size, "%s", hardware);
  } else if (processor[0] != '\0') {
    snprintf(model, model_size, "%s", processor);
  } else {
    snprintf(model, model_size, "Unknown CPU");
  }

  fclose(f);
}

// Helper function to read CPU speed from /proc/cpuinfo on Linux
static int get_cpu_speed_linux() {
  FILE* f = fopen("/proc/cpuinfo", "r");
  if (!f) {
    return 0;
  }

  char line[256];
  double speed_mhz = 0.0;
  double bogomips = 0.0;

  while (fgets(line, sizeof(line), f)) {
    // Try "cpu MHz" field (x86/x86_64)
    if (strncmp(line, "cpu MHz", 7) == 0) {
      char* colon = strchr(line, ':');
      if (colon) {
        speed_mhz = atof(colon + 1);
        break;  // Prefer cpu MHz if available
      }
    }
    // Try "BogoMIPS" field (ARM and others)
    else if (strncmp(line, "BogoMIPS", 8) == 0) {
      char* colon = strchr(line, ':');
      if (colon) {
        bogomips = atof(colon + 1);
      }
    }
  }

  fclose(f);

  // Prefer cpu MHz, fallback to BogoMIPS (which is a rough approximation)
  if (speed_mhz > 0.0) {
    return (int)speed_mhz;
  } else if (bogomips > 0.0) {
    return (int)bogomips;  // BogoMIPS is roughly equivalent to MHz on some systems
  }

  return 0;
}

// Helper function to read CPU times from /proc/stat on Linux
static int get_cpu_times_linux(int cpu_index, uint64_t* user, uint64_t* nice, uint64_t* sys, uint64_t* idle,
                               uint64_t* irq) {
  FILE* f = fopen("/proc/stat", "r");
  if (!f) {
    return -1;
  }

  char line[256];
  char cpu_name[16];
  snprintf(cpu_name, sizeof(cpu_name), "cpu%d", cpu_index);

  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, cpu_name, strlen(cpu_name)) == 0) {
      // Format: cpu# user nice system idle iowait irq softirq
      unsigned long long u, n, s, i, iowait, irq_time, softirq;
      if (sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu", &u, &n, &s, &i, &iowait, &irq_time, &softirq) >= 4) {
        // Convert from jiffies to milliseconds (assuming 100 HZ)
        *user = u * 10;
        *nice = n * 10;
        *sys = s * 10;
        *idle = i * 10;
        *irq = (irq_time + softirq) * 10;
        fclose(f);
        return 0;
      }
    }
  }

  fclose(f);
  return -1;
}
#endif

// node:os.cpus implementation
static JSValue js_os_cpus(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue cpus_array = JS_NewArray(ctx);

#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  int num_cpus = si.dwNumberOfProcessors;

  for (int i = 0; i < num_cpus; i++) {
    JSValue cpu_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, cpu_obj, "model", JS_NewString(ctx, "Unknown CPU"));
    JS_SetPropertyStr(ctx, cpu_obj, "speed", JS_NewInt32(ctx, 0));

    JSValue times_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, times_obj, "user", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, times_obj, "nice", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, times_obj, "sys", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, times_obj, "idle", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, times_obj, "irq", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, cpu_obj, "times", times_obj);

    JS_SetPropertyUint32(ctx, cpus_array, i, cpu_obj);
  }
#elif defined(__linux__)
  // Linux implementation with real CPU data
  long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cpus <= 0)
    num_cpus = 1;

  // Get model name (same for all CPUs)
  char model[256];
  get_cpu_model_linux(model, sizeof(model));

  for (long i = 0; i < num_cpus; i++) {
    JSValue cpu_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, cpu_obj, "model", JS_NewString(ctx, model));

    // Get CPU speed
    int speed = get_cpu_speed_linux();
    JS_SetPropertyStr(ctx, cpu_obj, "speed", JS_NewInt32(ctx, speed));

    // Get CPU times
    uint64_t user = 0, nice = 0, sys = 0, idle = 0, irq = 0;
    if (get_cpu_times_linux(i, &user, &nice, &sys, &idle, &irq) == 0) {
      JSValue times_obj = JS_NewObject(ctx);
      JS_SetPropertyStr(ctx, times_obj, "user", JS_NewInt64(ctx, user));
      JS_SetPropertyStr(ctx, times_obj, "nice", JS_NewInt64(ctx, nice));
      JS_SetPropertyStr(ctx, times_obj, "sys", JS_NewInt64(ctx, sys));
      JS_SetPropertyStr(ctx, times_obj, "idle", JS_NewInt64(ctx, idle));
      JS_SetPropertyStr(ctx, times_obj, "irq", JS_NewInt64(ctx, irq));
      JS_SetPropertyStr(ctx, cpu_obj, "times", times_obj);
    } else {
      // Fallback if we can't read times
      JSValue times_obj = JS_NewObject(ctx);
      JS_SetPropertyStr(ctx, times_obj, "user", JS_NewInt32(ctx, 0));
      JS_SetPropertyStr(ctx, times_obj, "nice", JS_NewInt32(ctx, 0));
      JS_SetPropertyStr(ctx, times_obj, "sys", JS_NewInt32(ctx, 0));
      JS_SetPropertyStr(ctx, times_obj, "idle", JS_NewInt32(ctx, 0));
      JS_SetPropertyStr(ctx, times_obj, "irq", JS_NewInt32(ctx, 0));
      JS_SetPropertyStr(ctx, cpu_obj, "times", times_obj);
    }

    JS_SetPropertyUint32(ctx, cpus_array, i, cpu_obj);
  }
#else
  // Other Unix-like systems - basic implementation
  long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cpus <= 0)
    num_cpus = 1;

  for (long i = 0; i < num_cpus; i++) {
    JSValue cpu_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, cpu_obj, "model", JS_NewString(ctx, "Unknown CPU"));
    JS_SetPropertyStr(ctx, cpu_obj, "speed", JS_NewInt32(ctx, 0));

    JSValue times_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, times_obj, "user", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, times_obj, "nice", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, times_obj, "sys", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, times_obj, "idle", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, times_obj, "irq", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, cpu_obj, "times", times_obj);

    JS_SetPropertyUint32(ctx, cpus_array, i, cpu_obj);
  }
#endif

  return cpus_array;
}

// Helper function to calculate CIDR prefix from netmask
static int netmask_to_cidr(struct sockaddr* netmask) {
  if (netmask->sa_family == AF_INET) {
    uint32_t mask = ntohl(((struct sockaddr_in*)netmask)->sin_addr.s_addr);
    int cidr = 0;
    while (mask) {
      cidr += (mask & 1);
      mask >>= 1;
    }
    return cidr;
  } else if (netmask->sa_family == AF_INET6) {
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)netmask;
    int cidr = 0;
    for (int i = 0; i < 16; i++) {
      uint8_t byte = addr6->sin6_addr.s6_addr[i];
      while (byte) {
        cidr += (byte & 1);
        byte >>= 1;
      }
    }
    return cidr;
  }
  return 0;
}

// node:os.networkInterfaces implementation
static JSValue js_os_network_interfaces(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  uv_interface_address_t* addresses;
  int count;
  int err = uv_interface_addresses(&addresses, &count);

  if (err != 0) {
    return JS_ThrowTypeError(ctx, "Failed to get network interfaces: %s", uv_strerror(err));
  }

  JSValue result = JS_NewObject(ctx);

  for (int i = 0; i < count; i++) {
    uv_interface_address_t* addr = &addresses[i];
    const char* name = addr->name;

    // Get or create array for this interface name
    JSValue iface_array = JS_GetPropertyStr(ctx, result, name);
    if (JS_IsUndefined(iface_array)) {
      iface_array = JS_NewArray(ctx);
      JS_SetPropertyStr(ctx, result, name, JS_DupValue(ctx, iface_array));
    }

    // Create address object
    JSValue addr_obj = JS_NewObject(ctx);

    // Set address
    char ip[INET6_ADDRSTRLEN];
    if (addr->address.address4.sin_family == AF_INET) {
      uv_ip4_name(&addr->address.address4, ip, sizeof(ip));
      JS_SetPropertyStr(ctx, addr_obj, "address", JS_NewString(ctx, ip));
      JS_SetPropertyStr(ctx, addr_obj, "family", JS_NewString(ctx, "IPv4"));

      // Set netmask
      char netmask_str[INET6_ADDRSTRLEN];
      uv_ip4_name(&addr->netmask.netmask4, netmask_str, sizeof(netmask_str));
      JS_SetPropertyStr(ctx, addr_obj, "netmask", JS_NewString(ctx, netmask_str));

      // Calculate CIDR
      int cidr = netmask_to_cidr((struct sockaddr*)&addr->netmask.netmask4);
      char cidr_str[64];
      snprintf(cidr_str, sizeof(cidr_str), "%s/%d", ip, cidr);
      JS_SetPropertyStr(ctx, addr_obj, "cidr", JS_NewString(ctx, cidr_str));
    } else if (addr->address.address4.sin_family == AF_INET6) {
      uv_ip6_name(&addr->address.address6, ip, sizeof(ip));
      JS_SetPropertyStr(ctx, addr_obj, "address", JS_NewString(ctx, ip));
      JS_SetPropertyStr(ctx, addr_obj, "family", JS_NewString(ctx, "IPv6"));

      // Set netmask
      char netmask_str[INET6_ADDRSTRLEN];
      uv_ip6_name(&addr->netmask.netmask6, netmask_str, sizeof(netmask_str));
      JS_SetPropertyStr(ctx, addr_obj, "netmask", JS_NewString(ctx, netmask_str));

      // Calculate CIDR
      int cidr = netmask_to_cidr((struct sockaddr*)&addr->netmask.netmask6);
      char cidr_str[128];
      snprintf(cidr_str, sizeof(cidr_str), "%s/%d", ip, cidr);
      JS_SetPropertyStr(ctx, addr_obj, "cidr", JS_NewString(ctx, cidr_str));

      // Add scopeid for IPv6
      JS_SetPropertyStr(ctx, addr_obj, "scopeid", JS_NewInt32(ctx, addr->address.address6.sin6_scope_id));
    }

    // Set MAC address
    char mac[18];
    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", (unsigned char)addr->phys_addr[0],
             (unsigned char)addr->phys_addr[1], (unsigned char)addr->phys_addr[2], (unsigned char)addr->phys_addr[3],
             (unsigned char)addr->phys_addr[4], (unsigned char)addr->phys_addr[5]);
    JS_SetPropertyStr(ctx, addr_obj, "mac", JS_NewString(ctx, mac));

    // Set internal flag
    JS_SetPropertyStr(ctx, addr_obj, "internal", JS_NewBool(ctx, addr->is_internal));

    // Add to array
    uint32_t array_len;
    JSValue len_val = JS_GetPropertyStr(ctx, iface_array, "length");
    JS_ToUint32(ctx, &array_len, len_val);
    JS_FreeValue(ctx, len_val);
    JS_SetPropertyUint32(ctx, iface_array, array_len, addr_obj);

    JS_FreeValue(ctx, iface_array);
  }

  uv_free_interface_addresses(addresses, count);
  return result;
}

// node:os.getPriority implementation
static JSValue js_os_get_priority(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  pid_t pid = 0;  // Default to current process

  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    int32_t pid_arg;
    if (JS_ToInt32(ctx, &pid_arg, argv[0]) < 0) {
      return JS_ThrowTypeError(ctx, "pid must be a number");
    }
    pid = (pid_t)pid_arg;
  }

#ifdef _WIN32
  // Windows implementation
  HANDLE process_handle;
  if (pid == 0) {
    process_handle = GetCurrentProcess();
  } else {
    process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!process_handle) {
      return JS_ThrowTypeError(ctx, "Failed to open process: %d", GetLastError());
    }
  }

  DWORD priority_class = GetPriorityClass(process_handle);
  if (pid != 0) {
    CloseHandle(process_handle);
  }

  if (priority_class == 0) {
    return JS_ThrowTypeError(ctx, "Failed to get priority class");
  }

  // Map Windows priority class to Unix nice value
  int priority;
  switch (priority_class) {
    case IDLE_PRIORITY_CLASS:
      priority = 19;
      break;
    case BELOW_NORMAL_PRIORITY_CLASS:
      priority = 10;
      break;
    case NORMAL_PRIORITY_CLASS:
      priority = 0;
      break;
    case ABOVE_NORMAL_PRIORITY_CLASS:
      priority = -7;
      break;
    case HIGH_PRIORITY_CLASS:
      priority = -14;
      break;
    case REALTIME_PRIORITY_CLASS:
      priority = -20;
      break;
    default:
      priority = 0;
  }

  return JS_NewInt32(ctx, priority);
#else
  // Unix implementation
  errno = 0;
  int priority = getpriority(PRIO_PROCESS, pid);

  if (priority == -1 && errno != 0) {
    return JS_ThrowTypeError(ctx, "getpriority failed: %s", strerror(errno));
  }

  return JS_NewInt32(ctx, priority);
#endif
}

// node:os.setPriority implementation
static JSValue js_os_set_priority(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  pid_t pid = 0;
  int32_t priority;

  if (argc == 0) {
    return JS_ThrowTypeError(ctx, "priority is required");
  }

  if (argc == 1) {
    // setPriority(priority)
    if (JS_ToInt32(ctx, &priority, argv[0]) < 0) {
      return JS_ThrowTypeError(ctx, "priority must be a number");
    }
  } else {
    // setPriority(pid, priority)
    int32_t pid_arg;
    if (JS_ToInt32(ctx, &pid_arg, argv[0]) < 0) {
      return JS_ThrowTypeError(ctx, "pid must be a number");
    }
    pid = (pid_t)pid_arg;

    if (JS_ToInt32(ctx, &priority, argv[1]) < 0) {
      return JS_ThrowTypeError(ctx, "priority must be a number");
    }
  }

  // Validate priority range
  if (priority < -20 || priority > 19) {
    return JS_ThrowRangeError(ctx, "priority must be between -20 and 19");
  }

#ifdef _WIN32
  // Windows implementation
  HANDLE process_handle;
  if (pid == 0) {
    process_handle = GetCurrentProcess();
  } else {
    process_handle = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!process_handle) {
      return JS_ThrowTypeError(ctx, "Failed to open process: %d", GetLastError());
    }
  }

  // Map Unix nice value to Windows priority class
  DWORD priority_class;
  if (priority >= 19) {
    priority_class = IDLE_PRIORITY_CLASS;
  } else if (priority >= 10) {
    priority_class = BELOW_NORMAL_PRIORITY_CLASS;
  } else if (priority >= 0) {
    priority_class = NORMAL_PRIORITY_CLASS;
  } else if (priority >= -7) {
    priority_class = ABOVE_NORMAL_PRIORITY_CLASS;
  } else if (priority >= -14) {
    priority_class = HIGH_PRIORITY_CLASS;
  } else {
    priority_class = REALTIME_PRIORITY_CLASS;
  }

  BOOL result = SetPriorityClass(process_handle, priority_class);
  if (pid != 0) {
    CloseHandle(process_handle);
  }

  if (!result) {
    return JS_ThrowTypeError(ctx, "Failed to set priority class: %d", GetLastError());
  }

  return JS_UNDEFINED;
#else
  // Unix implementation
  if (setpriority(PRIO_PROCESS, pid, priority) == -1) {
    return JS_ThrowTypeError(ctx, "setpriority failed: %s", strerror(errno));
  }

  return JS_UNDEFINED;
#endif
}

// node:os.loadavg implementation
static JSValue js_os_loadavg(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue loadavg_array = JS_NewArray(ctx);

#if defined(_WIN32) || defined(__ANDROID__)
  // Windows and Android don't have load average, return zeros
  JS_SetPropertyUint32(ctx, loadavg_array, 0, JS_NewFloat64(ctx, 0.0));
  JS_SetPropertyUint32(ctx, loadavg_array, 1, JS_NewFloat64(ctx, 0.0));
  JS_SetPropertyUint32(ctx, loadavg_array, 2, JS_NewFloat64(ctx, 0.0));
#else
  double loadavg[3];
  if (getloadavg(loadavg, 3) == 3) {
    JS_SetPropertyUint32(ctx, loadavg_array, 0, JS_NewFloat64(ctx, loadavg[0]));
    JS_SetPropertyUint32(ctx, loadavg_array, 1, JS_NewFloat64(ctx, loadavg[1]));
    JS_SetPropertyUint32(ctx, loadavg_array, 2, JS_NewFloat64(ctx, loadavg[2]));
  } else {
    // Fallback to zeros if getloadavg fails
    JS_SetPropertyUint32(ctx, loadavg_array, 0, JS_NewFloat64(ctx, 0.0));
    JS_SetPropertyUint32(ctx, loadavg_array, 1, JS_NewFloat64(ctx, 0.0));
    JS_SetPropertyUint32(ctx, loadavg_array, 2, JS_NewFloat64(ctx, 0.0));
  }
#endif

  return loadavg_array;
}

// node:os.uptime implementation
static JSValue js_os_uptime(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  // Windows implementation
  ULONGLONG uptime_ms = GetTickCount64();
  double uptime_seconds = uptime_ms / 1000.0;
  return JS_NewFloat64(ctx, uptime_seconds);
#else
  // Unix implementation - read from /proc/uptime on Linux, or use sysctl on other Unix
  FILE* f = fopen("/proc/uptime", "r");
  if (f) {
    double uptime;
    if (fscanf(f, "%lf", &uptime) == 1) {
      fclose(f);
      return JS_NewFloat64(ctx, uptime);
    }
    fclose(f);
  }

  // Fallback: return 0 if we can't determine uptime
  return JS_NewFloat64(ctx, 0.0);
#endif
}

// node:os.totalmem implementation
static JSValue js_os_totalmem(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (GlobalMemoryStatusEx(&mem_status)) {
    return JS_NewFloat64(ctx, (double)mem_status.ullTotalPhys);
  }
  return JS_NewFloat64(ctx, 0);
#else
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGESIZE);
  if (pages > 0 && page_size > 0) {
    return JS_NewFloat64(ctx, (double)pages * page_size);
  }
  return JS_NewFloat64(ctx, 0);
#endif
}

// node:os.freemem implementation
static JSValue js_os_freemem(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (GlobalMemoryStatusEx(&mem_status)) {
    return JS_NewFloat64(ctx, (double)mem_status.ullAvailPhys);
  }
  return JS_NewFloat64(ctx, 0);
#elif defined(__APPLE__)
  // macOS-specific implementation using vm_statistics64
  vm_size_t page_size;
  vm_statistics64_data_t vm_stat;
  mach_msg_type_number_t host_count = sizeof(vm_stat) / sizeof(natural_t);

  if (host_page_size(mach_host_self(), &page_size) == KERN_SUCCESS &&
      host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stat, &host_count) == KERN_SUCCESS) {
    // Free memory = free pages + inactive pages + speculative pages
    uint64_t free_memory =
        (uint64_t)(vm_stat.free_count + vm_stat.inactive_count + vm_stat.speculative_count) * page_size;
    return JS_NewFloat64(ctx, (double)free_memory);
  }
  return JS_NewFloat64(ctx, 0);
#else
  // Linux and other Unix systems
  long pages = sysconf(_SC_AVPHYS_PAGES);
  long page_size = sysconf(_SC_PAGESIZE);
  if (pages > 0 && page_size > 0) {
    return JS_NewFloat64(ctx, (double)pages * page_size);
  }
  return JS_NewFloat64(ctx, 0);
#endif
}

// node:os.version implementation
static JSValue js_os_version(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  // Windows: return OS version string
  OSVERSIONINFOEXA osvi;
  ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
  if (GetVersionExA((OSVERSIONINFOA*)&osvi)) {
    char version[256];
    snprintf(version, sizeof(version), "Windows %d.%d Build %d", osvi.dwMajorVersion, osvi.dwMinorVersion,
             osvi.dwBuildNumber);
    return JS_NewString(ctx, version);
  }
  return JS_NewString(ctx, "Windows");
#else
  // Unix: read /proc/version or use uname -v
  FILE* f = fopen("/proc/version", "r");
  if (f) {
    char version[256];
    if (fgets(version, sizeof(version), f)) {
      // Remove trailing newline
      char* newline = strchr(version, '\n');
      if (newline)
        *newline = '\0';
      fclose(f);
      return JS_NewString(ctx, version);
    }
    fclose(f);
  }

  // Fallback to uname
  struct utsname uts;
  if (uname(&uts) == 0) {
    char version[512];
    snprintf(version, sizeof(version), "%s %s %s", uts.sysname, uts.release, uts.version);
    return JS_NewString(ctx, version);
  }

  return JS_NewString(ctx, "Unknown");
#endif
}

// node:os.machine implementation
static JSValue js_os_machine(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
#ifdef _WIN64
  return JS_NewString(ctx, "x86_64");
#else
  return JS_NewString(ctx, "i686");
#endif
#else
  struct utsname uts;
  if (uname(&uts) == 0) {
    return JS_NewString(ctx, uts.machine);
  }
  return JS_NewString(ctx, "unknown");
#endif
}

// node:os.availableParallelism implementation
static JSValue js_os_available_parallelism(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return JS_NewInt32(ctx, si.dwNumberOfProcessors);
#else
  long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cpus <= 0)
    num_cpus = 1;
  return JS_NewInt32(ctx, (int)num_cpus);
#endif
}

// Helper function to create os.constants object
static JSValue create_os_constants(JSContext* ctx) {
  JSValue constants = JS_NewObject(ctx);

  // Create signals object
  JSValue signals = JS_NewObject(ctx);
#ifndef _WIN32
#ifdef SIGHUP
  JS_SetPropertyStr(ctx, signals, "SIGHUP", JS_NewInt32(ctx, SIGHUP));
#endif
#ifdef SIGINT
  JS_SetPropertyStr(ctx, signals, "SIGINT", JS_NewInt32(ctx, SIGINT));
#endif
#ifdef SIGQUIT
  JS_SetPropertyStr(ctx, signals, "SIGQUIT", JS_NewInt32(ctx, SIGQUIT));
#endif
#ifdef SIGILL
  JS_SetPropertyStr(ctx, signals, "SIGILL", JS_NewInt32(ctx, SIGILL));
#endif
#ifdef SIGTRAP
  JS_SetPropertyStr(ctx, signals, "SIGTRAP", JS_NewInt32(ctx, SIGTRAP));
#endif
#ifdef SIGABRT
  JS_SetPropertyStr(ctx, signals, "SIGABRT", JS_NewInt32(ctx, SIGABRT));
#endif
#ifdef SIGBUS
  JS_SetPropertyStr(ctx, signals, "SIGBUS", JS_NewInt32(ctx, SIGBUS));
#endif
#ifdef SIGFPE
  JS_SetPropertyStr(ctx, signals, "SIGFPE", JS_NewInt32(ctx, SIGFPE));
#endif
#ifdef SIGKILL
  JS_SetPropertyStr(ctx, signals, "SIGKILL", JS_NewInt32(ctx, SIGKILL));
#endif
#ifdef SIGUSR1
  JS_SetPropertyStr(ctx, signals, "SIGUSR1", JS_NewInt32(ctx, SIGUSR1));
#endif
#ifdef SIGSEGV
  JS_SetPropertyStr(ctx, signals, "SIGSEGV", JS_NewInt32(ctx, SIGSEGV));
#endif
#ifdef SIGUSR2
  JS_SetPropertyStr(ctx, signals, "SIGUSR2", JS_NewInt32(ctx, SIGUSR2));
#endif
#ifdef SIGPIPE
  JS_SetPropertyStr(ctx, signals, "SIGPIPE", JS_NewInt32(ctx, SIGPIPE));
#endif
#ifdef SIGALRM
  JS_SetPropertyStr(ctx, signals, "SIGALRM", JS_NewInt32(ctx, SIGALRM));
#endif
#ifdef SIGTERM
  JS_SetPropertyStr(ctx, signals, "SIGTERM", JS_NewInt32(ctx, SIGTERM));
#endif
#ifdef SIGCHLD
  JS_SetPropertyStr(ctx, signals, "SIGCHLD", JS_NewInt32(ctx, SIGCHLD));
#endif
#ifdef SIGCONT
  JS_SetPropertyStr(ctx, signals, "SIGCONT", JS_NewInt32(ctx, SIGCONT));
#endif
#ifdef SIGSTOP
  JS_SetPropertyStr(ctx, signals, "SIGSTOP", JS_NewInt32(ctx, SIGSTOP));
#endif
#ifdef SIGTSTP
  JS_SetPropertyStr(ctx, signals, "SIGTSTP", JS_NewInt32(ctx, SIGTSTP));
#endif
#ifdef SIGTTIN
  JS_SetPropertyStr(ctx, signals, "SIGTTIN", JS_NewInt32(ctx, SIGTTIN));
#endif
#ifdef SIGTTOU
  JS_SetPropertyStr(ctx, signals, "SIGTTOU", JS_NewInt32(ctx, SIGTTOU));
#endif
#ifdef SIGURG
  JS_SetPropertyStr(ctx, signals, "SIGURG", JS_NewInt32(ctx, SIGURG));
#endif
#ifdef SIGXCPU
  JS_SetPropertyStr(ctx, signals, "SIGXCPU", JS_NewInt32(ctx, SIGXCPU));
#endif
#ifdef SIGXFSZ
  JS_SetPropertyStr(ctx, signals, "SIGXFSZ", JS_NewInt32(ctx, SIGXFSZ));
#endif
#ifdef SIGVTALRM
  JS_SetPropertyStr(ctx, signals, "SIGVTALRM", JS_NewInt32(ctx, SIGVTALRM));
#endif
#ifdef SIGPROF
  JS_SetPropertyStr(ctx, signals, "SIGPROF", JS_NewInt32(ctx, SIGPROF));
#endif
#ifdef SIGWINCH
  JS_SetPropertyStr(ctx, signals, "SIGWINCH", JS_NewInt32(ctx, SIGWINCH));
#endif
#ifdef SIGIO
  JS_SetPropertyStr(ctx, signals, "SIGIO", JS_NewInt32(ctx, SIGIO));
#endif
#ifdef SIGSYS
  JS_SetPropertyStr(ctx, signals, "SIGSYS", JS_NewInt32(ctx, SIGSYS));
#endif
#endif  // !_WIN32
  JS_SetPropertyStr(ctx, constants, "signals", signals);

  // Create errno object
  JSValue errno_obj = JS_NewObject(ctx);
#ifdef E2BIG
  JS_SetPropertyStr(ctx, errno_obj, "E2BIG", JS_NewInt32(ctx, E2BIG));
#endif
#ifdef EACCES
  JS_SetPropertyStr(ctx, errno_obj, "EACCES", JS_NewInt32(ctx, EACCES));
#endif
#ifdef EADDRINUSE
  JS_SetPropertyStr(ctx, errno_obj, "EADDRINUSE", JS_NewInt32(ctx, EADDRINUSE));
#endif
#ifdef EADDRNOTAVAIL
  JS_SetPropertyStr(ctx, errno_obj, "EADDRNOTAVAIL", JS_NewInt32(ctx, EADDRNOTAVAIL));
#endif
#ifdef EAGAIN
  JS_SetPropertyStr(ctx, errno_obj, "EAGAIN", JS_NewInt32(ctx, EAGAIN));
#endif
#ifdef ECONNREFUSED
  JS_SetPropertyStr(ctx, errno_obj, "ECONNREFUSED", JS_NewInt32(ctx, ECONNREFUSED));
#endif
#ifdef ECONNRESET
  JS_SetPropertyStr(ctx, errno_obj, "ECONNRESET", JS_NewInt32(ctx, ECONNRESET));
#endif
#ifdef EEXIST
  JS_SetPropertyStr(ctx, errno_obj, "EEXIST", JS_NewInt32(ctx, EEXIST));
#endif
#ifdef EINVAL
  JS_SetPropertyStr(ctx, errno_obj, "EINVAL", JS_NewInt32(ctx, EINVAL));
#endif
#ifdef EMFILE
  JS_SetPropertyStr(ctx, errno_obj, "EMFILE", JS_NewInt32(ctx, EMFILE));
#endif
#ifdef ENOENT
  JS_SetPropertyStr(ctx, errno_obj, "ENOENT", JS_NewInt32(ctx, ENOENT));
#endif
#ifdef ENOMEM
  JS_SetPropertyStr(ctx, errno_obj, "ENOMEM", JS_NewInt32(ctx, ENOMEM));
#endif
#ifdef ENOTDIR
  JS_SetPropertyStr(ctx, errno_obj, "ENOTDIR", JS_NewInt32(ctx, ENOTDIR));
#endif
#ifdef EPERM
  JS_SetPropertyStr(ctx, errno_obj, "EPERM", JS_NewInt32(ctx, EPERM));
#endif
#ifdef EPIPE
  JS_SetPropertyStr(ctx, errno_obj, "EPIPE", JS_NewInt32(ctx, EPIPE));
#endif
#ifdef ETIMEDOUT
  JS_SetPropertyStr(ctx, errno_obj, "ETIMEDOUT", JS_NewInt32(ctx, ETIMEDOUT));
#endif
  JS_SetPropertyStr(ctx, constants, "errno", errno_obj);

  // Create priority object
  JSValue priority = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, priority, "PRIORITY_LOW", JS_NewInt32(ctx, 19));
  JS_SetPropertyStr(ctx, priority, "PRIORITY_BELOW_NORMAL", JS_NewInt32(ctx, 10));
  JS_SetPropertyStr(ctx, priority, "PRIORITY_NORMAL", JS_NewInt32(ctx, 0));
  JS_SetPropertyStr(ctx, priority, "PRIORITY_ABOVE_NORMAL", JS_NewInt32(ctx, -7));
  JS_SetPropertyStr(ctx, priority, "PRIORITY_HIGH", JS_NewInt32(ctx, -14));
  JS_SetPropertyStr(ctx, priority, "PRIORITY_HIGHEST", JS_NewInt32(ctx, -20));
  JS_SetPropertyStr(ctx, constants, "priority", priority);

  return constants;
}

// Initialize node:os module for CommonJS
JSValue JSRT_InitNodeOs(JSContext* ctx) {
  JSValue os_obj = JS_NewObject(ctx);

  // Add methods
  JS_SetPropertyStr(ctx, os_obj, "arch", JS_NewCFunction(ctx, js_os_arch, "arch", 0));
  JS_SetPropertyStr(ctx, os_obj, "platform", JS_NewCFunction(ctx, js_os_platform, "platform", 0));
  JS_SetPropertyStr(ctx, os_obj, "type", JS_NewCFunction(ctx, js_os_type, "type", 0));
  JS_SetPropertyStr(ctx, os_obj, "release", JS_NewCFunction(ctx, js_os_release, "release", 0));
  JS_SetPropertyStr(ctx, os_obj, "hostname", JS_NewCFunction(ctx, js_os_hostname, "hostname", 0));
  JS_SetPropertyStr(ctx, os_obj, "tmpdir", JS_NewCFunction(ctx, js_os_tmpdir, "tmpdir", 0));
  JS_SetPropertyStr(ctx, os_obj, "homedir", JS_NewCFunction(ctx, js_os_homedir, "homedir", 0));
  JS_SetPropertyStr(ctx, os_obj, "userInfo", JS_NewCFunction(ctx, js_os_userInfo, "userInfo", 0));
  JS_SetPropertyStr(ctx, os_obj, "endianness", JS_NewCFunction(ctx, js_os_endianness, "endianness", 0));
  JS_SetPropertyStr(ctx, os_obj, "version", JS_NewCFunction(ctx, js_os_version, "version", 0));
  JS_SetPropertyStr(ctx, os_obj, "machine", JS_NewCFunction(ctx, js_os_machine, "machine", 0));
  JS_SetPropertyStr(ctx, os_obj, "availableParallelism",
                    JS_NewCFunction(ctx, js_os_available_parallelism, "availableParallelism", 0));

  // Add system information methods
  JS_SetPropertyStr(ctx, os_obj, "cpus", JS_NewCFunction(ctx, js_os_cpus, "cpus", 0));
  JS_SetPropertyStr(ctx, os_obj, "networkInterfaces",
                    JS_NewCFunction(ctx, js_os_network_interfaces, "networkInterfaces", 0));
  JS_SetPropertyStr(ctx, os_obj, "getPriority", JS_NewCFunction(ctx, js_os_get_priority, "getPriority", 1));
  JS_SetPropertyStr(ctx, os_obj, "setPriority", JS_NewCFunction(ctx, js_os_set_priority, "setPriority", 2));
  JS_SetPropertyStr(ctx, os_obj, "loadavg", JS_NewCFunction(ctx, js_os_loadavg, "loadavg", 0));
  JS_SetPropertyStr(ctx, os_obj, "uptime", JS_NewCFunction(ctx, js_os_uptime, "uptime", 0));
  JS_SetPropertyStr(ctx, os_obj, "totalmem", JS_NewCFunction(ctx, js_os_totalmem, "totalmem", 0));
  JS_SetPropertyStr(ctx, os_obj, "freemem", JS_NewCFunction(ctx, js_os_freemem, "freemem", 0));

  // Add constants
  JS_SetPropertyStr(ctx, os_obj, "EOL",
                    JS_NewString(ctx,
#ifdef _WIN32
                                 "\r\n"
#else
                                 "\n"
#endif
                                 ));
  JS_SetPropertyStr(ctx, os_obj, "devNull",
                    JS_NewString(ctx,
#ifdef _WIN32
                                 "\\\\.\\nul"
#else
                                 "/dev/null"
#endif
                                 ));
  JS_SetPropertyStr(ctx, os_obj, "constants", create_os_constants(ctx));

  return os_obj;
}

// Initialize node:os module for ES modules
int js_node_os_init(JSContext* ctx, JSModuleDef* m) {
  JSValue os_module = JSRT_InitNodeOs(ctx);

  // Export as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, os_module));

  // Export individual functions
  JSValue arch = JS_GetPropertyStr(ctx, os_module, "arch");
  JS_SetModuleExport(ctx, m, "arch", JS_DupValue(ctx, arch));
  JS_FreeValue(ctx, arch);

  JSValue platform = JS_GetPropertyStr(ctx, os_module, "platform");
  JS_SetModuleExport(ctx, m, "platform", JS_DupValue(ctx, platform));
  JS_FreeValue(ctx, platform);

  JSValue type = JS_GetPropertyStr(ctx, os_module, "type");
  JS_SetModuleExport(ctx, m, "type", JS_DupValue(ctx, type));
  JS_FreeValue(ctx, type);

  JSValue release = JS_GetPropertyStr(ctx, os_module, "release");
  JS_SetModuleExport(ctx, m, "release", JS_DupValue(ctx, release));
  JS_FreeValue(ctx, release);

  JSValue hostname = JS_GetPropertyStr(ctx, os_module, "hostname");
  JS_SetModuleExport(ctx, m, "hostname", JS_DupValue(ctx, hostname));
  JS_FreeValue(ctx, hostname);

  JSValue tmpdir = JS_GetPropertyStr(ctx, os_module, "tmpdir");
  JS_SetModuleExport(ctx, m, "tmpdir", JS_DupValue(ctx, tmpdir));
  JS_FreeValue(ctx, tmpdir);

  JSValue homedir = JS_GetPropertyStr(ctx, os_module, "homedir");
  JS_SetModuleExport(ctx, m, "homedir", JS_DupValue(ctx, homedir));
  JS_FreeValue(ctx, homedir);

  JSValue userInfo = JS_GetPropertyStr(ctx, os_module, "userInfo");
  JS_SetModuleExport(ctx, m, "userInfo", JS_DupValue(ctx, userInfo));
  JS_FreeValue(ctx, userInfo);

  JSValue endianness = JS_GetPropertyStr(ctx, os_module, "endianness");
  JS_SetModuleExport(ctx, m, "endianness", JS_DupValue(ctx, endianness));
  JS_FreeValue(ctx, endianness);

  JSValue version = JS_GetPropertyStr(ctx, os_module, "version");
  JS_SetModuleExport(ctx, m, "version", JS_DupValue(ctx, version));
  JS_FreeValue(ctx, version);

  JSValue machine = JS_GetPropertyStr(ctx, os_module, "machine");
  JS_SetModuleExport(ctx, m, "machine", JS_DupValue(ctx, machine));
  JS_FreeValue(ctx, machine);

  JSValue availableParallelism = JS_GetPropertyStr(ctx, os_module, "availableParallelism");
  JS_SetModuleExport(ctx, m, "availableParallelism", JS_DupValue(ctx, availableParallelism));
  JS_FreeValue(ctx, availableParallelism);

  JSValue cpus = JS_GetPropertyStr(ctx, os_module, "cpus");
  JS_SetModuleExport(ctx, m, "cpus", JS_DupValue(ctx, cpus));
  JS_FreeValue(ctx, cpus);

  JSValue networkInterfaces = JS_GetPropertyStr(ctx, os_module, "networkInterfaces");
  JS_SetModuleExport(ctx, m, "networkInterfaces", JS_DupValue(ctx, networkInterfaces));
  JS_FreeValue(ctx, networkInterfaces);

  JSValue getPriority = JS_GetPropertyStr(ctx, os_module, "getPriority");
  JS_SetModuleExport(ctx, m, "getPriority", JS_DupValue(ctx, getPriority));
  JS_FreeValue(ctx, getPriority);

  JSValue setPriority = JS_GetPropertyStr(ctx, os_module, "setPriority");
  JS_SetModuleExport(ctx, m, "setPriority", JS_DupValue(ctx, setPriority));
  JS_FreeValue(ctx, setPriority);

  JSValue loadavg = JS_GetPropertyStr(ctx, os_module, "loadavg");
  JS_SetModuleExport(ctx, m, "loadavg", JS_DupValue(ctx, loadavg));
  JS_FreeValue(ctx, loadavg);

  JSValue uptime = JS_GetPropertyStr(ctx, os_module, "uptime");
  JS_SetModuleExport(ctx, m, "uptime", JS_DupValue(ctx, uptime));
  JS_FreeValue(ctx, uptime);

  JSValue totalmem = JS_GetPropertyStr(ctx, os_module, "totalmem");
  JS_SetModuleExport(ctx, m, "totalmem", JS_DupValue(ctx, totalmem));
  JS_FreeValue(ctx, totalmem);

  JSValue freemem = JS_GetPropertyStr(ctx, os_module, "freemem");
  JS_SetModuleExport(ctx, m, "freemem", JS_DupValue(ctx, freemem));
  JS_FreeValue(ctx, freemem);

  JSValue eol = JS_GetPropertyStr(ctx, os_module, "EOL");
  JS_SetModuleExport(ctx, m, "EOL", JS_DupValue(ctx, eol));
  JS_FreeValue(ctx, eol);

  JSValue devNull = JS_GetPropertyStr(ctx, os_module, "devNull");
  JS_SetModuleExport(ctx, m, "devNull", JS_DupValue(ctx, devNull));
  JS_FreeValue(ctx, devNull);

  JSValue constants = JS_GetPropertyStr(ctx, os_module, "constants");
  JS_SetModuleExport(ctx, m, "constants", JS_DupValue(ctx, constants));
  JS_FreeValue(ctx, constants);

  JS_FreeValue(ctx, os_module);
  return 0;
}
