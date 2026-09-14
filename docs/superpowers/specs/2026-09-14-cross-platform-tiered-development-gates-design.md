# 跨平台分层开发门禁设计

**状态：** 已获用户批准，待编写逐文件实施计划。

**目标：** 允许 Linux、Windows 或 macOS 开发机在只具备一个受支持 Qt 工具链时完成可审计的本机验证并提交代码，同时把跨 ABI、跨架构、Sanitizer、视觉、性能、安装和发布验证留给具备对应能力的 CI 或参考机。任何未执行项都必须保持为“待验证”，不能被解释为通过。

## 一、问题与设计原则

当前完整平台脚本面向发布参考环境，要求同一台机器具备多个编译器、shared/static/LTO、clang-tidy、ASan/UBSan、性能档案、截图基线和发布工具。普通开发机通常只具备 Linux GCC、Windows MSVC/MinGW 或当前架构 macOS Qt kit 中的一种，强制运行完整脚本会造成两种错误结果：开发者绕过全部测试，或者把环境缺失误报成代码失败。

本设计将验证拆为三个独立层级：

1. 开发机最小门禁证明当前改动在一个原生受支持工具链上能够严格编译，并通过不依赖参考环境的自动测试。
2. CI 平台门禁证明同一提交在目标 ABI、架构、静态链接、安装和打包组合上成立。
3. 参考机专项门禁证明视觉基线、性能阈值和真实桌面交互满足发布要求。

低层级通过不能替代高层级证据。代码可以在最小门禁通过后提交和推送，但只有要求的 CI 与参考机证据齐全后才能进入发布候选。

## 二、开发机最小门禁

### 2.1 允许的本机工具链

每台开发机只需选择一个实际安装且与 Qt ABI 匹配的 shared preset：

| 平台 | 允许的最小门禁 preset |
|---|---|
| Linux | `linux-gcc-debug` |
| Windows MSVC | `windows-msvc2022-release` |
| Windows MinGW | `windows-mingw-release` |
| macOS Apple Silicon | `macos-clang-release-arm64` |
| macOS Intel | `macos-clang-release-x86_64` |

Windows 开发机不需要同时安装 MSVC 与 MinGW。macOS 开发机不需要为了普通提交安装另一 CPU 架构的 Qt SDK。Linux 开发机不需要为了普通提交同时具备 Clang、静态 Qt、性能参考档案或发布容器。

### 2.2 固定执行内容

开发机最小门禁执行以下步骤，任一步骤失败都不得把本机门禁记录为通过：

1. 对工作区和暂存区分别运行 `git diff --check` 与 `git diff --cached --check`。
2. 使用所选 preset 配置工程，启用测试和 Example，并保持 `ZZ_WARNINGS_AS_ERRORS=ON`。
3. 完整编译当前 preset，确保一方库、测试目标和 Example 都经过本机编译器。
4. 运行普通单元、组件、架构和平台契约测试，排除带有 `benchmark`、`screenshot`、`install`、`packaging` 或 `release` 标签的专项入口。
5. 使用调用方提供的 CTest 正则额外运行与本次改动直接相关的定向测试；生产代码提交必须提供至少一个定向测试正则。
6. 输出操作系统、处理器架构、CMake、Qt、编译器、preset、定向测试正则、排除标签及最终结果。输出只进入终端或被开发者主动保存到忽略目录，不写入源码树中的跟踪文件。

纯文档提交不强制配置和编译，但仍必须执行两类 `git diff --check`，并直接以
`cmake -DZZ_SOURCE_DIR=<源码根目录> -P tests/Architecture/ZzDocumentationAudit.cmake`
运行已有文档审计，不依赖预先存在的构建目录。该模式由脚本显式参数选择，不能
依据扩展名静默猜测。

### 2.3 统一入口

新增两个薄脚本，复用现有 `CMakePresets.json` 和 CTest 标签，不复制编译参数：

- `scripts/ci/run-local-development-gate.sh`：服务 Linux 和 macOS。
- `scripts/ci/run-local-development-gate.ps1`：服务 Windows PowerShell 7。

脚本接受 `--preset`、`--tests` 和可选 `--docs-only`。preset 使用固定允许列表；平台与 preset 不匹配、定向正则为空、工具缺失或原生命令返回非零时，脚本立即失败。`--docs-only` 与构建参数互斥，只运行差异检查和文档审计。

脚本不修改全局 Git 配置、不安装 hook、不下载 SDK、不自动暂存或提交文件，也不生成“跳过即通过”的结果。

## 三、CI 与参考机职责

### 3.1 CI 平台门禁

GitHub CI 继续负责同一提交的目标平台组合：

- Ubuntu 22.04 持续发布构建与 AppImage 审计。
- Windows MSVC 2022 与 Qt MinGW 构建、测试、部署和 ZIP 审计。
- macOS arm64 与 x86_64 构建、测试、架构检查和 DMG 审计。
- 需要时执行 shared/static、LTO、安装消费和包重定位。

