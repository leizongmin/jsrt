# 内置模块控制台输出测试

## 概述
这个测试文件 (`test_builtin_modules_console.js`) 专门用于测试使用 `console.log(require('内置模块'))` 这种模式的用例。

## 测试的问题
之前，运行如下命令会导致栈溢出（stack overflow）：
```bash
echo 'console.log(require("node:path"))' | ./bin/jsrt_g
```

## 根本原因
问题是由于在 Node.js 路径模块初始化中创建了循环引用：
- `path.posix = path` (在 Unix 系统上)
- `path.win32 = path` (在 Windows 系统上)

当 `console.log()` 尝试序列化这些对象时，会陷入无限循环。

## 修复方案
1. **模块缓存**: 在 `NodeModuleEntry` 结构中添加了 `cached_module` 字段，以防止重复初始化
2. **消除循环引用**: 替换了 `JS_DupValue(ctx, path_obj)` 为 `JS_NewObject(ctx)`，创建独立的对象而不是自引用

## 测试覆盖
测试文件涵盖了以下场景：

### 基本测试
- ✅ `console.log(require("node:path"))`
- ✅ `console.log(require("node:os"))`
- ✅ `console.log(require("node:util"))`

### 模块缓存测试
- ✅ 验证模块被正确缓存并返回相同的实例
- ✅ 避免无限递归

### 循环引用修复验证
- ✅ 确保 `path.posix` 和 `path.win32` 属性存在但不会引起无限循环
- ✅ `JSON.stringify()` 能够正常工作而不会无限递归

### 边缘情况测试
- ✅ 嵌套的 `console.log` 调用
- ✅ 在表达式中直接调用 require 的方法
- ✅ 同一行中加载多个模块

## 运行测试
```bash
# 运行特定的测试
./bin/jsrt_g test/test_builtin_modules_console.js

# 运行所有测试
make test

# 测试原始问题命令
echo 'console.log(require("node:path"))' | ./bin/jsrt_g
```

## 预期输出
修复后，上述命令应该输出路径模块对象而不是崩溃：
```
Object { join: [Function: join], resolve: [Function: resolve], normalize: [Function: normalize], isAbsolute: [Function: isAbsolute], dirname: [Function: dirname], basename: [Function: basename], extname: [Function: extname], relative: [Function: relative], sep: /, delimiter: :, posix: Object [posix] { join: [Function: join], resolve: [Function: resolve] }, win32: Object [win32] {  } }
```

## 相关文件
- 测试文件: `test/test_builtin_modules_console.js`
- 修复的源文件: 
  - `src/node/node_modules.h` - 添加了模块缓存结构
  - `src/node/node_modules.c` - 实现了模块缓存逻辑
  - `src/node/node_path.c` - 修复了循环引用问题