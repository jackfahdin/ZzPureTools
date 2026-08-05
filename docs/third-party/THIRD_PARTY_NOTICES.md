# 第三方软件通知

本文档记录 ZzPureToolsPro 当前 QWindowKit 依赖链中可由仓库文件直接确认的版本、版权和许可证证据。这里的“发布包位置”是最终发布包要求的位置；当前开发安装不代表发布许可审查已经完成。

## ZzPureToolsPro

- 许可证：MIT。
- SPDX 结论：`MIT`。
- 版权声明：Copyright (c) 2026 ZzPureToolsPro contributors。
- 源码证据：仓库根 `LICENSE`，其 SHA-256 由 `release-evidence.json` 锁定。
- 发布包许可证位置：`share/ZzPureToolsPro/licenses/PROJECT-LICENSE`。
- 审核状态：许可证正文已经落地；具名项目所有者批准记录仍由 `project.license` 阻塞项跟踪。

## QWindowKit

- 版本：1.5.1.0。
- 上游：https://github.com/stdware/qwindowkit。
- 上游提交：`2813c1f810cb3fb1999a14ad524124562081f2c2`。
- 源码归档 SHA-256：`cd0d3ad3c94ce5c0965337f2e59262613d684f46d6ce0c45613726e751d3d90c`。
- 许可证：Apache-2.0。
- SPDX 结论：`Apache-2.0`；实现脉络中单独保留的 FramelessHelper 通知为 `MIT`。该结论仍受发布证据门禁约束，不替代来源审核。
- 版权声明：Copyright (C) 2023-present Stdware Collections；Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)。Linux 原生上下文还包含 Copyright (C) 2025-2027 Wing-summer (wingsummer)。
- 源码证据：`ZzThirdParty/qwindowkit/CMakeLists.txt`、`ZzThirdParty/qwindowkit/LICENSE`、`ZzThirdParty/qwindowkit/src/core/windowagentbase.cpp`、`ZzThirdParty/qwindowkit/src/core/contexts/linuxx11context.cpp`。
- 发布包许可证位置：`share/ZzPureToolsPro/licenses/qwindowkit/LICENSE`。
- 分发说明：共享构建把 QWindowKit 链入 `ZzWindowKit`；静态构建以重命名的私有归档分发，不安装 QWindowKit 头或 CMake package。

## FramelessHelper 实现脉络

- 上游：https://github.com/wangwenx190/framelesshelper。
- 许可证：MIT。
- SPDX 结论：`MIT`。
- 版权声明：Copyright (C) 2021-2023 by wangwenx190 (Yuhang Zhao)。
- 源码证据：`ZzThirdParty/qwindowkit/README.md` 说明实现脉络；`ZzThirdParty/qwindowkit/docs/framelesshelper-related.md` 保存 MIT 许可证正文和版权声明；QWindowKit 当前源码文件标注 Apache-2.0。
- 发布包通知位置：`share/ZzPureToolsPro/THIRD_PARTY_NOTICES.md`。
- 分发说明：仓库不包含独立的 FramelessHelper 二进制或归档。其实现脉络的上游来源和再许可关系仍须随 `qwindowkit.upstream-provenance` 一并审核，本文档不把该关系写成已经完成的发布许可结论。

## qmsetup

- 版本：1.0.0.0。
- 上游：https://github.com/stdware/qmsetup。
- 上游提交：`bd2ce397ee1400e4a72d3ed8ce6b6baed24baeb4`，由 QWindowKit 固定的 submodule 指针确认。
- 许可证：MIT。
- SPDX 结论：`MIT`。
- 版权声明：Copyright (c) Stdware Collections。
- 源码证据：`ZzThirdParty/qwindowkit/qmsetup/LICENSE`。
- 发布包许可证位置：`share/ZzPureToolsPro/licenses/qwindowkit/qmsetup-LICENSE`。
- 分发说明：qmsetup 只参与配置和构建，不作为 ZzPureToolsPro 运行库安装。`ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp` 含有修改自 Qt 5.15.2 `qttools/src/shared/winutils/utils.cpp` 的代码；Qt 原文件适用 GPLv3 加 Qt GPL Exception，不能被 qmsetup 顶层 MIT 声明覆盖，其再分发依据由 `qmsetup.windeployqt-5.15.2-derived-work` 阻塞项跟踪。

