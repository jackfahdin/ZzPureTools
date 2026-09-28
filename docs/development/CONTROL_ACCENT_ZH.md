# 控件强调色

## 能力与使用

`ZzFluentStyle` 支持与 FluentUIStyle 相同的按钮 `accent` 属性用法。
普通控件仍使用标准表面；启用强调色外观后，按钮主体使用强调色。
选择标记、开关、进度、评分和输入焦点等原有强调色区域可独立改色。
这不是给整个窗口或面板染色的功能。

```cpp
#include <ZzFluentUI/ZzControlAppearance.h>

using ZzFluentUI::ZzButtonAppearance;
using ZzFluentUI::ZzControlAppearance;

// 原生 QPushButton、QToolButton；运行时切换会触发重绘。
button->setProperty("accent", true);

// 统一类型化入口：也支持 ZzPushButton、ZzSplitButton、ZzIconButton。
ZzControlAppearance::setButtonAppearance(button, ZzButtonAppearance::Accent);
ZzControlAppearance::setButtonAppearance(button, ZzButtonAppearance::Standard);

// Fluent 按钮也可直接调用已有的 setAppearance；ZzIconButton 现已支持。
iconButton->setAppearance(ZzButtonAppearance::Accent);

// 按控件覆盖；其他控件继续跟随应用主题。
ZzControlAppearance::setAccentColor(button, QColor("#8752b5"));
ZzControlAppearance::setAccentColor(progressBar, QColor("#167344"));

// 清除局部强调色，重新跟随父控件或应用主题。
ZzControlAppearance::resetAccentColor(button);
```

使用前应安装 `ZzFluentStyle`，所有调用均在 GUI 线程进行。接口不持有控件。
原生工具按钮的 `Standard`/`Subtle` 保留原有 `autoRaise` 语义。
`ZzPushButton` 的 default/checked 强调状态也继续保留；设置 Standard 不会取消
Qt 的默认按钮或勾选状态。

## 颜色、继承与交互规则

| 情况 | 行为 |
| --- | --- |
| 未设置局部颜色 | 跟随应用主题，包括浅色、深色和高对比模式 |
| `setAccentColor(widget, color)` | 设置 Active/Inactive 的 Accent、Highlight、HighlightedText；禁用角色继续跟随主题 |
| 全局强调色或主题改变 | 局部颜色保持；未覆盖的颜色角色正常更新 |
| `resetAccentColor(widget)` 或传无效 QColor | 仅移除上述角色的 Active/Inactive 覆盖；保留其他 palette 角色 |
| 父控件设置局部颜色 | 遵循 Qt 本身的 palette 继承；不增加自定义继承系统 |
| 按钮悬停、按下 | 使用不同强调色填充；分割按钮两个命中区域各自反馈 |
| 禁用按钮 | 使用主题禁用表面与文字，不保留饱和强调色底色 |
| 主题色图标 | 强调色按钮中使用对应的前景色，继续复用图标缓存 |
| 自定义色／保留原色图标 | 保留调用方明确选择的颜色策略，`setIconColor` 的优先级不变 |

前景色按 WCAG 相对亮度选择黑色或白色；为保证可读性，推荐使用不透明强调色。
高对比模式默认仍使用项目原来的黄色强调色，显式局部覆盖仍有效，黑白前景自动适配。
普通键盘焦点框继续采用主题的 FocusStroke；输入控件原有强调色焦点下划线随局部颜色变化。

高级用法可以直接调整 palette。强调色按钮优先读取显式 `QPalette::Accent`，
也支持只覆盖 `Highlight` 的既有用法；显式 `HighlightedText` 会被尊重。
需要让各种自绘控件一致改色时，使用 `setAccentColor` 同步三个角色，
不要仅设置 `Button` 背景或为单个控件创建新 Style。

## 实现与性能边界

- `ZzControlAppearance` 为无状态工具类，接口与私有颜色算法分离，不增加 Pimpl 分配。
- 局部颜色使用 Qt palette 的 resolve mask；重置时按角色移除覆盖，不保存整份旧主题。
- 对比度算法的 256 项线性亮度表仅初始化一次，绘制时直接查表；状态颜色只做固定次整数计算。
- 不新增计时器、动画对象、全控件扫描或逐帧主题快照；不改变现有图标缓存容量。
- 评分、Pivot、Tab 和条目选择指示条不再硬读全局 Accent，从控件 palette 取色。
- 参考的是 FluentUIStyle 的能力与调用方式；实现沿用本项目的渲染、主题与缓存结构，未复制其源文件。

## 查看与验证

`ZzPureToolsExample` → 组件 → 基础控件 → **强调色与局部外观**：
包含紫色按钮、绿色原生按钮、工具按钮、图标按钮、局部开关及恢复主题按钮。

回归测试覆盖原生属性、正常/悬停/按下/禁用填充、纯黑白边界、图标对比度、
主题切换与重置、分割按钮区域隔离，以及评分缓存改色。

```bash
cmake --build build/linux-gcc-debug --target ZzPureToolsExample \
  ZzButtonControlsTest ZzSplitButtonTest ZzRatingControlTest \
  ZzFluentStyleTest ZzFluentStandardControlsTest ZzFluentScreenshotTest --parallel 2
ctest --test-dir build/linux-gcc-debug \
  -R '^fluent\.(buttons|split-button|rating-control|style|standard-controls|screenshot-.*)$|^example\.(puretools-integration|fluent-controls)$' \
  --output-on-failure --parallel 2
```

本次使用现有 Linux Qt 6.11.1 / GCC 15 验证；没有下载新 Qt，也没有修改参考项目。
新接口仅使用 Qt 6.8 已提供的跨平台 API。Windows、macOS 的原生运行结果仍需相应平台验证。

2026-09-28 验证结果：21 项定向 CTest 全部通过，包括四档缩放截图、Example 冒烟、
按钮及关联控件、公共头自包含、架构和文档审计；原有截图基线未改写。
另外核对了浅色、深色、高对比模式下紫色、绿色、黄色强调色的实际渲染。
本次未运行完整性能矩阵，不将绘制热路径静态审查等同于性能阈值测量。
