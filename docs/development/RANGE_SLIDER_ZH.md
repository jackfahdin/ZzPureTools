# 范围滑块迁移设计与验收

目标：将 FluentUIStyle 的 ExRangeSlider 外观与交互迁入 ZzFluentUI，示例入口为“自定义控件 → 范围滑块(RangeSlider)”。参考项目保持只读。

## 设计

采用独立 QWidget 控件 `ZzRangeSlider`，公开头 + unique_ptr 私有状态，Qt 6.8+ 公开 API、C++20。控件在 GUI 线程使用。
以整型 minimum/maximum、lowerValue/upperValue 表达范围；始终保持 minimum ≤ lower ≤ upper ≤ maximum。
原版 QML 风格 first/second 节点不是本轮对外接口，提供可读 lowerPosition/upperPosition 区分拖动预览和已提交数值。

外观按原版：4 px 圆角轨道，手柄外圆半径 9 px、中心距边缘 10 px；内圆正常/悬停/按下比例 0.55/0.65/0.40；300 ms OutCubic。刻度在轨道两侧，长度 4 px、距轨道 6 px。
浅/深主题、禁用状态遵循原颜色；高对比度使用系统 palette；减少动效、隐藏与禁用时停止动画。

支持水平、垂直、RTL，点击轨道选择最近端点、拖动端点、滚轮、方向键、PageUp/Down、Home/End。
水平 Up/Down 切换 upper/lower，垂直 Right/Left 切换 upper/lower；Tab 正常离开控件。
提供 NoSnap / SnapAlways / SnapOnRelease，吸附以 minimum 为原点、singleStep 为间隔。
tracking=false 时仅更新 position 与 sliderMoved，释放时提交 value；拖动期间中断（隐藏/禁用/失焦/改变范围方向）取消未提交预览并清理提示。
数值提示是控件自身持有的 tooltip，属性 `valueTipEnabled` 默认 false，可在示例切换。
无障碍提供两个有数值接口的端点，不能只发布一个不可编辑的文本范围。

## 使用

链接 `Zz::FluentUI`，使用公开头文件即可：

```cpp
#include <ZzFluentUI/ZzRangeSlider.h>

auto *slider = new ZzFluentUI::ZzRangeSlider(parent);
slider->setRange(0, 100);
slider->setValues(20, 80);
slider->setTickInterval(10);
slider->setValueTipEnabled(true);
slider->setSingleStep(5);
slider->setSnapMode(ZzFluentUI::ZzRangeSlider::SnapMode::SnapOnRelease);
QObject::connect(slider, &ZzFluentUI::ZzRangeSlider::valuesChanged,
                 receiver, [receiver](int lower, int upper) {
    // 将已提交的区间应用到业务数据。
    receiver->applyRange(lower, upper);
});
```

关闭 `tracking` 后，`sliderMoved(lower, upper)` 和 `lowerPosition()/upperPosition()`
提供拖动预览，`lowerValue()/upperValue()` 保持已提交值，松开后才发出值变化信号。
调用 `setValues` 时会排序并限制到合法范围，整体变化只发出一次 `valuesChanged`。
`setTickPosition(false)` 隐藏刻度；`setOrientation(Qt::Vertical)` 切换为纵向。
水平 RTL 使用 `setLayoutDirection(Qt::RightToLeft)`。

键盘聚焦后，水平使用左右键调整当前端点、上下键选择端点；纵向使用上下键调整、左右键选择。
PageUp/PageDown 使用 `pageStep`，Home/End 调整到当前端点允许的边界；Tab 正常切换焦点。

## 任务 1：控件与行为回归

新增 `widgets/include/ZzFluentUI/ZzRangeSlider.h`、`widgets/src/ZzRangeSlider.cpp`、`widgets/src/private/ZzRangeSliderPrivate.h/.cpp`。
新增 `tests/ZzRangeSliderTest.cpp`，接入 ZzFluentUI/CMakeLists.txt 与 tests/CMakeLists.txt。
两个端点的无障碍接口独立放在 `private/ZzRangeSliderAccessible.h/.cpp`，由 `ZzRangeSliderAccessibilityTest` 验证。

- [x] 先编写真实事件测试、最小接口桩并看到断言失败。
- [x] 实现范围收敛、批量设置一次 valuesChanged（避免原版重复信号）、拖动与吸附。
- [x] 实现原版绘制、动画、提示及无障碍。
- [x] 运行 `ZzRangeSliderTest`；覆盖 tracking true/false、交叉限位、相等端点、RTL/纵向、键盘、滚轮、INT_MIN/MAX、退化范围、刻度绘制复杂度、隐藏/禁用清理及减少动效。

## 任务 2：独立示例与原版截图对照

新增 `examples/ZzPureToolsExample/ZzExampleRangeSliderPage.cpp`，通过现有 ShowcasePage 装配；RouteCatalog 新增 range-slider，自定义控件分组第一项，保留其余路由。
补英文翻译、CMake 源文件及现有导航数量回归。

- [x] 按 PageRangeSlider 创建“演示”和“属性”卡片，默认 0–100 / 20–80，刻度间隔 10、tracking=true、tooltip=true。
- [x] 属性可调刻度开关/间隔、tracking、tooltip，并增加紧凑的方向、RTL、禁用与吸附选项以验收公开能力。
- [x] 同 Qt、字体、palette 直接编译参考 ExRangeSlider，截取浅深色同尺寸控件，人工对比。
- [x] 为新控件添加三主题、四档 DPR 截图基线，核对后更新。

## 任务 3：审查与验证

- [x] 独立规格/代码审查并修复重要问题。
- [x] 构建示例，运行范围滑块、原滑块 tooltip、截图、中文/英文示例集成、工作区烟测。
- [x] 更新本文验证结果，交付源码和可点击对照图；Windows/MSVC 未验证则明确说明。

## 验证结果

2026-09-30，Linux、GCC 15、Qt 6.11.1、offscreen：

- 构建 `ZzPureToolsExample`、两个范围滑块测试、截图测试和工作区烟测成功。
- `ZzRangeSliderTest` 24 项、`ZzRangeSliderAccessibilityTest` 5 项通过（含 QtTest 初始化和清理）。
- 10 个 CTest 全部通过：两个范围滑块测试、原滑块 tooltip、四档 Fluent 截图、中文/英文示例集成、工作区烟测。
- 新增 12 张基线：浅色、深色、高对比度 × DPR 1 / 1.25 / 1.5 / 2。未更新其他控件基线。
- 实际示例导航和属性联动烟测通过；英文资源生成 423 条已完成翻译，无未完成项。

独立审查发现并修复了信号重入漏通知、回调销毁后的访问、吸附取消重复释放、重合端点切换期间取消遗漏。
减少动效运行中切换、高对比度轨道可见性均先通过回归复现失败，再修复通过。
控件使用 Qt 公开 API；本轮未在 Windows/MSVC 或 Qt 6.8 上实际构建。

本地验收产物（build 目录，不纳入版本控制）：
[真实示例页面](../../build/range-slider-preview/range-slider.png)、
[浅色原版对照](../../build/range-slider-reference/comparison-light.png)、
[深色原版对照](../../build/range-slider-reference/comparison-dark.png)。
