# 基础控件独立演示页

在 ZzPureToolsExample 的 Activity Bar 点击“组件”，展开“基础控件”，选择对应条目。
每个控件拥有独立路由和中央页面，同一控件的不同状态放在同页，页面没有外层标签。
中文界面以“中文名(EnglishName)”展示导航和页面标题；英文界面使用英文控件名。

顺序参考 FluentUIStyle Gallery 基础页的布局行序，并在相应位置插入本项目扩展控件。

| 顺序 | 导航名称 | 路由 |
| --- | --- | --- |
| 1 | 按钮(PushButton) | `push-button` |
| 2 | 图标按钮(IconButton) | `icon-button` |
| 3 | 工具按钮(ToolButton) | `tool-button` |
| 4 | 单选框(RadioButton) | `radio-button` |
| 5 | 复选框(CheckBox) | `check-box` |
| 6 | 开关(ToggleSwitch) | `toggle-switch` |
| 7 | 单行输入(LineEdit) | `line-edit` |
| 8 | 多行输入(PlainTextEdit) | `plain-text-edit` |
| 9 | 组合框(ComboBox) | `combo-box` |
| 10 | 多选组合框(MultiSelectComboBox) | `multi-select-combo-box` |
| 11 | 整数输入(SpinBox) | `spin-box` |
| 12 | 小数输入(DoubleSpinBox) | `double-spin-box` |
| 13 | 日期选择(CalendarPicker) | `calendar-picker` |
| 14 | 滚轮选择(RollerPicker) | `roller-picker` |
| 15 | 滑块(Slider) | `slider` |
| 16 | 进度条(ProgressBar) | `progress-bar` |
| 17 | 进度环(ProgressRing) | `progress-ring` |

“消息条(MessageBar)”和“信息徽标(InfoBadge)”各有独立页面，归到“反馈与状态”；
旧的跨控件指示条对比移到“交互 → 选中指示条对比”。原 `controls` 混合页面已删除，
首页基础控件快捷入口导航到 `push-button`。本轮不拆分卡片、数据视图等其他组合演示页。

## 实现位置

单行输入页的密码示例复用库中的 `ZzPasswordBox`，使用 `Toggle` 模式：默认隐藏，
输入后点击尾部 `Eye` 图标显示，再点击 `EyeSlash` 隐藏。按钮支持键盘空格，
文字、光标和选区保持；清空、禁用、隐藏或窗口失活后恢复隐藏。
密码框只保留查看按钮，普通输入框继续提供原生清除按钮。

```cpp
#include <ZzFluentUI/ZzPasswordBox.h>

auto *password = new ZzFluentUI::ZzPasswordBox(parent);
password->setRevealMode(ZzFluentUI::ZzPasswordRevealMode::Toggle);
password->setPlaceholderText(tr("输入密码"));
```

库的默认模式仍是 `Peek`（按住查看），调用方也可选择 `Hidden` 或 `Visible`。
这些行为全部由库组件提供，示例只负责选择模式与展示。

- `examples/ZzPureToolsExample/ZzExampleRouteCatalog.cpp`：路由、双语标题、分区和展示顺序。
- `ZzExampleControlKind.h`：独立控件页的类型。
- `ZzExampleControlPage.h/.cpp`：纯展示页面接口。
- `ZzExampleControlPagePrivate.h/.cpp`：统一排版、各类预览及本地交互。
- `ZzExamplePageFactory.cpp`：按路由类型创建页面实例。

所有控件页使用既有 `Recreatable` 生命周期和有界缓存。只构造当前路由的控件，不先创建
整个旧混合页再隐藏其他控件。开关、数值反馈等仅修改本页展示状态，不访问应用业务模型。

性能轮转中的旧 `controls` 路由改为 `push-button`，展示负载已有变化；该场景的耗时不能
直接作为旧混合页性能的等价比较。本轮未重设性能阈值或宣称性能门禁结果。

## 验证与查看

```bash
cmake --build build/linux-gcc-debug \
  --target ZzPureToolsExample ZzExampleWorkspaceSmokeTest --parallel 2
ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^example\.'
./build/linux-gcc-debug/examples/ZzPureToolsExample/ZzPureToolsExample
```

可选导出所有路由的真实窗口截图，产物留在 build 目录，不作为新基线：

```bash
ZZ_EXAMPLE_ROUTE_SCREENSHOT_DIR="$PWD/build/linux-gcc-debug/control-page-previews" \
ctest --test-dir build/linux-gcc-debug --output-on-failure \
  -R '^example\.puretools-integration$'
```

路由烟测检查页面可达和专属控件构成、按钮切换、进度数值与文本同步；工作区测试同时验证
导航变长出现滚动条时仍填满侧面板可用宽度。首页截图基线按原有四档 DPR 与三主题更新，
比较阈值保持不变。Windows 与 macOS 仍需在对应平台实机验证。