某个平台 CI 尚未配置或临时不可用时，该平台保持“待 CI 验证”，不得由另一平台的成功结果替代。

### 3.2 参考机专项门禁

下列结果只由匹配档案的参考环境产生：

- ASan/UBSan 和 clang-tidy 使用具备项目规定 Clang 工具链的 Linux 环境。
- Linux 截图基线只由当前登记的 Linux 视觉参考机生成，必须覆盖 Light、Dark、HighContrast、RTL 和规定 DPR，并在关闭更新模式后重跑。
- 性能报告只由匹配版本化性能档案、CPU 亲和性、显示后端和 GPU 身份的参考机生成。
- 真实窗口交互验收由对应 Linux、Windows 或 macOS 物理/可审计远程桌面完成。

视觉、热路径、生命周期、编译系统、安装接口或平台代码发生变化时，进入发布候选前必须补齐受影响的专项门禁。普通业务文档变更不触发无关的视觉或性能门禁。

## 四、提交与状态语义

生产代码提交正文必须包含：

1. 本机操作系统、Qt、编译器和 preset。
2. 最小门禁与定向测试的实际结果。
3. 未执行的 CI、Sanitizer、视觉、性能和真机项目。

推荐表述：

```text
本机最小门禁已通过：<平台 / Qt / 编译器 / preset>。
定向测试：<CTest 正则及结果>。
待验证：<未执行的目标平台或专项门禁>。
```

禁止使用“全部通过”描述只执行了本机最小门禁的提交。允许提交和推送的状态不等于允许发布；发布脚本仍以现有结构化发布证据为唯一事实源。

## 五、当前指示条问题修复

当前 `e11e964` 引入的生命周期缺陷与视觉基线欠账作为独立修复批次处理，不通过降低门禁或增加 Sanitizer 抑制绕过。

### 5.1 生命周期根因与修复边界

`ZzItemSelectionAnimation` 和 `ZzTabIndicatorAnimation` 的事件过滤器先把 `QPointer<T>` 转换为派生指针，再检查接收事件的对象是否仍属于该派生类型。Qt 析构期间对象已经退化为 `QWidget`、但 `QPointer` 尚未清空，因而触发非法向下转换；标签路径还会继续调用失效的 `QTabBar` 方法。

修复必须：

1. 在任何有类型的 `QPointer` 访问之前，先对事件携带的原始 `QObject *` 执行运行时类型检查。
2. 类型已经退化时直接交还基础事件过滤器，不访问视图、标签、选择模型或动画目标几何。
3. 增加普通视图和标签栏销毁的聚焦回归场景，并在修复前、修复后使用 ASan/UBSan preset 完成 RED/GREEN 验证。
4. 不改变正常隐藏、缩放、布局方向、主题变化、快速切换和 reduced motion 的终态行为。

### 5.2 代码归属和注释

`ZzFluentStylePrivate` 析构实现移回 `ZzFluentStylePrivate.cpp`，该文件显式包含完成删除动画类型所需的私有头；`ZzItemSelectionAnimation.cpp` 只实现同名主要类型。此次新增的复杂逻辑英文注释统一改为简体中文，不改动测试断言和公共 API。

### 5.3 视觉基线

当前 Linux 参考机先生成受影响的实际图和差异图，人工检查指示条、标签文字、内容间距、Activity Bar、Workspace 与 Example。只有确认差异来自已批准的新视觉而非裁切、重叠、残影或状态错误时，才更新对应基线。更新后必须关闭基线更新模式，在四档 DPR 下重新运行 Fluent、Workspace 与 Example 截图入口。

## 六、验证与提交边界

实现分为三个中文提交：

1. 增加分层开发门禁脚本、脚本契约测试和构建文档。
2. 以 TDD 修复选择指示条析构生命周期、代码归属和中文注释。
3. 经人工检查后更新受影响的 Linux 视觉基线，并记录实际截图验证结果。

每个提交只包含对应文件和测试。`logcat.log`、`multi-window-requirements.md`、构建目录、测试报告和临时图片不读取、不暂存、不提交。

最终验收至少包括：

- Linux 本地开发门禁的正向执行，以及非法 preset、缺少定向正则和文档模式参数冲突的负向契约。
- GCC Debug 定向测试和不含环境专项标签的最小矩阵。
- Clang ASan/UBSan 下普通视图与标签栏析构测试通过且没有运行时诊断。
- 四档 DPR 的 Fluent、Workspace 与 Example 截图入口关闭更新模式后通过。
- 安装消费、公开头和架构边界不因私有实现调整退化。
- 工作区只保留任务开始前已经存在的两个未跟踪文件，`master` 不混入其他改动。
