# 任务 5：DockPanel 与 WorkspaceShell 实施报告

## 结果

- 新增 `ZzDockPanel` 四文件 PIMPL，保留 `QDockWidget` 的 features、allowedAreas、toggle action、浮动和停靠协议，并提供可访问标题栏与 `takeContentWidget()`。
- 新增 `ZzWorkspacePanelId`、`ZzWorkspaceTitleMode` 和 `ZzWorkspaceShell` 四文件 PIMPL。Shell 只协调宿主已有标题栏、Activity/Side/Tab/Palette/Dock，不调用 `setCentralWidget()`，也不创建 `ZzWindowAgent`。
- 工厂在创建对象前校验空 host、顶层 host、GUI 线程以及 titleBar 的线程和祖先关系；面板注册在所有权转移前完成 ID、标题、内容、父对象、线程和 area 校验。
- 布局格式使用 `ZZWS` magic、schema 1、固定 `QDataStream::Qt_6_8`、payload 长度和 SHA-256；输入上限为 1 MiB。payload 保存 Qt state、左右 Side 状态/宽度/当前 ID、Side area/order、当前 Tab 提示和标题模式。

## TDD 证据

### 首次红灯

先完整写入 `ZzDockPanelTest.cpp`、`ZzWorkspaceShellTest.cpp` 和测试 CMake，再运行：

```text
cmake --build --preset linux-gcc-debug --target ZzDockPanelTest ZzWorkspaceShellTest --parallel 2
```

结果退出码为 1，首个预期失败为：

```text
fatal error: ZzFluentUI/ZzDockPanel.h: No such file or directory
```

失败原因是新增公共类型尚不存在，不是测试语法或 fixture 错误。由于构建失败，计划命令中的后续 `&& ctest` 按预期未执行。

### 追加变异红灯

- 新增“Qt 已恢复 Dock、Shell 因 Side 宽度约束失败”的事务测试时，`restoreLayout()` 错误返回成功；改为 Qt-first 恢复、Shell 精确应用和双快照回放后转绿。
- 新增“目标 Shell 注册顺序不同”的恢复测试时，实际顺序为 `beta, alpha`，期望 `alpha, beta`；补齐 Activity 模型重排后转绿。
- 新增布局恢复后的 Activity 当前索引断言时实际为空；在页面 current ID 恢复后同步左右 Activity Bar，随后转绿。

## 覆盖摘要

- Dock 4 个行为场景：原生配置/toggle action、features 按钮显隐和无障碍、真实 `setFloating()` 与 `addDockWidget()`/重新停靠、内容 take。
- Shell 15 个行为场景：工厂输入和跨线程、禁止替换 central widget、注册失败不转移所有权、Side/Dock take、全局重复 ID、badge、显示隐藏、四种标题模式、置顶可见性和最大化状态、布局 envelope 全校验、未知 ID、注册顺序、双快照回滚、host 提前销毁。
- 所有权测试直接断言成功注册后的 QObject parent、take 后空 parent、Dock 删除、重复/非法注册时 parent 不变，以及 host 先销毁后内容 QPointer 失效。
- 回滚测试分别覆盖 Qt restore 失败时 Shell 快照不变，以及 Qt restore 成功后 Shell apply 失败时 Qt Dock 区域和 Shell Side 宽度同时回滚。

## 最终验证

shared Debug（`BUILD_SHARED_LIBS=TRUE`、`CMAKE_BUILD_TYPE=Debug`）：

```text
cmake --build --preset linux-gcc-debug --target ZzDockPanelTest ZzWorkspaceShellTest ZzPublicHeadersTest --parallel 2
ctest --preset linux-gcc-debug -R 'dock-panel|workspace-shell|architecture.public-headers|architecture.boundaries' --output-on-failure
```

结果：4/4 通过，0 失败。

static Debug（`BUILD_SHARED_LIBS=FALSE`、`CMAKE_BUILD_TYPE=Debug`）：

```text
cmake --build --preset linux-static-release --target ZzDockPanelTest ZzWorkspaceShellTest ZzPublicHeadersTest --parallel 2
ctest --preset linux-static-release -R 'dock-panel|workspace-shell|architecture.public-headers|architecture.boundaries' --output-on-failure
```

结果：4/4 通过，0 失败。公共头独立编译与架构边界检查均通过；源码扫描未发现 `setCentralWidget`、`ZzWindowAgent`、Qt private 头、业务/SSH/网络/设置 API 或 stylesheet。

