# Claude Code Agents for jsrt

本文档介绍为 jsrt 项目配置的专业化 Claude Code agents，它们能帮助你更高效地开发、测试和维护这个 JavaScript 运行时。

## 什么是 Claude Code Agents？

Claude Code Agents 是专门为特定任务配置的 AI 助手，每个 agent 都有明确的职责范围、专业知识和工具集。它们能够自动执行复杂任务，提供专业建议，并确保代码质量。

## 可用的 Agents

### 0. 🎯 jsrt-parallel-coordinator 🥇
**职责**：协调多个 agents 并行工作

**专长**：
- 分析复杂任务并分解为并行子任务
- 调度合适的 agents 同时工作
- 管理任务依赖和资源冲突
- 聚合并行执行结果

**使用场景**：
```bash
# 当你需要：
- 加速复杂的开发任务
- 同时执行多个独立检查
- 运行预定义的并行工作流
```

---

### 1. 🧪 jsrt-test-runner 🟢
**职责**：运行测试并分析失败原因

**专长**：
- 执行完整测试套件或单个测试
- 分析测试失败的根本原因
- 使用 AddressSanitizer 检测内存问题
- 支持 WPT (Web Platform Tests) 测试

**使用场景**：
```bash
# 当你需要：
- 调试测试失败
- 查找内存泄漏
- 验证 API 符合 Web 标准
```

---

### 2. 🔧 jsrt-module-developer 🔵
**职责**：开发新的标准库模块

**专长**：
- 创建符合项目规范的 C 模块
- 正确使用 QuickJS API
- 处理跨平台兼容性
- 确保 WinterCG 合规性

**使用场景**：
```bash
# 当你需要：
- 添加新的内置模块（如 fs、path、crypto）
- 扩展现有模块功能
- 实现 Web 标准 API
```

---

### 3. 🐛 jsrt-memory-debugger 🔴
**职责**：调试内存问题

**专长**：
- 分析 AddressSanitizer 输出
- 识别内存泄漏模式
- 修复 use-after-free 错误
- 优化内存使用

**使用场景**：
```bash
# 当你遇到：
- 段错误（Segmentation fault）
- 内存泄漏
- ASAN 报告的错误
- 资源清理问题
```

---

### 4. 🌍 jsrt-cross-platform 🟣
**职责**：确保跨平台兼容性

**专长**：
- Windows/Linux/macOS 适配
- 处理平台特定的系统调用
- 动态库加载抽象
- 构建系统配置

**使用场景**：
```bash
# 当你需要：
- 添加平台特定功能
- 修复 Windows 编译错误
- 处理路径分隔符问题
- 实现缺失的 POSIX 函数
```

---

### 5. 👁️ jsrt-code-reviewer 🟡
**职责**：代码审查和质量保证

**专长**：
- 检查代码质量和安全性
- 验证内存管理正确性
- 确保遵循项目规范
- 检查标准合规性

**使用场景**：
```bash
# 在以下情况使用：
- 提交 PR 前的自检
- 审查他人的代码
- 检查安全漏洞
- 验证 API 设计
```

---

### 6. ⚡ jsrt-build-optimizer 🟠
**职责**：优化构建配置

**专长**：
- CMake 配置优化
- 编译器标志调优
- 依赖管理
- 构建速度优化

**使用场景**：
```bash
# 当你需要：
- 加快编译速度
- 减小二进制体积
- 添加新的依赖
- 配置 CI/CD
```

---

### 7. 🎨 jsrt-formatter 🔷
**职责**：维护代码格式一致性

**专长**：
- clang-format 配置
- 代码风格规范
- 命名约定
- 注释规范

**使用场景**：
```bash
# 使用时机：
- 提交代码前格式化
- 统一代码风格
- 配置格式化规则
- 设置 pre-commit hooks
```

---

### 8. 🎯 jsrt-quickjs-expert 🟪
**职责**：QuickJS 引擎集成专家

**专长**：
- QuickJS C API 使用
- JavaScript 与 C 绑定
- Promise 和异步操作
- 性能优化

**使用场景**：
```bash
# 当你需要：
- 创建 JS/C 函数绑定
- 处理 JSValue 生命周期
- 实现自定义类
- 集成 libuv 异步操作
```

---

### 9. 📚 jsrt-example-creator 🟦
**职责**：创建示例代码

**专长**：
- 编写教育性示例
- 展示 API 用法
- 错误处理模式
- 最佳实践演示

**使用场景**：
```bash
# 当你需要：
- 为新功能创建示例
- 编写使用文档
- 展示复杂用法
- 创建教程
```

---

### 10. 🌐 jsrt-wpt-compliance 🟢
**职责**：确保 Web Platform Tests 合规性

**专长**：
- WPT 测试集成
- Web 标准 API 实现
- 测试适配和验证
- 合规性报告

**使用场景**：
```bash
# 当你实现：
- Console API
- Timer API
- URL/URLSearchParams
- WebCrypto API
- Fetch API
```

---

### 11. 🔄 jsrt-wintercg-compliance 🔵
**职责**：确保 WinterCG 规范合规性

**专长**：
- Minimum Common Web Platform API
- 跨运行时兼容性
- 标准全局对象和函数
- API 签名验证

**使用场景**：
```bash
# 需要确保：
- 与 Node.js/Deno/Bun 兼容
- 实现必需的全局 API
- 代码可移植性
- 标准错误处理
```

