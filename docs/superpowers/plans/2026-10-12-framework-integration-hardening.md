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

- [x] 添加回归：setup callback 的标题保留；运行时 setWindowTitle 同步标题栏；LanguageChange 不覆盖自定义标题或空标题；两个窗口互不干扰。

```cpp
window->setWindowTitle(QStringLiteral("My Application"));
QCOMPARE(window->titleBar()->title(), QStringLiteral("My Application"));
QEvent languageChange(QEvent::LanguageChange);
QCoreApplication::sendEvent(window, &languageChange);
QCOMPARE(window->windowTitle(), QStringLiteral("My Application"));
```

- [x] 构建并运行新场景，确认旧实现失败。
- [x] 在 setup callback 之前设置默认标题并绑定 windowTitleChanged；记录默认标题是否仍由框架管理，翻译时仅更新默认标题。
- [x] 运行 `puretools.multi-window.*`、`puretools.translation.*`、Builder 和 Workspace 的标题相关测试，再提交。

## 任务 2：可复用辅助窗口装配

**文件：** 新建 `ZzPureTools/widgets/include/ZzPureTools/ZzAuxiliaryWindowChrome.h`、对应 `widgets/src/ZzAuxiliaryWindowChrome.cpp` 与私有实现；修改 `ZzPureTools/CMakeLists.txt`、Example Settings/About private 装配；新增 `ZzPureTools/tests/ZzAuxiliaryWindowChromeTest.cpp`。

- [x] 定义 GUI 线程专用 RAII 装配对象，工厂返回 `ZzResult<std::unique_ptr<ZzAuxiliaryWindowChrome>>`，输入为 `QMainWindow *` 和应用 `ZzThemeController *`。观察 getter 提供 titleBar/windowAgent。窗口仍由调用者或 Qt 父子树拥有。
- [x] 回归验证：标题/图标同步、关闭命令、只注册关闭按钮、拒绝重复/已有菜单的宿主、主题同步、宿主先销毁也安全；先运行失败用例。
- [x] 实现统一 FluentTitleBar + WindowAgent 装配；macOS 使用原生按钮能力判断。不把 FluentUI 依赖下放到 WindowKit，不建立第二套主题控制器。
- [x] Settings/About 保存装配对象，删除重复 chrome 和隐藏按钮连接，保留业务 View/Presenter、翻译及弹窗所有权。
- [x] 构建 Example、公开头和测试，运行辅助窗口及 Example settings/about 回归；独立审查后提交。

## 任务 3：可独立构建的应用起步工程

**文件：** 新建 `examples/ZzApplicationStarter/{CMakeLists.txt,main.cpp}`；修改 `tests/InstallConsumer/CMakeLists.txt` 和安装消费检查脚本；接入文档最后一起修改。

- [x] 起步工程仅 `find_package(ZzPureToolsFrame 0.1 CONFIG REQUIRED)`，链接 `Zz::PureTools`、`Zz::FluentUI`、`Zz::WindowKit`；不依赖源码私有头。
- [x] 用真实 Builder 注册页面、setup callback 设置标题/图标、themeToggled 保存主题，提供新窗口和辅助窗口按钮。
- [x] 提供 `--smoke` 模式，使用临时设置文件，检查标题、双窗口主题、辅助窗口关闭与应用 shutdown，不污染用户配置；测试失败时返回非零。
- [x] 安装消费者通过复制起步工程到隔离消费目录并构建/运行它，验证已移动的安装前缀；跑 `ctest --preset linux-gcc-benchmarks -R '^install.consumer$' --output-on-failure`。
- [x] 提交起步工程与消费回归。

## 任务 4：性能与桌面证据

**文件：** `benchmarks/ZzGaugeAnimationBenchmark.cpp`、`tests/Platform/ZzPerformanceThresholdContract.cmake`（仅在诊断证实脚本错误时修改）、`docs/performance/CI_PERFORMANCE_REVIEW_2026-10-12_ZH.md`、`docs/development/PLATFORM_SUPPORT_ZH.md`。

