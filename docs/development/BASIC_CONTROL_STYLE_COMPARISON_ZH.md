# 基础控件与 FluentUIStyle 的对比改进

日期：2026-09-28。范围按 Example 的“基础控件”17 个入口核查，保留 Qt 原生交互与本项目性能约束。参考源码只读，不引入其样式库或整套动画系统。

| 控件 | 参考实现 / 本项目实现 | 对比结论与处理 |
| --- | --- | --- |
| PushButton | `CE_PushButton*` / `drawPushButton` | 保留普通、强调色与选中配色；轻量款不应在 hover 或禁用时突然出现框，调整表面和边框。 |
| IconButton | 图标 QToolButton / `ZzIconButton` | 修复显式 Standard 仍被 autoRaise 当作透明款；演示采用紧凑尺寸，保留库的自适应图标能力。 |
| ToolButton | `CC_ToolButton` / `drawToolButtonPanel/WithMenu` | 上轮已统一圆角、菜单箭头；本轮复核显式外观与 autoRaise 的优先级。 |
| RadioButton | `PE_IndicatorRadioButton` / `drawRadioIndicator` | 上轮圆环、状态反馈及禁用已完成；复验并统一选择控件演示排布。 |
| CheckBox | `drawCheckBox` / `drawCheckIndicator` | 增加 hover/press；强调色勾选及半选不再叠深色描边，禁用使用中性色，统一标签间距。 |
| ToggleSwitch | `drawSwitchButton` / `ZzToggleSwitch` | 关闭态采用描边轨道与较小滑块；hover 放大、press 拉伸；保留单个滑移动画、整轨道/文字命中及 RTL。 |
| LineEdit | `drawLineEditFrame` / `drawInputPanel` | 参考细边框和底部焦点线；带框输入额外保留两侧 8px 留白，尺寸提示同步，无框嵌入编辑器不重复加边距；保留只读、禁用与输入行为。 |
| PlainTextEdit | 带框输入绘制 / `drawInputPanel` | 与单行输入统一边框，检查 viewport 对圆角的覆盖。 |
| ComboBox | `CC_ComboBox` / `drawComboBox` | 普通框采用按钮表面，可编辑框保留输入焦点线；标签单次绘制，统一原生下拉列表及高对比选中前景。保留独占箭头区、长文本预留和 RTL，详见 [改进记录](../superpowers/plans/2026-09-29-combo-box-fluent-refresh.md)。 |
| MultiSelectComboBox | 参考项目无等价原生基础控件 / `ZzMultiSelectComboBox` | 保留筛选、多选与摘要逻辑，沿用 ComboBox 的统一外观。 |
| SpinBox | `CC_SpinBox` / `drawSpinBox` | 支持 Vertical、HorizontalSides、HorizontalRight、PlusMinusHorizontalSides；默认右侧横排，共用尺寸和命中几何、字体图标缓存及单一输入底线。[中文语义与用法](SPIN_BOX_LAYOUTS_ZH.md)。 |
| DoubleSpinBox | 同上 / `ZzDoubleSpinBox` | 与整数输入统一，保留小数精度、范围与输入校验。 |
| CalendarPicker | 日期编辑和日历代理 / `ZzCalendarPicker`、`ZzCalendar` | 保留可用的弹出日历，复验边框、日期导航和键盘。 |
| RollerPicker | 本项目滚轮选择扩展 / `ZzRollerPicker`、`ZzRoller` | 不以普通时间编辑替换滚轮；检查入口、弹层、选中行与主题一致性。 |
| Slider | `CC_Slider` / `drawSlider` | 原来将轨道厚度误用为控件厚度，手柄被裁切；已修复默认尺寸，采用外壳+强调色内圆及 hover/press 反馈；键盘焦点描边收在手柄内，避免两端裁切。 |
| ProgressBar | `CE_ProgressBar*` / `drawProgressBar` | 已有独立细线、圆端、忙碌设计，保留；复验主题、禁用、纵向及数值联动。 |
| ProgressRing | `drawProgressRing` / `ZzProgressRing` | 保留独立环形组件与已有动画生命周期，复验确定/忙碌与禁用。 |

## 实施与验收

- [x] 先用绘制/实际控件回归测试复现显式外观、复选状态、开关反馈、输入焦点与留白、滑块裁切。
- [x] 在库内修复表面与尺寸；只为演示调整排布、禁用样例与图标尺寸，不用 Example 样式表替代库实现。
- [x] 更新 Linux Debug Example；检查全部 17 页，复验控件语义、主题和四档截图，受影响基线审阅后更新，不放宽阈值。
- [x] 独立审查补充高对比禁用强调色按钮轮廓、滑块键盘焦点边缘回归；保留原生命中和交互逻辑。

## 验证记录与边界

- 环境：Linux x86_64、Qt 6.11.1、GCC 15.2.0、`linux-gcc-debug`，使用现有 Qt，无新增下载或依赖。
- 全量本机最小门禁：216 项普通 CTest；其中基础控件定向复验 10 项。新增绘制与几何测试均先验证修改前失败，再验证修复。
- 截图：控件、工作区、综合 Example 的 100%、125%、150%、200% 共 12 项 CTest。只更新控件的 7 个受影响场景 × 3 主题 × 4 DPR，共 84 张 PNG；工作区、Example 基线未改，阈值未改。
- 受影响场景为综合控件、标准控件广度、文本输入、组合框、数值输入、搜索建议和多选组合框。后三类及搜索建议共同复用输入/复选绘制，因此随公共样式同步。
- 人工查看 17 个独立页面、三主题综合控件与输入场景、200% 标准控件图；四档自动比较覆盖 RTL、只读、禁用和弹层场景。
- 菜单禁用测试改为直接比较禁用状态下选择前后图像，避免把合法的中性色当成悬停错误；同时修正高对比禁用项被套用选中文字颜色的问题。
- 未新增每次交互的动画实例、定时器或绘制时资源解析；现有动画生命周期、原生语义、无障碍和基础设施数量测试通过。未运行专项性能矩阵，不以此声称新的性能门禁通过。
- Windows/macOS 本轮仅静态核对 Qt API 和类型使用；原生 CI、ASan/UBSan、clang-tidy、专项性能和物理桌面人工验收均待验证。

本机运行入口：

```bash
./build/linux-gcc-debug/examples/ZzPureToolsExample/ZzPureToolsExample
```

基础控件各自的页面仍保留中文（英文控件名）导航；本轮没有增加公开 API，也没有修改参考项目。
