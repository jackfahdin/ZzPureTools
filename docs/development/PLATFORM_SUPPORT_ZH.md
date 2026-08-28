# 平台支持与验收状态

## 状态词汇

平台记录只允许以下三个状态：

- `未执行`：没有可接受的对应环境结果。
- `静态验证通过`：对应原生 runner 已完成配置、构建、CTest 和二进制检查，但没有签署的交互验收。
- `真机验收通过`：原生 runner 和人工 checklist 都完成，人员、日期、设备、结果与证据齐全，且没有未解决的发布阻断问题。

自动化只能将一行从 `未执行` 提升为 `静态验证通过`。提升到 `真机验收通过` 必须由测试人员依据对应人工清单签署；另一 OS、另一 ABI、另一窗口系统或模拟显示结果不能代替。

## 软件材质背景语义

`ZzWindowBackdrop::Automatic` 在 Linux 当前使用 `ZzWindowKit` 内部的软件材质层。该层只在宿主窗口内部绘制基于 `QPalette::Window` 的固定低频纹理，不采样桌面、不读取桌面根窗口，也不依赖 X11、Wayland、Windows 或 macOS 原生句柄。因此它是轻量的宿主内软件材质，不等价于系统 Mica、Acrylic 或 Blur。

Windows 的原生 Mica/Acrylic、macOS 的原生 Blur 仍由现有私有 QWindowKit 后端优先处理；只有 `Automatic` 的原生路径失败时才允许进入同一软件 fallback。显式请求 `Blur`、`Acrylic`、`Mica` 或 `MicaAlt` 不会静默降级。Linux 对显式系统材质继续返回 `Unsupported`，调用方应根据能力和返回状态决定界面策略。

软件层不增加原生能力位，不使用 Qt Private、平台 native API、`QGraphicsEffect` 或每帧动画。高对比度、无障碍或性能受限场景由调用方明确请求 `ZzWindowBackdrop::None`，项目不通过平台私有设置猜测用户意图。

软件材质性能基准目标为 `benchmark.backdrop`，报告包含 `enable-time`、`frame-time`、`rebuild-time` 和 `object-count`，并记录 Qt、编译器、显示平台、DPR、GPU 身份、提交和 preset。当前已完成基准本体与 schema 验证，尚未建立独立版本化基线，也没有把该场景接入正式回归门禁；offscreen/Xvfb 报告不能替代真实桌面交互验收。

## Linux 发布参考环境登记

当前只有一台可用机器，因此项目选用 `local-release-xvfb` 作为活动 Linux 发布参考环境。它是 Ubuntu 26.04.1、Qt 6.11.1、GCC 15.2.0 的本机档案，已经保存 Xvfb 性能基线和四个 GCC 发布组合的自动测试记录。主机没有物理显示器，所以这些记录不能提升下方 KDE/GNOME 真机会话行。

原规划的 `ubuntu2204-github-ci` 继续作为 Ubuntu 22.04 immutable 性能参考档案保留，
等待受审核镜像或替代主机后独立验证。continuous build 使用 Ubuntu 22.04 GitHub 托管
runner 生成 AppImage，但托管镜像、CPU 和负载会变化，因此该构建记录不能填充或更新
这份性能档案。两个档案的工具链、显示指纹和性能 JSON 不得混用，切换活动性能档案
前必须提交新档案的独立证据。

| 档案 | 角色 | 结构化记录 | 性能记录 |
|---|---|---|---|
| `local-release-xvfb` | 当前活动本机发布参考环境 | `docs/performance/profiles/local-release-xvfb.json` | `docs/performance/reference/linux/` |
| `ubuntu2204-github-ci` | 保留的 immutable 性能/替代主机参考环境 | `docs/performance/profiles/ubuntu2204-github-ci.json` | 独立验证后新增，不覆盖本机记录 |

这项选择只说明当前从哪台机器执行 Linux 自动发布门禁，不解除项目许可证、第三方来源、Windows/macOS 原生 runner 或三平台人工交互证据要求。

## 标准 Qt 控件覆盖边界

`ZzFluentUI` 对标准 Qt Widgets 采用应用级 `ZzFluentStyle` 覆盖视觉层，
而不是为每个 Qt 控件复制一个同义 `Zz` 状态机。当前合同测试覆盖复选框、单选框、
滑块、单行和多行文本、组合框、进度条、LCD 数字、列表/表格/树视图、菜单栏、
工具栏和状态栏的原生模型、选择、键盘、弹出、禁用、焦点、RTL 与无障碍语义。
标准控件的主题和 DPR 视觉场景使用 `fluent.screenshot-*` 中的
`standard-breadth-light`、`standard-breadth-dark` 和
`standard-breadth-high-contrast` 基线；它们与其他截图场景共用同一套
`ZzFluentScreenshotTest`、文字遮罩和像素比较规则。

