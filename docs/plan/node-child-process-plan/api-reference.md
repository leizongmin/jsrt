# Node.js child_process API Reference

Complete API specification for implementation in jsrt.

## Table of Contents

1. [Asynchronous Methods](#asynchronous-methods)
2. [Synchronous Methods](#synchronous-methods)
3. [ChildProcess Class](#childprocess-class)
4. [Options Objects](#options-objects)
5. [Error Codes](#error-codes)

## Asynchronous Methods

### child_process.spawn(command[, args][, options])

Spawns a new process using the given command.

**Parameters:**
- `command` {string} The command to run
- `args` {string[]} List of string arguments
- `options` {Object} See [SpawnOptions](#spawnoptions)

**Returns:** {ChildProcess}

**Example:**
```javascript
const { spawn } = require('child_process');
const ls = spawn('ls', ['-lh', '/usr']);

ls.stdout.on('data', (data) => {
  console.log(`stdout: ${data}`);
});

ls.on('close', (code) => {
  console.log(`child process exited with code ${code}`);
});
```

### child_process.exec(command[, options][, callback])

Spawns a shell and runs command within that shell, buffering the output.

**Parameters:**
- `command` {string} The command to run, with space-separated arguments
- `options` {Object} See [ExecOptions](#execoptions)
- `callback` {Function} Called when process terminates
  - `error` {Error|null}
  - `stdout` {string|Buffer}
  - `stderr` {string|Buffer}

**Returns:** {ChildProcess}

**Example:**
```javascript
const { exec } = require('child_process');

exec('cat *.js | wc -l', (error, stdout, stderr) => {
  if (error) {
    console.error(`exec error: ${error}`);
    return;
  }
  console.log(`stdout: ${stdout}`);
});
```

### child_process.execFile(file[, args][, options][, callback])

Similar to exec() but does not spawn a shell by default.

**Parameters:**
- `file` {string} The name or path of the executable file to run
- `args` {string[]} List of string arguments
- `options` {Object} See [ExecFileOptions](#execfileoptions)
- `callback` {Function} Same as exec()

**Returns:** {ChildProcess}

**Example:**
```javascript
const { execFile } = require('child_process');

execFile('node', ['--version'], (error, stdout, stderr) => {
  if (error) {
    throw error;
  }
  console.log(stdout);
});
```

### child_process.fork(modulePath[, args][, options])

Spawns a new Node.js process and invokes a specified module with an IPC channel.

**Parameters:**
- `modulePath` {string} The module to run in the child
- `args` {string[]} List of string arguments
- `options` {Object} See [ForkOptions](#forkoptions)

**Returns:** {ChildProcess}

**Example:**
```javascript
const { fork } = require('child_process');

const child = fork('child.js');
child.on('message', (msg) => {
  console.log('Message from child', msg);
});
child.send({ hello: 'world' });
```

## Synchronous Methods

### child_process.spawnSync(command[, args][, options])

Synchronous version of spawn(). Blocks until child process exits.

**Parameters:**
- `command` {string}
- `args` {string[]}
- `options` {Object} See [SpawnSyncOptions](#spawnsyncoptions)

**Returns:** {Object}
- `pid` {number} Pid of the child process
- `output` {Array} Array of results from stdio output
- `stdout` {Buffer|string}
- `stderr` {Buffer|string}
- `status` {number|null} Exit code, or null if killed
- `signal` {string|null} Signal used to kill, or null
- `error` {Error|undefined} Error object if spawn failed

**Example:**
```javascript
const { spawnSync } = require('child_process');

const result = spawnSync('ls', ['-lh', '/usr']);
console.log('stdout:', result.stdout.toString());
console.log('exit code:', result.status);
```

### child_process.execSync(command[, options])

Synchronous version of exec(). Returns stdout of the command.

**Parameters:**
- `command` {string}
- `options` {Object} See [ExecSyncOptions](#execsyncoptions)

**Returns:** {Buffer|string} The stdout from the command

**Throws:** {Error} If command exits with non-zero status

**Example:**
```javascript
const { execSync } = require('child_process');

try {
  const stdout = execSync('ls -lh /usr');
  console.log('stdout:', stdout.toString());
} catch (error) {
  console.error('Error:', error.status);  // Exit code
}
```

### child_process.execFileSync(file[, args][, options])

Synchronous version of execFile().

**Parameters:**
- `file` {string}
- `args` {string[]}
- `options` {Object} See [ExecFileSyncOptions](#execfilesyncoptions)

**Returns:** {Buffer|string} The stdout from the command

**Example:**
```javascript
const { execFileSync } = require('child_process');

const output = execFileSync('node', ['--version']);
console.log(output.toString());
```

## ChildProcess Class

Instances of ChildProcess are EventEmitters representing spawned child processes.

### Events

#### Event: 'close'

- `code` {number} Exit code if child exited normally
- `signal` {string} Signal that terminated the process

Emitted when stdio streams have been closed.

#### Event: 'disconnect'

Emitted after calling `subprocess.disconnect()` or `process.disconnect()` in child.

#### Event: 'error'

- `err` {Error}

Emitted when:
- The process could not be spawned
- The process could not be killed
- Sending message to child failed

#### Event: 'exit'

- `code` {number|null} Exit code if child exited normally
- `signal` {string|null} Signal that terminated the process

Emitted when child process ends.

#### Event: 'message'

- `message` {Object} Parsed JSON object or primitive value
- `sendHandle` {Handle} Server or Socket object (jsrt: may not support initially)

Emitted when child process uses `process.send()`.

#### Event: 'spawn'

Emitted when child process has spawned successfully.

### Properties

#### subprocess.channel
- {Object|undefined} A pipe representing the IPC channel to the child

#### subprocess.connected
- {boolean} Set to false after `subprocess.disconnect()` is called

#### subprocess.exitCode
- {number|null} Exit code of child process, or null if still running

#### subprocess.killed
- {boolean} True if kill() was used to send signal to child

#### subprocess.pid
- {number|undefined} Process ID of the child process

#### subprocess.signalCode
- {string|null} Signal received by child process, or null

#### subprocess.stderr
- {stream.Readable|null} Represents child's stderr

#### subprocess.stdin
- {stream.Writable|null} Represents child's stdin

#### subprocess.stdout
- {stream.Readable|null} Represents child's stdout

#### subprocess.stdio
- {Array} Sparse array of pipes corresponding to child's file descriptors

### Methods

#### subprocess.disconnect()

Closes IPC channel between parent and child.

#### subprocess.kill([signal])

- `signal` {string|number} Default: 'SIGTERM'
- Returns: {boolean}

Sends signal to child process.

#### subprocess.ref()

Calling ref() on a child will undo unref(), ensuring parent waits for child to exit.

#### subprocess.send(message[, sendHandle][, options][, callback])

- `message` {Object}
- `sendHandle` {Handle} (jsrt: may not support initially)
- `options` {Object}
- `callback` {Function}
- Returns: {boolean}

Sends message to child when IPC channel exists.

#### subprocess.unref()

Allows parent to exit independently of child, unless there's an active IPC channel.

## Options Objects

### SpawnOptions

```typescript
interface SpawnOptions {
  cwd?: string;                    // Current working directory
  env?: Object;                    // Environment key-value pairs
  argv0?: string;                  // Explicitly set argv[0]
  stdio?: Array|string;            // Child's stdio configuration
  detached?: boolean;              // Prepare child to run independently
  uid?: number;                    // User identity (POSIX)
  gid?: number;                    // Group identity (POSIX)
  serialization?: string;          // IPC serialization type ('json' or 'advanced')
  shell?: boolean|string;          // Run command in shell
  windowsVerbatimArguments?: boolean;  // No quoting/escaping on Windows
  windowsHide?: boolean;           // Hide subprocess console on Windows
  signal?: AbortSignal;            // Abort signal to cancel spawning
  timeout?: number;                // Max time in ms for process to run
  killSignal?: string|number;      // Signal to use when timeout occurs
}
```

### ExecOptions

Extends SpawnOptions with:

```typescript
interface ExecOptions extends SpawnOptions {
  encoding?: string;               // Character encoding (default: 'buffer')
  shell?: string;                  // Shell to execute command (default: '/bin/sh')
  timeout?: number;                // Max time in ms (default: undefined)
  maxBuffer?: number;              // Largest amount of data allowed on stdout/stderr
                                   // (default: 1024 * 1024 = 1 MB)
  killSignal?: string|number;      // Signal to use when timeout/maxBuffer (default: 'SIGTERM')
  windowsHide?: boolean;           // Hide console on Windows (default: false)
}
```

### ExecFileOptions

Same as ExecOptions but `shell` defaults to `false`.

### ForkOptions

Extends SpawnOptions with:

```typescript
interface ForkOptions extends SpawnOptions {
  execPath?: string;               // Executable used to create child process
  execArgv?: string[];             // List of string arguments passed to executable
  silent?: boolean;                // If true, stdin/stdout/stderr piped to parent
  stdio?: Array|string;            // Supports 'pipe', 'ignore', 'inherit', or array
  detached?: boolean;              // Prepare child to run independently
  windowsVerbatimArguments?: boolean;
}
```

### SpawnSyncOptions

```typescript
interface SpawnSyncOptions {
  cwd?: string;
  input?: string|Buffer|TypedArray|DataView;  // Value passed as stdin
  argv0?: string;
  stdio?: Array|string;
  env?: Object;
  uid?: number;
  gid?: number;
  timeout?: number;                // Max time in ms (default: undefined)
  killSignal?: string|number;      // Signal when timeout (default: 'SIGTERM')
  maxBuffer?: number;              // Largest stdout/stderr (default: 1024 * 1024)
  encoding?: string;               // Character encoding (default: 'buffer')
  shell?: boolean|string;
  windowsVerbatimArguments?: boolean;
  windowsHide?: boolean;
}
```

### ExecSyncOptions

Same as SpawnSyncOptions with shell defaults to true.

### ExecFileSyncOptions

Same as SpawnSyncOptions with shell defaults to false.

### Stdio Configuration

The `stdio` option configures pipes established between parent and child:

**String shortcuts:**
- `'pipe'` - Creates pipe (default)
- `'ignore'` - Ignore this file descriptor
- `'inherit'` - Pass through corresponding parent stdio
- `'overlapped'` - Same as `'pipe'` on Windows

**Array form:** `[stdin, stdout, stderr]` or longer for additional fds

**Array element values:**
- `'pipe'` - Create pipe (streams become subprocess.stdio[fd])
- `'ipc'` - Create IPC channel (only 1 allowed, for fork())
- `'ignore'` - Ignore this fd
- `'inherit'` - Inherit from parent
- `'overlapped'` - Same as 'pipe' on Windows
- Stream object - Share readable/writable stream
- Positive integer - Share fd with specific parent fd
- `null`/`undefined` - Use default value (0-2: 'pipe', >=3: 'ignore')

**Examples:**
```javascript
// Default
stdio: ['pipe', 'pipe', 'pipe']

// Inherit parent's stdio
stdio: 'inherit'

// Ignore stdin, inherit stdout/stderr
stdio: ['ignore', 'inherit', 'inherit']

// IPC channel on fd 3 (for fork)
stdio: ['pipe', 'pipe', 'pipe', 'ipc']

// Write to file
const fs = require('fs');
stdio: ['ignore', fs.openSync('out.log', 'a'), 'ignore']
```

## Error Codes

### ERR_CHILD_PROCESS_IPC_REQUIRED

Thrown when using `subprocess.send()` but child was not spawned with IPC channel.

### ERR_CHILD_PROCESS_STDIO_MAXBUFFER

Thrown when exec/execFile output exceeds `maxBuffer` option.

### ERR_INVALID_ARG_TYPE

Invalid argument type (e.g., expected string, got number).

### ERR_INVALID_ARG_VALUE

Invalid argument value (e.g., invalid signal name).

### ERR_OUT_OF_RANGE

Value out of acceptable range (e.g., negative timeout).

### System Errors

Standard errno codes from libuv:
- `ENOENT` - File/command not found
- `EACCES` - Permission denied
- `E2BIG` - Argument list too long
- `EAGAIN` - Resource temporarily unavailable
- etc.

**Error object properties:**
- `code` - String error code (e.g., 'ENOENT')
- `errno` - Numeric error code
- `syscall` - System call that failed (e.g., 'spawn')
- `path` - File path (if applicable)

## Platform Differences

### Signals

**POSIX (Linux/macOS):**
- Full POSIX signal support: SIGTERM, SIGKILL, SIGHUP, SIGINT, etc.
- Signal names are case-sensitive strings

**Windows:**
- Limited signal support
- Only SIGKILL (process.kill) and SIGTERM (graceful shutdown) work
- Other signals throw errors

### Detached Processes

**POSIX:**
- Detached child becomes session leader
- Survives parent termination
- No controlling terminal

**Windows:**
- Spawned with CREATE_NEW_CONSOLE flag
- Runs independently

### uid/gid Options

- Only supported on POSIX platforms
- Throws error on Windows
- Requires appropriate privileges

## Implementation Priority

### Phase 1 (MVP)
- spawn() with basic options (cwd, env, stdio: pipe/ignore/inherit)
- ChildProcess events (spawn, exit, close, error)
- stdio streams (stdin/stdout/stderr)
- Basic error handling

### Phase 2
- exec() and execFile()
- Buffering and maxBuffer
- Timeout mechanism

### Phase 3
- Sync APIs (spawnSync, execSync, execFileSync)

### Phase 4
- fork() and IPC channel
- process.send() and 'message' event
- Message serialization

### Phase 5
- Platform-specific options (uid/gid, windowsHide, detached)
- Signal handling enhancements
- AbortSignal support

## Testing Requirements

Each API must have tests covering:
1. **Success cases** - Normal operation
2. **Error cases** - Invalid args, spawn failures, command not found
3. **Stdio** - All stdio modes (pipe, ignore, inherit)
4. **Options** - All option combinations
5. **Events** - All event emissions with correct data
6. **Memory** - ASAN validation, no leaks
7. **Edge cases** - Empty args, special characters, large buffers
8. **Platform** - Linux minimum, macOS if available

## References

- Node.js v21 Documentation: https://nodejs.org/docs/latest-v21.x/api/child_process.html
- libuv Process Guide: http://docs.libuv.org/en/v1.x/guide/processes.html
- Node.js Error Codes: https://nodejs.org/api/errors.html#nodejs-error-codes
