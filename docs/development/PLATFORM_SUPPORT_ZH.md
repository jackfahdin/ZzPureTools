# 平台支持与验收状态

## 状态词汇

平台记录只允许以下三个状态：

- `未执行`：没有可接受的对应环境结果。
- `静态验证通过`：对应原生 runner 已完成配置、构建、CTest 和二进制检查，但没有签署的交互验收。
- `真机验收通过`：原生 runner 和人工 checklist 都完成，人员、日期、设备、结果与证据齐全，且没有未解决的发布阻断问题。

自动化只能将一行从 `未执行` 提升为 `静态验证通过`。提升到 `真机验收通过` 必须由测试人员依据对应人工清单签署；另一 OS、另一 ABI、另一窗口系统或模拟显示结果不能代替。

## Linux 发布参考环境登记

当前只有一台可用机器，因此项目选用 `local-release-xvfb` 作为活动 Linux 发布参考环境。它是 Ubuntu 26.04、Qt 6.11.1、GCC 15.2.0 的本机档案，已经保存 Xvfb 性能基线和四个 GCC 发布组合的自动测试记录。主机没有物理显示器，所以这些记录不能提升下方 KDE/GNOME 真机会话行。

原规划的 `ubuntu2204-github-ci` 继续作为 Ubuntu 22.04 兼容参考档案保留，等待用户上传 GitHub 或购置替代主机后独立验证。它不再是当前本机自动发布门禁的前置条件；提供受审核 immutable image 后，Linux runner 会追加兼容检查。两个档案的工具链、显示指纹和性能 JSON 不得混用，切换活动档案前必须提交新档案的独立证据。

| 档案 | 角色 | 结构化记录 | 性能记录 |
|---|---|---|---|
| `local-release-xvfb` | 当前活动本机发布参考环境 | `docs/performance/profiles/local-release-xvfb.json` | `docs/performance/reference/linux/` |
| `ubuntu2204-github-ci` | 保留的未来 CI/替代主机参考环境 | `docs/performance/profiles/ubuntu2204-github-ci.json` | 独立验证后新增，不覆盖本机记录 |

这项选择只说明当前从哪台机器执行 Linux 自动发布门禁，不解除项目许可证、第三方来源、Windows/macOS 原生 runner 或三平台人工交互证据要求。

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
| `macos-12-arm64-shared` | 未执行 | macOS 12+ | Apple Clang 15+ | Qt 6.8+ / `QT_MACOS_ARM64_ROOT` | arm64 | shared | 未执行 | 未执行 | - | - | - | - |
| `macos-12-arm64-static` | 未执行 | macOS 12+ | Apple Clang 15+ | Qt 6.8+ / `QT_MACOS_ARM64_ROOT` | arm64 | static | 未执行 | 未执行 | - | - | - | - |
| `macos-12-x86_64-shared` | 未执行 | macOS 12+ | Apple Clang 15+ | Qt 6.8+ / `QT_MACOS_X86_64_ROOT` | x86_64 | shared | 未执行 | 未执行 | - | - | - | - |
| `macos-12-x86_64-static` | 未执行 | macOS 12+ | Apple Clang 15+ | Qt 6.8+ / `QT_MACOS_X86_64_ROOT` | x86_64 | static | 未执行 | 未执行 | - | - | - | - |

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
