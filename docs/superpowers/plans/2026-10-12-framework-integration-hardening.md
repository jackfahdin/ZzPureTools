# 框架接入与发布证据收尾计划

> **面向 AI 代理的工作者：** 使用 executing-plans 逐任务执行；窗口装配、Example 迁移与安装消费者按依赖顺序内联实现，完成后独立代码审查。

**目标：** 让独立应用获得与 Example 一致的窗口体验，同时准确区分自动测试、性能参考与真实桌面验收。

**架构：** QWidget 的窗口标题是显示标题的来源，默认标题保留翻译能力，应用设置过的标题不被框架覆盖。ZzPureTools 提供辅助窗口 chrome 装配对象，Example 只负责业务内容；独立 starter 通过 find_package 消费安装包。

**技术栈：** C++20、Qt 6 Widgets、CMake/CTest、QWindowKit。

## 全局约束

- 历史性能基线不修改；环境指纹不一致时相对门禁保持待复核，不调宽门限换取绿色。
- 不读取、修改或提交 `docs/research/`。
- 在 `codex/framework-integration-hardening` 分支执行，复用当前已配置的 Linux 构建缓存；不推送远端。
- Windows/macOS 真机验收不能由 Linux offscreen/Xvfb 结果代替。
- README 放在实现与验证之后修改。按交付物拆分本地提交。

## 任务 1：窗口标题生命周期

**文件：** `ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.{h,cpp}`、`ZzPureTools/tests/ZzMultiWindowIsolationTest.cpp`、`ZzPureTools/tests/CMakeLists.txt`。

- [ ] 添加回归：setup callback 的标题保留；运行时 setWindowTitle 同步标题栏；LanguageChange 不覆盖自定义标题或空标题；两个窗口互不干扰。

```cpp
window->setWindowTitle(QStringLiteral("My Application"));
QCOMPARE(window->titleBar()->title(), QStringLiteral("My Application"));
QEvent languageChange(QEvent::LanguageChange);
QCoreApplication::sendEvent(window, &languageChange);
QCOMPARE(window->windowTitle(), QStringLiteral("My Application"));
```

- [ ] 构建并运行新场景，确认旧实现失败。
- [ ] 在 setup callback 之前设置默认标题并绑定 windowTitleChanged；记录默认标题是否仍由框架管理，翻译时仅更新默认标题。
- [ ] 运行 `puretools.multi-window.*`、`puretools.translation.*`、Builder 和 Workspace 的标题相关测试，再提交。

## 任务 2：可复用辅助窗口装配

**文件：** 新建 `ZzPureTools/widgets/include/ZzPureTools/ZzAuxiliaryWindowChrome.h`、对应 `widgets/src/ZzAuxiliaryWindowChrome.cpp` 与私有实现；修改 `ZzPureTools/CMakeLists.txt`、Example Settings/About private 装配；新增 `ZzPureTools/tests/ZzAuxiliaryWindowChromeTest.cpp`。

- [ ] 定义 GUI 线程专用 RAII 装配对象，工厂返回 `ZzResult<std::unique_ptr<ZzAuxiliaryWindowChrome>>`，输入为 `QMainWindow *` 和应用 `ZzThemeController *`。观察 getter 提供 titleBar/windowAgent。窗口仍由调用者或 Qt 父子树拥有。
- [ ] 回归验证：标题/图标同步、关闭命令、只注册关闭按钮、拒绝重复/已有菜单的宿主、主题同步、宿主先销毁也安全；先运行失败用例。
- [ ] 实现统一 FluentTitleBar + WindowAgent 装配；macOS 使用原生按钮能力判断。不把 FluentUI 依赖下放到 WindowKit，不建立第二套主题控制器。
- [ ] Settings/About 保存装配对象，删除重复 chrome 和隐藏按钮连接，保留业务 View/Presenter、翻译及弹窗所有权。
- [ ] 构建 Example、公开头和测试，运行辅助窗口及 Example settings/about 回归；独立审查后提交。

## 任务 3：可独立构建的应用起步工程

**文件：** 新建 `examples/ZzApplicationStarter/{CMakeLists.txt,main.cpp}`；修改 `tests/InstallConsumer/CMakeLists.txt` 和安装消费检查脚本；接入文档最后一起修改。

- [ ] 起步工程仅 `find_package(ZzPureToolsFrame 0.1 CONFIG REQUIRED)`，链接 `Zz::PureTools`、`Zz::FluentUI`、`Zz::WindowKit`；不依赖源码私有头。
- [ ] 用真实 Builder 注册页面、setup callback 设置标题/图标、themeToggled 保存主题，提供新窗口和辅助窗口按钮。
- [ ] 提供 `--smoke` 模式，使用临时设置文件，检查标题、双窗口主题、辅助窗口关闭与应用 shutdown，不污染用户配置；测试失败时返回非零。
- [ ] 安装消费者通过复制起步工程到隔离消费目录并构建/运行它，验证已移动的安装前缀；跑 `ctest --preset linux-gcc-benchmarks -R '^install.consumer$' --output-on-failure`。
- [ ] 提交起步工程与消费回归。

## 任务 4：性能与桌面证据

**文件：** `benchmarks/ZzGaugeAnimationBenchmark.cpp`、`tests/Platform/ZzPerformanceThresholdContract.cmake`（仅在诊断证实脚本错误时修改）、`docs/performance/CI_PERFORMANCE_REVIEW_2026-10-12_ZH.md`、`docs/development/PLATFORM_SUPPORT_ZH.md`。

- [ ] 核查 gauge 的采样语义与动画调度，区分 Paint 起始间隔和 paintEvent 成本；重跑相关绝对门限并保留原始报告。
- [ ] 核实 16 个门限场景与 12 份历史基线的缺口；缺失参考和指纹变化保持显式失败，不能伪造同环境基线。
- [ ] 执行可用的窗口/主题自动回归；列出 Windows 混合 DPI、Snap、IME、键盘焦点和深色/高对比度的人工步骤及预期。
- [ ] 修正平台文档中 backdrop 未接入、首次 CI 结果未登记等过时描述，并区分历史记录与当前分支验证；提交。

## 任务 5：文档、审查与交付

**文件：** `docs/development/FRAMEWORK_INTEGRATION_ZH.md`、`README.md`、本计划。

- [ ] 把接入伪代码旁补充可运行 starter 入口、构建命令、标题/辅助窗口所有权规则。
- [ ] 最后更新 README 的起步路径与验证边界，保持现有视觉结构。
- [ ] 运行公开头、ArchitectureAudit、DocumentationAudit、相关单元/集成回归；独立审查整个改动，处理有效问题。
- [ ] 文档单独提交；记录通过项和外部环境阻塞项，不声称全部发布门禁通过。

## 执行记录

开始于 `f0cf1c0`；计划执行前工作树仅有用户的 `docs/research/` 未跟踪目录。
