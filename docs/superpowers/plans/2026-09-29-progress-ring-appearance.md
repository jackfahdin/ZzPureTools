# 环形进度外观改进计划

## 目标与设计

参考 FluentUIStyle 的 `drawProgressRing()` 和 `ExProgressRing`：默认环宽 6px、
建议尺寸 120px、最小建议尺寸 48px、90° 忙碌圆弧以默认 800ms 周期旋转。
保留库内 `ZzProgressRing` 四文件结构、Qt 范围和值语义、圆头、主题和单一持久动画。
不复制参考项目的 Qt 私有动画、墙上时钟、中心子控件管理或计时器拨盘。

数值默认使用当前字体族、随短边 18% 缩放的 12～32px 半粗体。
显式 `setFont()` 时按调用方字体原样绘制；`setFont(QFont())` 恢复自动字号。
圆环按浮点矩形精确居中，外围留 2px；文字只能画在圆环内部的内接正方形内，
空间过小时隐藏，超长文字省略。忙碌状态不显示百分比。

新增库内 `indeterminateDuration` 属性及 getter/setter/signal，单位毫秒，
收敛到 200～60000；切换速度保留当前角度、复用原动画，不改业务值。
隐藏、禁用、确定进度及减少动效继续停机，静态忙碌弧固定在十二点方向。

## 实施顺序

- [x] `ZzFluentUI/tests/ZzProgressRingTest.cpp`：先写并运行真实图像回归，证明当前中央文字
  不随尺寸变化；通过元属性测试周期配置缺失。覆盖运行中改速的角度连续性、对象数量及静态状态。
- [x] `ZzFluentUI/widgets/include/ZzFluentUI/ZzProgressRing.h`、`widgets/src/ZzProgressRing.cpp`：
  公开周期属性与中文 Doxygen，更新默认几何、居中及安全文本绘制。
- [x] `ZzFluentUI/widgets/src/private/ZzProgressRingPrivate.h/.cpp`：环宽默认 6，
  周期默认 800ms；保留相位更改周期，默认文字按控件尺寸和显式字体计算。
- [x] `examples/ZzPureToolsExample/ZzExampleControlPagePrivate.cpp`：展示新的默认环、
  紧凑无文字环，配置环宽和忙碌周期；仅调用公开接口。
- [x] `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`：固定截图字体、覆盖默认环宽，
  审阅更新三主题四档 DPR 的进度环基线，关闭更新模式复验。
- [x] `docs/development/PROGRESS_RING_ZH.md` 和 README：记录默认值、跨项目使用和限制。

## 验证

```bash
cmake --build build/linux-gcc-debug --target ZzProgressRingTest ZzFluentScreenshotTest ZzPureToolsExample --parallel 2
QT_QPA_PLATFORM=offscreen ./build/linux-gcc-debug/ZzFluentUI/tests/ZzProgressRingTest
./scripts/ci/run-local-development-gate.sh --preset linux-gcc-debug --tests '^fluent.progress-ring$'
ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^fluent.screenshot-'
```

使用现有本机 Qt，重编译用户使用的 Debug Example；一次中文提交，不推送。
如实记录本机证据，其他平台、Qt 6.8、性能与物理桌面测试另列待验证。

## 本次验证结果

- 本机 Linux x86_64、Qt 6.11.1、GCC 15.2.0、CMake 4.3.3，使用 `linux-gcc-debug`。
- 新增文字缩放与周期配置测试在实现前均失败，实现后通过；完整进度环测试 14 项通过，
  包含初始化／清理、长文字／紧凑尺寸、字体覆盖、相位连续性、信号和既有生命周期测试。
- 最小开发门禁 217 项普通测试及 1 项定向测试通过，Debug Example 已重编译。
- 更新 12 张环形进度截图后，关闭更新模式，四档完整截图测试通过。
  已检查浅色、深色、高对比及常用 DPR 的渲染；未更新 Qt 6.8 专用基线。
- 独立审查未发现阻塞问题；文档审计和差异空白检查通过。
- 待验证：Windows/macOS CI、Qt 6.8 运行、ASan/UBSan、clang-tidy、性能基准、物理桌面交互。
