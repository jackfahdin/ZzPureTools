# 第三方软件通知

本文档记录 ZzPureToolsPro 当前 QWindowKit 依赖链中可由仓库文件直接确认的版本、版权和许可证证据。这里的“发布包位置”是最终发布包要求的位置；当前开发安装不代表发布许可审查已经完成。

## QWindowKit

- 版本：1.5.1.0。
- 上游：https://github.com/stdware/qwindowkit。
- 许可证：Apache-2.0。
- 版权声明：Copyright (C) 2023-present Stdware Collections；Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)。Linux 原生上下文还包含 Copyright (C) 2025-2027 Wing-summer (wingsummer)。
- 源码证据：`ZzThirdParty/qwindowkit/CMakeLists.txt`、`ZzThirdParty/qwindowkit/LICENSE`、`ZzThirdParty/qwindowkit/src/core/windowagentbase.cpp`、`ZzThirdParty/qwindowkit/src/core/contexts/linuxx11context.cpp`。
- 发布包许可证位置：`share/ZzPureToolsPro/licenses/qwindowkit/LICENSE`。
- 分发说明：共享构建把 QWindowKit 链入 `ZzWindowKit`；静态构建以重命名的私有归档分发，不安装 QWindowKit 头或 CMake package。

## FramelessHelper 实现脉络

- 上游：https://github.com/wangwenx190/framelesshelper。
- 许可证：MIT。
- 版权声明：Copyright (C) 2021-2023 by wangwenx190 (Yuhang Zhao)。
- 源码证据：`ZzThirdParty/qwindowkit/README.md` 说明实现脉络；`ZzThirdParty/qwindowkit/docs/framelesshelper-related.md` 保存 MIT 许可证正文和版权声明；QWindowKit 当前源码文件标注 Apache-2.0。
- 发布包通知位置：`share/ZzPureToolsPro/THIRD_PARTY_NOTICES.md`。
- 分发说明：仓库不包含独立的 FramelessHelper 二进制或归档。其实现脉络的上游来源和再许可关系仍须随 `qwindowkit.upstream-provenance` 一并审核，本文档不把该关系写成已经完成的发布许可结论。

## qmsetup

- 许可证：MIT。
- 版权声明：Copyright (c) Stdware Collections。
- 源码证据：`ZzThirdParty/qwindowkit/qmsetup/LICENSE`。
- 发布包许可证位置：`share/ZzPureToolsPro/licenses/qwindowkit/qmsetup-LICENSE`。
- 分发说明：qmsetup 只参与配置和构建，不作为 ZzPureToolsPro 运行库安装。`ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp` 含有注明修改自 Qt `windeployqt 5.15.2` 的代码；其再分发依据由 `qmsetup.windeployqt-5.15.2-derived-work` 阻塞项跟踪。

## syscmdline

- 版本：1.0.0.0。
- 上游：https://github.com/SineStriker/syscmdline。
- 许可证：MIT。
- 版权声明：Copyright (c) 2023 SineStriker。
- 源码证据：`ZzThirdParty/qwindowkit/qmsetup/src/syscmdline/CMakeLists.txt`、`ZzThirdParty/qwindowkit/qmsetup/src/syscmdline/LICENSE`。
- 发布包许可证位置：`share/ZzPureToolsPro/licenses/qwindowkit/syscmdline-LICENSE`。
- 分发说明：syscmdline 是 qmsetup 构建工具依赖，不作为 ZzPureToolsPro 运行库安装。

## 发布阻塞项

- `qwindowkit.upstream-provenance`：当前 vendor 目录没有可验证的上游 commit，也没有原始归档 SHA-256；发布前必须由来源证据补齐并复核。
- `qmsetup.windeployqt-5.15.2-derived-work`：发布前必须确认注明派生自 `windeployqt 5.15.2` 的本地代码及其再分发依据。

在上述阻塞项消除、项目许可证落地并通过最终许可证安装审计前，不得把当前构建标记为可发布二进制包。
