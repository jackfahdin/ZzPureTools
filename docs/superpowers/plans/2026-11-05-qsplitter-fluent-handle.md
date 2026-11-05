# QSplitter Fluent 胶囊手柄实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将上游 FluentUI3Style 的 QSplitter 胶囊分隔手柄行为适配到 ZzFluentStyle，并保证主题、方向、悬停和 DockWidget 行为可回归验证。

**架构：** 在 ZzFluentStyle 的 `CE_Splitter` 分支中只绘制真正的 QSplitterHandle，使用 ZzThemeSnapshot 的颜色令牌和现有 PanelSplitterExtent 几何令牌。QSplitter 的手柄宽度保持由控件自身管理，DockWidget 分隔条继续交给 Qt 原生样式。

**技术栈：** Qt 6 Widgets、QProxyStyle、QPainter、ZzThemeSnapshot、Qt Test。

---

### 任务 1：为 splitter 绘制契约编写失败测试

**文件：**
- 修改：`ZzFluentUI/tests/ZzFluentStyleTest.cpp`

- [x] **步骤 1：添加方向和主题绘制测试**

  构造横向与纵向 `QStyleOption`，调用 `drawControl(CE_Splitter, ...)`，验证足够空间时中心胶囊使用 `ControlStrongFill`，悬停时使用 `TextSecondary`，两种主题均取自当前快照。

- [x] **步骤 2：添加窄手柄和 DockWidget 隔离测试**

  验证主轴小于 64 或副轴小于 2 时不绘制，并通过 `QStyleOption` 的普通 splitter 绘制入口保持空白区域透明。

- [x] **步骤 3：运行测试确认当前实现失败**

  运行：`cmake --build --preset windows-msvc2022-release --target ZzFluentStyleTest --parallel`

  预期：新增 splitter 颜色/绘制断言失败，因为当前 `ZzFluentStyle` 尚未处理 `CE_Splitter`。

### 任务 2：实现主题化 QSplitter 胶囊手柄

**文件：**
- 修改：`ZzFluentUI/widgets/src/ZzFluentStyle.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`

- [x] **步骤 1：增加 QSplitterHandle 的样式入口**

  在 `drawControl` 的 `CE_Splitter` 分支中，仅当 `widget` 是 `QSplitterHandle` 时调用私有绘制函数；其他上下文交给基样式，避免影响 DockWidget 分隔条。

- [x] **步骤 2：按方向和令牌绘制胶囊**

  使用 `PanelSplitterExtent` 作为手柄副轴尺寸，主轴固定 64 逻辑像素，使用 `ControlStrongFill`、`TextSecondary` 和 `TextDisabled` 分别处理常态、悬停和禁用状态，并在空间不足时直接跳过绘制。

- [x] **步骤 3：为 QSplitterHandle 开启和清理 hover 属性**

  在 `polish`/`unpolish` 中对 `QSplitterHandle` 设置和清理 `WA_Hover`，不改变普通 QWidget 或 QDockWidget 的属性。

### 任务 3：验证并提交

**文件：**
- 修改：`ZzFluentUI/tests/ZzFluentStyleTest.cpp`

- [x] **步骤 1：运行目标测试**

  运行：`ctest --test-dir build/windows-msvc2022-release -R 'fluent.style|fluent.dock-panel' --output-on-failure`

- [ ] **步骤 2：运行相关构建和静态检查**

  运行：`cmake --build --preset windows-msvc2022-release --target ZzPureToolsExample --parallel`

- [x] **步骤 3：检查差异并提交线性变更**

  运行：`git diff --check`，确认只包含 splitter 样式、测试和本计划文件后提交：

  ```bash
  git add ZzFluentUI/widgets/src/ZzFluentStyle.cpp ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp ZzFluentUI/tests/ZzFluentStyleTest.cpp docs/superpowers/plans/2026-11-05-qsplitter-fluent-handle.md
  git commit -m "feat(FluentUI): 添加 QSplitter 胶囊分隔手柄"
  ```
