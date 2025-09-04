# JSRT HTTP协议解析器集成设计文档

## 概述

本设计文档描述了在JSRT项目中集成llhttp作为HTTP协议解析器，用于改进现有fetch API实现的方案。llhttp是Node.js使用的高性能HTTP解析器，具有零拷贝、严格标准兼容和优秀性能特征。

## 背景

### 当前状态
- JSRT目前的fetch实现可能使用简单的字符串解析或基础HTTP处理
- 缺乏完整的HTTP/1.1标准支持
- 性能和内存效率有待提升

### 目标
- 集成llhttp作为标准HTTP协议解析器
- 提供完整的HTTP/1.1支持
- 提升HTTP处理性能和标准兼容性
- 为未来HTTP/2支持奠定基础

## 架构设计

### 组件结构

```
jsrt/
├── deps/
│   ├── quickjs/           # 现有QuickJS依赖
│   ├── libuv/            # 现有libuv依赖
│   └── llhttp/           # 新增llhttp依赖
├── src/
│   ├── http/             # HTTP处理模块（新增）
│   │   ├── parser.c      # llhttp封装层
│   │   ├── parser.h      # 解析器API定义
│   │   ├── request.c     # HTTP请求处理
│   │   ├── response.c    # HTTP响应处理
│   │   └── fetch.c       # 重构的fetch实现
│   └── std/
│       └── fetch.c       # 现有fetch模块（将被重构）
```

### 核心组件

#### 1. HTTP解析器封装层 (`src/http/parser.c`)

提供llhttp的C API封装，简化集成：

```c
typedef struct {
    llhttp_t parser;
    llhttp_settings_t settings;
    jsrt_http_message_t* current_message;
    JSContext* ctx;
    void* user_data;
} jsrt_http_parser_t;

// 解析器生命周期
jsrt_http_parser_t* jsrt_http_parser_create(JSContext* ctx, jsrt_http_type_t type);
void jsrt_http_parser_destroy(jsrt_http_parser_t* parser);
jsrt_http_result_t jsrt_http_parser_execute(jsrt_http_parser_t* parser, const char* data, size_t len);
```

#### 2. HTTP消息结构 (`src/http/parser.h`)

统一的HTTP请求/响应表示：

```c
typedef struct {
    // 基本信息
    int major_version;
    int minor_version;
    int status_code;          // 响应用
    char* status_message;     // 响应用
    char* method;             // 请求用
    char* url;                // 请求用
    
    // 头部信息
    jsrt_http_headers_t headers;
    
    // 体数据
    jsrt_buffer_t body;
    
    // 解析状态
    int complete;
    int error;
} jsrt_http_message_t;
```

#### 3. Fetch API重构 (`src/http/fetch.c`)

基于llhttp的新fetch实现：

```c
// 异步fetch实现
typedef struct {
    JSValue promise;
    JSValue resolve_func;
    JSValue reject_func;
    uv_tcp_t socket;
    jsrt_http_parser_t* parser;
    jsrt_http_message_t* response;
    char* request_data;
    size_t request_len;
} jsrt_fetch_context_t;

JSValue jsrt_fetch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
```

## 实现计划

### 阶段1：依赖集成
1. **添加llhttp子模块**
   ```bash
   git submodule add https://github.com/nodejs/llhttp.git deps/llhttp
   ```

2. **CMakeLists.txt集成**
   - 配置llhttp编译选项
   - 链接llhttp库到jsrt

3. **构建系统验证**
   - 确保跨平台兼容性
   - 验证依赖关系正确

### 阶段2：解析器封装
1. **实现parser.c核心功能**
   - llhttp回调函数封装
   - 内存管理
   - 错误处理

2. **HTTP消息结构定义**
   - 头部字典实现
   - 体数据缓冲区管理
   - 内存池优化

3. **单元测试**
   - 解析器基础功能测试
   - 边界条件测试

### 阶段3：Fetch重构
1. **异步网络层**
   - libuv TCP集成
   - 连接池（可选）
   - 超时处理

2. **HTTP客户端实现**
   - 请求构建
   - 响应解析
   - 状态管理

3. **JavaScript API绑定**
   - Promise支持
   - 错误处理
   - 类型转换

