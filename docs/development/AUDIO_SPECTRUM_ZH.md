# 音频频谱 ZzAudioSpectrum

`ZzAudioSpectrum` 显示声音的频率分布；`ZzAudioLevelMeter` 显示各声道的整体电平。
频谱控件内置私有 KissFFT 分析器，接收 PCM 或外部频带，不打开设备、不解码文件，
也不要求链接 QtMultimedia。Example 的「音频频谱(AudioSpectrum)」页可直接运行
正弦、多音、扫频和静音演示，不会播放声音。

## 接收 PCM

安装框架后链接 `Zz::FluentUI`，只需包含公开头文件：

```cpp
#include <ZzFluentUI/ZzAudioSpectrum.h>

auto *spectrum = new ZzFluentUI::ZzAudioSpectrum(parent);
spectrum->setSampleRate(48000);
spectrum->setChannelCount(2);
spectrum->setFftSize(2048);
spectrum->setBarCount(64);

// 来自解码器的交错浮点 PCM：L0, R0, L1, R1, ...
// 每个样本归一化到 [-1, 1]，容器长度是样本数，不是帧数。
spectrum->pushSamples(interleavedSamples);

// 同一条流也可使用有符号 16 位、小端、交错的 PCM 字节。
// 切换两种输入格式会清空旧数据；不要在同一条流中混用。
spectrum->pushPcm16Le(pcmBytes);
```

两个推送方法可从普通工作线程调用；内部有互斥锁，不适合硬实时音频回调。
推荐由一个生产者按流顺序推送。其他 API（包括配置、`clear()`、外部频带）只在 GUI
线程调用。销毁控件前必须停止并等待生产者退出；不要让生产线程与析构并发。
FFT 和绘制在 GUI 线程执行，不持有输入锁。

字节输入自动拼接跨调用的半个采样和不完整声道帧。浮点输入将 NaN/Inf 作为零，
超范围值钳制到 [-1, 1]。固定环形缓冲最多保存 8 个 FFT 窗口，溢出保留最新完整帧，
超大输入先跳过旧部分再解析。默认单声道配置占用 64 KiB 样本缓冲，示例双声道占用
128 KiB，最大配置占用
2 MiB；不包含 FFT 工作区、快照及调用方持有的容器。

## 使用外部频带

已经有分析器时可跳过内置 FFT：

```cpp
spectrum->setInputMode(ZzFluentUI::ZzAudioSpectrum::ExternalBands);
spectrum->setBarCount(32);
spectrum->setAnimationEnabled(false);
spectrum->setBandLevels(QVector<qreal>(32, 0.5));
```

输入长度必须等于 `barCount()`；非有限值导致整帧被拒绝，有限值钳制到 [0, 1]。
外部模式不接受 PCM，PCM 模式不接受外部频带，切换模式清空旧状态。
关闭动画时首帧立即显示，运行期间的后续帧按 `refreshInterval()` 合并，仅显示最新帧。

`levels()` 是目标频带，`displayedLevels()` 是当前柱高，`peakLevels()` 是峰值位置。
三个对应信号用于观察状态；高频输入下不要在信号槽中执行重任务。

## 分辨率与显示参数

| 参数 | 默认值 | 可选范围／含义 |
| --- | --- | --- |
| sampleRate / channelCount | 44100 Hz / 1 | 8000～192000 Hz / 1～8 声道 |
| fftSize | 2048 | 1024、2048、4096、8192 |
| barCount | 64 | 8～256；窄窗口仅合并绘制，不改变逻辑频带数 |
| minimumFrequency / maximumFrequency | 60 / 16000 Hz | 实际上限不超过采样率一半 |
| minimumDecibels / maximumDecibels | -54 / -3 dB | 归一化显示范围，不是硬件校准读数 |
| spectralTilt | +3 dB/倍频程 | 视觉倾斜补偿；设为 0 关闭，增益限制 -6～+9 dB |
| channelMix | PowerAverage | 默认平均声道功率，反相信号不会抵消；WaveformAverage 平均波形 |
| refreshInterval | 16 ms | 1～1000 ms；每次最多分析最新一窗 |
| inputTimeout | 120 ms | 无新输入后归零衰减；0 表示保持最后输入 |
| gravity / peakHoldDuration | 4.2 / 200 ms | 下落加速度与峰值实际保持时长 |

FFT 使用 Hann 窗和窗增益校正，频带按对数划分；`centerFrequencies()` 返回实际中心
频率。更大的 FFT 提高低频分辨率，但需要更多样本：2048 点／48 kHz 约 42.7 ms，
8192 点约 170.7 ms。这是窗口时长，不等于整条音频链路的延迟。宽频带采用功率与峰值
混合，定位是音乐可视化，不应代替校准频谱测量仪。

分析参数变化会清空历史。配置数值会钳制到公开头文件注明的范围；非法 FFT 档位、
枚举值及非有限配置值会被忽略。

## 外观与生命周期

默认开启彩虹、峰值帽、渐隐倒影和平滑动画。关闭彩虹后使用主题强调色；
`setAccentColor()` / `setBackgroundColor()` 支持自定义色，传入无效 `QColor{}` 恢复主题。
高对比主题优先使用主题背景和前景颜色，覆盖彩虹与自定义颜色。
柱间距、圆角、峰值高度和倒影透明度均可调。

隐藏或禁用控件时停止分析与动画，输入缓冲仍有上限；恢复时只使用最新未过期数据。
输入断流超过超时后，新一段 PCM 重新积累完整 FFT 窗口，避免旧数据再次出现。
空闲且显示稳定时停止计时器，可用 `isRunning()` 观察。
关闭动画或启用减少动态效果时直接呈现，仍保留合并刷新和超时处理。
`clear()` 清空样本、残余帧、柱高及峰值，旧排队通知不会恢复旧画面。

## 验证与测量

数值和输入契约由 `fluent.spectrum-analyzer` 验证；交互、生命周期、线程投递、
主题及像素绘制由 `fluent.audio-spectrum` 验证；示例由 `example.audio-spectrum` 验证。
安装消费者还会通过公开接口输入正弦信号，检查私有 FFT 在共享与静态 SDK 中的链接。

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzAudioSpectrumTest ZzSpectrumAnalyzerTest ZzExampleAudioSpectrumTest --parallel 4
ctest --test-dir build/linux-gcc-debug \
  -R '^(fluent\.(audio-spectrum|spectrum-analyzer)|example.audio-spectrum)$' \
  --output-on-failure
```

本机测量与限制见 [频谱测量记录](../performance/AUDIO_SPECTRUM_2026-11-05_ZH.md)。
依赖来源和许可证见 [第三方通知](../third-party/THIRD_PARTY_NOTICES.md)。
