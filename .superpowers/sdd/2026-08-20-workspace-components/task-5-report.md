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
