# 数值输入框的四种按钮模式

`ZzSpinBox`（整数）与 `ZzDoubleSpinBox`（小数）共用 `ZzSpinBoxButtonLayout`。

| 模式 | 中文名称 | 从左到右排版时的语义 |
| --- | --- | --- |
| `Vertical` | 右侧竖排箭头 | 数值在左；右上向上箭头增大，右下向下箭头减小。 |
| `HorizontalSides` | 两侧箭头 | 左侧向下箭头减小，中间数值，右侧向上箭头增大。 |
| `HorizontalRight` | 右侧横排箭头 | 数值在左；右侧先放减小箭头，再放增大箭头。默认模式。 |
| `PlusMinusHorizontalSides` | 两侧加减按钮 | 左侧减号减小，中间数值，右侧加号增大。 |

RTL 排版将位置整体镜像，符号及其增减作用不变。布局仅影响呈现和按钮位置，不改变范围、步长、精度、单位或业务数据。

```cpp
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>

auto *integer = new ZzFluentUI::ZzSpinBox(parent);
integer->setRange(1, 64);
integer->setButtonLayout(ZzFluentUI::ZzSpinBoxButtonLayout::Vertical);

auto *decimal = new ZzFluentUI::ZzDoubleSpinBox(parent);
decimal->setDecimals(2);
decimal->setSingleStep(0.25);
decimal->setButtonLayout(
    ZzFluentUI::ZzSpinBoxButtonLayout::PlusMinusHorizontalSides);
```

`buttonLayout()` 读取当前模式；`buttonLayoutChanged()` 通知模式变化。可以在窗口显示后直接调用 `setButtonLayout()`，输入区域及自然尺寸会立即刷新，无需调整窗口大小。

设置模式时同步对应的 Qt 箭头/加减符号。调用者之后仍可用 `setButtonSymbols()` 显式覆盖符号。`NoButtons` 的隐藏状态优先，切换模式不会擅自重新显示按钮。重复设置相同模式不发送通知，无效枚举被忽略。

四种布局共用库内绘制、尺寸和命中计算；符号使用公共字体图标缓存，数值编辑、键盘步进和长按仍由 Qt 处理。标准 Qt 数值和日期时间控件保留竖排布局。

示例位置：基础控件中的“整数输入(SpinBox)”和“小数输入(DoubleSpinBox)”页面，每个页面分别展示全部四种模式。
