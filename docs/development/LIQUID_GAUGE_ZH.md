# 水波进度球

`ZzFluentUI::ZzLiquidGauge` 迁自 FluentUIStyle 的 `ExLiquidGauge`，继承 `QProgressBar`。
示例入口位于“自定义控件 → 水波进度球”，路由为 `liquid-gauge`。

```cpp
#include <ZzFluentUI/ZzLiquidGauge.h>

auto *gauge = new ZzFluentUI::ZzLiquidGauge(parent);
gauge->setRange(0, 100);
gauge->setValue(68);
gauge->setShape(ZzFluentUI::ZzLiquidGauge::CircleShape);
gauge->setWaveAmplitude(6.0);
gauge->setWaveCount(3);
gauge->setWaveAnimationDuration(2400);
```

支持圆形、矩形、水滴、三角形裁剪，双层水波，以及液面上下分别着色的文字。
范围、数值、`%p/%v/%m` 格式、文字显示及 `reset()` 沿用 `QProgressBar`。
液面始终从下向上填充；方向及 `invertedAppearance` 不改变绘制方式。
等值量程（包括 `0,0`）显示空液面，不切换为不确定进度。

波幅、轮廓线宽和间距限制为 0～100 逻辑像素，波形数量为 1～20，动画周期为
100～60000 毫秒，后层透明度为 0～1，字号为 0～200 像素。字号 0 自动适配尺寸。
浮点属性忽略 NaN 和无穷值；越界有限值钳制后仅在实际改变时发出通知。

默认颜色跟随调色板：水波使用 `Accent`，背景使用 `Base`，轮廓跟随水波，
液面上/下文字分别使用 `Text` / `HighlightedText`。传入无效 `QColor()` 恢复自动色。
示例的“自动”开关关闭后固定当前颜色，再通过色块选择自定义颜色；重置恢复主题色。

隐藏、禁用、关闭动画、波幅为 0 或主题启用“减少动态效果”时暂停计时器并保留相位。
恢复时不补算暂停时长；`isRunning()` 可查询实际播放状态。控件须在 GUI 线程使用。

验证覆盖属性归一化、空满液面、大整数范围、主题回退、动画暂停恢复及小尺寸裁剪；
另有浅色、深色、高对比度在 100%、125%、150%、200% 缩放下的截图基线。
同尺寸、字体、配色及静止动画下，八组样例在浅/深色主题与参考实现逐像素一致。
当前验证环境为 Linux / GCC / Qt 6.11.1，未验证 Windows / MSVC。
