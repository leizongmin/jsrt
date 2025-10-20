# Child Process Examples

This directory contains examples demonstrating the `node:child_process` module in jsrt.

## Available Examples

### Basic Examples

#### `spawn_basic.js`
Demonstrates basic `spawn()` usage:
- Spawning a simple command
- Capturing stdout/stderr
- Handling exit events
- Error handling

Run:
```bash
./bin/jsrt examples/child_process/spawn_basic.js
```

#### `spawn_advanced.js`
Advanced `spawn()` features:
- Custom environment variables
- Working directory (cwd) option
- Detached processes
- Process management with ref/unref

Run:
```bash
./bin/jsrt examples/child_process/spawn_advanced.js
```

#### `exec_example.js`
Shell execution with `exec()`:
- Simple shell commands
- Shell pipes and redirection
- Environment variable expansion
- Complex shell scripting
- Error handling

Run:
```bash
./bin/jsrt examples/child_process/exec_example.js
```

#### `sync_example.js`
Synchronous process execution:
- `execSync()` for buffered shell commands
- `spawnSync()` for direct command execution
- Timeout handling
- Error handling
- Exit code checking

Run:
```bash
./bin/jsrt examples/child_process/sync_example.js
```

## API Overview

### Asynchronous Methods

- **`spawn(command, args, options)`** - Spawns a process without a shell
  - Returns: `ChildProcess` instance
  - Use for: Long-running processes, streaming output

- **`exec(command, options, callback)`** - Runs command in a shell, buffers output
  - Returns: `ChildProcess` instance
  - Callback receives: `(error, stdout, stderr)`
  - Use for: Shell scripts, short commands

- **`execFile(file, args, options, callback)`** - Like exec but without shell
  - More efficient than exec
  - Use for: Running executables directly

- **`fork(modulePath, args, options)`** - Special case for spawning Node.js processes
  - Includes IPC channel
  - Use for: Creating child Node.js processes

### Synchronous Methods

- **`spawnSync(command, args, options)`** - Synchronous spawn
  - Returns: `{status, signal, stdout, stderr, error}`
  - Blocks until process completes

- **`execSync(command, options)`** - Synchronous exec
  - Returns: stdout buffer/string
  - Throws on non-zero exit code

- **`execFileSync(file, args, options)`** - Synchronous execFile
  - Returns: stdout buffer/string
  - Throws on non-zero exit code

## Common Options

All methods support these options:

- `cwd` - Working directory
- `env` - Environment variables
- `stdio` - Stdio configuration ('pipe', 'ignore', 'inherit')
- `timeout` - Maximum execution time (ms)
- `uid` - User ID (POSIX)
- `gid` - Group ID (POSIX)
- `detached` - Run process independently
- `windowsHide` - Hide console window (Windows)
- `shell` - Run in shell (exec uses shell by default)

## ChildProcess Events

- `spawn` - Process has been spawned
- `exit` - Process has exited (code, signal)
- `close` - All stdio streams have closed
- `error` - Error occurred (e.g., command not found)
- `disconnect` - IPC channel disconnected (fork only)
- `message` - IPC message received (fork only)

## ChildProcess Properties

- `pid` - Process ID
- `stdin` - Writable stream to process stdin
- `stdout` - Readable stream from process stdout
- `stderr` - Readable stream from process stderr
- `killed` - Whether kill() was called
- `exitCode` - Exit code (after exit)
- `signalCode` - Signal that terminated process

## ChildProcess Methods

- `kill([signal])` - Send signal to process
- `send(message)` - Send IPC message (fork only)
- `disconnect()` - Close IPC channel (fork only)
- `ref()` - Keep event loop alive
- `unref()` - Allow event loop to exit

## Platform Differences

### POSIX (Linux, macOS)
- Full signal support (SIGTERM, SIGKILL, SIGINT, etc.)
- uid/gid process permissions
- Shell: `/bin/sh -c "command"`

### Windows
- Limited signal support (SIGTERM → TerminateProcess)
- windowsHide option to hide console
- Shell: `cmd.exe /c "command"`

## Error Handling

Always handle errors when spawning processes:

```javascript
const child = spawn('command', ['args']);

child.on('error', (err) => {
  // Handle spawn errors (e.g., command not found)
  console.error('Failed to start:', err);
});

child.on('exit', (code, signal) => {
  if (code !== 0) {
    // Handle non-zero exit codes
    console.error(`Process failed with code ${code}`);
  }
});
```

For synchronous methods, use try-catch:

```javascript
try {
  const output = execSync('command');
} catch (error) {
  console.error('Command failed:', error.message);
  console.error('Exit code:', error.status);
}
```

## Memory and Performance

- Use `spawn()` for long-running processes or large output
- Use `exec()` for short commands with small output (has maxBuffer limit)
- Synchronous methods block the event loop - use sparingly
- Clean up child processes with `kill()` when done
- Use `unref()` to allow parent to exit without waiting

## See Also

- [Node.js child_process documentation](https://nodejs.org/api/child_process.html)
- Test files in `/repo/test/node/child_process/`
- jsrt documentation in `/repo/docs/`
