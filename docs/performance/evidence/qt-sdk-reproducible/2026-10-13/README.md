# Qt SDK 构建复现与 Multimedia 验收 · 2026-10-13

从锁定摘要的 Qt 官方源码包完整构建独立 SDK，并使用该 SDK 编译框架和 Example。
本轮源码身份为 `aa003170c7aed4146467987b47d847aec351c3ee`，构建入口提交为
`43e96cb`；后续只整理文档与证据。执行方式见[构建指南](../../../QT_PRECISION_SDK_ZH.md)。

## 结果

| 检查 | 结果 | 证据 |
|---|---|---|
| SDK 完整构建 | QtBase、QtSvg、QtTools/Linguist、QtShaderTools、QtMultimedia 成功安装 | [完整日志](sdk-build.log)、[命令](sdk-commands.txt) |
| 配方重跑 | 五个模块全部 `ninja: no work to do`，重新安装成功 | [重跑日志](resume-build.log) |
| Qt 原生回归 | 7/7 CTest；498 个检查通过、2 个 Windows 专用检查跳过 | `test-qtimer.log`、`test-qchronotimer.log`、`test-qeventdispatcher.log`、`test-qabstractanimation.log` |
| 框架与 Example | Release / GCC 15.2，项目告警视为错误，构建通过 | [配置](framework-configure.log)、[构建](framework-build.log) |
| 音频专项 | 3/3 CTest；13 个检查通过，0 跳过 | [音频日志](audio-tests.log) |
| 脚本行为与文档 | 4/4 CTest 通过 | [日志](script-ctest.log) |
| SDK 身份 | 原生测试、音频测试及 Example 的 Qt 动态库均来自指定前缀 | `ldd-*.log`、[Example 依赖](framework-ldd.log) |

Qt 上游 `QEventDispatcher_no_glib::postEventFromEventHandler` 保留预期 XFAIL，
没有把它改写为普通成功。两项 skip 均为 QChronoTimer 的 Windows 专用用例；
真实音频用例没有跳过。Offscreen 后端的 `propagateSizeHints` 警告不影响电平断言。

## 环境与输入

- 主机：Ubuntu 26.04.1 / Linux x86_64 / GCC 15.2 / GLib 2.88。
- Qt：6.11.1，Release/shared；GLib、XCB、XCB EGL 开启，GLX 插件关闭。
- 音频：Qt FFmpeg 后端，系统 FFmpeg 8.0.1-3ubuntu2，PulseAudio 支持开启。
- 新安装：`/home/zz/.local/share/ZzPureTools/qt-6.11.1-repro-full-yInNRcEC`。
- 构建目录：`/home/zz/.cache/ZzPureTools/qt-6.11.1-repro-yInNRcEC/final-build`。
- 框架构建：隔离 worktree 的 `build/linux-qt-sdk-audio`。

源码摘要见 [sources.sha256](sources.sha256)，配方和补丁摘要见
[sdk-recipe.txt](sdk-recipe.txt)，库摘要见 [libraries.sha256](libraries.sha256)。
基础 XCB/xkbcommon 开发依赖复用之前的解包内容，其摘要见
[原依赖记录](../../qt-precision-sdk/2026-10-12/dependency-packages.sha256)。
新增加的音频开发包及匹配运行包仅解包到任务目录，摘要见
[multimedia-dependencies.sha256](multimedia-dependencies.sha256)；未执行系统安装。
运行时实际依赖路径由 ldd 日志记录，媒体后端版本由真实解码日志记录。

## 失败与防误判验证

1. 初次构建漏掉 QtShaderTools，Multimedia 配置失败，见
   [原失败](initial-multimedia-configure-failure.log)。补齐锁定模块后，使用另一套
   全新构建与安装目录从头验证，没有把原失败标记成成功。
2. 临时 `qt.conf` 隔离媒体插件，只保留 offscreen 平台插件。
   [修改前](audio-strict-before.log) 无后端时 skip、退出 0；
   [严格模式修改后](audio-strict-after.log) fail、退出 1；恢复插件后
   [原安装 SDK 正向验证](audio-strict-positive-stock.log) 通过。
   该负向测试验证门禁行为；最终新 SDK 的真实解码证据是 `audio-tests.log`。
3. 目录保护测试覆盖已有安装 sentinel、普通锁文件、锁文件符号链接和坏下载缓存。
   审查发现锁打开方式原先会截断文件，先复现后改为只读预检查与非截断锁。
   [最终行为检查](build-contract.log) 通过。
4. [运行身份契约](runtime-contract.log) 实际编译带不同 RPATH 的临时动态库程序：
   正确前缀通过，错误 Qt 路径被拒绝，外部 `LD_LIBRARY_PATH` 不影响验收；
   “进程成功但播放 skip”的日志也被拒绝。

## 验证边界

本轮证明构建脚本、原生定时行为、框架编译与真实音频解码可用，未重跑完整
性能门禁或全量框架回归，也没有验证声卡出声、麦克风、蓝牙或其他平台。
音频由测试生成三秒双声道 PCM WAV 并静音播放，检查独立电平、隐藏暂停与模拟切换。

原安装 Qt 和之前的精度 SDK 保持原样；活动性能 profile 与 16 份基线未改。
新 SDK 的模块和二进制身份不同，不得直接套用旧 profile 摘要。
这是本机验收，尚不代表本轮新增工具的远端 CI 结果。
