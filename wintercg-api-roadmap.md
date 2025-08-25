# jsrt WinterCG Minimum Common API 实现规划

## 项目概述

本文档根据 [WinterCG (Web-interoperable Runtimes Community Group)](https://wintercg.org/) 的 Minimum Common API 规范，评估当前 jsrt JavaScript 运行时的实现情况，并制定详细的 API 补充实施计划。

## 当前实现状态

### ✅ 已实现的 API

#### 1. Console API (部分实现)
- **已实现**：`console.log()` 
- **状态**：基础功能完整，支持多参数、对象格式化、颜色输出
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
**当前状态**：❌ 不完整  
**需要添加**：
- `console.error()`
- `console.warn()`
- `console.info()`
- `console.debug()`
- `console.trace()`
- `console.assert()`
- `console.time()` / `console.timeEnd()`
- `console.count()` / `console.countReset()`
- `console.group()` / `console.groupEnd()` / `console.groupCollapsed()`
- `console.clear()`
- `console.table()`
- `console.dir()`

**实现复杂度**：🟡 中等  
**预估工作量**：2-3 天

#### 2. Encoding API
**当前状态**：❌ 未实现  
**需要实现**：
- `TextEncoder` 类
  - `encode(input)` - UTF-8 编码
  - `encodeInto(input, destination)`
- `TextDecoder` 类
  - `decode(input, options)` - UTF-8 解码
  - 构造函数支持不同编码（至少 UTF-8）

**实现复杂度**：🟡 中等  
**预估工作量**：3-4 天  
**技术要求**：需要处理 UTF-8/UTF-16 转换

#### 3. Base64 Utilities
**当前状态**：❌ 未实现  
**需要实现**：
- `atob(encodedData)` - Base64 解码
- `btoa(stringToEncode)` - Base64 编码

**实现复杂度**：🟢 简单  
**预估工作量**：1 天

#### 4. Performance API (基础)
**当前状态**：❌ 未实现  
**需要实现**：
- `performance.now()` - 高精度时间戳
- `performance.timeOrigin` - 时间起点

**实现复杂度**：🟢 简单  
**预估工作量**：1-2 天  
**技术要求**：使用 libuv 的高精度时间 API

### 第二阶段：网络和数据处理 API (优先级：高)

#### 5. URL API
**当前状态**：❌ 未实现  
**需要实现**：
- `URL` 类
  - 构造函数 `new URL(url, base)`
  - 属性：`href`, `origin`, `protocol`, `host`, `hostname`, `port`, `pathname`, `search`, `hash`
  - 方法：`toString()`, `toJSON()`
- `URLSearchParams` 类
  - 构造函数 `new URLSearchParams(init)`
  - 方法：`get()`, `set()`, `append()`, `delete()`, `has()`, `forEach()`, `toString()`

**实现复杂度**：🔴 困难  
**预估工作量**：5-7 天  
**技术要求**：URL 解析和验证逻辑

#### 6. AbortController / AbortSignal
**当前状态**：❌ 未实现  
**需要实现**：
- `AbortController` 类
  - `signal` 属性
  - `abort(reason)` 方法
- `AbortSignal` 类
  - `aborted` 属性
  - `reason` 属性
  - `addEventListener()` / `removeEventListener()`
  - 静态方法：`AbortSignal.abort()`, `AbortSignal.timeout()`

**实现复杂度**：🔴 困难  
**预估工作量**：4-5 天  
**依赖**：需要先实现 Event/EventTarget

### 第三阶段：事件系统 (优先级：中高)

#### 7. Event / EventTarget
**当前状态**：❌ 未实现  
**需要实现**：
- `Event` 类
  - 构造函数 `new Event(type, eventInit)`
  - 属性：`type`, `target`, `currentTarget`, `bubbles`, `cancelable`, `defaultPrevented`
  - 方法：`preventDefault()`, `stopPropagation()`, `stopImmediatePropagation()`
- `EventTarget` 类
  - `addEventListener(type, listener, options)`
  - `removeEventListener(type, listener, options)`
  - `dispatchEvent(event)`

**实现复杂度**：🔴 困难  
**预估工作量**：6-8 天  
**技术要求**：事件传播机制、内存管理

### 第四阶段：数据处理 API (优先级：中)

#### 8. Structured Clone
**当前状态**：❌ 未实现  
**需要实现**：
- `structuredClone(value, options)`
- 支持的数据类型：基本类型、对象、数组、Date、RegExp、Map、Set 等

**实现复杂度**：🔴 困难  
**预估工作量**：4-6 天  
**技术要求**：深度克隆算法、循环引用处理

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
**当前状态**：❌ 未实现  
**需要实现**：
- `ReadableStream` 类
- `WritableStream` 类  
- `TransformStream` 类
- 基础的流控制和背压处理

**实现复杂度**：🔴 非常困难  
**预估工作量**：10-15 天  
**技术要求**：复杂的异步流控制逻辑

#### 11. Fetch API
**当前状态**：❌ 未实现  
**需要实现**：
- `fetch(resource, options)` 函数
- `Request` 类
- `Response` 类
- `Headers` 类
- 支持 HTTP/HTTPS 请求

**实现复杂度**：🔴 非常困难  
**预估工作量**：15-20 天  
**技术要求**：HTTP 客户端实现、与 libuv 集成
**依赖**：Streams API, URL API, AbortController

#### 12. Blob API
**当前状态**：❌ 未实现  
**需要实现**：
- `Blob` 类
  - 构造函数 `new Blob(array, options)`
  - 属性：`size`, `type`
  - 方法：`slice()`, `stream()`, `text()`, `arrayBuffer()`

**实现复杂度**：🔴 困难  
**预估工作量**：4-6 天  
**依赖**：Streams API

#### 13. FormData API
**当前状态**：❌ 未实现  
**需要实现**：
- `FormData` 类
  - 构造函数 `new FormData(form)`
  - 方法：`append()`, `delete()`, `get()`, `getAll()`, `has()`, `set()`, `forEach()`

**实现复杂度**：🟡 中等  
**预估工作量**：3-4 天  
**依赖**：Blob API (对于文件上传)

## 📊 实现优先级矩阵

| API 分类 | 实现复杂度 | WinterCG 重要性 | 生态系统影响 | 推荐优先级 |
|---------|-----------|---------------|-------------|-----------|
| Console API 扩展 | 🟡 中等 | 🔴 高 | 🟡 中等 | 1 |
| Base64 Utilities | 🟢 简单 | 🟡 中等 | 🟢 高 | 2 |
| Performance API | 🟢 简单 | 🟡 中等 | 🟡 中等 | 3 |
| Encoding API | 🟡 中等 | 🔴 高 | 🔴 高 | 4 |
| Event/EventTarget | 🔴 困难 | 🔴 高 | 🔴 高 | 5 |
| AbortController | 🔴 困难 | 🔴 高 | 🔴 高 | 6 |
| URL API | 🔴 困难 | 🔴 高 | 🔴 高 | 7 |
| Crypto API | 🟡 中等 | 🟡 中等 | 🟡 中等 | 8 |
| Structured Clone | 🔴 困难 | 🟡 中等 | 🟡 中等 | 9 |
| Streams API | 🔴 非常困难 | 🔴 高 | 🔴 高 | 10 |
| Fetch API | 🔴 非常困难 | 🔴 高 | 🔴 高 | 11 |
| Blob API | 🔴 困难 | 🟡 中等 | 🟡 中等 | 12 |
| FormData API | 🟡 中等 | 🟡 中等 | 🟡 中等 | 13 |

## 🏗️ 建议实施路线图

### Phase 1: 基础扩展 (预计 1-2 周)
1. **Console API 扩展** - 完善控制台输出功能
2. **Base64 Utilities** - 添加编码解码支持  
3. **Performance API** - 提供时间测量能力

### Phase 2: 编码和事件基础 (预计 2-3 周)
4. **Encoding API** - 文本编码转换支持
5. **Event/EventTarget** - 事件系统基础设施
6. **AbortController** - 取消操作支持

### Phase 3: URL 和 Web 标准 (预计 2-3 周)  
7. **URL API** - URL 解析和操作
8. **Crypto API** - 基础加密功能
9. **Structured Clone** - 对象克隆支持

### Phase 4: 高级 Web API (预计 4-6 周)
10. **Streams API** - 流处理支持
11. **Fetch API** - HTTP 客户端
12. **Blob API** - 二进制数据处理
13. **FormData API** - 表单数据处理

## 📁 文件组织建议

建议在 `src/std/` 目录下创建以下新文件：

```
src/std/
├── console.c (现有，需扩展)
├── timer.c (现有，已完成)  
├── module.c (现有，已完成)
├── encoding.c (新增)
├── encoding.h (新增)
├── base64.c (新增)
├── base64.h (新增)
├── performance.c (新增)
├── performance.h (新增)
├── event.c (新增)
├── event.h (新增)
├── url.c (新增)
├── url.h (新增)
├── crypto.c (新增)
├── crypto.h (新增)
├── streams.c (新增)
├── streams.h (新增)
├── fetch.c (新增)
├── fetch.h (新增)
├── blob.c (新增)
├── blob.h (新增)
└── formdata.c (新增)
└── formdata.h (新增)
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