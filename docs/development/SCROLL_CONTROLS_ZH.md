# 滚动条与滚动区域

`ZzFluentUI::ZzScrollBar` 继承 `QScrollBar`，保留 Qt 的范围、分页、滚轮、
键盘、拖动和无障碍接口。`ZzScrollArea` 默认安装横纵两个 `ZzScrollBar`。
绘制位于库内 `ZzFluentStyle`，其他项目安装此样式即可复用。

参考 FluentUIStyle 的细滑块和悬停展开设计：

| 状态 | 外观与行为 |
| --- | --- |
| 普通 | 2.5px 圆头滑块，使用主题次要文字色 |
| 悬停 | 167ms 展开至 6px，轨道与两端小三角同步淡入 |
| 拖动或键盘焦点 | 展开滑块，使用强调色 |
| 禁用 | 收起滑块，使用禁用调色板，不运行动画 |
| 减少动效 | 直接切换状态，无过渡 |

横纵方向共用绘制规则。12px 操作槽和 24px 最小滑块长度保持不变，
不因为视觉变细缩小命中范围。横向箭头朝左右，纵向箭头朝上下；
点击按 `singleStep` 滚动，长按连续步进，松开停止，到达边界时对应箭头变淡。
两端各固定预留 12px，不随悬停变化，滑块不会突然跳动。
短尺寸优先保留 24px 滑块空间，再对称缩减箭头区域；长度不超过 24px 时省略箭头。
RTL 与反向显示共同决定数值方向，箭头、分页和拖动使用一致的几何。
高对比主题的次要文字为白色，拖动使用高对比强调色。
原生 `QScrollBar` 使用相同外观；平滑悬停动画由 `ZzScrollBar` 提供。

```cpp
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzScrollBar.h>

auto *bar = new ZzFluentUI::ZzScrollBar(Qt::Horizontal, parent);
bar->setRange(0, 100);
bar->setPageStep(25);
bar->setValue(35);

auto *area = new ZzFluentUI::ZzScrollArea(parent);
area->setWidgetResizable(true);
area->setWidget(content); // 转移内容控件所有权。
```

每个滚动条复用一个 `QVariantAnimation`，绘制期间不创建动画、定时器或控件。
隐藏、禁用时停止动画；快速移入移出从当前进度继续。
此前绘制将 `MouseOver` 状态强制映射为完整展开，覆盖了进入动画，
现改为直接使用动画进度，轨道透明度也跟随该进度。

Example 的“基础控件 → 滚动条(ScrollBar)”展示横纵普通/禁用状态及双轴滚动区域。
回归测试检查真实中间帧、轨道淡入、动画反向连续性和命中几何稳定性，
并沿用范围、拖动、键盘、RTL、极端范围及对象数量测试。

## 初次样式调整验证（2026-09-29）

- 本机 Linux、Qt 6.11.1、GCC 15.2、`linux-gcc-debug` 完整构建。
- 本机开发门禁：217 项常规测试通过，滚动控件和注释滚动条两组定向测试通过。
- 滚动控件测试共 16 项通过；新增横纵中间帧测试在修复前均失败。
- 100%、125%、150%、200% 四档完整截图检查通过；更新三主题共 12 张滚动条基线。
- 检查 Example 路由导出的真实页面截图，展示层仅使用库组件及标准布局。
- 未执行 Windows/macOS 编译、Qt 6.8 运行、性能门禁或物理桌面人工验收。

验证日志位于 `build/linux-gcc-debug/scrollbar-gate.log` 与
`build/linux-gcc-debug/scrollbar-screenshots.log`，不纳入源码提交。

## 两端箭头验证（2026-09-29）

- 同一本机 Debug 环境完整构建通过；217 项常规测试及两组定向测试通过。
- 滚动控件 27 项通过，新增点击/长按、边界淡化、极短尺寸和八组方向组合测试。
- 横向 RTL 几何与 Qt 原生拖动换算保持一致，八组测试均可从最小值拖动到最大值。
- 四档完整截图通过，更新 12 张滚动条基线，新增纵向悬停截图覆盖上下三角。
- 独立审查未发现阻断问题；本轮未新增动画对象、定时器或按钮控件。
- 日志：`build/linux-gcc-debug/scrollbar-arrows-gate.log`、
  `build/linux-gcc-debug/scrollbar-arrows-screenshots.log`。
- Windows/macOS、Qt 6.8、性能门禁与物理桌面未在本轮验证。