Linux 是这些合同的实际构建与自动测试平台。Windows MSVC、Windows MinGW 和
macOS 当前只进行静态检查；offscreen/Xvfb 能证明绘制和公共 Qt API 契约，不能替代
真实窗口系统中的鼠标命中、焦点、原生菜单行为或 DPI 交互验收。

## 工作区组件平台边界

2026-08-21 的工作区组件验证在 Linux `offscreen` 环境执行，使用 GNU 15.2、
Qt 6.11.1、Release/shared/LTO。它覆盖四档 DPR 截图、公开头编译、安装消费者、
ArchitectureAudit 与 `ZzWorkspaceShell` 的布局/所有权测试；该结果仅证明 Qt
公共 API 和软件渲染路径，不提升任一真实桌面会话状态。

2026-08-26 的 Task 15 本机运行来自远程 TTY（`Remote=yes`、`Type=tty`）和专用
Xvfb。统一 Linux gate 的 Debug 配置、151 个 CTest、shared/static/LTO、ASan/UBSan、
clang-tidy 和工作区性能检查均已完成；结果补充了可审计的 GCC/Clang 与性能证据，但
不满足本地活动桌面与物理输出要求。Linux 五种交互行、真实 tab 拖放五区 Overlay
截图、IME 与重启恢复仍为“未执行”；Ubuntu 22.04 兼容档案也继续等待独立 immutable
image 验证。延迟 Side 面板三轮 startup/idle 原始报告见
`docs/performance/evidence/deferred-side-panel/2026-08-26/`。

Windows MSVC、Windows MinGW 和 macOS 对工作区组件均为“未执行”：没有在对应
SDK、ABI 或原生窗口系统上配置、构建或运行消费者。现有 preset、公共头和
ArchitectureAudit 只提供源码/静态合同，不能替代这些原生记录。

## GitHub 托管 CI 状态

`.github/workflows/ci.yml` 现在定义五个平台的 continuous build：Ubuntu 22.04
x86_64、Windows Server 2022 MSVC/Qt MinGW x86_64，以及 macOS 15 arm64/x86_64。
固定下载页为
<https://github.com/jackfahdin/ZzPureTools/releases/tag/continuous-build>。它是滚动更新、
未签名的 `Pre-release`，只有五个平台同一提交的构建、CTest、部署审计和 smoke 全部
通过后才更新。

五个发布平台 ID 和自动证据分别为：

- `linux-x86_64`：AppImage，执行 Ubuntu 22.04 ELF/runtime 审计与 Xvfb smoke。
- `windows-msvc2022-x86_64`：MSVC 2022 ZIP，执行 PE/runtime 审计与 offscreen smoke。
- `windows-mingw-x86_64`：Qt MinGW ZIP，执行 PE/runtime 审计与 offscreen smoke。
- `macos-arm64`：Apple Silicon DMG，执行 Mach-O 审计、DMG 挂载与 smoke。
- `macos-x86_64`：Intel DMG，执行 Mach-O 审计、DMG 挂载与 smoke。

每个平台包都有相邻 `SHA-256` 和独立 build info，发布 job 先验证五组文件属于同一
commit。首次完整工作流结果尚未登记，因此下方目标平台行保持原状态。CI smoke 不等于真机验收；
Windows Server 2022 不替代 Windows 10/11，Ubuntu Xvfb 不替代
KDE/GNOME X11/Wayland，macOS 托管 runner 也不提供已签署的真实显示器交互证据。
远端通过后只能按实际匹配的 OS、架构、ABI 和证据提升“静态验证通过”，不能直接
提升为“真机验收通过”。详细权限、事务和日志处理见
`docs/development/GITHUB_ACTIONS_ZH.md`。

## Windows 矩阵

| ID | 状态 | OS/版本 | 工具链/版本 | Qt 版本/根标识 | 架构 | 链接 | Configure/Build/CTest | 交互结果 | 设备/显示器 | 证据路径 | 日期 | 审核人 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `windows-10-msvc-shared` | 未执行 | Windows 10 22H2 | MSVC 19.38+ | Qt 6.8+ / `QT_MSVC_ROOT` | x86_64 | shared | 未执行 | 未执行 | - | - | - | - |
| `windows-10-msvc-static` | 未执行 | Windows 10 22H2 | MSVC 19.38+ | Qt 6.8+ / `QT_MSVC_ROOT` | x86_64 | static | 未执行 | 未执行 | - | - | - | - |
| `windows-10-mingw-shared` | 未执行 | Windows 10 22H2 | Qt MinGW-w64 GCC 13+ | Qt 6.8+ / `QT_MINGW_ROOT` | x86_64 | shared | 未执行 | 未执行 | - | - | - | - |
| `windows-10-mingw-static` | 未执行 | Windows 10 22H2 | Qt MinGW-w64 GCC 13+ | Qt 6.8+ / `QT_MINGW_ROOT` | x86_64 | static | 未执行 | 未执行 | - | - | - | - |
| `windows-11-msvc-shared` | 未执行 | Windows 11 | MSVC 19.38+ | Qt 6.8+ / `QT_MSVC_ROOT` | x86_64 | shared | 未执行 | 未执行 | - | - | - | - |
| `windows-11-msvc-static` | 未执行 | Windows 11 | MSVC 19.38+ | Qt 6.8+ / `QT_MSVC_ROOT` | x86_64 | static | 未执行 | 未执行 | - | - | - | - |
| `windows-11-mingw-shared` | 未执行 | Windows 11 | Qt MinGW-w64 GCC 13+ | Qt 6.8+ / `QT_MINGW_ROOT` | x86_64 | shared | 未执行 | 未执行 | - | - | - | - |
| `windows-11-mingw-static` | 未执行 | Windows 11 | Qt MinGW-w64 GCC 13+ | Qt 6.8+ / `QT_MINGW_ROOT` | x86_64 | static | 未执行 | 未执行 | - | - | - | - |

