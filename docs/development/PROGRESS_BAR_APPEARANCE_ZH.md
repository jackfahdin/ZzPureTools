# 线性进度条外观

标准 `QProgressBar` 使用 `ZzFluentStyle` 后默认呈现细线进度条。
无需 Example 或自定义绘制，其他 Qt Widgets 项目可以直接使用相同能力。

| 外观 | 中文语义 | 轨道／填充厚度 |
| --- | --- | --- |
| `ZzProgressBarAppearance::Thin` | 细线，默认；强调已完成部分 | 1px／3px |
| `ZzProgressBarAppearance::Thick` | 粗线；等厚圆角条 | 4px／4px |

表中尺寸为设备无关逻辑像素。控件非常窄时等比缩小；两种模式共享 4px 布局槽，
运行时切换不会改变文字位置或自然尺寸。百分比文字与线条分区，保留 `setFormat()`、
`setAlignment()` 和竖向文字方向。

```cpp
#include <QtWidgets/QProgressBar>
#include <ZzFluentUI/ZzControlAppearance.h>

auto *progress = new QProgressBar(parent);
progress->setRange(0, 100);
progress->setValue(68);
ZzFluentUI::ZzControlAppearance::setProgressBarAppearance(
    progress, ZzFluentUI::ZzProgressBarAppearance::Thick);

// 未知进度：两种外观都支持同一条共享忙碌动画。
progress->setRange(0, 0);
progress->setTextVisible(false);
```

接口只改变指定控件的外观，不修改进度值、业务状态或其他进度条。
空指针设置无操作；空指针查询、未设置和未知枚举值都返回／采用 `Thin`。
应在 GUI 线程使用公开接口，不直接写内部动态属性。

轨道、填充、文字分别使用当前 palette 的 `Mid`、`Highlight`、`Text`。
局部颜色复用 `ZzControlAppearance::setAccentColor()`，禁用状态使用 Disabled 颜色组。
支持深浅主题、高对比、水平／垂直、RTL 和 `invertedAppearance`。

忙碌动画继续由样式级唯一 `QVariantAnimation` 驱动：两种外观共用，不新增每控件计时器。
减少动效、禁用和无真实控件上下文时使用静态居中短段；最后一个忙碌控件退出后停止动画。
确定值更新不添加数值插值动画。环形进度使用独立的 `ZzProgressRing`，不受此接口影响。

Example 的“基础控件 → 进度条(ProgressBar)”可查看细线和粗线的确定、忙碌、禁用状态，
下面的滑块同步调整两种确定进度，并使用公共百分比提示。
