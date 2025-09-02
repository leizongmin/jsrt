# Windows 构建指南 (MSYS2)

## 快速开始

### 1. 安装 MSYS2
- 下载地址: https://www.msys2.org/
- 运行安装程序，使用默认路径 `C:\msys64`

### 2. 安装构建工具
在 Windows 中运行：
```cmd
setup-msys2.bat
```
这将自动安装所需的工具包。

### 3. 构建项目
在 MSYS2 MINGW64 终端中：
```bash
cd /c/Users/你的用户名/work/jsrt
./build-msys2.sh
```

## 详细步骤

### 步骤 1: 安装 MSYS2

1. 访问 https://www.msys2.org/
2. 下载安装程序 (msys2-x86_64-*.exe)
3. 运行安装程序，安装到默认位置 `C:\msys64`
4. 安装完成后，MSYS2 会自动打开一个终端窗口

### 步骤 2: 安装必要的包

在 MSYS2 MINGW64 终端中运行：
```bash
# 更新包数据库
pacman -Syu

# 安装构建工具
pacman -S --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    make \
    git
```

### 步骤 3: 克隆和构建项目

```bash
# 克隆仓库
git clone https://github.com/leizongmin/jsrt.git
cd jsrt

# 初始化子模块
git submodule update --init --recursive

# 运行构建脚本
./build-msys2.sh

# 或手动构建
mkdir -p target/release
cd target/release
cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release ../..
make -j$(nproc)
```

### 步骤 4: 运行测试

```bash
# 在构建目录中
ctest --verbose

# 或运行示例
./jsrt.exe ../../examples/hello.js
```

## 文件说明

- `setup-msys2.bat` - Windows 批处理脚本，自动配置 MSYS2 环境
- `build-msys2.sh` - MSYS2 构建脚本，自动安装依赖并构建项目
- `test.bat` - Windows 测试脚本

## 构建类型

支持两种构建类型：
```bash
./build-msys2.sh Release  # 优化构建（默认）
./build-msys2.sh Debug    # 调试构建
```

## 输出位置

构建完成后，可执行文件位于：
- Release 构建: `target/msys2-release/jsrt.exe`
- Debug 构建: `target/msys2-debug/jsrt.exe`

## 常见问题

### 1. 找不到 MSYS2
确保 MSYS2 安装在默认位置 `C:\msys64`。如果安装在其他位置，请修改脚本中的路径。

### 2. 包安装失败
如果包安装失败，尝试：
```bash
# 更新包数据库
pacman -Syuu

# 清理缓存
pacman -Scc

# 重新安装
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake make git
```

### 3. CMake 配置失败
确保在 MINGW64 终端中运行，不是 MSYS 或 MINGW32：
- 开始菜单 → MSYS2 → MSYS2 MINGW64

### 4. 构建失败
检查子模块是否正确初始化：
```bash
git submodule update --init --recursive
```

### 5. 在 Windows 终端中使用
如果想在 Windows Terminal 中使用 MSYS2：
1. 打开 Windows Terminal 设置
2. 添加新配置文件
3. 命令行设置为: `C:\msys64\usr\bin\bash.exe -l -c "cd ~ && exec bash"`
4. 启动目录设置为: `C:\msys64\home\%USERNAME%`

## 开发提示

### VSCode 集成
在 VSCode 中使用 MSYS2 终端：
1. 安装 "C/C++" 扩展
2. 在设置中配置终端：
```json
"terminal.integrated.profiles.windows": {
  "MSYS2": {
    "path": "C:\\msys64\\usr\\bin\\bash.exe",
    "args": ["--login", "-i"],
    "env": {
      "MSYSTEM": "MINGW64",
      "CHERE_INVOKING": "1"
    }
  }
}
```

### 调试
使用 GDB 调试：
```bash
# 构建调试版本
./build-msys2.sh Debug

# 使用 GDB
gdb target/msys2-debug/jsrt.exe
```

## 依赖版本要求

- GCC: 7.0+
- CMake: 3.16+
- Make: 3.81+
- Git: 2.0+