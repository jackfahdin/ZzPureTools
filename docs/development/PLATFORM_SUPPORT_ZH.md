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

软件材质性能基准目标为 `benchmark.backdrop`，报告包含 `enable-time`、`frame-time`、`rebuild-time` 和 `object-count`，并记录 Qt、编译器、显示平台、DPR、GPU 身份、提交和 preset。该场景已接入绝对预算及相对回归门限配置，但尚缺独立版本化参考报告，因此完整性契约仍阻断发布。offscreen/Xvfb 报告不能替代真实桌面交互验收。

## Linux 发布参考环境登记

当前只有一台可用机器，因此项目选用 `local-release-xvfb` 作为活动 Linux 发布参考环境。它是 Ubuntu 26.04.1、Qt 6.11.1、GCC 15.2.0 的本机档案，已经保存 Xvfb 性能基线和四个 GCC 发布组合的自动测试记录。主机没有物理显示器，所以这些记录不能提升下方 KDE/GNOME 真机会话行。

2026-10-12 复核时，主机 Mesa 已从 26.0.3 更新到 26.0.8，kernel 也发生变化。
按负责人决定保留历史参考，不将新环境报告混入旧基线；相对性能门禁待复核。
活动档案登记不表示当前主机仍满足该档案指纹，详见
[性能复核记录](../performance/CI_PERFORMANCE_REVIEW_2026-10-12_ZH.md)。

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

Linux 有本机合同与截图记录；Windows MSVC、Windows MinGW 和 macOS 的托管构建与
CTest 证据见下方 GitHub CI 状态。offscreen/Xvfb 能证明绘制和公共 Qt API 契约，不能替代
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

上述 8 月本机记录没有 Windows/macOS 的原生构建证据；之后的托管 CI 记录见下节。
下方目标 OS/ABI/链接方式组合没有补齐对应验收证据，仍保留“未执行”。
托管 runner 成功不能直接填入不同系统版本或静态链接组合的验收行。

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
commit。已登记历史完整成功矩阵
[`a9396b1`](https://github.com/jackfahdin/ZzPureTools/actions/runs/38049701780)。
2026-10-12 查询的
[`f0cf1c0`](https://github.com/jackfahdin/ZzPureTools/actions/runs/38211345718)
运行已结束：配置契约成功，五个平台 job 失败，发布跳过。此处只登记 job 结论，
不据此推断每个平台的具体失败原因，也不把历史成功推广到新提交。
CI smoke 不等于真机验收；
Windows Server 2022 不替代 Windows 10/11，Ubuntu Xvfb 不替代
KDE/GNOME X11/Wayland，macOS 托管 runner 也不提供已签署的真实显示器交互证据。
远端通过后只能按实际匹配的 OS、架构、ABI 和证据提升“静态验证通过”，不能直接
提升为“真机验收通过”。详细权限、事务和日志处理见
`docs/development/GITHUB_ACTIONS_ZH.md`。

## 窗口接入自动回归与人工验收

`ZzApplicationStarter` 由安装消费者复制到独立目录，通过 `find_package` 构建，
覆盖自定义标题、内嵌字体图标、主题保存/恢复、双窗口、辅助窗口关闭/重开及退出清理。
`ZzAuxiliaryWindowChrome` 统一自绘关闭按钮；macOS 按 `NativeSystemButtons` 保留
原生系统控件，不承诺隐藏原生最小化和缩放按钮。

Linux offscreen 的自动结果只能证明上述 Qt 层契约。下一次 Windows 真机验收应同时
运行 Starter 与 Example，按以下步骤补充记录；macOS/Linux 继续使用对应平台清单。

| 操作 | 预期与需记录的证据 |
|---|---|
| 两个窗口在 100%/150%/200% 显示器间移动，最大化后还原 | 图标、文字、按钮命中区随 DPI 更新；记录两窗口与设置/关于窗口截图 |
| 悬停最大化按钮并使用 Windows 11 Snap，再拖动还原 | Snap 可触发；标题栏按钮不会误触发窗口拖动 |
| 输入中文并在主窗/辅助窗间切换焦点 | IME 候选位置与焦点正确，关闭辅助窗后主窗可继续输入 |
| Tab/Shift+Tab 遍历，Enter/Space 激活按钮 | 焦点可见、顺序正确，关闭只影响目标窗口 |
| 切换浅色/深色/高对比度，重启 Starter | 两主窗及辅助窗配色同步，保存主题可恢复，标题不回退成框架默认值 |

此表是操作步骤，不是通过记录；签署仍使用
[Windows 清单](../release/MANUAL_WINDOWS_CHECKLIST_ZH.md)、
[macOS 清单](../release/MANUAL_MACOS_CHECKLIST_ZH.md) 和
[Linux 清单](../release/MANUAL_LINUX_CHECKLIST_ZH.md)。

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