### 阶段4：测试和优化
1. **集成测试**
   - 各种HTTP场景测试
   - 性能基准测试
   - 内存泄漏检查

2. **兼容性验证**
   - 现有代码迁移
   - API兼容性确保
   - 跨平台测试

## 技术细节

### llhttp回调处理

```c
// 回调函数设置
int on_message_begin(llhttp_t* parser) {
    jsrt_http_parser_t* jsrt_parser = (jsrt_http_parser_t*)parser->data;
    jsrt_parser->current_message = jsrt_http_message_create();
    return 0;
}

int on_url(llhttp_t* parser, const char* at, size_t length) {
    jsrt_http_parser_t* jsrt_parser = (jsrt_http_parser_t*)parser->data;
    jsrt_parser->current_message->url = jsrt_strndup(at, length);
    return 0;
}

int on_header_field(llhttp_t* parser, const char* at, size_t length);
int on_header_value(llhttp_t* parser, const char* at, size_t length);
int on_body(llhttp_t* parser, const char* at, size_t length);
int on_message_complete(llhttp_t* parser);
```

### 内存管理策略

1. **解析器级别**：使用QuickJS内存管理函数
2. **消息级别**：实现专用内存池
3. **字符串处理**：零拷贝策略，引用原始数据
4. **清理机制**：RAII风格的资源管理

### 错误处理

```c
typedef enum {
    JSRT_HTTP_OK = 0,
    JSRT_HTTP_ERROR_INVALID_DATA,
    JSRT_HTTP_ERROR_MEMORY,
    JSRT_HTTP_ERROR_NETWORK,
    JSRT_HTTP_ERROR_TIMEOUT,
    JSRT_HTTP_ERROR_PROTOCOL
} jsrt_http_error_t;
```

## 性能考虑

### 优化策略
1. **零拷贝解析**：尽可能避免数据复制
2. **内存池**：减少小对象分配开销
3. **缓冲区重用**：网络缓冲区复用
4. **延迟解析**：按需解析头部值

### 性能指标
- 解析吞吐量：目标>100MB/s
- 内存开销：每个连接<4KB基础开销
- 延迟：解析延迟<1ms

## 兼容性

### 支持的特性
- HTTP/1.0 和 HTTP/1.1
- 标准和自定义头部
- 分块传输编码
- 连接keep-alive
- 重定向处理（JavaScript层）

### 平台支持
- Linux：完全支持
- macOS：完全支持  
- Windows：通过MinGW/MSVC支持

## 测试策略

### 单元测试
```javascript
// test/test_http_parser.js
const assert = require("jsrt:assert");

// 基础解析测试
const response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello";
const parsed = jsrt.http.parse(response);
assert.strictEqual(parsed.status, 200);
assert.strictEqual(parsed.body, "Hello");
```

### 集成测试
```javascript
// test/test_fetch_llhttp.js
const assert = require("jsrt:assert");

// Fetch集成测试
fetch("http://httpbin.org/get")
  .then(response => response.json())
  .then(data => {
    assert.ok(data.url);
    console.log("Fetch with llhttp works!");
  });
```

## 迁移路径

### 向后兼容性
1. 保持现有fetch API不变
2. 内部实现平滑替换
3. 提供降级机制（可选）

### 部署策略
1. 功能开关控制新解析器
2. A/B测试验证稳定性
3. 逐步迁移现有代码

## 风险和缓解

### 技术风险
1. **依赖复杂性**：llhttp可能增加构建复杂度
   - 缓解：详细的构建文档和CI验证

2. **性能退化**：新实现可能比简单解析慢
   - 缓解：性能基准测试和优化

3. **内存使用**：llhttp可能增加内存开销
   - 缓解：内存使用监控和优化

### 开发风险
1. **开发周期**：集成工作可能比预期复杂
   - 缓解：分阶段实现，逐步验证

2. **维护负担**：新增代码增加维护成本
   - 缓解：良好的代码结构和文档

## 后续规划

### 短期目标
1. 完成基础集成（4周）
2. 通过所有测试（2周）
3. 性能优化（2周）

### 长期规划
1. HTTP/2支持研究
2. WebSocket集成考虑
3. 缓存层实现

---

**文档版本**: v1.0  
**创建日期**: 2025-01-09  
**最后更新**: 2025-01-09  
**负责人**: Claude Code Assistant