## 如何使用这些 Agents

### 在 Claude Code 中调用

1. **直接请求特定任务**：
   ```
   "帮我调试这个测试失败的问题"
   "添加一个新的 fs 模块"
   "检查这段代码的内存泄漏"
   ```

2. **明确指定 agent**：
   ```
   "使用 jsrt-memory-debugger 分析这个崩溃"
   "用 jsrt-wpt-compliance 检查 console API 实现"
   ```

3. **批量任务**：
   ```
   "审查这个 PR 的所有更改"
   "为新功能创建完整的测试和示例"
   ```

4. **并行执行**：
   ```
   "同时运行格式化、测试和内存检查"
   "使用 full-feature-development 工作流"
   "并行执行所有验证检查"
   ```

## Agent 颜色编码系统

| 颜色 | Agent | 含义 |
|------|-------|------|
| 🥇 金色 | jsrt-parallel-coordinator | 协调管理 |
| 🟢 绿色 | jsrt-test-runner, jsrt-wpt-compliance | 测试和验证 |
| 🔵 蓝色 | jsrt-module-developer, jsrt-wintercg-compliance | 开发和标准 |
| 🔴 红色 | jsrt-memory-debugger | 问题诊断 |
| 🟣 紫色 | jsrt-cross-platform | 平台适配 |
| 🟡 黄色 | jsrt-code-reviewer | 质量审查 |
| 🟠 橙色 | jsrt-build-optimizer | 构建优化 |
| 🔷 青色 | jsrt-formatter | 代码格式 |
| 🟪 品红 | jsrt-quickjs-expert | 引擎专家 |
| 🟦 青绿 | jsrt-example-creator | 文档示例 |

## 最佳实践

### 选择合适的 Agent

| 任务类型 | 推荐 Agent | 颜色 |
|---------|-----------|------|
| 新功能开发 | jsrt-module-developer | 🔵 |
| 调试崩溃 | jsrt-memory-debugger | 🔴 |
| 测试失败 | jsrt-test-runner | 🟢 |
| 代码审查 | jsrt-code-reviewer | 🟡 |
| 性能优化 | jsrt-build-optimizer + jsrt-quickjs-expert | 🟠 + 🟪 |
| 标准合规 | jsrt-wpt-compliance + jsrt-wintercg-compliance | 🟢 + 🔵 |

### 工作流示例

#### 开发新功能的完整流程：

1. **规划阶段**
   - 使用 `jsrt-wintercg-compliance` 检查标准要求
   - 使用 `jsrt-module-developer` 设计模块结构

2. **实现阶段**
   - 使用 `jsrt-module-developer` 编写代码
   - 使用 `jsrt-quickjs-expert` 处理 JS/C 绑定
   - 使用 `jsrt-cross-platform` 处理平台差异

3. **测试阶段**
   - 使用 `jsrt-test-runner` 运行测试
   - 使用 `jsrt-memory-debugger` 检查内存问题
   - 使用 `jsrt-wpt-compliance` 验证标准合规

4. **优化阶段**
   - 使用 `jsrt-formatter` 格式化代码
   - 使用 `jsrt-build-optimizer` 优化构建
   - 使用 `jsrt-code-reviewer` 最终审查

5. **文档阶段**
   - 使用 `jsrt-example-creator` 创建示例
   - 更新相关文档

## 配置文件位置

所有 agent 配置文件位于：
```
.claude/
├── agents/                        # Agent 定义
│   ├── jsrt-parallel-coordinator.md
│   ├── jsrt-test-runner.md
│   ├── jsrt-module-developer.md
│   ├── jsrt-memory-debugger.md
│   ├── jsrt-cross-platform.md
│   ├── jsrt-code-reviewer.md
│   ├── jsrt-build-optimizer.md
│   ├── jsrt-formatter.md
│   ├── jsrt-quickjs-expert.md
│   ├── jsrt-example-creator.md
│   ├── jsrt-wpt-compliance.md
│   └── jsrt-wintercg-compliance.md
└── workflows/                     # 工作流配置
    └── parallel-workflows.json    # 并行工作流定义
```

## 自定义和扩展

如需自定义 agent 行为，可以编辑对应的配置文件。每个配置文件包含：

- `type`: agent 类型（sub-agent）
- `name`: agent 名称
- `description`: 简短描述
- `tools`: 可用工具列表
- 详细的指令和工作流程

## 常见问题

**Q: Agent 会自动运行吗？**
A: 不会。Agent 只在你明确请求相关任务时才会激活。

**Q: 可以同时使用多个 agents 吗？**
A: 可以。Claude Code 会根据任务需要协调多个 agents 工作。

**Q: Agent 的建议总是正确的吗？**
A: Agent 提供专业建议，但仍需要人工审核，特别是关键决策。

**Q: 如何知道哪个 agent 在工作？**
A: Claude Code 通常会说明正在使用哪个 agent 或执行什么类型的任务。

## 更新历史

- 2024-09-05: 初始版本，包含 11 个专业 agents
  - 支持 WPT 和 WinterCG 标准合规性检查
  - 完整的开发、测试、优化工作流支持
  - 添加颜色编码系统
- 2024-09-05: 添加并行执行支持
  - 新增 jsrt-parallel-coordinator 协调器
  - 创建 6 个预定义并行工作流
  - 支持 2-3 倍速度提升
  - 详细的并行执行指南