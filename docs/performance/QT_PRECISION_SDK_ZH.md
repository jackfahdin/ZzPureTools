# Linux Qt 精度修复 SDK：构建与验证

这套脚本将 Qt 6.11.1 的两份定时精度补丁、官方源码包和构建步骤固定下来，
供本地验证与独立 SDK 部署使用。它不会替换已有 Qt，也不会自动更新性能基线。

“可复现”指相同源码、补丁与配置可以重复执行并追溯。编译器、系统开发库和
运行库仍由主机提供，不承诺跨机器二进制逐字相同。当前实测环境是 Linux x86_64，
不能据此宣称 Windows、macOS 或其他 GLib 版本已通过。

## 两种构建配置

| 配置 | 模块 | 用途 |
|---|---|---|
| 默认 | QtBase、QtSvg、QtTools/Linguist | 框架、Example 与定时精度验证 |
| `--with-multimedia` | 默认模块 + QtShaderTools、QtMultimedia | 增加 FFmpeg 解码与 PulseAudio 支持 |

Multimedia 的视频基础设施需要 ShaderTools，即使应用只消费音频也需要构建它。
两种配置必须使用各自的构建目录和安装目录。此工具没有把 Qt 源码编译加入托管 CI；
Linux CI 只运行脚本的轻量行为契约。

## 准备依赖

需要 Bash、Git、curl、tar/xz、SHA-256 工具、flock、CMake、Ninja、pkg-config
及支持 Qt 6.11 的 C/C++ 编译器。GCC 15.2 是本轮实测版本。

Ubuntu/Debian 的开发依赖可参考：

```bash
sudo apt-get install build-essential cmake ninja-build git curl xz-utils pkg-config \
  libglib2.0-dev libfontconfig1-dev libfreetype-dev libegl-dev libgl-dev \
  libx11-dev libx11-xcb-dev libxext-dev libxi-dev libxrender-dev libsm-dev libice-dev \
  libxkbcommon-dev libxkbcommon-x11-dev libxcb1-dev libxcb-cursor-dev \
  libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
  libxcb-render-util0-dev libxcb-shape0-dev libxcb-shm0-dev libxcb-sync-dev \
  libxcb-xfixes0-dev libxcb-xinerama0-dev libxcb-xinput-dev libxcb-xkb-dev \
  libxcb-util-dev libxcb-dri3-dev libxcb-present-dev

# 仅 Multimedia 配置需要：
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev \
  libswresample-dev libswscale-dev libpulse-dev
```

请先核对发行版提供的版本；脚本不会执行上述系统安装命令。
使用已经解包的开发依赖时，可传 `--dependency-prefix /path/to/dependencies/usr`。
解包前缀中的链接文件必须能解析到匹配的库；仅复制头文件不够。
不要把其他 Qt 安装当作开发依赖前缀，以免混用 SDK。

## 构建

从框架仓库根目录执行，安装目录须位于仓库外，且第一次使用时为空：

```bash
export ZZ_SDK_WORK="$HOME/.cache/ZzPureTools/qt-6.11.1-precision-base"
export ZZ_SDK_PREFIX="$HOME/.local/share/ZzPureTools/qt-6.11.1-precision-base"

bash scripts/sdk/build-qt-precision-sdk.sh \
  --work-dir "$ZZ_SDK_WORK" --prefix "$ZZ_SDK_PREFIX" \
  --cc /usr/bin/gcc-15 --cxx /usr/bin/g++-15 --jobs 8
```

构建包含 Multimedia 的版本时，换一组全新的路径并追加 `--with-multimedia`：

```bash
export ZZ_SDK_WORK="$HOME/.cache/ZzPureTools/qt-6.11.1-precision-multimedia"
export ZZ_SDK_PREFIX="$HOME/.local/share/ZzPureTools/qt-6.11.1-precision-multimedia"

bash scripts/sdk/build-qt-precision-sdk.sh \
  --work-dir "$ZZ_SDK_WORK" --prefix "$ZZ_SDK_PREFIX" \
  --cc /usr/bin/gcc-15 --cxx /usr/bin/g++-15 --jobs 8 --with-multimedia
```

源码下载自 Qt 官方归档，通过仓库中
[`qt-6.11.1.sha256`](../../scripts/sdk/qt-6.11.1.sha256) 校验。
两份补丁也校验固定摘要，并只应用到新解包的源码。
GLib、XCB、xkbcommon-x11 和 XCB EGL 是必需功能；GLX 插件不构建。
Multimedia 配置强制要求 FFmpeg/PulseAudio，缺依赖时配置失败，不静默退化。

