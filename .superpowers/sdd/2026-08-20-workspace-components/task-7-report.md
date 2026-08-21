# 任务 7：性能、截图、安装消费与平台审计报告

## 交付

- 提交 `580b02a` 注册 `benchmark.workspace-components`，固定覆盖标题菜单、
  Activity、10 万节点 Explorer、200 Tab、1 万命令、64 Side/32 Dock 布局和完整
  工作区渲染；八项指标均为 observe。
- 提交 `f44b4fe` 增加真实工作区截图面与 12 张 Linux 基线：Light、Dark、
  HighContrast 分别覆盖 DPR 100、125、150、200。
- 安装消费者显式编译 14 个工作区公开头并构造最小工作区；PublicHeaderConsumer
  逐个编译安装树全部 135 个头，同时要求工作区头实际存在。
- ArchitectureAudit 对工作区来源额外拒绝 SSH/SFTP、网络、设置、repository、
  database 和 domain include；全局审计继续覆盖 Qt Private、业务泄漏、裸色、
  stylesheet、中文 Doxygen、PIMPL、命名空间与类型命名。

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
记录到 `docs/performance/PERFORMANCE_BASELINE_ZH.md`。`temp_image/` 为既有用户目录，
未修改、未暂存。

## 平台边界

Linux offscreen 结果证明 Qt 公共 API、软件渲染、截图和安装消费者合同。Windows
MSVC、Windows MinGW、macOS 的原生构建和运行均未执行，文档保持“未执行”；它们不能
由 Linux、Xvfb 或公共头静态编译替代。完整 `platform.package-relocation` 在此受限执行
环境中超过单命令 30 秒窗口，因此以 fresh prefix B 的安装消费者运行和全量头消费者
构建保留等价分步证据；正式门禁仍必须在不受该窗口限制的 runner 上执行该完整 CTest。
