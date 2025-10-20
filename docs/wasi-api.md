# WASI Module API Reference

This document describes the `WASI` class exposed by `node:wasi` / `jsrt:wasi` in jsrt.
It mirrors the Node.js WASI preview1 API while incorporating jsrt-specific behaviour and
constraints.

## Overview

```javascript
import { WASI } from 'node:wasi';

const wasi = new WASI({
  args: ['app', '--flag'],
  env: { MODE: 'test' },
  preopens: {
    '/sandbox': '/home/user/sandbox',
  },
  returnOnExit: true,
});

const wasmModule = await WebAssembly.compile(await readFile('app.wasm'));
const instance = await WebAssembly.instantiate(wasmModule, wasi.getImportObject());
const exitCode = wasi.start(instance);
```

The class orchestrates WebAssembly System Interface (WASI) integration using WAMR under
the hood. It provides predictable sandboxing, configurable stdio, return-on-exit mode,
and compatibility with both CommonJS (`require('node:wasi')`) and ES modules
(`import { WASI } from 'node:wasi'`).

## Constructor

```javascript
new WASI([options])
```

### Options

| Option | Type | Default | Description |
| ------ | ---- | ------- | ----------- |
| `args` | `string[]` | `[]` | Command-line arguments visible to the WASM program. Stored as UTF-8 and surfaced via `args_get`/`args_sizes_get`. |
| `env` | `Record<string,string>` | `{}` | Environment variables converted into `KEY=VALUE` pairs for `environ_get`/`environ_sizes_get`. Keys may not contain `=`. |
| `preopens` | `Record<string,string>` | `{}` | Maps virtual directories (e.g. `/sandbox`) to host paths. Used to seed the WASI file descriptor table with preopened directories. Attempts to escape a preopen are rejected. |
| `stdin`, `stdout`, `stderr` | `number` | `0`, `1`, `2` | Host file descriptors used for standard streams. Present for Node.js parity; jsrt forwards reads/writes via these descriptors. |
| `returnOnExit` | `boolean` | `false` | When `true`, `_start` calling `proc_exit` returns the exit code instead of terminating the host process. When `false`, `proc_exit` terminates jsrt. |
| `version` | `"preview1"` | `"preview1"` | WASI ABI version. Only `preview1` is currently supported; other values throw. |

### Error semantics

The constructor validates every option:

* Non-array `args` or non-string entries throw `TypeError`.
* `env` must be an object with string values; keys containing `=` produce a `TypeError`.
* `preopens` entries must reference existing directories; invalid paths throw `TypeError`.
* Unsupported `version` values throw `TypeError` with Node.js-compatible messaging.

## Methods

### `getImportObject()`

```javascript
const imports = wasi.getImportObject();
```

Returns the import object that must be supplied during instantiation. The object includes
the `wasi_snapshot_preview1` namespace populated with all implemented syscalls. The import
object is cached, so repeated calls return the same identity.

Errors:

* Throws if the WASI instance has been permanently invalidated (e.g. due to a failed attach).

### `start(instance)`

```javascript
const exitCode = wasi.start(instance);
```

Executes the `_start` export on a `WebAssembly.Instance`. Behaviour notes:

* Validates that `instance` exposes an exported `memory` and `_start` function.
* Fails if `start` has already been invoked successfully (`Error: WASI instance already started`).
* Ensures `_initialize` is **not** exported; use `initialize()` instead.
* When `returnOnExit` is `true`, returns the exit code emitted by `proc_exit`. Otherwise the
  process terminates with that code.

Errors propagate as `Error` instances with the same wording as Node.js when applicable
(`The "instance" option must be of type object`, `The instance.exports must contain a
"memory" export`, etc.).

### `initialize(instance)`

```javascript
const result = wasi.initialize(instance);
```

Invokes the `_initialize` export if present. Intended for components that separate module
initialisation from main entry. Notes:

* Validates that `_start` is **not** exported (mutual exclusion with `start`).
* Prevents multiple invocations by tracking an `initialized` flag.
* Returns `undefined` when `_initialize` is missing (no-op). When `returnOnExit` is true
  and `proc_exit` is raised during `_initialize`, the exit code is returned instead.

### `start()` vs `initialize()` order

Only one of the lifecycle methods may succeed; calling the other afterwards throws an error
(`Error: WASI instance already started` / `Error: WASI instance already initialized`).

## Examples

### Hello world

```javascript
import { readFile } from 'node:fs/promises';
import { WASI } from 'node:wasi';

const wasi = new WASI({ returnOnExit: true });
const wasm = await WebAssembly.compile(await readFile('hello.wasm'));
const instance = await WebAssembly.instantiate(wasm, wasi.getImportObject());
const exitCode = wasi.start(instance);
console.log(`Program exited with code ${exitCode}`);
```

### Sandboxed file I/O

```javascript
const wasi = new WASI({
  preopens: {
    '/sandbox': '/tmp/jsrt-wasi',
  },
  returnOnExit: true,
});

const wasm = await WebAssembly.compile(await readFile('fs-demo.wasm'));
const instance = await WebAssembly.instantiate(wasm, wasi.getImportObject());
wasi.start(instance);
```

Inside the WASM module, only `/sandbox` is accessible. Attempts to traverse outside the
preopened directory (`../` or absolute paths) fail with `ENOTCAPABLE`.

## WASI preview1 coverage

The implementation targets the full preview1 surface required for Node.js compatibility.

| Category | Status | Notes |
| -------- | ------ | ----- |
| Arguments & environment | ✅ | `args_get`, `args_sizes_get`, `environ_get`, `environ_sizes_get` |
| Proc info | ✅ | `proc_exit` (honours `returnOnExit`), `random_get`, `clock_time_get`, `clock_res_get` (process/thread clocks return `ENOSYS`) |
| File descriptors | ✅ | `fd_read`, `fd_write`, `fd_close`, `fd_seek`, `fd_tell`, `fd_fdstat_get`, `fd_fdstat_set_flags`, `fd_prestat_get`, `fd_prestat_dir_name`, etc. |
| Paths & directories | ✅ | `path_open`, `path_readlink`, `path_filestat_get`, `path_unlink_file`, `path_create_directory`, `path_remove_directory`, `path_rename`. Traversal outside preopens is blocked. |
| Scheduling & sockets | ⚪️ | `poll_oneoff`, socket-family functions return `ENOSYS` (explicit stubs). |

Unsupported or unimplemented functions return `ENOSYS` consistently so callers can detect
missing functionality without undefined behaviour.

## Security considerations

* **Sandboxing** — Only directories listed in `preopens` are accessible. Path traversal is
  prevented through canonicalisation; attempts result in `ENOTCAPABLE`.
* **File descriptor rights** — Rights masks are enforced per descriptor; write-only handles
  cannot be read and vice versa.
* **Process termination** — With `returnOnExit: false` the host process terminates when
  `proc_exit` is invoked. Use `returnOnExit: true` for embedded runtimes or tests.
* **Randomness** — `random_get` reads from `/dev/urandom`. Ensure the host provides a secure
  source before executing untrusted modules.

## Compatibility notes

* Module aliasing: `node:wasi` and `jsrt:wasi` refer to the same implementation.
* Both CommonJS and ESM entry points are supported. The default export is the `WASI` class,
  with named exports matching Node.js.
* Behavioural differences vs Node.js are documented in the migration guide (Task 7.4).