- [x] 核查 gauge 的采样语义与动画调度，区分 Paint 起始间隔和 paintEvent 成本；重跑相关绝对门限并保留原始报告。
- [x] 核实 16 个门限场景与 12 份历史基线的缺口；缺失参考和指纹变化保持显式失败，不能伪造同环境基线。
- [x] 执行可用的窗口/主题自动回归；列出 Windows 混合 DPI、Snap、IME、键盘焦点和深色/高对比度的人工步骤及预期。
- [x] 修正平台文档中 backdrop 未接入、首次 CI 结果未登记等过时描述，并区分历史记录与当前分支验证；提交。

## 任务 5：文档、审查与交付

**文件：** `docs/development/FRAMEWORK_INTEGRATION_ZH.md`、`README.md`、本计划。

- [x] 把接入伪代码旁补充可运行 starter 入口、构建命令、标题/辅助窗口所有权规则。
- [x] 最后更新 README 的起步路径与验证边界，保持现有视觉结构。
- [x] 运行公开头、ArchitectureAudit、DocumentationAudit、相关单元/集成回归；独立审查整个改动，处理有效问题。
- [x] 文档单独提交；记录通过项和外部环境阻塞项，不声称全部发布门禁通过。

## 执行记录

开始于 `f0cf1c0`；计划执行前工作树仅有用户的 `docs/research/` 未跟踪目录。

### 完成交付

任务 1–5 的工程实现、诊断和文档均已收尾；勾选不代表正式性能门禁或真机验收通过。

- 标题同步覆盖 setup、运行时、自定义空标题、多窗口和翻译信号重入；测试先失败后通过。
- 新增 `ZzAuxiliaryWindowChrome` 并迁移 Example Settings/About；独立审查发现的图标 setter 问题已用实际 QLabel pixmap 断言复现并修复。
- Starter 提供独立 `find_package` 工程、内嵌图标、主题恢复/保存、新窗口与关于窗口；补充关闭后重开及 shutdown 断言。
- 辅助窗口的自绘按钮只保留关闭；macOS 保留原生系统控件。这是平台能力边界，不承诺隐藏原生最小化/缩放按钮。
- 动画 Paint 分发 P95 约 0.21 ms，帧间隔超限不能直接归因为绘制代码，因此没有修改生产动画、指标语义或预算。
- 按原约束保留历史基线、复用本地构建缓存，Windows/macOS 原生桌面验收仍待执行。

### 验证记录

| 检查 | 实际结果 |
|---|---|
| GCC 15.2 / Qt 6.11.1 Release + LTO | 完整构建通过，公共头编译通过 |
| 窗口、Builder、翻译、辅助窗口 | 37 项定向回归通过；Workspace 与 Example 集成回归通过 |
| 非 benchmark 大套（排除独立安装消费） | 263 项中首轮 261 项通过；架构装配入口、嵌套构建数量两项登记遗漏修复后分别复验通过 |
| `install.consumer` | 从源码全新构建/安装框架，复制 Starter 并编译运行，通过（185.42 s） |
| 最终 Starter smoke | 补充重开断言后，独立外部消费构建与 CTest 通过 |
| `platform.package-relocation` | 扩展后完整重跑通过（247.05 s）；移除前缀 A，公开头、消费测试和复制出的 Starter 都使用前缀 B |
| 门禁脚本与架构边界 | 搬迁测试同轮复验通过，3/3 |
| 文档与审查 | DocumentationAudit 与差异空白检查通过；分项及整体独立审查，无遗留代码阻断项 |
| 未插桩动画绝对门限 | 仪表 P95 16.721027 ms，开关 17.014635 ms，均超过 16.7 ms，保留失败 |
| 性能参考完整性 | 16 个场景仅 12 份历史参考，契约仍失败；Mesa/kernel 指纹变化，相对门禁待复核 |

