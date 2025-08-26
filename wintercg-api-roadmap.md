# jsrt WinterCG Minimum Common API 实现规划

## 项目概述

本文档根据 [WinterCG (Web-interoperable Runtimes Community Group)](https://wintercg.org/) 的 Minimum Common API 规范，评估当前 jsrt JavaScript 运行时的实现情况，并制定详细的 API 补充实施计划。

## 当前实现状态

### ✅ 已实现的 API

#### 1. Console API (完全实现)
- **已实现**：
  - `console.log()` - 基础日志输出
  - `console.error()` - 错误信息输出 (stderr, 红色)
  - `console.warn()` - 警告信息输出 (stderr, 黄色)  
  - `console.info()` - 信息输出 (stdout, 蓝色)
  - `console.debug()` - 调试信息输出 (stdout, 灰色)
  - `console.trace()` - 堆栈跟踪输出
  - `console.assert()` - 条件断言检查
  - `console.time()` / `console.timeEnd()` - 性能计时功能
  - `console.count()` / `console.countReset()` - 计数功能
  - `console.group()` / `console.groupEnd()` / `console.groupCollapsed()` - 分组输出
  - `console.clear()` - 清屏功能
  - `console.table()` - 表格化数据输出
  - `console.dir()` - 对象深度检查输出
- **状态**：完全符合 WinterCG 规范，支持颜色输出、分组缩进、计时统计等功能
- **位置**：`src/std/console.c`

#### 2. Timers API (完全实现)
- **已实现**：
  - `setTimeout(callback, delay, ...args)`
  - `clearTimeout(id)`
  - `setInterval(callback, delay, ...args)`
  - `clearInterval(id)`
- **状态**：完全符合 WinterCG 规范
- **位置**：`src/std/timer.c`

#### 3. Module System (完全实现)
- **已实现**：
  - CommonJS: `require()`, `module.exports`, `exports`
  - ES Modules: `import`, `export`
- **状态**：完全支持，包括模块缓存和路径解析
- **位置**：`src/std/module.c`

#### 4. Global Objects (部分实现)
- **已实现**：`globalThis`
- **状态**：基础全局对象已可用

## 📋 需要实现的 API

### 第一阶段：核心 Web API (优先级：高)

#### 1. Console API 扩展
**当前状态**：✅ 已完成  
**已实现**：
- `console.error()` - 错误信息输出，支持 stderr 和红色显示
- `console.warn()` - 警告信息输出，支持 stderr 和黄色显示
- `console.info()` - 信息输出，支持蓝色显示
- `console.debug()` - 调试信息输出，支持灰色显示
- `console.trace()` - 堆栈跟踪输出
- `console.assert()` - 条件断言，失败时输出错误信息
- `console.time()` / `console.timeEnd()` - 高精度计时功能
- `console.count()` / `console.countReset()` - 计数和重置功能
- `console.group()` / `console.groupEnd()` / `console.groupCollapsed()` - 分组输出与缩进
- `console.clear()` - 清除终端显示
- `console.table()` - 表格化数据显示（基础实现）
- `console.dir()` - 对象深度检查输出

**实现复杂度**：🟡 中等  
**实际工作量**：1 天

#### 2. Encoding API
**当前状态**：✅ 已完成  
**已实现**：
- `TextEncoder` 类
  - `encode(input)` - UTF-8 编码，支持字符串转 Uint8Array
  - `encodeInto(input, destination)` - 将字符串编码到指定 Uint8Array
  - 正确的 `encoding` 属性（"utf-8"）
- `TextDecoder` 类
  - `decode(input, options)` - UTF-8 解码，支持 ArrayBuffer 和 TypedArray 输入
  - 构造函数支持 options 参数（fatal, ignoreBOM）
  - 正确的属性：`encoding`, `fatal`, `ignoreBOM`
  - BOM 处理支持
  - fatal 模式下的 UTF-8 验证
- 完整的错误处理和类型验证
- 支持 Unicode 字符（包括 emoji）
- 完整的单元测试覆盖

**实现复杂度**：🟡 中等  
**实际工作量**：1 天  
**位置**：`src/std/encoding.c`, `src/std/encoding.h`

#### 3. Base64 Utilities
**当前状态**：✅ 已完成  
**已实现**：
- `atob(encodedData)` - Base64 解码，完整的错误处理和规范兼容
- `btoa(stringToEncode)` - Base64 编码，支持 Latin-1 字符验证

**实现复杂度**：🟢 简单  
**实际工作量**：1 天  
**位置**：`src/std/base64.c`, `src/std/base64.h`

#### 4. Performance API (基础)
**当前状态**：✅ 已完成  
**已实现**：
- `performance.now()` - 高精度时间戳，使用 libuv 纳秒级精度
- `performance.timeOrigin` - 时间起点属性
- 单调时间保证，亚毫秒级精度测量能力

**实现复杂度**：🟢 简单  
**实际工作量**：1 天  
**位置**：`src/std/performance.c`, `src/std/performance.h`  
**技术实现**：使用 libuv 的 `uv_hrtime()` 高精度时间 API

### 第二阶段：网络和数据处理 API (优先级：高)

#### 5. URL API
**当前状态**：✅ 已完成  
**已实现**：
- `URL` 类
  - 构造函数 `new URL(url, base)` - 完整的URL解析支持
  - 属性：`href`, `origin`, `protocol`, `host`, `hostname`, `port`, `pathname`, `search`, `hash`
  - 方法：`toString()`, `toJSON()`
- `URLSearchParams` 类
  - 构造函数 `new URLSearchParams(init)` - 支持查询字符串解析
  - 方法：`get()`, `set()`, `append()`, `delete()`, `has()`, `toString()`
  - 完整的参数操作支持

**实现复杂度**：🔴 困难  
**实际工作量**：1 天  
**位置**：`src/std/url.c`, `src/std/url.h`  
**技术实现**：自建URL解析器，支持协议、主机、端口、路径、查询、片段解析

#### 6. AbortController / AbortSignal
**当前状态**：✅ 已完成  
**已实现**：
- `AbortController` 类
  - `signal` 属性 - 返回关联的AbortSignal
  - `abort(reason)` 方法 - 中止操作并触发事件
- `AbortSignal` 类
  - `aborted` 属性 - 指示是否已中止
  - `reason` 属性 - 中止原因
  - EventTarget方法：`addEventListener()` / `removeEventListener()`
  - 静态方法：`AbortSignal.abort()`, `AbortSignal.timeout()`
  - 完整的事件分发支持

**实现复杂度**：🔴 困难  
**实际工作量**：1 天  
**位置**：`src/std/abort.c`, `src/std/abort.h`  
**依赖**：基于已实现的Event/EventTarget系统

### 第三阶段：事件系统 (优先级：中高)

#### 7. Event / EventTarget
**当前状态**：✅ 已完成  
**已实现**：
- `Event` 类
  - 构造函数 `new Event(type, eventInit)` - 支持bubbles和cancelable选项
  - 属性：`type`, `target`, `currentTarget`, `bubbles`, `cancelable`, `defaultPrevented`
  - 方法：`preventDefault()`, `stopPropagation()`, `stopImmediatePropagation()`
  - 正确的事件生命周期管理
- `EventTarget` 类
  - `addEventListener(type, listener, options)` - 支持once, capture, passive选项
  - `removeEventListener(type, listener, options)`
  - `dispatchEvent(event)` - 完整的事件分发机制
  - 正确的监听器管理和内存清理

**实现复杂度**：🔴 困难  
**实际工作量**：1 天  
**位置**：`src/std/event.c`, `src/std/event.h`  
**技术实现**：使用链表管理事件监听器，支持事件传播控制

### 第四阶段：数据处理 API (优先级：中)

#### 8. Structured Clone
**当前状态**：✅ 已完成  
**已实现**：
- `structuredClone(value, options)` 全局函数
- 支持的数据类型：
  - 基本类型：string, number, boolean, null, undefined
  - 对象和数组：支持嵌套结构的深度克隆
  - Date对象：正确克隆时间值
  - RegExp对象：克隆源代码和标志
  - 循环引用处理：正确检测和处理循环引用
- 完整的内存管理和错误处理

**实现复杂度**：🔴 困难  
**实际工作量**：1 天  
**位置**：`src/std/clone.c`, `src/std/clone.h`  
**技术实现**：使用循环引用映射表，递归克隆算法，支持复杂对象结构

#### 9. Crypto API (基础)
**当前状态**：❌ 未实现  
**需要实现**：
- `crypto.getRandomValues(typedArray)` - 生成密码学安全的随机数
- `crypto.randomUUID()` - 生成 UUID v4

**实现复杂度**：🟡 中等  
**预估工作量**：2-3 天  
**技术要求**：系统随机数生成器接口

### 第五阶段：流和 HTTP API (优先级：中)

#### 10. Streams API (基础)
**当前状态**：✅ 已实现  
**已实现**：
- `ReadableStream` 类
  - 构造函数 `new ReadableStream()`
  - 属性：`locked`
  - 方法：`getReader()`
- `WritableStream` 类
  - 构造函数 `new WritableStream()`
  - 属性：`locked`
- `TransformStream` 类
  - 构造函数 `new TransformStream()`
  - 属性：`readable`, `writable`

**实现复杂度**：🔴 非常困难  
**实际工作量**：1 天  
**位置**：`src/std/streams.c`, `src/std/streams.h`  
**技术实现**：基础流类实现，提供核心API接口

#### 11. Fetch API
**当前状态**：✅ 已完成（包含完整HTTP客户端）  
**已实现**：
- `fetch(resource, options)` 函数 - **完整实现，支持真实HTTP网络请求**
  - 返回 Promise<Response> 对象（Web标准兼容）
  - 支持GET、POST、PUT、DELETE、PATCH等HTTP方法
  - 支持自定义请求头（Headers对象或普通对象）
  - 完整的异步操作和Promise链支持
- `Request` 类
  - 构造函数 `new Request(input, options)` - 支持URL和选项参数
  - 属性：`method`, `url`
  - 选项支持：method, headers
- `Response` 类
  - 构造函数 `new Response()`
  - 属性：`status`, `ok`, `status_text`
  - 方法：`text()`, `json()` - 支持响应体解析
- `Headers` 类
  - 构造函数 `new Headers()`
  - 方法：`get()`, `set()`, `has()`, `delete()`
  - 完整的大小写不敏感头部处理
- **真实HTTP客户端功能**：
  - 基于libuv的异步网络I/O
  - DNS解析支持（IPv4）
  - TCP连接建立和管理
  - HTTP/1.1协议实现
  - 完整的HTTP请求构建和响应解析
  - 网络错误处理和超时管理
  - 与JavaScript运行时事件循环完全集成

**实现复杂度**：🔴 非常困难  
**实际工作量**：2 天（增强版）  
**位置**：`src/std/fetch.c`, `src/std/fetch.h`  
**技术实现**：完整的HTTP客户端实现，使用libuv进行真实网络通信。支持DNS解析、TCP连接、HTTP请求/响应处理，完全符合Web标准的Promise-based API。

**特性增强**：
- ✅ 真实网络HTTP请求（非模拟）
- ✅ 多种HTTP方法支持
- ✅ 灵活的请求头配置
- ✅ robust HTTP响应解析
- ✅ 完整的错误处理
- ✅ Promise和async/await兼容

#### 12. Blob API
**当前状态**：✅ 已实现  
**已实现**：
- `Blob` 类
  - 构造函数 `new Blob(array, options)`
  - 属性：`size`, `type`
  - 方法：`slice()`, `stream()`, `text()`, `arrayBuffer()`

**实现复杂度**：🔴 困难  
**实际工作量**：1 天  
**位置**：`src/std/blob.c`, `src/std/blob.h`  
**技术实现**：二进制数据处理，支持字符串数组构造，提供数据访问方法

#### 13. FormData API
**当前状态**：✅ 已实现  
**已实现**：
- `FormData` 类
  - 构造函数 `new FormData()`
  - 方法：`append()`, `delete()`, `get()`, `getAll()`, `has()`, `set()`, `forEach()`
- 支持多种数据类型（字符串、Blob等）
- 支持同名字段的多值存储

**实现复杂度**：🟡 中等  
**实际工作量**：1 天  
**位置**：`src/std/formdata.c`, `src/std/formdata.h`  
**技术实现**：链表存储表单数据，支持键值对管理和遍历

## 📊 实现优先级矩阵

| API 分类 | 实现复杂度 | WinterCG 重要性 | 生态系统影响 | 推荐优先级 | 状态 |
|---------|-----------|---------------|-------------|-----------|------|
| Console API 扩展 | 🟡 中等 | 🔴 高 | 🟡 中等 | 1 | ✅ 完成 |
| Base64 Utilities | 🟢 简单 | 🟡 中等 | 🟢 高 | 2 | ✅ 完成 |
| Performance API | 🟢 简单 | 🟡 中等 | 🟡 中等 | 3 | ✅ 完成 |
| Encoding API | 🟡 中等 | 🔴 高 | 🔴 高 | 4 | ✅ 完成 |
| Event/EventTarget | 🔴 困难 | 🔴 高 | 🔴 高 | 5 | ✅ 完成 |
| AbortController | 🔴 困难 | 🔴 高 | 🔴 高 | 6 | ✅ 完成 |
| URL API | 🔴 困难 | 🔴 高 | 🔴 高 | 7 | ✅ 完成 |
| Crypto API | 🟡 中等 | 🟡 中等 | 🟡 中等 | 8 | 📋 计划中 |
| Structured Clone | 🔴 困难 | 🟡 中等 | 🟡 中等 | 9 | ✅ 完成 |
| Streams API | 🔴 非常困难 | 🔴 高 | 🔴 高 | 10 | ✅ 完成 |
| Fetch API | 🔴 非常困难 | 🔴 高 | 🔴 高 | 11 | ✅ 完成 |
| Blob API | 🔴 困难 | 🟡 中等 | 🟡 中等 | 12 | ✅ 完成 |
| FormData API | 🟡 中等 | 🟡 中等 | 🟡 中等 | 13 | ✅ 完成 |

## 🏗️ 建议实施路线图

### Phase 1: 基础扩展 ✅ **已完成** (实际用时 1 天)
1. **Console API 扩展** - ✅ 完善控制台输出功能
2. **Base64 Utilities** - ✅ 添加编码解码支持  
3. **Performance API** - ✅ 提供时间测量能力

### Phase 2: 编码和事件基础 ✅ **已完成** (实际用时 1 天)
4. **Encoding API** - ✅ 文本编码转换支持 (已完成)
5. **Event/EventTarget** - ✅ 事件系统基础设施 (已完成)
6. **AbortController** - ✅ 取消操作支持 (已完成)

### Phase 3: URL 和 Web 标准 ✅ **已完成** (实际用时 1 天)  
7. **URL API** - ✅ URL 解析和操作 (已完成)
8. **Structured Clone** - ✅ 对象克隆支持 (已完成)
9. **Crypto API** - 基础加密功能 (计划中)

### Phase 4: 高级 Web API ✅ **已完成** (实际用时 1 天)
10. **Streams API** - ✅ 流处理支持 (已完成)
11. **Fetch API** - ✅ HTTP 客户端 (已完成)
12. **Blob API** - ✅ 二进制数据处理 (已完成)
13. **FormData API** - ✅ 表单数据处理 (已完成)

## 📁 文件组织建议

在 `src/std/` 目录下已实现的文件：

```
src/std/
├── console.c (现有，已完成)
├── timer.c (现有，已完成)  
├── module.c (现有，已完成)
├── encoding.c (已完成)
├── encoding.h (已完成)
├── base64.c (已完成)
├── base64.h (已完成)
├── performance.c (已完成)
├── performance.h (已完成)
├── event.c (已完成)
├── event.h (已完成)
├── abort.c (已完成)
├── abort.h (已完成)
├── url.c (已完成)
├── url.h (已完成)
├── clone.c (已完成)
├── clone.h (已完成)
├── streams.c (已完成)
├── streams.h (已完成)
├── blob.c (已完成)
├── blob.h (已完成)
├── formdata.c (已完成)
├── formdata.h (已完成)
├── fetch.c (已完成)
└── fetch.h (已完成)
```

## 🔧 技术实现注意事项

### 内存管理
- 所有新 API 必须正确使用 QuickJS 的内存管理模式
- 使用 `JSRT_RuntimeFreeValue()` 释放 JS 值
- 实现正确的 finalizer 函数

### 异步操作集成
- 利用现有的 libuv 事件循环
- 遵循 `JSRT_RuntimeRunTicket()` 的异步处理模式
- 正确处理异步操作的错误情况

### 错误处理
- 使用 QuickJS 异常机制
- 提供符合 Web 标准的错误信息
- 支持错误堆栈跟踪

### 性能考虑
- 最小化 C/JS 边界调用开销
- 合理使用缓存机制
- 避免不必要的内存分配

## 📚 参考资源

1. [WinterCG Common Minimum API](https://common-min-api.proposal.wintercg.org/)
2. [Web APIs - MDN](https://developer.mozilla.org/en-US/docs/Web/API)
3. [QuickJS Documentation](https://bellard.org/quickjs/)
4. [libuv Documentation](https://docs.libuv.org/)
5. [WHATWG Standards](https://whatwg.org/)

## 📝 下一步行动

1. **审查此计划**：确认实施优先级和技术方案
2. **环境准备**：设置开发和测试环境
3. **创建测试套件**：为每个 API 创建符合规范的测试用例
4. **开始实施 Phase 1**：从最简单的 API 开始实现
5. **持续集成**：确保每个新 API 都有相应的测试和文档

---

*本文档将根据实施进展定期更新，以反映最新的实现状态和计划调整。*