## syscmdline

- 版本：1.0.0.0。
- 上游：https://github.com/SineStriker/syscmdline。
- 上游提交：`0c9f3de8b11bd2f33b03bea5521bf446af4ead69`，由 qmsetup 固定的 submodule 指针确认。
- 许可证：MIT。
- SPDX 结论：`MIT`。
- 版权声明：Copyright (c) 2023 SineStriker。
- 源码证据：`ZzThirdParty/qwindowkit/qmsetup/src/syscmdline/CMakeLists.txt`、`ZzThirdParty/qwindowkit/qmsetup/src/syscmdline/LICENSE`。
- 发布包许可证位置：`share/ZzPureToolsPro/licenses/qwindowkit/syscmdline-LICENSE`。
- 分发说明：syscmdline 是 qmsetup 构建工具依赖，不作为 ZzPureToolsPro 运行库安装。

## ZzLog、spdlog 与 fmt

- ZzLog：版本 0.1.0，项目内日志封装；发布包许可证位置为 `share/ZzPureToolsPro/licenses/ZzLog/LICENSE`。
- spdlog：来源 `https://github.com/gabime/spdlog.git`，导入修订 `d24088deaa441a79267df8ae3dbc567fbe2a5e03`，声明版本 2.0.0（未发布开发分支），SPDX 结论为 `MIT`；发布包许可证位置为 `share/ZzPureToolsPro/licenses/ZzLog/spdlog-LICENSE.txt`。
- fmt：来源 `https://github.com/fmtlib/fmt`，版本 12.1.0，源码归档 SHA-256 为 `ea7de4299689e12b6dddd392f9896f08fb0777ac7168897a244a6d6085043fea`，SPDX 结论为 `MIT`；发布包许可证位置为 `share/ZzPureToolsPro/licenses/ZzLog/fmt-LICENSE.txt`。
- 版本和源码身份同时记录于 `ZzThirdParty/ZzLog/DEPENDENCIES.md` 及对应 vendored 源码；正式发布仍须经过安装许可证审计。

## GNU C++ 运行库

- Linux 发布构建可从经过审核的 Ubuntu 22.04 不可变构建镜像中随包安装 `libstdc++.so.6` 和 `libgcc_s.so.1`。
- 运行库来源和版本由该镜像内选定的 GCC 13.1+ 编译器 `-print-file-name` 结果确定，禁止从执行检查的宿主系统临时选择。
- libstdc++ 与 libgcc 适用 GPL-3.0-or-later with GCC Runtime Library Exception；发布包许可证位置为 `share/ZzPureToolsPro/licenses/gcc-runtime/COPYING3` 和 `share/ZzPureToolsPro/licenses/gcc-runtime/COPYING.RUNTIME`。
- SPDX 结论：`GPL-3.0-or-later WITH GCC-exception-3.1`。
- 仅 `ZZ_BUNDLE_GNU_RUNTIME=ON` 的 Linux GNU Release shared 包包含上述两个运行库；Ubuntu 22.04 门禁必须验证实际加载的是包内文件。

## 发布阻塞项

- `qwindowkit.upstream-provenance`：上游 commit、子模块 commit 和归档 SHA-256 已固定；发布前仍须由具名审核人签署来源审核记录。
- `qmsetup.windeployqt-5.15.2-derived-work`：上游源码、许可证和本地摘要已固定；发布前仍须由具名审核人签署 GPLv3 加 Qt GPL Exception 下的再分发结论。
- `project.license`：仓库根 `LICENSE` 和 `MIT` SPDX 表达式已经落地；发布前仍须提供具名项目所有者批准记录。正式包中的固定位置为 `share/ZzPureToolsPro/licenses/PROJECT-LICENSE`。

在上述阻塞项消除、项目许可证具名批准记录完成并通过最终许可证安装审计前，不得把当前构建标记为可发布二进制包。
