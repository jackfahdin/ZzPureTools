# ZzFluent 发展路线图

本文档定义 ZzFluent 的阶段性开发目标与实施计划。所有开发工作应围绕本路线图展开，并在每个阶段结束时同步更新 README 与文档。

---

## 总体目标

打造一个**现代化、跨平台、易于扩展**的 Qt 6 Fluent UI 控件库：

- 提供 40+ 常用 Fluent UI 控件
- 支持 Light / Dark / System 主题与实时切换
- 支持无边框窗口与自定义标题栏（可选集成 QWindowKit）
- 提供 Acrylic / Mica 等 Fluent 视觉特效
- 提供完善的文档、示例与 SDK 分发

---

## 阶段一：基础夯实（第 1-2 周）

**目标**：让项目达到可稳定开发、可展示的状态。

### 任务清单

- [ ] 修通 CI/CD 流程，确保手动触发与 Release 构建稳定。
- [ ] 完善 `README.md`、`AGENTS.md`、本路线图文档。
- [ ] 在 CI 中加入代码格式检查（clang-format）与静态检查（clang-tidy）。
- [ ] 统一现有文件命名与注释风格，补充所有现有类的 Doxygen 注释。
- [ ] 整理 `.clang-format` / `.clang-tidy` 配置，确保与 C++20 兼容。
- [ ] 为示例程序添加更直观的界面截图或 GIF。

### 验收标准

- 任意 push / 手动触发后，所有平台矩阵至少能编译通过。
- README 包含构建说明、控件列表、使用示例。
- 所有公开类具备完整 Doxygen 注释。

---

## 阶段二：核心基础设施强化（第 3-6 周）

**目标**：把架构从"能跑"提升到"好扩展"。

### 任务清单

#### 2.1 主题系统增强

- [ ] `ZzTheme` 支持读取系统强调色（Windows / macOS / Linux）。
- [ ] 扩展 `ZzColorTokens`，增加更多语义化颜色（如 error、warning、success）。
- [ ] 扩展 `ZzMetricTokens`，增加字体、阴影、Elevation 层级。
- [ ] 支持主题序列化/反序列化（JSON/YAML 配置文件）。

#### 2.2 动画系统扩展

- [ ] `ZzAnimator` 支持自定义缓动函数（Easing Function）。
- [ ] 支持多属性同时动画（位置、大小、透明度、颜色）。
- [ ] 引入 `ZzAnimationController` 管理控件生命周期内的动画。

#### 2.3 绘制原语丰富

- [ ] `ZzPaintPrimitives` 增加阴影绘制（Drop Shadow / Inner Shadow）。
- [ ] 增加渐变填充（Linear / Radial Gradient）。
- [ ] 增加模糊背景辅助函数（为 Acrylic/Mica 做准备）。
- [ ] 增加图标绘制辅助（支持 SVG / 字体图标）。

#### 2.4 样式引擎

- [ ] 引入 `ZzStyleEngine` 或 `ZzStyleFactory`，统一根据控件类型分配 Delegate。
- [ ] 支持运行时切换 Delegate，便于主题/皮肤扩展。

### 验收标准

- 新增一个控件时，90% 的绘制逻辑可复用现有原语。
- 主题切换能在 100ms 内完成全窗口重绘。
- 动画帧率稳定 60fps。

---

## 阶段三：核心控件补齐（第 7-16 周）

**目标**：实现 Fluent UI 最常见的基础控件，形成可用控件集。

### 3.1 输入类控件

| 控件 | 优先级 | 说明 |
|------|--------|------|
| `ZzLineEdit` | 高 | 单行输入框，支持占位符、清除按钮、密码模式 |
| `ZzTextEdit` | 高 | 多行文本编辑框 |
| `ZzSpinBox` | 中 | 整数微调框 |
| `ZzDoubleSpinBox` | 中 | 浮点数微调框 |
| `ZzSearchBox` | 中 | 搜索框，带搜索/清除图标 |

### 3.2 选择类控件

| 控件 | 优先级 | 说明 |
|------|--------|------|
| `ZzCheckBox` | 高 | 复选框 |
| `ZzRadioButton` | 高 | 单选框 |
| `ZzSwitch` | 高 | 开关按钮 |
| `ZzSlider` | 高 | 滑动条 |
| `ZzMultiSelectComboBox` | 低 | 多选下拉框 |

### 3.3 展示类控件

| 控件 | 优先级 | 说明 |
|------|--------|------|
| `ZzProgressBar` | 高 | 进度条（确定/不确定） |
| `ZzLabel` | 高 | 文本标签 |
| `ZzToolTip` | 中 | 工具提示 |
| `ZzInfoBar` / `ZzToast` | 中 | 信息提示条 |

### 3.4 容器与对话框