## 疑虑与说明

- `linux-static-release` 没有独立 static Debug preset。本机的 `GCC_13/GXX_13/QT_ROOT` 环境变量为空，首次强制 Debug 重配因找不到编译器失败；随后显式使用 shared 缓存已验证的 GCC 15 和 Qt 6.11.1 路径配置该目录。最终 CMakeCache 已确认 static + Debug，构建和测试均通过。
- offscreen 平台下 `addDockWidget()` 不会自动清除已有 floating 状态；测试按 Qt 原生合同显式调用 `setFloating(false)` 后验证目标 Dock area，没有在组件中伪造额外停靠协议。
- 未修改或提交 `temp_image/`。

## 首轮审查修复（2026-08-21）

### 审查发现与回归测试

- `takePanel()` 在确认内容转移成功前删除 Activity 行或 Dock，失败时会破坏注册表、界面和模型的一致性。
- Side/Dock 注册内容没有外部 `destroyed` 清理连接，内容被外部删除后残留注册和 Dock，并永久占用面板 ID。
- Side 的 `addWidget()`、Dock 的 `setWidget()`/`addDockWidget()` 会同步发出 Qt 信号；原实现直到这些调用完成后才写入注册表，同步回调可以重复注册或取走半提交面板。
- Shell 只观察页面 `windowTitleChanged`，`setPageTitle()` 后缺少明确的页面展示变化通知。
- Dock 自定义关闭按钮调用 `hide()`，不会进入 Qt 的 `closeEvent` 协议。
- Workspace Shell 的公开 QWidget/QPointer getter 缺少一致的 GUI 线程断言和发布版兜底。

新增真实组件回归覆盖：失败 take 状态保留、外部内容销毁和 ID 复用、Side `currentWidgetChanged` 重入、Dock 内容 `ParentChange` 重入、`pagePresentationChanged` 标题刷新以及自定义 Dock close 的 `QEvent::Close`。

### TDD 红灯证据

先只加入测试并构建：

```text
cmake --build --preset linux-gcc-debug --target ZzDockPanelTest ZzWorkspaceShellTest --parallel 2
```

首次结果为编译失败：测试引用的 `pagePresentationChanged` 尚不存在。将该测试临时改为运行时信号查找后，运行 focused tests 得到 7 个 WorkspaceShell 失败和 1 个 DockPanel 失败：

- Side 失败 take 后 Activity 行实际为 0（期望保留为 1）。
- Dock 失败 take 找不到应保留的 Dock。
- 外部删除 Side 内容后模型行实际仍为 1（期望为 0）。
- 外部删除 Dock 内容后 Dock 仍存在，ID 无法复用。
- Side/Dock 同步注册回调中的重复注册均实际成功。
- 标题展示变化信号不存在且标题未刷新。
- Dock close 按钮产生 0 个 `QEvent::Close`（期望 1）。

### 修复与绿灯证据

- 注册在所有权转移前写入 provisional `PanelRecord`，预占 ID；每个同步 Qt 调用后按 ID 和内容身份重新解析，失败统一 rollback。
- 记录内容销毁连接、注册/移除事务状态和稳定内容身份；外部销毁会清理 Activity、Dock 和注册记录并允许 ID 复用。宿主析构期间不再对已失效的 QMainWindow layout 调用 `removeDockWidget()`。
- `takePanel()` 先验证实际容器归属和事务状态，只有成功 take 后才删除模型、Dock 和记录。
- `ZzTabWidget::setPageTitle()` 发出 `pagePresentationChanged(QWidget *)`；Shell 只对当前页刷新标题。
- Dock close 按钮改为 `close()`；Shell 公开 getter 统一加入 GUI 线程断言和发布版安全返回值。

shared Debug 与 static Debug 均通过：

```text
cmake --build --preset linux-gcc-debug --target ZzDockPanelTest ZzWorkspaceShellTest ZzPublicHeadersTest --parallel 2
ctest --preset linux-gcc-debug -R 'dock-panel|workspace-shell|tab-controls|architecture.public-headers|architecture.boundaries' --output-on-failure
5/5 passed

cmake --build --preset linux-static-release --target ZzDockPanelTest ZzWorkspaceShellTest ZzPublicHeadersTest --parallel 2
ctest --preset linux-static-release -R 'dock-panel|workspace-shell|tab-controls|architecture.public-headers|architecture.boundaries' --output-on-failure
5/5 passed
```
