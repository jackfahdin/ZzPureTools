# 滑块调节提示

`ZzSliderValueTip` 属于 `ZzFluentUI`，可附加到任何原生 `QSlider`，不需要 Example 或替换滑块类型。外观由当前样式提供，推荐使用应用级 `ZzFluentStyle`。

```cpp
#include <QtWidgets/QSlider>
#include <ZzFluentUI/ZzSliderValueTip.h>

auto *slider = new QSlider(Qt::Horizontal, parent);
slider->setRange(20, 80);
slider->setValue(50);
auto *tip = ZzFluentUI::ZzSliderValueTip::attach(slider);
// 调节到 50 时显示 50%，不是 50 / 80。
tip->setMode(ZzFluentUI::ZzSliderValueTipMode::Value);      // 显示原始整数。
tip->setMode(ZzFluentUI::ZzSliderValueTipMode::Disabled);   // 关闭提示。
tip->setMode(ZzFluentUI::ZzSliderValueTipMode::Percentage); // 默认百分比。
```

目标链接 `Zz::FluentUI` 即可使用。`attach()` 的返回值由滑块拥有，无需自行管理；同一滑块重复调用会复用组件并更新模式。传入空滑块返回 `nullptr`。模式变化通过 `modeChanged` 信号通知，无效枚举不改变现有模式。

| 操作 | 提示行为 |
| --- | --- |
| 鼠标拖动 | 持续跟随滑柄，松开隐藏；`tracking=false` 时显示尚未提交的 `sliderPosition` |
| 点击轨道 | 显示调节结果，松开隐藏；步进仍遵循 Qt |
| 方向键、Home/End、PageUp/PageDown | 调节时显示，停止输入约 900ms 后隐藏 |
| 滚轮 | 调节时显示，约 900ms 后隐藏；继续调节重新计时 |
| 程序调用 `setValue()` | 不主动打开提示；已打开时同步数值 |
| 隐藏、禁用、失焦、窗口失活 | 隐藏提示并停止计时 |

百分比依据实际最小值与最大值计算，四舍五入为整数；最小值与最大值相同时显示 `0%`。换算支持完整 `int` 范围。水平提示位于滑柄上方，垂直提示位于侧面，边缘处自动避让。支持 RTL、反向外观、字体和主题更新。

提示不抢焦点、不接收鼠标输入。组件只在首次调节时创建一个可复用提示窗口；隐藏时不观察祖先移动、不运行隐藏计时器，没有常驻轮询。删除组件会清理提示窗口，原生滑块继续工作。

示例入口：基础控件 → 滑块(Slider)。水平与垂直滑块演示百分比，另一个滑块演示原始数值。
