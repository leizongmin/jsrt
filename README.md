# jsrt
A small JavaScript runtime.

## Setup up development environment

### Requirements

- **gcc**
- **make**
- **cmake**
- **clang-format**

### `launch.json` for VSCode

```json
{
  // Ref: https://go.microsoft.com/fwlink/?linkid=830387
  "version": "0.2.0",
  "configurations": [
    {
        "type": "lldb",
        "request": "launch",
        "name": "Launch JS File",
        "program": "${workspaceFolder}/bin/jsrt_g",
        "args": ["${file}"],
        "cwd": "${workspaceFolder}"
    }
  ]
}
```

### Use Zig C Compiler

Run the following command to [download and install the latest Zig](https://ziglang.org/download/) or [install Zig from a package manager](https://github.com/ziglang/zig/wiki/Install-Zig-from-a-Package-Manager):

```bash
export install_zig_version=0.13.0
cd /tmp
wget https://ziglang.org/download/$install_zig_version/zig-linux-x86_64-$install_zig_version.tar.xz
tar -xvf zig-linux-x86_64-$install_zig_version.tar.xz
mv zig-linux-x86_64-$install_zig_version /opt/zig
sudo ln -s /opt/zig/zig /usr/local/bin/zig
```

Use Zig C compiler to build the project:

```bash
make clean && CC="zig cc" CXX="zig c++" make all
```

Here is a cross-compiling example:

```bash
make clean && CC="zig cc -target x86_64-linux-musl" CXX="zig c++ -target x86_64-linux-musl" make all
```


# Build and Run

Run the following command to build the project:

```bash
make
```

Run the following command to run a JavaScript file:

```bash
./bin/jsrt test.js
```


# License

```
MIT License

Copyright (c) 2024 LEI Zongmin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
