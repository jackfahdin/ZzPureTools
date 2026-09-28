# 工具按钮与单选框外观改进

**目标：** 参考 FluentUIStyle 的工具按钮表面层级和单选圆环状态，改进库样式及独立演示页。

**范围：** 使用现有 QToolButton、QRadioButton 和 ZzFluentStyle，不引入外部样式库。保留 Qt 菜单、键盘、互斥与无障碍行为；标题栏保留轻量按钮及取消选中框的约定。

参考：`FluentUIStyle/fluentui3style/fluentui3style.cpp` 的 `PE_PanelButtonTool`、`PE_IndicatorRadioButton`、`radioButtonInnerRadius`；参考其视觉与状态关系，使用本项目主题色和绘制实现。

## 实施

- [x] 在 `ZzCommandStatusSurfacesTest.cpp` 检查普通工具按钮的静止表面与轻量透明表面不同；在 `ZzFluentStandardControlsTest.cpp` 检查单选框悬停、按下、禁用状态。运行旧实现确认失败。
- [x] 修改 `ZzFluentStyle.cpp` 的单选框尺寸、间距和工具按钮箭头；修改 `ZzFluentStylePrivate.h/.cpp` 的单选框独立绘制与工具按钮普通/轻量/强调色表面。圆环选中采用强调色，中心跟随主题，hover 放大、press 缩小；禁用不响应 hover/press。不新增常驻动画和计时器。
- [x] 修改 `ZzExampleControlPagePrivate.cpp`，工具按钮展示图标、图文、轻量、强调色和菜单；单选框按组纵向展示并加入已选禁用状态。同步英文翻译。
- [x] 编译 Debug Example 和相关测试；复验选择语义、工具栏、强调色及截图，人工检查两页实际截图，必要时仅更新受影响基线，保持阈值不变。
- [x] 更新验证记录，以中文标题和详细正文提交，不推送。Windows/macOS 仅静态检查。

## 验证命令

```bash
cmake --build build/linux-gcc-debug --target ZzFluentStandardControlsTest ZzCommandStatusSurfacesTest ZzPureToolsExample --parallel 2
ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^fluent\.(standard-controls|command-status-surfaces)$'
ZZ_EXAMPLE_ROUTE_SCREENSHOT_DIR="$PWD/build/linux-gcc-debug/control-page-previews" ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^example\.puretools-integration$'
```

## 验证结果

- Linux GCC 15 / Qt 6.11.1 Debug：Example 和相关测试目标编译通过。
- 先确认普通按钮静止无表面、单选框 hover 无变化、即时菜单箭头区重叠三项回归失败，修改后通过。额外验证 36px 窄菜单按钮可完整绘制 16×16 图标，避免标题栏图标被挤没。
- 最终相关 CTest 26/26 通过，覆盖标准控件、工具栏/命令栏、标题栏、按钮、无障碍、全部 Example 烟测和控件/Example 四档截图。
- 仅重生成 ZzFluentUI 的综合、标准控件广度、命令与状态三个场景 × 三主题 × 四档 DPR，共 36 张基线；关闭更新模式复验通过，未调整阈值。Example 原有基线未修改。
- 实际查看工具按钮、单选框独立页面截图，以及综合场景深色/高对比、200% 浅色、标准控件和命令工具栏截图。菜单箭头调整为同行尾部，图标空间不足时收窄箭头区。
- 独立代码审查未发现阻塞项，单选互斥、Qt 菜单命中、RTL 和焦点行为保留。Windows/macOS 仅静态检查，未宣称实机或性能门禁通过。
