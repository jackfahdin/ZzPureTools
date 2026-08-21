# 任务 7：性能、截图、安装消费与平台审计报告

## 交付

- 提交 `580b02a` 注册 `benchmark.workspace-components`，固定覆盖标题菜单、
  Activity、10 万节点 Explorer、200 Tab、1 万命令、64 Side/32 Dock 布局和完整
  工作区渲染；八项指标均为 observe。
- 提交 `f44b4fe` 增加真实工作区截图面与 12 张 Linux 基线：Light、Dark、
  HighContrast 分别覆盖 DPR 100、125、150、200。
- 提交 `5f7d351` 为结果列表增加不超过 8 个 QWidget 的固定预算，并拒绝全透明
  工作区渲染。
- 提交 `329cdd1` 将工作区 ArchitectureAudit 扩展到 FluentUI 与 PureTools widget
  模块的全部公开头、实现和 private 文件；PIMPL 与业务依赖均有可执行夹具合同。
- 提交 `3f9dcb1` 增加 480 x 540 窄工作区三主题、四档 DPR 的 12 张基线和几何断言。
- 安装消费者显式编译 14 个工作区公开头并构造最小工作区；PublicHeaderConsumer
  逐个编译安装树全部 135 个头，同时要求工作区头实际存在。
- ArchitectureAudit 对工作区全部公开头、实现和 private 文件额外拒绝 SSH/SFTP、
  网络、设置、repository、database 和 domain include；每个公开 Qt widget 必须有
  同名 Private 前置声明和 `std::unique_ptr<...Private> d_ptr`。全局审计继续覆盖
  Qt Private、业务泄漏、裸色、stylesheet、中文 Doxygen、命名空间与类型命名。

## 红绿证据

安装消费者先包含 `ZzWorkspaceShell`，但故意未链接 `Zz::PureTools`。真实 prefix B
消费者链接失败，错误为 `ZzWorkspaceShell::~ZzWorkspaceShell()`、`create()`、
`ZzWorkspacePanelId`、`registerSidePanel()` 与 `registerDockPanel()` 未定义。随后加入
`Zz::PureTools` 后，消费者链接并在 `QT_QPA_PLATFORM=offscreen` 下成功运行。

ArchitectureAudit 首次发现工作区新增代码的裸尺寸、陈旧视觉白名单与未加 `Zz` 前缀
的私有辅助类型。修复仅限以下门禁要求：

- TitleBar 和 DockPanel 使用命名尺寸替代裸字面量；删除已不再命中的标题栏白名单。
- CommandPalette、TabWidget、WorkspaceShell 的私有辅助类型重命名为 `Zz*`。
- 完整审计跳过 Example 的 `/tests/`，避免将 Qt Test 内联测试类按生产实现检查。

这些改动不改变公开 API 或工作区运行行为。

本轮补强的红绿证据如下：

- 将结果列表预算暂设为 0 后，`benchmark.workspace-components` 以
  `command palette result view exceeded widget virtualization budget` 失败；恢复到
  固定上限 8 后同一真实 benchmark 通过。
- 新增工作区架构夹具时，合同因检查器不存在而失败；实现检查器后，good fixture
  通过，private `SshClient.h` fixture 被 `WORKSPACE_PRESENTATION_DEPENDENCY` 拒绝，
  无 PIMPL public widget fixture 被 `WORKSPACE_PUBLIC_WIDGET_PIMPL` 拒绝。
- 新增窄屏三主题测试后，DPR 100 的 68 个既有场景通过，3 个新场景仅因缺少
  `workspace-narrow-*.png` 基线失败；生成 12 张新基线后，四档 DPR 比较均通过。
  几何断言覆盖折叠菜单、标题、主题、置顶、最小化、最大化、关闭、图标及
  Activity/Tab/Dock 区域的非重叠与可见性。
- 工作区 foundation 公开头也纳入扫描；临时向 `ZzActivityArea.h` 注入
  `SshClient.h` 后完整审计以 `WORKSPACE_PRESENTATION_DEPENDENCY` 失败。完整扫描
  同时暴露 Qt 的 `QFontDatabase` 被旧子串规则误判，规则改为匹配 include 路径起始或
  目录分段后，完整审计与三类 PIMPL/依赖夹具重新通过。

## 验证

```text
cmake --build --preset linux-gcc-benchmarks --target ZzFluentUI ZzPureTools --parallel 2
成功

ctest --preset linux-gcc-benchmarks -R '^(fluent\.(tab-controls|command-palette|dock-panel|title-bar)|puretools\.workspace-shell)$' --output-on-failure
5/5 通过

ctest --preset linux-gcc-benchmarks -R '^fluent\.screenshot-(100|125|150|200)$' --output-on-failure
4/4 通过

cmake -DZZ_SOURCE_DIR=... -DZZ_TARGET_MANIFEST=... -P tests/Architecture/ZzArchitectureAudit.cmake
Complete Zz architecture audit passed

cmake --build .../public-header-consumer --target ZzInstalledPublicHeaders --parallel 4
135 个安装头目标通过
```

三轮 workspace observe JSON 使用相同环境指纹；具体 P50/P95/max 范围和噪声结论已
记录到 `docs/performance/PERFORMANCE_BASELINE_ZH.md`。新证据为
`docs/performance/evidence/workspace-components/2026-08-21/round-{1,2,3}.json`，源码
身份均为 `3f9dcb1cb7a11d5a28bd96045caf144794e69cc5`，SHA-256 依次为
`54b0b553235b9ab9b4dbe17eba219c9e4e91f4a9ecfb49eaf5529a9160ce57e4`、
`292a2b0dd0e869f6ad151d6c1d5de5b0d9dcd09cb5cc5be08f4fcf896adc0598`、
`aca39dd571a60d7f6aad21907adf80c18bc91d7f4e50072c089f76a7d98cad2a`。
`temp_image/` 为既有用户目录，未修改、未暂存。

## 平台边界

Linux offscreen 结果证明 Qt 公共 API、软件渲染、截图和安装消费者合同。Windows
MSVC、Windows MinGW、macOS 的原生构建和运行均未执行，文档保持“未执行”；它们不能
由 Linux、Xvfb 或公共头静态编译替代。完整 `platform.package-relocation` 已在不受限
runner 上通过 `1/1`，耗时 47.08 秒；此前关于该项无法完整执行的描述已更正。