Windows 静态结果的证据至少引用 `build/gate-evidence/windows-native.log`；真机结果还要引用 `docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md` 中对应系统、DPI、显示器和 ABI 的截图或日志。

## macOS 矩阵

| ID | 状态 | OS/版本 | 工具链/版本 | Qt 版本/根标识 | 架构 | 链接 | Configure/Build/CTest | 交互结果 | 设备/显示器 | 证据路径 | 日期 | 审核人 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `macos-13-arm64-shared` | 未执行 | macOS 13.3+ | Apple Clang 15+ | Qt 6.8+ / `QT_MACOS_ARM64_ROOT` | arm64 | shared | 未执行 | 未执行 | - | - | - | - |
| `macos-13-arm64-static` | 未执行 | macOS 13.3+ | Apple Clang 15+ | Qt 6.8+ / `QT_MACOS_ARM64_ROOT` | arm64 | static | 未执行 | 未执行 | - | - | - | - |
| `macos-13-x86_64-shared` | 未执行 | macOS 13.3+ | Apple Clang 15+ | Qt 6.8+ / `QT_MACOS_X86_64_ROOT` | x86_64 | shared | 未执行 | 未执行 | - | - | - | - |
| `macos-13-x86_64-static` | 未执行 | macOS 13.3+ | Apple Clang 15+ | Qt 6.8+ / `QT_MACOS_X86_64_ROOT` | x86_64 | static | 未执行 | 未执行 | - | - | - | - |

macOS 静态结果的证据至少引用 `build/gate-evidence/macos-native.log`，其中 `lipo` 必须证明每个 probe 只有预期架构；真机结果同时引用签署的 macOS 人工清单。

## Linux 交互矩阵

| ID | 状态 | OS/版本 | 工具链/版本 | Qt 版本/根标识 | 架构 | 链接 | Configure/Build/CTest | 交互结果 | 设备/显示器 | 证据路径 | 日期 | 审核人 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `linux-x11-kde` | 未执行 | 目标 Linux / X11 KDE | GCC 13.1+ 与 Clang 17+ | Qt 6.8+ / `QT_ROOT` | x86_64 | shared + static | 未执行 | 未执行 | - | - | - | - |
| `linux-x11-gnome` | 未执行 | 目标 Linux / X11 GNOME | GCC 13.1+ 与 Clang 17+ | Qt 6.8+ / `QT_ROOT` | x86_64 | shared + static | 未执行 | 未执行 | - | - | - | - |
| `linux-wayland-kde` | 未执行 | 目标 Linux / Wayland KDE | GCC 13.1+ 与 Clang 17+ | Qt 6.8+ / `QT_ROOT` | x86_64 | shared + static | 未执行 | 未执行 | - | - | - | - |
| `linux-wayland-gnome` | 未执行 | 目标 Linux / Wayland GNOME | GCC 13.1+ 与 Clang 17+ | Qt 6.8+ / `QT_ROOT` | x86_64 | shared + static | 未执行 | 未执行 | - | - | - | - |
| `linux-qt-fallback` | 未执行 | 目标 Linux / forced Qt context | GCC 13.1+ 与 Clang 17+ | Qt 6.8+ / `QT_ROOT` | x86_64 | shared + static | 未执行 | 未执行 | - | - | - | - |

Linux 静态结果的证据至少引用 `build/gate-evidence/linux-native.log`。`local-release-xvfb` 的自动性能记录可作为 runner 证据的一部分，但只有真实 KDE/GNOME 会话、物理或可审计远程显示设备和签署的 Linux 人工清单才能完成对应交互行。

## 更新规则

1. 先保存原生 runner 日志，再将对应行改为 `静态验证通过`，填写工具链精确版本、Qt 精确版本或 SDK 标识、构建结果、日志路径、日期和审核人。
2. 在目标设备执行对应清单。每项填写实际结果、截图或日志和问题链接。
3. 只有清单字段完整、没有 release-blocking 问题，且 runner 日志与同一产物摘要一致时，才能将该行改为 `真机验收通过`。
4. 环境、Qt ABI、架构、窗口系统或链接方式变化后新增证据；不得修改旧证据使其看起来属于新环境。
