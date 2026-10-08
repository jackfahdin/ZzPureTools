# 音频电平表实现计划

目标：迁移 FluentUIStyle 的 ExAudioLevelMeter 及 PageAudioLevelMeter，接在边框光束后。
架构：ZzAudioLevelMeter 为只读 QWidget，私有实现管理电平状态、动画和绘制；示例独立管理模拟输入及可选 Qt Multimedia 播放。
技术栈：C++20、Qt 6.8+ 公开 API。本地 Linux / GCC / Qt 6.11.1 验证，不自动提交。

## 全局约束

- 参考仓库只读；不读写或提交 docs/research。
- 单声道、立体声、多声道、刻度位置与生成模式、统一/阈值/渐变颜色沿用参考 API 和几何。
- 电平表接收 dBFS/线性幅度，普通线程的输入排队到 GUI；不访问硬件或直接处理硬实时回调。
- 页面隐藏暂停模拟输入和本地播放；控件隐藏/禁用不持续运行动画，减少动态效果时直接显示输入。
- 浅深主题默认颜色跟随应用；显式颜色覆盖保留，高对比度保证可读。
- 示例按参考展示音乐栏、单声道/立体声和分为基础/刻度/动画/颜色的属性页。
- 音乐播放用可选 Qt Multimedia；无模块仍可构建并展示模拟输入，不增加核心库依赖。

## 任务 1：独立电平表控件

- [x] 先创建 ZzAudioLevelMeterTest 并验证失败，覆盖线性转换、非法数值、范围钳制、峰值衰减/超时、暂停及普通线程输入。
- [x] 新增 widgets/include/ZzFluentUI/ZzAudioLevelMeter.h、widgets/src/ZzAudioLevelMeter.cpp 和私有状态/绘制文件；接入库和测试 CMake。
- [x] 运行 `cmake --build build/linux-gcc-debug --target ZzAudioLevelMeterTest -j4` 与对应 CTest，独立审查规格及质量。

## 任务 2：示例与播放

- [x] 创建 ZzExampleAudioLevelMeterPage.cpp，接入路由、工厂、页面类型、英文翻译和烟测。
- [x] 可复用边框光束页的颜色按钮/卡片助手；保证两页既有外观不变。
- [x] 独立实现公开 QAudioBufferOutput 音频帧到每声道峰值 dBFS，覆盖浮点/整数 PCM、静音、空帧与极值。
- [x] 本地音乐选择、播放/暂停、进度/音量和错误状态可用；未选择音乐时模拟输入，切换页面停止无效工作。
- [x] 烟测验证属性编辑能更新预览，播放工具不可用时有可理解的退化展示。

## 任务 3：视觉与最终验证

- [x] 用真实参考控件、相同尺寸/字体/固定电平做浅深色对照。
- [x] 新增浅色、深色、高对比度与四档 DPR 截图基线，覆盖单/双/多声道和三种配色。
- [x] 构建示例与测试，运行相关组件测试、四档截图、中英文集成及工作区烟测。
- [x] 独立复核整个未提交变更，修正重要问题，记录真实验证证据。

## 使用与行为

```cpp
#include <ZzFluentUI/ZzAudioLevelMeter.h>

auto *meter = new ZzFluentUI::ZzAudioLevelMeter(parent);
meter->setScalePosition(ZzFluentUI::ZzAudioLevelMeter::CenterScale);
meter->setColorMode(ZzFluentUI::ZzAudioLevelMeter::ThresholdColors);
meter->setStereoLevels(-12.0, -6.0); // dBFS，左右声道独立
```

- 核心不依赖 Multimedia。普通工作线程可提交电平，其他属性和生命周期在 GUI 线程管理。
- 关闭动画或启用“减少动态效果”后直接呈现输入，暂停输入超时；隐藏/禁用停止动画计时器。
- 示例“基础、刻度、动画、颜色”四页沿用参考的 PivotSlide 样式；自选颜色在切换主题后保留。
- 音乐栏显示音量调整前的每声道峰值 dBFS。暂停、结束或跳转进度时清空电平；隐藏页面暂停播放，重新显示不自动播放。
- 可选 `ZZ_EXAMPLE_AUDIO_PLAYBACK=OFF`，或未安装 Qt Multimedia 时保留模拟模式。公开的 QAudioBufferOutput 需要 Qt 6.8+ 和 FFmpeg 后端；其他后端可能正常播放但无法提供电平，页面会显示提示。

## 验证记录

本地环境：Linux、GCC 15、Qt 6.11.1。示例及全部相关目标编译通过。

- CTest 12/12：`fluent.audio-level-meter`、`fluent.border-beam`、四档 `fluent.screenshot-*`、`example.audio-levels`、`example.audio-meter-simulation`、`example.audio-meter-playback`、中英文 `example.puretools-integration*`、`example.workspace-smoke`。
- 核心 QtTest 12 passed，覆盖非法输入、峰值与超时、线程排队、同步删除/重入、卡顿后新峰值计时及高对比过载可见性。核心接口与两项缺陷均观察 RED → GREEN。
- 真实临时双声道 WAV 通过独立解码能力探针，再验证输入源的独立声道、隐藏暂停、恢复不自动播放和模拟切换；本地播放测试 4 passed、0 skipped。缺少解码帧能力的平台明确跳过该项，保留 PCM 和模拟测试。
- 同一源文件分别带/不带 Multimedia 编译测试，验证可选模块退化路径。
- 新增 12 张基线：浅色、深色、高对比 × DPR 1/1.25/1.5/2。
- 与真实 ExAudioLevelMeter 同尺寸、字体、电平对照：四个常规面板浅深色逐像素一致；完整对照差异为浅色 0.1301%、深色 0.1350%，来自密集刻度避让和禁用声道标签透明度。
- 真实窗口及完整页面预览：`build/audio-level-meter-preview/`；参考对照：`build/audio-meter-reference/`。
- 独立审查发现并修复高对比过载色/未激活色、阈值编辑回写及 FFmpeg 后端能力判断。未修改参考仓库或 docs/research，未提交。

验证边界：未在 Windows/MSVC 运行。ASan 完整构建被已有 ZzFluentStylePrivate.cpp 的 Clang signedness 警告（视为错误）阻断，不能声明 ASan 通过。