非性能结论来自一次大套加失败项定向复验，未声称在单次重跑中全绿。
性能证据与复现方法见
[追加诊断](../../performance/evidence/window-hardening/2026-10-12/README.md)；
安装、运行库路径和 API 所有权见
[应用接入指南](../../development/FRAMEWORK_INTEGRATION_ZH.md)。

### 提交拆分

- `7e1f22b`：执行计划。
- `38bcc73`：标题同步与翻译生命周期。
- `773a669`：辅助窗口装配及 Example 迁移。
- `70da953`：独立 Starter 与安装消费回归。
- `e7a3630`：架构装配入口登记。
- `4f6c87e`：性能原始证据与平台验收边界。
- 后续独立提交：搬迁消费回归与构建数量契约；接入文档、计划执行记录与最后修改的 README。

所有提交保存在 `codex/framework-integration-hardening`，未合并 master、未推送。

### 性能门禁续查

用户要求继续处理后，补充 GLib/UNIX、纯 Qt、跳过 Paint 和批量刻度绘制对照。
批量绘制没有稳定收益，试改已撤回，控件库已重新构建。
有效修复为 `9084745`：把正式参考完整性与比较器自测分开，解决缺基线提前退出、
后续测试未执行的问题；保持原完整性测试名称和失败条件，新增缺失场景明细及真实校验器正反例。
独立审查确认未削弱门禁。比较器、采集与门禁脚本契约通过；最终动画定向复验仍有
仪表超限和四份参考缺失两项失败，见[隔离诊断证据](../../performance/evidence/animation-isolation/2026-10-12/README.md)。

### 逐帧追踪与失败归档续查

- [x] 用同一单调时钟关联动画 Timer、值变化、UpdateRequest、Paint 和 XCB 同步。
- [x] 实跑共享内存开/关/关/开四轮对照，保留原始 CSV 与分析工具。确认同步等待对部分慢帧的影响，未证明禁用共享内存能过门限。
- [x] 修复 CTest 失败后报告未归档、再次运行复用目录的问题；新增动态契约先红后绿，保留门禁/日志失败退出码。
- [x] 独立审查并修复新测试在 macOS 错误注册的问题，6 项相关契约与审计通过。
- [x] 恢复默认环境复跑原始基准，开关 P95 16.615112 ms、本轮通过；仪表 16.759650 ms、仍失败，四份参考缺失仍失败。

代码修复提交 `a6002e8`；追踪原始数据与说明单独提交，README 最后更新。
证据见[逐帧追踪](../../performance/evidence/frame-timeline/2026-10-12/README.md)。
没有修改生产控件、历史参考、CPU 策略或 Qt 安装，也没有把未生效的 Qt 定时器替换实验计入结论。

### Qt 定时精度续查

- [x] 重建独立 UNIX 分发器诊断库，八次运行均用函数入口标记确认替换生效。
- [x] 原仪表/开关基准串行 ABBA 对照；仪表取整组 P95 16.740077 / 16.722756 ms，精确组 16.237982 / 16.220062 ms。
- [x] 保存候选 Qt 补丁与五项边界检查；原 SDK 两项精度检查失败、候选五项通过。补丁可应用性检查通过，SDK 未改动。
- [x] 修复正式脚本继承未纳入指纹的诊断参数：新增 24 组合回归先红后绿，相关 7 项契约与审计通过，独立审查无阻断项。
- [x] 代码单独提交 `1cf79f1`；证据与执行记录随后提交，主 README 最后修改。

详见[Qt 定时精度证据](../../performance/evidence/qt-timer-precision/2026-10-12/README.md)。
这次仅在 UNIX 诊断环境确认改善；默认 GLib 仍取整，没有重新运行默认正式基准，
不能标记正式门禁已修复。候选补丁尚未完成 SDK 和跨平台构建验证。
后续评估 GLib 等待精度或独立 UNIX 配置；四份参考缺失及历史环境指纹差异继续保留。
