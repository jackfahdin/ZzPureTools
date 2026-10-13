# 独立 Qt SDK 可复现构建与音频验收

> 执行方式：在隔离 worktree 内按任务顺序实施，每项验证后分别提交，README 最后更新。

**目标：** 将已验证的 Qt 6.11.1 定时精度补丁制作成可重复执行的 Linux SDK 构建流程，并验证含 Multimedia 的真实音频解码路径。

**架构：** 下载官方分模块源码包并校验锁定的 SHA-256；逐模块配置、构建、安装到新的外部前缀。基础配置构建 qtbase/qtsvg/qttools，Multimedia 配置增加 QtShaderTools、QtMultimedia 和 FFmpeg + PulseAudio 依赖。构建目录、安装目录绑定同一配方，拒绝覆盖未知安装；保存源码、补丁、工具链与产物摘要。可复现指输入与步骤可追溯，不承诺不同系统的二进制逐字一致。

**技术栈：** Bash、CMake/Ninja、GCC、Qt Test、FFmpeg、PulseAudio。

## 约束与取舍

- 用户已同意先 SDK 脚本再 Multimedia；不更换原 Qt 或当前活动性能基线。
- 选择校验过的官方源码包，避免本地源码拷贝遗漏修改；不用仅固定可移动标签的 git clone。
- 不自动安装系统包；支持明确的额外开发依赖前缀。缺少必要依赖立即失败。
- Linux/x86_64 为本轮实测范围，Windows/macOS 不宣称适用。
- 不在托管 CI 中重新编译 Qt；仅把轻量脚本行为测试接入 Linux CTest。
- 音频验收要求真实 WAV 解码，静音运行；不把这当作声卡出声或麦克风验收。

## 任务 1：受控 SDK 构建入口

文件：`scripts/sdk/build-qt-precision-sdk.sh`、`scripts/sdk/qt-6.11.1.sha256`、`tests/Platform/ZzQtSdkBuildTest.sh`、`tests/Platform/CMakeLists.txt`。

- [x] 先写行为测试：非法参数、危险/已有安装前缀、损坏下载缓存必须非零退出，已有 sentinel 内容不变。运行 `bash tests/Platform/ZzQtSdkBuildTest.sh "$PWD"` 记录缺失实现的失败。
- [x] 实现 `--work-dir`、`--prefix`、`--jobs`、`--dependency-prefix`、`--with-multimedia`、`--prepare-only`；使用数组传参、SHA 校验、目录配方标记和锁，失败不写完成标记。
- [x] 重新跑行为测试，实跑完整 SDK 构建；运行四组 Qt 原生定时/动画测试，7/7 CTest、498 个检查通过，2 个 Windows 专用检查跳过。
- [x] 记录构建清单、SDK 依赖与验证结果，提交脚本和测试（`43e96cb`）。

## 任务 2：Multimedia 与真实音频验收

文件：构建脚本 Multimedia 分支、`examples/ZzPureToolsExample/tests/ZzExampleAudioMeterSourceTest.cpp`、`scripts/sdk/verify-qt-precision-sdk.sh`。

- [x] 为专项音频验收添加严格模式：`ZZ_REQUIRE_AUDIO_PLAYBACK=1` 时，后端不可用或无法解码必须失败；普通跨平台测试保留原可选行为。通过临时 qt.conf 隔离媒体插件，验证无后端从 skip/退出 0 变为 fail/退出 1，恢复插件后真实播放通过。
- [x] 使用独立含 Multimedia 的 SDK 前缀，强制 `FEATURE_ffmpeg=ON`、`FEATURE_pulseaudio=ON`，从新的框架构建目录配置 `ZZ_EXAMPLE_AUDIO_PLAYBACK=ON`。
- [x] 执行 `example.audio-meter-simulation`、`example.audio-meter-playback`、`example.audio-levels`，3/3 CTest、13 个检查通过、0 跳过；检查 Example 和全部测试的实际 Qt 链接路径。
- [x] 提交音频验收与脚本（`aa00317`），保存成功及失败证据。

## 任务 3：文档与交付

文件：`docs/performance/QT_PRECISION_SDK_ZH.md`、`docs/performance/evidence/qt-sdk-reproducible/2026-10-13/`、根 `README.md`。

- [x] 文档列出系统依赖、基础/Multimedia 配方、重跑与故障恢复、证据位置、环境指纹边界。
- [x] 自查安装隔离、下载与补丁完整性、无后端不得假通过；轻量契约和文档审计 4/4 通过，实际构建/音频测试通过。
- [x] README 最后更新，单独提交；记录用户已反馈上一批 master CI 全绿，本轮不监控 CI。

## 实施中发现与调整

- 官方 Multimedia 构建实际要求 ShaderTools；首次失败保留，补充锁定模块后使用另一组全新路径完整重建。
- 审查发现锁文件在归属检查前被截断；先用 sentinel 复现，再调整为只读预检查、符号链接拒绝、非截断锁和持锁复核。
- SDK 验收核对 CMake 实际包目录、源码 checkout 和所有测试/Example 的 Qt 动态库实际路径，避免仅凭记录变量误判。另以日志确认播放用例真正通过、没有跳过。
