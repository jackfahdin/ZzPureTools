# 音频频谱本机验证与测量

日期：2026-11-05。频谱实现提交：`c441672`；依赖为 KissFFT 131.1.0，
固定修订 `8f47a67f595a6641c566087bf5277034be64f24d`。

## 环境与测量边界

本机为 Intel Core i7-14700、Ubuntu 26.04 x86_64、Linux 7.0.0-42-generic，
编译器 GCC 15.2.0。测量使用 Qt 6.8.3 官方 Linux SDK、Release、共享库、LTO 关闭、
严格警告开启。绘制平台为 `offscreen`，800×320 逻辑像素，DPR 1。

未锁定 CPU、频率或隔离桌面负载。以下为描述性微基准，不是 P95、端到端音频延迟，
也不能证明其他硬件或平台达到相同耗时。本轮没有修改性能门限、活动参考基线或历史报告。
QtTest 日志中的 GCC 10.3.1 是官方 Qt 的构建编译器，项目本身使用 GCC 15.2.0。

## 结果

| 路径 | 配置 | 每次耗时 |
| --- | --- | --- |
| 分析器 | 双声道、2048 点、64 频带、功率平均 | 0.027 ms |
| 分析器 | 8 声道、8192 点、256 频带、功率平均 | 0.44 ms |
| 同步更新与绘制 | 64 柱、彩虹＋倒影＋峰值 | 0.66 ms |
| 同步更新与绘制 | 256 柱、彩虹＋倒影＋峰值 | 2.4 ms |

分析基准包括 Hann 窗、逐声道 FFT、功率合成与频带映射，复用已经配置的分析器，
不含 PCM 推送、线程排队或绘制。绘制基准使用外部频带、关闭动画，在隐藏控件上
交替提交两组柱高并 `render()` 到预分配 QImage；包括状态发布和绘制，不含 FFT、
窗口系统合成或计时器调度。两项独立测量不能当作完整帧延迟承诺。

固定样本缓冲容量为 `8 × fftSize × channelCount × sizeof(float)`：默认单声道
64 KiB，示例双声道 128 KiB，最高配置 2 MiB。FFT 工作区与快照另计。
计时器空闲停止、隐藏／禁用停算、最新窗口处理与超时归零由行为测试验证，
没有将这些断言包装成整机 CPU 占用测量。

原始输出：

- [分析器](evidence/audio-spectrum/2026-11-05/qt683-release-analysis.txt)
- [同步更新与绘制](evidence/audio-spectrum/2026-11-05/qt683-release-render.txt)

## 复现

使用独立构建目录，并将 `QT_ROOT` 指向 Qt 6.8.3 SDK：

```bash
cmake -S . -B build/spectrum-qt683-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DQt6_DIR="$QT_ROOT/lib/cmake/Qt6" \
  -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON \
  -DZZ_EXAMPLE_AUDIO_PLAYBACK=OFF -DZZ_WARNINGS_AS_ERRORS=ON
cmake --build build/spectrum-qt683-release --target \
  ZzAudioSpectrumTest ZzSpectrumAnalyzerTest ZzExampleAudioSpectrumTest \
  ZzPureToolsExample --parallel 4

build/spectrum-qt683-release/ZzFluentUI/tests/ZzSpectrumAnalyzerTest \
  analysisBenchmark -minimumtotal 300 -median 5
QT_QPA_PLATFORM=offscreen ZZ_SPECTRUM_BENCHMARK=1 \
  build/spectrum-qt683-release/ZzFluentUI/tests/ZzAudioSpectrumTest \
  directRenderBenchmark -minimumtotal 300 -median 5
```

Qt 6.8.3 SDK 在本机隔离安装于 `build/spectrum-qt683-sdk/6.8.3/gcc_64`；包含
qtbase、qtsvg、qtdeclarative、qttools 与对应 ICU。该构建没有 QtMultimedia，
验证了频谱库和合成信号示例不依赖音频设备或多媒体模块。

## 验证结果与修复

| 验证 | 结果 |
| --- | --- |
| Qt 6.11.1 / Debug / GCC 严格构建 | 频谱、分析器、样式、电平表、Example、工作区冒烟均通过 |
| Qt 6.8.3 / Release / GCC 严格构建 | 频谱、分析器、样式、电平表、Example 均通过 |
| 示例回归 | 两个版本的中英文集成冒烟、100/125/150/200% 截图通过；基线和门限未改写 |
| 架构与打包 | 完整架构审计、FluentUI 边界、运行许可 staging 契约通过 |
| 共享 SDK | 完整安装、公开头独立编译、安装消费者 2/2 通过，实际 PCM→FFT 显示检查通过 |
| 静态 SDK | 独立 Release 构建、完整安装、安装消费者 2/2 通过，同样执行实际 FFT |
| 私有依赖 | 共享库不导出 FFT 符号；静态库 8 个入口均有 `zz_fluent_` 前缀，无原名全局符号 |
| 许可分发 | Runtime 和 Development 均有完整 KissFFT 许可与来源记录；静态完整安装逐文件核验一致 |

独立复审与回归测试覆盖并修复了：外部频带直接模式的刷新合并、清空或重新配置后的
唤醒丢失、输入断流后少量新数据重放旧频谱、峰值保持时间初始化、隐藏事件停表以及
倒影自定义透明度。输入测试还覆盖奇数分包、跨声道半帧、超大输入尾部对齐、反相双声道、
NaN/Inf 和工作线程推送；渲染测试覆盖实时主题切换、彩虹、峰值帽、倒影、DPR 2 和窄尺寸。

Qt 6.8 与 GCC 15 严格构建发现已有 TabBar 的临时 QVariant 转换触发 `-Warray-bounds`，
已改用等价的 const 左值转换，继续保留 `-Werror`。完整架构审计中已有的颜色豁免位置
也同步到当前代码行，未扩大豁免范围。

安装测试使用完整 `cmake --install`。额外尝试仅安装 `Development` 组件时，已有
ZzLog 包配置属于 `Unspecified` 组件，消费者无法找到 `ZzLogConfig.cmake`；本轮未改变
这一已有分组件打包限制。KissFFT 在 Development 中的许可完整性已单独验证。
当前接入请使用完整安装，不能将本报告视为 Development-only SDK 已通过。

Windows/MSVC、MinGW、macOS 构建及真实显示服务器交互未在本机执行，须由对应平台
验证。本轮只在本地线性提交，不推送或监控远端 CI。
