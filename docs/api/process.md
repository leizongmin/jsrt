# Process API Documentation

The `process` object provides information about, and control over, the current jsrt process. It is a global object and can be accessed from anywhere without requiring a module import.

## Table of Contents

- [Basic Information](#basic-information)
- [Properties](#properties)
- [Methods](#methods)
- [Events](#events)
- [Unix-Specific APIs](#unix-specific-apis)
- [Advanced Features](#advanced-features)
- [Stub APIs](#stub-apis)

## Basic Information

### process.pid
- **Type:** `<number>`
- **Description:** Returns the PID (Process ID) of the current process.

### process.ppid
- **Type:** `<number>`
- **Description:** Returns the PID of the parent process.

### process.platform
- **Type:** `<string>`
- **Description:** Returns the operating system platform (`'linux'`, `'darwin'`, `'win32'`).

### process.arch
- **Type:** `<string>`
- **Description:** Returns the CPU architecture (`'x64'`, `'arm64'`, `'ia32'`, etc.).

### process.argv
- **Type:** `<Array<string>>`
- **Description:** Returns an array containing the command-line arguments passed when the jsrt process was launched.

### process.argv0
- **Type:** `<string>`
- **Description:** Returns the value of `argv[0]` passed when jsrt was launched.

### process.execPath
- **Type:** `<string>`
- **Description:** Returns the absolute pathname of the jsrt executable.

### process.execArgv
- **Type:** `<Array<string>>`
- **Description:** Returns the set of jsrt-specific command-line options passed when the process was launched.

## Properties

### process.version
- **Type:** `<string>`
- **Description:** Returns the jsrt version string.

### process.versions
- **Type:** `<Object>`
- **Description:** Returns an object listing the version strings of jsrt and its dependencies.

### process.env
- **Type:** `<Object>`
- **Description:** Returns an object containing the user environment. Supports reading and writing environment variables.

### process.exitCode
- **Type:** `<number>` | `<undefined>`
- **Description:** A number that will be used as the process exit code when the process exits gracefully.

### process.title
- **Type:** `<string>`
- **Description:** Gets or sets the current process title displayed in `ps` or Activity Monitor.
- **Platform Support:** Linux (via `prctl`), macOS (limited), Windows (SetConsoleTitle)

### process.config
- **Type:** `<Object>`
- **Description:** Returns an object containing the JavaScript representation of the configure options used to compile jsrt.

### process.release
- **Type:** `<Object>`
- **Description:** Returns an object containing metadata related to the current release.

### process.features
- **Type:** `<Object>`
- **Description:** Returns an object containing feature flags for the current jsrt build.

### process.sourceMapsEnabled
- **Type:** `<boolean>`
- **Description:** Returns whether source map support is enabled (stub implementation).

## Methods

### process.exit([code])
- **Parameters:**
  - `code` `<number>` - Exit code (default: `process.exitCode` or 0)
- **Description:** Terminates the process synchronously with the specified exit code.

### process.cwd()
- **Returns:** `<string>`
- **Description:** Returns the current working directory of the jsrt process.

### process.chdir(directory)
- **Parameters:**
  - `directory` `<string>` - Path to change to
- **Description:** Changes the current working directory. Throws an exception on failure.

### process.uptime()
- **Returns:** `<number>`
- **Description:** Returns the number of seconds the current jsrt process has been running.

### process.hrtime([time])
- **Parameters:**
  - `time` `<Array>` - Optional previous time from `hrtime()`
- **Returns:** `<Array>`
- **Description:** Returns the current high-resolution real time as `[seconds, nanoseconds]` tuple.

### process.hrtime.bigint()
- **Returns:** `<bigint>`
- **Description:** Returns the current high-resolution real time in nanoseconds as a BigInt.

### process.nextTick(callback[, ...args])
- **Parameters:**
  - `callback` `<Function>` - Function to invoke on the next tick
  - `...args` - Additional arguments to pass to the callback
- **Description:** Adds `callback` to the "next tick queue". This queue is fully drained after the current operation completes and before the event loop continues.

### process.memoryUsage()
- **Returns:** `<Object>`
  - `rss` `<number>` - Resident Set Size
  - `heapTotal` `<number>` - Total heap size
  - `heapUsed` `<number>` - Used heap size
  - `external` `<number>` - Memory used by C++ objects
  - `arrayBuffers` `<number>` - Memory used by ArrayBuffers
- **Description:** Returns an object describing the memory usage of the jsrt process in bytes.

### process.memoryUsage.rss()
- **Returns:** `<number>`
- **Description:** Returns the Resident Set Size (RSS) in bytes.

### process.cpuUsage([previousValue])
- **Parameters:**
  - `previousValue` `<Object>` - Optional previous result from `cpuUsage()`
- **Returns:** `<Object>`
  - `user` `<number>` - User CPU time in microseconds
  - `system` `<number>` - System CPU time in microseconds
- **Description:** Returns the user and system CPU time usage of the current process.

### process.resourceUsage()
- **Returns:** `<Object>`
- **Description:** Returns an object containing resource usage for the current process. Includes CPU time, memory, I/O, and context switch statistics.

### process.availableMemory()
- **Returns:** `<number>`
- **Description:** Returns the amount of free system memory in bytes.

### process.constrainedMemory()
- **Returns:** `<number>` | `<undefined>`
- **Description:** Returns the amount of memory available due to limits imposed on the process (e.g., cgroups). Returns `undefined` if no constraint.

### process.kill(pid[, signal])
- **Parameters:**
  - `pid` `<number>` - Process ID
  - `signal` `<string>` | `<number>` - Signal to send (default: `'SIGTERM'`)
- **Description:** Sends a signal to the process identified by `pid`.

### process.emitWarning(warning[, options])
- **Parameters:**
  - `warning` `<string>` | `<Error>` - Warning to emit
  - `options` `<Object>` - Optional options (type, code, ctor)
- **Description:** Emits a custom or application-specific process warning.

### process.setUncaughtExceptionCaptureCallback(fn)
- **Parameters:**
  - `fn` `<Function>` | `<null>` - Callback function
- **Description:** Sets a function that will be called when an uncaught exception occurs.

### process.hasUncaughtExceptionCaptureCallback()
- **Returns:** `<boolean>`
- **Description:** Returns whether a callback has been set via `setUncaughtExceptionCaptureCallback()`.

### process.loadEnvFile([path])
- **Parameters:**
  - `path` `<string>` - Path to .env file (default: `.env`)
- **Description:** Loads a `.env` file and sets environment variables.

### process.getActiveResourcesInfo()
- **Returns:** `<Array<string>>`
- **Description:** Returns an array of strings describing the types of active resources keeping the event loop alive.

### process.setSourceMapsEnabled(val)
- **Parameters:**
  - `val` `<boolean>` - Whether to enable source maps
- **Description:** Enables or disables source map support (stub implementation).

### process.ref()
- **Description:** Creates a reference to keep the event loop alive.

### process.unref()
- **Description:** Removes the reference, allowing the event loop to exit.

## Events

The `process` object extends `EventEmitter` and emits the following events:

### Event: 'beforeExit'
- **Listener:** `(code) => {}`
- **Description:** Emitted when jsrt empties its event loop and has no additional work to schedule. Normally, the jsrt process will exit when there is no work scheduled, but a listener registered on the `'beforeExit'` event can make asynchronous calls, causing the process to continue.

### Event: 'exit'
- **Listener:** `(code) => {}`
- **Description:** Emitted when the jsrt process is about to exit. There is no way to prevent the exiting of the event loop at this point, and once all `'exit'` listeners have finished running, the process will exit.

### Event: 'warning'
- **Listener:** `(warning) => {}`
- **Description:** Emitted whenever a process warning is emitted via `process.emitWarning()`.

### Event: 'uncaughtException'
- **Listener:** `(error, origin) => {}`
- **Description:** Emitted when an uncaught JavaScript exception bubbles all the way back to the event loop.

### Event: 'uncaughtExceptionMonitor'
- **Listener:** `(error, origin) => {}`
- **Description:** Emitted before an 'uncaughtException' event is emitted. This event is intended for monitoring only.

### Event: 'unhandledRejection'
- **Listener:** `(reason, promise) => {}`
- **Description:** Emitted whenever a Promise is rejected and no error handler is attached to the promise.

### Event: 'rejectionHandled'
- **Listener:** `(promise) => {}`
- **Description:** Emitted whenever a Promise has been rejected and an error handler was attached to it later than one turn of the event loop.

### Signal Events
Signal events are emitted when the jsrt process receives a signal:

- `'SIGINT'` - Emitted when Ctrl+C is pressed
- `'SIGTERM'` - Emitted on termination request
- `'SIGHUP'` - Emitted when terminal is closed (Unix only)
- `'SIGQUIT'` - Emitted on quit signal (Unix only)
- `'SIGUSR1'`, `'SIGUSR2'` - User-defined signals (Unix only)
- `'SIGBREAK'` - Emitted on Ctrl+Break (Windows only)

Example:
```javascript
process.on('SIGINT', () => {
  console.log('Received SIGINT');
  process.exit(0);
});
```

## Unix-Specific APIs

The following APIs are only available on Unix-like systems (Linux, macOS, etc.) and are `undefined` on Windows.

### process.getuid()
- **Returns:** `<number>`
- **Description:** Returns the numeric user identity of the process.

### process.geteuid()
- **Returns:** `<number>`
- **Description:** Returns the numeric effective user identity of the process.

### process.getgid()
- **Returns:** `<number>`
- **Description:** Returns the numeric group identity of the process.

### process.getegid()
- **Returns:** `<number>`
- **Description:** Returns the numeric effective group identity of the process.

### process.setuid(id)
- **Parameters:**
  - `id` `<number>` | `<string>` - User ID or username
- **Description:** Sets the user identity of the process. Requires root privileges.

### process.seteuid(id)
- **Parameters:**
  - `id` `<number>` | `<string>` - Effective user ID or username
- **Description:** Sets the effective user identity of the process.

### process.setgid(id)
- **Parameters:**
  - `id` `<number>` | `<string>` - Group ID or group name
- **Description:** Sets the group identity of the process. Requires root privileges.

### process.setegid(id)
- **Parameters:**
  - `id` `<number>` | `<string>` - Effective group ID or group name
- **Description:** Sets the effective group identity of the process.

### process.getgroups()
- **Returns:** `<Array<number>>`
- **Description:** Returns an array of supplementary group IDs.

### process.setgroups(groups)
- **Parameters:**
  - `groups` `<Array<number | string>>` - Array of group IDs or names
- **Description:** Sets the supplementary group IDs. Requires root privileges.

### process.initgroups(user, extraGroup)
- **Parameters:**
  - `user` `<string>` | `<number>` - User name or ID
  - `extraGroup` `<string>` | `<number>` - Extra group name or ID
- **Description:** Initializes the group access list. Requires root privileges.

### process.umask([mask])
- **Parameters:**
  - `mask` `<string>` | `<number>` - Optional new umask value
- **Returns:** `<number>`
- **Description:** Gets or sets the file mode creation mask. If `mask` is provided, returns the previous value.

## Advanced Features

### process.on(eventName, listener)
- **Description:** Registers an event listener. Supports all standard EventEmitter methods (`once`, `off`, `removeListener`, `removeAllListeners`, etc.).

## Stub APIs

The following APIs are present for Node.js compatibility but are not fully implemented:

### process.report
- **Type:** `<Object>`
- **Description:** Diagnostic report generation (stub). Methods return `null`.
  - `process.report.writeReport([filename][, err])`
  - `process.report.getReport([err])`
  - Properties: `directory`, `filename`, `reportOnFatalError`, etc.

### process.permission
- **Type:** `<Object>`
- **Description:** Permission system (stub). Always returns `true`.
  - `process.permission.has(scope[, reference])`

### process.finalization
- **Type:** `<Object>`
- **Description:** Finalization registry (stub). No-op implementations.
  - `process.finalization.register(ref, callback)`
  - `process.finalization.unregister(token)`

### process.dlopen(module, filename)
- **Description:** Native addon loading (not implemented). Throws an error.

### process.getBuiltinModule(id)
- **Parameters:**
  - `id` `<string>` - Module identifier
- **Returns:** `<Object>` | `<null>`
- **Description:** Returns a builtin module if available (stub). Currently returns `null`.

## Platform Support

| Feature | Linux | macOS | Windows |
|---------|-------|-------|---------|
| Basic Properties | ✓ | ✓ | ✓ |
| Methods | ✓ | ✓ | ✓ |
| Events | ✓ | ✓ | ✓ |
| Signal Handling | ✓ | ✓ | Limited |
| Unix Permissions | ✓ | ✓ | ✗ |
| Resource Monitoring | ✓ | ✓ | Partial |
| Process Title | ✓ | Limited | ✓ |

## Memory Safety

All process APIs have been validated with AddressSanitizer (ASAN) to ensure:
- No memory leaks
- No use-after-free errors
- No buffer overflows
- Proper cleanup on process exit

## Examples

### Basic Usage
```javascript
console.log('Process ID:', process.pid);
console.log('Platform:', process.platform);
console.log('Arguments:', process.argv);
console.log('Working Directory:', process.cwd());
```

### Signal Handling
```javascript
process.on('SIGINT', () => {
  console.log('Received SIGINT. Cleaning up...');
  // Perform cleanup
  process.exit(0);
});
```

### Resource Monitoring
```javascript
const usage = process.memoryUsage();
console.log(`Memory: ${usage.rss / 1024 / 1024} MB`);

const cpu = process.cpuUsage();
console.log(`CPU: ${cpu.user + cpu.system} µs`);
```

### Environment Variables
```javascript
// Read environment variable
console.log('HOME:', process.env.HOME);

// Set environment variable
process.env.MY_VAR = 'value';

// Load from .env file
process.loadEnvFile('.env');
```

### Event Loop Control
```javascript
// Schedule work on next tick
process.nextTick(() => {
  console.log('Next tick callback');
});

// Keep process alive
process.ref();

// Allow process to exit
process.unref();
```

## Node.js Compatibility

jsrt aims for high compatibility with Node.js process APIs. The following are the main differences:

- **Source Maps:** Stub implementation (feature flag only)
- **Diagnostic Reports:** Stub implementation
- **Permissions:** Stub implementation (always allows)
- **Native Addons:** Not supported (`dlopen` throws error)
- **Inspector:** Not available

For a complete compatibility matrix, see [Node.js Migration Guide](../migration/node-process.md).
