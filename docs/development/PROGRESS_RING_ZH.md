# 环形进度

`ZzFluentUI::ZzProgressRing` 是库内可复用的 Qt Widgets 控件，继承 `QProgressBar`。
参考 FluentUIStyle 的圆角环和数值比例，范围、信号和无障碍仍沿用 Qt 公共接口。

| 配置 | 默认值 | 说明 |
| --- | --- | --- |
| `ringWidth` | 6px | `setRingWidth()` 收敛到 1～64 个逻辑像素 |
| `indeterminateDuration` | 800ms | 忙碌圆弧一周的周期，收敛到 200～60000ms |
| `sizeHint()` | 120×120px | 环宽较大时建议尺寸随之增加 |
| `minimumSizeHint()` | 48×48px | 可显式设置更小尺寸，绘制会安全收缩 |
| 数值字体 | 随短边 18% 缩放，12～32px 半粗体 | 保留当前字体族；直接 `setFont()` 可覆盖 |

```cpp
#include <ZzFluentUI/ZzProgressRing.h>

auto *ring = new ZzFluentUI::ZzProgressRing(parent);
ring->setRange(0, 100);
ring->setValue(68);
ring->setFormat(QStringLiteral("%p%"));
ring->setRingWidth(6);

// 未知进度：圆头的 90° 圆弧持续旋转。
ring->setRange(0, 0);
ring->setIndeterminateDuration(1200);
ring->setTextVisible(false);
```

数值绘制限制在圆环内部的内接正方形；长文本省略，空间不足以容纳一行时不绘制文字。
可用 `setFont()` 指定数值字号与字重，`setFont(QFont())` 恢复自动比例。
自动模式使用继承到的字体族，但字号和字重由控件决定。
紧凑加载指示可使用 `setFixedSize(32, 32)`、`setRingWidth(3)`、`setTextVisible(false)`。

强调色复用 `ZzControlAppearance::setAccentColor()` 或主题 palette，支持浅色、深色、
高对比和禁用态。`invertedAppearance` 反转圆弧方向；布局方向不改变数值含义。
非零最小值及完整 int 范围按 64 位差值处理，确定值直接更新、不额外插值。

每个圆环只持有一个可复用 `QVariantAnimation`，不增加 `QTimer`。
运行中改变周期按当前相位映射到新周期，角度仅可能有毫秒量化误差，不重启窗口或动画。
隐藏、禁用、减少动效和退出忙碌状态会停止动画；减少动效时使用固定 90° 弧。
动画不改进度值或业务模型。

Example 的“基础控件 → 进度环(ProgressRing)”展示默认、忙碌、禁用和紧凑状态，
可通过公开接口调节环宽、周期和确定进度值。
