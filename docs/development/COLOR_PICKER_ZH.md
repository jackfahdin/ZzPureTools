# 颜色选择器

`ZzColorPicker` 提供内嵌颜色编辑；`ZzColorPickerButton` 将同一个选择器装配为即时生效的弹层；`ZzColorPickerDialog` 提供确定提交和取消回滚。正式示例入口为“颜色选择器(ColorPicker)”，路由 `color-picker` 紧随 `carousel`。

## 内嵌选择器

```cpp
#include <ZzFluentUI/ZzColorPicker.h>

auto *picker = new ZzFluentUI::ZzColorPicker(parent);
picker->setAppearance(ZzFluentUI::ZzColorPicker::Fluent);
picker->setAlphaEnabled(true);
picker->setCurrentColor(QColor("#944E9B"));
picker->setColorSpectrumShape(ZzFluentUI::ZzColorPicker::Ring);
picker->setColorRepresentation(ZzFluentUI::ZzColorPicker::Hsva);
connect(picker, &ZzFluentUI::ZzColorPicker::currentColorChanged,
        receiver, updateColor);
```

Fluent 外观包含颜色预览与可点击明暗色阶，以及色谱、色板、滑条三个页签。色谱支持 `Box` 和 `Ring`，方向键调整色相/饱和度，Shift 增大步长。左右竖向滑条分别调整明度与透明度。通道页可切换 `Rgba`/`Hsva`，HEX 使用 `#RRGGBB` 或启用透明度时的 `#AARRGGBB`。

以下显示属性相互独立，关闭当前页对应内容后自动切到仍可用的页签：

| Getter / Setter 后缀 | 内容 |
| --- | --- |
| `ColorSpectrumVisible` | 色谱页 |
| `ColorPaletteVisible` | 色板页 |
| `ColorPreviewVisible` | 顶部当前色与明暗色阶 |
| `AlphaSliderVisible` | 色谱页透明度条和通道页透明度滑条 |
| `ColorSliderVisible` | 整个通道页及其页签的可用性 |
| `ColorChannelTextInputVisible` | 通道页数值输入框和 HEX 输入框 |

Getter 使用 `isX()`，Setter 使用 `setX(bool)`，每项都有 `xChanged(bool)` 信号。`setPaletteColors()` 可设置自定义色板；`paletteColors()` 返回快照，`resetPaletteColors()` 恢复兼容默认色板。

示例页使用参考 48 色板，并可切换为自定义 6 色或组件默认 24 色。属性编辑器只控制内嵌实例，重置会同时恢复内嵌、按钮和对话框的初始颜色。按钮示例初值为 `#FFB900`，对话框初值为 `#0078D4`。

## 按钮弹层

```cpp
#include <ZzFluentUI/ZzColorPickerButton.h>

auto *button = new ZzFluentUI::ZzColorPickerButton(parent);
button->setSelectedColor(QColor("#FFB900"));
button->colorPicker()->setColorSpectrumShape(ZzFluentUI::ZzColorPicker::Ring);
connect(button, &ZzFluentUI::ZzColorPickerButton::selectedColorChanged,
        receiver, updateColor);
```

按钮默认使用 Fluent 外观并启用透明度。`colorPicker()` 返回固定复用实例的非拥有指针。弹层内编辑即时更新 `selectedColor`；再次点击、外部点击或 Escape 关闭弹层，关闭后恢复按钮焦点。弹层位置受屏幕可用区域约束。

## 确认对话框

```cpp
#include <ZzFluentUI/ZzColorPickerDialog.h>

auto *dialog = new ZzFluentUI::ZzColorPickerDialog(parent);
dialog->setTitle(tr("编辑颜色"));
dialog->setCurrentColor(QColor("#0078D4"));
connect(dialog, &ZzFluentUI::ZzColorPickerDialog::currentColorChanged,
        receiver, previewColor);
connect(dialog, &ZzFluentUI::ZzColorPickerDialog::colorSelected,
        receiver, commitColor);
dialog->open();
```

对话框默认使用 Fluent 外观并启用透明度。`currentColorChanged` 用于即时预览；`colorSelected` 仅在确定时发送。每次显示都会记录当前颜色，取消、Escape 或窗口关闭恢复本次打开初值。取消后，已发布的预览也会收到回滚通知。

实际 HEX 失焦立即更新颜色状态，并将颜色通知延迟到安全时机。在清焦点返回后，`commitPendingEdits()` 可安全同步冲刷待发通知，且不运行嵌套事件循环。对话框确定时已负责提交待编辑 HEX；普通调用者不需要查找或操作内部编辑器。颜色信号允许接收方同步删除控件；信号之后继续访问时应使用 `QPointer` 保护。

## 兼容与验证

默认 `ZzColorPicker` 仍是 `Compact`，默认关闭透明度编辑，原 24 色色板不变。无效颜色被拒绝，颜色规范化为 8 位 RGBA，同值不通知。关闭透明度编辑仅影响编辑入口，保留当前颜色的 alpha。色板过滤无效值和重复 RGBA，最多保留 256 色。灰色/黑色编辑保留有意义的色相。

没有新增运行时图片、字体或其他二进制资源。三页签和下拉箭头使用项目已内嵌的 Segoe Fluent Icons TTF；示例参考色板只是数据。新增 PNG 仅用于视觉测试基线。

针对性验证包含颜色核心、按钮/对话框、工作区真实输入、中英文路由烟测和公共头文件检查。新增视觉测试覆盖 Box、Ring、Palette、Channels、Button、Dialog 的浅色、深色、高对比主题及 100%、125%、150%、200% DPR；兼容 Compact 基线不更新。

验证环境为 Linux、Qt 6.11.1、GCC 15.2。Windows 与 Qt 6.8 尚未验证。
