# 水波进度球实现计划

> 使用 subagent-driven-development 分工实现，按 verification-before-completion 验证；本次不提交。

**目标：** 将 FluentUIStyle 的 ExLiquidGauge 和完整示例迁入 ZzFluentUI。
**架构：** ZzLiquidGauge 继承 QProgressBar，公开 Qt 属性，PIMPL 隔离绘制和动画状态。示例沿用已有卡片、属性滑块、颜色按钮和 PivotSlide 页签。
**技术栈：** C++20、Qt 6.8+ 公共 API、QtTest、CMake。

## 全局约束

- 工作目录为当前 ZzPureToolsFrame；参考 /home/zz/Jackfahdin/github/FluentUIStyle 只读。
- 不触碰 docs/research，不提交，不切换工作树。
- 与参考保持四种形状、双层波形、轮廓间距、液面上/下文字的绘制一致。
- 新公共接口使用中文 Doxygen、显式 Q_PROPERTY、导出宏及 PIMPL。

### Task 1: 控件与行为测试

文件：ZzFluentUI/widgets/include/ZzFluentUI/ZzLiquidGauge.h、widgets/src/ZzLiquidGauge.cpp、widgets/src/private/ZzLiquidGaugePrivate.h，ZzFluentUI/CMakeLists.txt，ZzFluentUI/tests/ZzLiquidGaugeTest.cpp 及 tests/CMakeLists.txt。

- [x] 先写真实行为测试并确认 RED，再实现并运行 GREEN。
- [x] 参考 ExWidgets/gauges/exliquidgauge.h/.cpp，公开相同 Shape 枚举和所有属性（将类名改为 ZzLiquidGauge），默认值、归一化边界与参考一致。
- [x] 动画隐藏、禁用、animationEnabled=false、waveAmplitude=0、主题 reducedMotion 时暂停；样式和主题变化立即更新。可增加只读 isRunning() 方便使用者观察生命周期；避免外部通知的删除/重入风险。
- [x] 保留 QProgressBar 文本格式、范围和 reset 语义；覆盖大整数范围，非有限输入拒绝，颜色恢复主题，0/100% 液面，小尺寸安全绘制及动画暂停/恢复。
- [x] 关键断言示例：setWaveAmplitude(-1) 后为 0；再写 -2 不重复发通知；NaN 不改变状态。静止模式矩形在 0% 中心为背景色，100% 中心为波色；隐藏后图像阶段不再推进。
- [x] 构建命令 cmake --build build/linux-gcc-debug --target ZzLiquidGaugeTest；QT_QPA_PLATFORM=offscreen build/linux-gcc-debug/ZzFluentUI/tests/ZzLiquidGaugeTest。

### Task 2: 示例页与完整验证

文件：examples/ZzPureToolsExample/ZzExampleLiquidGaugePage.cpp，示例 CMake、路由目录、页面枚举/工厂/分发、烟测、英文翻译；ZzFluentUI/tests/ZzFluentScreenshotTest.cpp 及新基线。

- [x] 先在 workspace smoke 加 liquid-gauge 路由存在断言确认失败，再接入第 37 项（radial-gauge 后）。
- [x] 四个 150px 示例，参考配色 #1677FF/#13C2C2/#722ED1/#FA8C16，共享值 60；260px 预览默认 68，基础/外观双页签，完整参数、恢复默认与代码示例。
- [x] 色块反映属性/主题回退；提供自动色切换，允许固定当前色。恢复默认后清除颜色覆盖。
- [x] 烟测实际验证共享数值、形状、文本、波幅、动画开关、颜色与重置；英文同步。
- [x] 新增 light/dark/high-contrast × 1/1.25/1.5/2 DPR 确定性截图，和真实参考控件同尺寸同配色比较。
- [x] 运行 focused behavior、截图、集成/英文、workspace smoke、公共头及边界架构测试；独立审查后交付预览。
