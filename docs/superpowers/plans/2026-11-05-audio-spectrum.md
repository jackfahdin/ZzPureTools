# 音频频谱实现计划

> **面向 AI 代理的工作者：** 使用 subagent-driven-development 执行，并在各阶段完成后进行规格及质量审查。用户已批准主线线性提交，不推送。

**目标：** 交付可复用的 KissFFT 频谱控件、示例、许可分发和测试证据。

**架构：** 私有无 QWidget 分析器、固定容量输入缓冲、GUI 动画与渲染三层；公开 ZzAudioSpectrum 采用 PIMPL。库不依赖 QtMultimedia。

**技术栈：** Qt 6.8+、C++20、KissFFT 131.1.0、CMake、Qt Test。

## 1. 依赖与许可证

文件：`ZzThirdParty/kissfft/`、`ZzFluentUI/CMakeLists.txt`、
`cmake/ZzInstallPackage.cmake`、`scripts/package/StageRuntimeLicenses.cmake`、
`tests/Platform/ZzReleasePackagingSupportContract.cmake`、`docs/third-party/THIRD_PARTY_NOTICES.md`。

- [x] 先扩展打包契约，要求 `licenses/KissFFT/COPYING` 和 `licenses/KissFFT/BSD-3-Clause`，执行确认失败。
- [x] 取官方固定修订核心源文件、版权和 BSD 正文，记录每文件 SHA-256；不使用上游应用的裁剪副本。
- [x] 私有对象编译并嵌入 ZzFluentUI，静态安装不泄漏未导出的 target；隐藏/前缀化符号。
- [x] 构建共享和静态 consumer 并验证许可 staging；通过后提交依赖。

```bash
cmake -DZZ_SOURCE_DIR="$PWD" -DZZ_TEST_ROOT="$PWD/build/spectrum-license-contract" -P tests/Platform/ZzReleasePackagingSupportContract.cmake
cmake --build --preset linux-gcc-debug --target ZzFluentStyleTest --parallel 4
```

## 2. 分析器、输入与控件

文件：`ZzFluentUI/widgets/include/ZzFluentUI/ZzAudioSpectrum.h`、
`widgets/src/ZzAudioSpectrum.cpp`、`widgets/src/private/ZzAudioSpectrumPrivate.{h,cpp}`、
`widgets/src/private/ZzSpectrumAnalyzer.{h,cpp}`、`widgets/src/private/ZzSpectrumInput.{h,cpp}`、
`ZzFluentUI/tests/ZzAudioSpectrumTest.cpp`、`ZzFluentUI/tests/ZzSpectrumAnalyzerTest.cpp`、
`ZzFluentUI/tests/CMakeLists.txt` 和模块 CMake 源清单。

- [x] 建立数值测试：静音、已知正弦/多音、幅度比、DC/Nyquist、反相双声道和不同 FFT 档位；先运行失败再实现分析。
- [x] 建立分包/容量/格式切换测试；先运行失败再实现有界缓冲和合并唤醒。
- [x] 建立控件测试：外部频带模式、主题、峰值保持、隐藏、禁用、超时、减少动态效果、clear 与销毁；先运行失败再实现。
- [x] 绘制验证覆盖浅/深/高对比、彩虹、倒影、峰值、小尺寸与 DPI；检查后台线程不触碰 QWidget。
- [x] 运行专项和已有电平表/样式回归，严格警告通过后提交。

```cpp
ZzFluentUI::ZzAudioSpectrum spectrum;
spectrum.setInputMode(ZzFluentUI::ZzAudioSpectrum::ExternalBands);
spectrum.setAnimationEnabled(false);
spectrum.setBandLevels(QVector<qreal>(spectrum.barCount(), 0.5));
QCOMPARE(spectrum.displayedLevels().size(), spectrum.barCount());
QCOMPARE(spectrum.displayedLevels().front(), 0.5);
spectrum.clear();
QCOMPARE(spectrum.displayedLevels().front(), 0.0);
```

```bash
cmake --build --preset linux-gcc-debug --target ZzAudioSpectrumTest ZzSpectrumAnalyzerTest --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build/linux-gcc-debug -R 'fluent\.(audio-spectrum|spectrum-analyzer|audio-level-meter|style)$' --output-on-failure
```

## 3. 示例、文档和验收

文件：`examples/ZzPureToolsExample/ZzExampleAudioSpectrumPage.cpp`、示例页注册/CMake/
翻译；`docs/development/AUDIO_SPECTRUM_ZH.md`；`docs/performance/` 中独立测量记录。

- [x] 注册独立频谱页，正弦、多音、扫频和静音可运行；暴露 FFT、频带、颜色、倒影、峰值及动画设置。
- [x] 编写示例冒烟验证控件存在、设置生效、输入显示变化及销毁安全。
- [x] 数值测试中加入可单独执行的性能采样，记录默认双声道和最大配置分析/刷新时间、缓冲上限、空闲行为；不改变性能门限。
- [x] 使用 Qt 6.8.3 和 6.11.1 本地可用 SDK 验证，运行共享/静态安装 consumer 和架构/打包契约；完成后提交示例与文档。
- [x] 对全部变更进行独立规格及质量审查，修复问题后重跑受影响测试；核对 git diff --check、提交及干净工作区。

```bash
cmake --build --preset linux-gcc-debug --target ZzPureToolsExample --parallel 4
git diff --check
```

## 执行记录

基线 `fluent.style`、`fluent.audio-level-meter` 已在 Linux Qt 6.11.1 通过。Windows/macOS CI 不在本轮本机验证声明范围内。

2026-11-05 已完成：KissFFT 依赖与许可、分析器／控件、合成信号示例、双版本验证、
共享与静态完整安装消费、使用指南和独立测量记录。规格及质量两阶段复审发现的问题均已修复。
专项测试曾复现清空后的唤醒丢失和断流后旧窗口重放，现均通过。

提交时将依赖许可修正和已有 Qt 6.8 / GCC 15 构建兼容修正独立拆出，保持线性历史。
开发期间使用本机隔离下载的 Qt 6.8.3 SDK；已有 Qt 6.11.1 SDK 未改动。

验收证据与范围见 `docs/performance/AUDIO_SPECTRUM_2026-11-05_ZH.md`。
其中明确记录完整安装已通过，但已有 Development-only 的 ZzLog 包配置遗漏仍存在；
本轮未将该额外检查宣称为通过。Windows/macOS 尚未实跑，未改性能基线或门限。