加 `--prepare-only` 可以仅下载、校验并应用补丁；它不表示 SDK 已构建完成。
如果在成功构建之后又运行准备模式，最后一次尝试不再是完整构建；重新运行完整命令后再验收。
离线使用时，在首次运行前把匹配摘要的源码包放到 `$ZZ_SDK_WORK/downloads/`。

## 失败与重跑

- 同一配方重复运行会校验已有源码并增量构建。可以调整 `--jobs`，或去掉 `--prepare-only` 继续构建。
- 配方绑定安装路径、构建路径、依赖前缀、编译器身份、脚本与源码锁定文件。脚本升级、切换模块或配置时使用新目录。
- 未识别的已有安装和构建目录会被拒绝，不覆盖；同一目录的并发运行也会被拒绝。
- 缓存摘要错误时保留错误文件并失败。检查下载来源后，将对应文件移走，再重新运行。
- 源码被手工修改后拒绝继续；使用新工作目录，避免把未记录的修改混入 SDK。
- 中断和失败保留日志与中间文件。每次尝试使用独立日志目录，只有全部模块安装成功才写入该次 `complete` 标记。

日志位于 `$ZZ_SDK_WORK/logs/run-*/`：命令、配方、源码摘要、各模块配置/构建/安装日志、
CMakeCache、环境信息与 Qt 动态库摘要。系统依赖不是源码锁文件的一部分；
归档构建时应同时保留开发包版本与摘要、实际动态依赖信息。

## 原生测试与框架音频验收

先验证 Qt 定时器和动画测试（包含默认 GLib 与 no_glib 路径）：

```bash
bash scripts/sdk/verify-qt-precision-sdk.sh \
  --sdk-work-dir "$ZZ_SDK_WORK" --prefix "$ZZ_SDK_PREFIX" --jobs 8
```

验收脚本要求最后一次构建成功，核对安装库摘要，并把结果写入
`$ZZ_SDK_WORK/verification/run-*/`。采用本地依赖前缀时，验证命令同样追加
`--dependency-prefix /path/to/dependencies/usr`。

验证 Multimedia 时，先用新 SDK 配置一个独立框架构建目录：

```bash
cmake -S . -B build/linux-qt-sdk-audio -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/gcc-15 -DCMAKE_CXX_COMPILER=/usr/bin/g++-15 \
  -DCMAKE_PREFIX_PATH="$ZZ_SDK_PREFIX" -DZZ_QT_PREFIX="$ZZ_SDK_PREFIX" \
  -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON \
  -DZZ_EXAMPLE_AUDIO_PLAYBACK=ON -DZZ_WARNINGS_AS_ERRORS=ON

bash scripts/sdk/verify-qt-precision-sdk.sh \
  --sdk-work-dir "$ZZ_SDK_WORK" --prefix "$ZZ_SDK_PREFIX" --jobs 8 \
  --with-multimedia --framework-build "$PWD/build/linux-qt-sdk-audio"
```

如果系统缺少 XKB 开发文件，框架配置还需要实际的 `XKB_INCLUDE_DIR` 和 `XKB_LIBRARY`；
不要照抄另一机器的路径。验收会构建 Example 和三个音频测试，逐项确认测试存在，
启用 `ZZ_REQUIRE_AUDIO_PLAYBACK=1`。专项模式下，无媒体后端或无法获得真实 WAV
解码帧均失败，不能以跳过代替通过。普通 CI 仍保留音频后端可选的原行为。
验收还核对实际 CMake 包目录、框架源码目录，以及原生测试、音频测试和 Example
加载的每个 Qt 动态库路径；运行时清除外部库和插件搜索路径覆盖，防止验证到另一套 Qt。

真实音频测试使用临时生成的三秒双声道 PCM WAV，静音播放，检查左右声道电平、
隐藏页面暂停以及切回模拟输入。不需要用户下载音频；它验证解码与组件接入，
不替代声卡出声、麦克风或蓝牙设备的人工验收。

## 与活动性能基线的关系

新的模块、SDK 路径、系统库或编译结果都可能改变环境指纹。
这套脚本不复制旧 profile 摘要，不自动重采样或启用新基线。
当前活动基线仍使用之前验收的独立 SDK；重建 SDK 的结果应先独立验证，
需要迁移基线时再完整采集与比较。

本轮完整构建、重跑、Qt 原生测试、音频专项及负向检查的原始记录见
[2026-10-13 验证证据](evidence/qt-sdk-reproducible/2026-10-13/README.md)。
