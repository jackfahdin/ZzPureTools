# 环形进度条

`ZzFluentUI::ZzProgressRing` 是库内可复用的 Qt Widgets 控件，继承 `QProgressBar`。
参考 FluentUIStyle 的圆角环和数值比例，范围、信号和无障碍仍沿用 Qt 公共接口。

| 配置 | 默认值 | 说明 |
| --- | --- | --- |
| `thickness` | 6px | 支持小数，收敛到 1～64 个逻辑像素；忽略非有限输入 |
| `ringWidth` | 6px | 兼容原整数 API，读取厚度的四舍五入值，设置时写入整数厚度 |
| `ringColor` / `trackColor` | 无效 QColor | 自动使用 palette 的 Accent / Mid；无效颜色恢复自动 |
| `title` | 空字符串 | 数值上方的独立标题 |
| `titleFont` / `valueFont` | QFont() | 自动适配尺寸；可分别指定字体 |
| `titleColor` / `valueColor` | 无效 QColor | 自动使用文本色，标题透明度为文本色的 72% |
| `textSpacing` | 4px | 标题与数值的间距，收敛到 0～100px |
| `centerWidget` | nullptr | 可交互的自定义中心控件，取代内置标题与数值 |
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
ring->setThickness(8.5);
ring->setTitle(QStringLiteral("已完成"));
QFont valueFont = ring->font();
valueFont.setPixelSize(24);
valueFont.setWeight(QFont::DemiBold);
ring->setValueFont(valueFont);

// 未知进度：圆头的 90° 圆弧持续旋转。
ring->setRange(0, 0);
ring->setIndeterminateDuration(1200);
ring->setTextVisible(false);
```

数值绘制限制在圆环内部的内接正方形；长文本省略，空间不足以容纳一行时不绘制文字。
可用 `setFont()` 指定数值字号与字重，`setFont(QFont())` 恢复自动比例。
单独设置的 `titleFont` / `valueFont` 优先于控件字体；传入 `QFont()` 恢复自动选择。
自动模式使用继承到的字体族，但字号和字重由控件决定。
紧凑加载指示可使用 `setFixedSize(32, 32)`、`setRingWidth(3)`、`setTextVisible(false)`。

强调色复用 `ZzControlAppearance::setAccentColor()` 或主题 palette，支持浅色、深色、
高对比和禁用态。`invertedAppearance` 反转圆弧方向；布局方向不改变数值含义。
非零最小值及完整 int 范围按 64 位差值处理，确定值直接更新、不额外插值。

`setCenterWidget(widget)` 接管控件并删除旧中心；`setCenterWidget(nullptr)` 恢复内置文字。
`takeCenterWidget()` 隐藏并解除中心控件的父对象，返回值由调用方接管。
自定义中心使用厚度加 6px 的内缩区域；`setTextVisible(false)` 同时隐藏中心控件。
外部删除中心控件或改变其父对象后，`centerWidget()` 立即清空，空指针变更通知延迟到事件循环，
避免在 Qt 的销毁或所有权变更栈内重入。期间若设置新中心，旧的延迟通知作废。

每个圆环只持有一个可复用 `QVariantAnimation`，不增加周期性 `QTimer`。
运行中改变周期按当前相位映射到新周期，角度仅可能有毫秒量化误差，不重启窗口或动画。
隐藏、禁用、减少动效和退出忙碌状态会停止动画；减少动效时使用固定 90° 弧。
动画不改进度值或业务模型。

Example 的“自定义控件 → 环形进度条(ProgressRing)”位于水波进度球之后，保留 `progress-ring` 路由。
页面展示 0/25/50/75/100% 和不确定状态，提供完整属性编辑、主题自动配色、可交互中心控件与重置。
动画周期编辑下限保留为 200ms（参考项目为 100ms），以兼容已有公开 API。

验证覆盖真实属性编辑、中英文路由烟测、中心控件所有权与重入、单动画生命周期，
以及浅色、深色、高对比在 100/125/150/200% 缩放下的截图。
与真实 ExProgressRing 匹配字体和配色后，0/25/50/75/100% 静态样例像素一致；
65% 样例仅圆弧末端存在角度舍入差异（1/16°），保留现有更精确的四舍五入。