| 控件 | 优先级 | 说明 |
|------|--------|------|
| `ZzGroupBox` | 中 | 分组框 |
| `ZzCard` | 中 | 卡片容器 |
| `ZzDialog` | 中 | 模态对话框 |
| `ZzFlyout` / `ZzPopover` | 中 | 弹出层 |

### 验收标准

- 每个控件具备：公开头文件、实现、Delegate（如需要）、示例、Doxygen 注释。
- 每个控件支持：Normal / Hover / Pressed / Disabled / Focus 状态。
- Light / Dark 主题下视觉效果一致。

---

## 阶段四：窗口层集成（第 17-22 周）

**目标**：提供完整的 Fluent 窗口体验。

### 任务清单

- [ ] 调研并集成 [QWindowKit](https://github.com/stdware/qwindowkit) 作为窗口层基础。
- [ ] 实现 `ZzPureTitleBar`：自定义标题栏，包含窗口图标、标题、最小化/最大化/关闭按钮。
- [ ] 实现 `ZzFluentWindow`：无边框 Fluent 风格主窗口。
- [ ] 支持 Windows 11 Snap Layout。
- [ ] 支持 macOS 原生系统按钮几何自定义。
- [ ] 支持 Linux 下基本的窗口边框处理。

### 验收标准

- `ZzFluentWindow` 在 Windows / macOS / Linux 上均可正常显示。
- 窗口拖动、缩放、最大化、最小化、关闭行为与原生窗口一致。
- 标题栏控件可自定义（添加菜单、搜索框、用户头像等）。

---

## 阶段五：视觉效果与动效打磨（第 23-28 周）

**目标**：让界面真正具备 Fluent Design 质感。

### 任务清单

- [ ] 实现 Acrylic 背景效果（Windows DWM）。
- [ ] 实现 Mica / Mica Alt 背景效果（Windows 11）。
- [ ] 实现 Reveal Highlight 照明效果（鼠标悬停时控件边缘微光）。
- [ ] 统一阴影系统（Elevation 1-5 层级）。
- [ ] 优化所有控件的动画曲线与时长。

### 验收标准

- Acrylic / Mica 在 Windows 10/11 上正常显示。
- 动画流畅自然，符合 Fluent Design 动画规范。

---

## 阶段六：高级组件与生态建设（第 29-40 周）

**目标**：从控件库升级为完整的 UI 解决方案。

### 任务清单

#### 6.1 数据视图

- [ ] `ZzListView`
- [ ] `ZzTreeView`
- [ ] `ZzTableView`

#### 6.2 导航与菜单

- [ ] `ZzMenu` / `ZzMenuBar`
- [ ] `ZzTabBar` / `ZzTabWidget`
- [ ] `ZzSideBar` / `ZzNavigationView`

#### 6.3 集成与分发

- [ ] 提供 vcpkg port。
- [ ] 提供 Conan 包。
- [ ] 支持 qmake 集成方式。
- [ ] 完善 SDK 使用文档与示例。

#### 6.4 Qt Quick 支持（可选）

- [ ] 调研 Qt Quick / QML 支持的可行性。
- [ ] 提供 QML 版本的控件封装（长期目标）。

### 验收标准

- 控件数量达到 40+。
- 提供至少两种包管理器分发方式。
- 文档覆盖所有公开 API。

---

## C++20 应用指南

在各阶段实现中，鼓励按以下优先级使用 C++20 特性：

| 特性 | 推荐场景 |
|------|----------|
| `concept` | 模板化 Delegate / 回调接口约束 |
| `std::ranges` | 遍历控件子项、过滤/转换数据 |
| `std::optional` | 可选配置项、可选返回值 |
| `std::expected` | 主题加载、配置文件解析的错误处理 |
| `std::span` | 绘制原语接收颜色/点数组 |
| `coroutine` | 异步加载资源、延迟初始化 |
| 指定初始化器 | 构造 `ZzColorTokens` 等结构体 |
| `using enum` | 简化状态枚举使用 |

**注意**：不要为了使用特性而使用特性，始终以可读性和性能为前提。

---

## 质量控制

每个阶段结束时进行以下检查：

1. **编译检查**：所有支持平台通过 CI。
2. **静态检查**：clang-tidy 无高优先级警告。
3. **格式检查**：clang-format 无差异。
4. **文档检查**：新增/修改的公开 API 已补充 Doxygen 注释。
5. **示例检查**：新控件已在示例程序中展示。
6. **回归检查**：已有控件行为未被破坏。

---

## 备注

- 本路线图为滚动计划，可根据实际进度和社区反馈调整优先级。
- 每个阶段完成后，应在 README 中更新当前进度。
- 大功能建议通过 Issue / PR 先进行设计讨论，避免方向偏离。
