# 任务 6：Example 公开接口串联报告

## 红灯

先提交了 `ZzExampleWorkspaceSmokeTest` 与测试 CMake 接入（提交
`6580bcd`），再运行简报指定的构建命令。首次生成 Smoke 目标时，构建报
`Cannot find source file: ../ZzExampleSessionModel.cpp`。该失败来自测试所依赖
的新 Example 会话模型尚未实现，随后补齐生产 API 后继续验证；没有将此红灯
误判为行为通过。

## 绿灯

本地会话模型和内容工厂提交为 `284a9c7`，WindowShell 迁移随后完成。主代理
已重新运行：

```text
cmake --build --preset linux-gcc-debug --target ZzPureToolsExample ZzExampleWorkspaceSmokeTest --parallel 2
ctest --preset linux-gcc-debug -R 'example.workspace-smoke|example.puretools-activity-model' --output-on-failure
```

两项目标构建成功，Smoke 与既有 ActivityModel 测试均通过（2/2）。

## Smoke 场景

- 通过 `ZzWorkspaceShell::create()` 创建真实工作区并挂载公开 workspace 根控件。
- 注册本地会话 Side Panel、终端 Tab、SFTP、日志、属性、任务 Dock，以及会话命令模型。
- 鼠标单击 Activity 激活会话侧栏；命令面板键盘回车激活“新建终端”。
- 创建并关闭终端 Tab；浮动 SFTP Dock。
- 使用 `ZzTabWidget::setPageTitle()` 更新当前标签，并验证
  `CurrentTabAndApplication` 标题策略。
- 保存、改变、恢复布局，验证侧栏宽度和 Dock 浮动状态 round-trip。
- 活动日志视图保留“位于尾部自动跟随、手动上翻暂停”的展示策略。

## 架构断言

以下命令在迁移后的 WindowShell 文件上均无匹配：

```text
rg -n 'QDockWidget' examples/ZzPureToolsExample/ZzExampleWindowShell*
rg -n 'QListView' examples/ZzPureToolsExample/ZzExampleWindowShell*
rg -n 'QToolBar' examples/ZzPureToolsExample/ZzExampleWindowShell*
rg -n 'setWindowTitle' examples/ZzPureToolsExample/ZzExampleWindowShell*
```

工作区 Activity、Command、Tab、Dock、标题和布局职责均由公开
`ZzWorkspaceShell`/Fluent 组件承载；Example WindowShell 只保留本地展示内容、
QAction 意图、导航/主题/关闭守卫和日志写入策略。

## 疑虑

- offscreen 直接启动检查在受限沙箱中无法写入用户测试日志目录，因此未在该
  沙箱内重复启动；目标构建与 Qt Test smoke 已通过。
- `temp_image/` 为既有未跟踪目录，未修改、未加入提交。

## Important 修复

- `dispatchWorkspaceCommand()` 现在检查四类 `showPanel()` 的
  `ZzResult`，失败时统一调用 `reportFailure()`，不再丢弃错误结果。
  Smoke 额外对未注册面板执行 `showPanel()` 并断言失败，覆盖失败可观察路径。
- `ZzExampleSessionModel::commandId()` 改为实例方法，校验索引有效性、
  column 0 以及模型身份必须是本对象拥有的命令模型。回归测试覆盖空索引、
  外来模型携带相同内部 role、错误列索引，均回退到 `NewTerminal`。

追加验证：

```text
cmake --build --preset linux-gcc-debug --target ZzPureToolsExample ZzExampleWorkspaceSmokeTest --parallel 2
ctest --preset linux-gcc-debug -R 'example.workspace-smoke|example.puretools-activity-model' --output-on-failure
```

结果为构建成功、2/2 测试通过；WindowShell 架构 `rg` 门禁仍全部无匹配。
