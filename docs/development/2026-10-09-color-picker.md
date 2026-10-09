# Fluent 颜色选择器设计与实现计划

> 使用 subagent-driven-development 逐任务实施、审查，使用 test-driven-development 和 verification-before-completion 验证。用户已批准完整范围；完成后等待用户要求提交，不自动提交或上传。

**目标：** 扩展已有 ZzColorPicker，对齐 FluentUIStyle 的三页颜色编辑，提供内嵌、按钮弹层、确认对话框和独立示例。

**架构：** ZzColorPicker 继续是 QWidget，currentColor 是唯一 RGBA 真值。色谱和渐变滑条为私有绘制部件；按钮和对话框组合同一个选择器。现有 Compact 呈现保留，新增 Fluent 呈现用于完整三页体验，兼容旧 Gallery 和旧截图。

**技术栈：** C++20、Qt 6.8+ 公共 API；本机 Linux Qt 6.11.1 验证。

## 全局约束与已确认设计

- 仓库 /home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame，参考 ../FluentUIStyle 只读。不读取、修改或暂存 docs/research；不提交本地图片。
- 当前干净 checkout 使用 codex/color-picker 分支，复用已有 build/linux-gcc-debug。所有编辑使用 apply_patch；不提交、不推送。
- 参考 ExWidgets/color/excolorpicker.cpp、excolorpickerbutton.cpp、excolorpickerdialog.cpp 和 Examples/Gallery/pages/basic/pagecolor.cpp。
- Fluent 外观：顶部当前色和可点击色阶，36 px 三分段 TTF 图标页签；色谱页左侧竖向明度条、中间 240 px 高 Hue×Saturation 色谱、右侧竖向透明度条；色板页规则网格；滑条页 RGB/HSV 下拉和 HEX，通道行依次为标签、数值框、渐变滑条。目标宽约 360 px，支持缩放与布局方向。布局以运行参考控件得到的截图为准。
- 方形和圆形色谱都可拖动、键盘操作。灰色或黑色编辑保留有意义的 hue，避免重新增大饱和度/明度后跳色。
- 使用已内嵌 Segoe Fluent Icons TTF 的 InkingTool、Color、Equalizer、ChevronDown 字形，码点从参考枚举核实；不引入图片或画替代图标。
- 保持已有 currentColor、alphaEnabled、paletteColors 接口：无效颜色拒绝、RGBA 8 位归一、同值不发信号、alphaEnabled 仅控制编辑而不清除 alpha；最多 256 项色板、过滤无效与重复值。默认 Compact、alphaEnabled=false、原 24 色色板不变。Fluent 示例可使用参考标准色板。
- 新枚举在 ZzColorPicker 内定义：Appearance { Compact, Fluent }、ColorRepresentation { Rgba, Hsva }、ColorSpectrumShape { Box, Ring }。各有 getter/setter、Q_PROPERTY 和 changed 信号。
- 新显示属性：colorSpectrumVisible、colorPaletteVisible、colorPreviewVisible、alphaSliderVisible、colorSliderVisible、colorChannelTextInputVisible，配 isX/setX 与 changed；默认 true。关闭当前页自动选择可见页，全部关闭无隐藏页残留。
- 色谱/渐变图缓存按尺寸、DPR 与颜色依赖失效；拖动时不分配无限 QObject，不创建定时器。主题边框、文字、焦点使用项目主题，颜色内容本身不被主题改色。无障碍名称、焦点、RTL、禁用和语言切换必须有效。
- 按钮弹层即时修改颜色，外部点击或 Escape 关闭且恢复焦点；点击同一按钮收起；定位限制在屏幕可用区域，重复打开复用实例。
- 对话框即时预览、确定才发 colorSelected；取消/Escape/关闭恢复本次打开初值；确定前提交待编辑 HEX；信号回调同步删除或重入安全。
- 不增加屏幕吸管、最近颜色持久化或无关页面批量改造。

## Task 1: 内嵌选择器、色谱与渐变滑条

**文件：** 修改 widgets/include/ZzFluentUI/ZzColorPicker.h、widgets/src/ZzColorPicker.cpp、widgets/src/private/ZzColorPickerPrivate.{h,cpp}、foundation/include/ZzFluentUI/ZzSegoeIconFont.h（均位于 ZzFluentUI）；增加私有 ZzColorSpectrum、ZzColorGradientSlider 和 Fluent 装配源文件，职责独立；接入 ZzFluentUI/CMakeLists.txt。测试 ZzFluentUI/tests/ZzColorPickerTest.cpp。

- [x] 先为缺失的 Fluent 实际交互写失败测试：通过 setProperty("appearance", 1) 启用后查找三页 tab、点击色谱改变颜色、键盘修改颜色。当前实现应因功能缺失失败，不依赖编译错误作为 RED。
```cpp
ZzColorPicker picker;
picker.setProperty("appearance", 1);
picker.show();
auto *tabs = picker.findChild<QTabBar *>("zzColorPickerTabs");
QVERIFY(tabs);
QCOMPARE(tabs->count(), 3);
```
- [x] 使用头部定义的枚举和属性扩展，拆分私有绘制与装配；Compact 保留旧装配。保持唯一模型、编辑器，切换外观不增长对象数、不丢失颜色和自定义色板。
- [x] 验证 RGB/HSV/HEX/色谱/透明度联动、同值去重、非法输入恢复、黑灰色 hue 保留、切页和隐藏回退、拖动边界、RTL、键盘和禁用；验证现有 Compact 测试继续通过。
- [x] 构建运行：cmake --build build/linux-gcc-debug --target ZzColorPickerTest -j6；ctest --test-dir build/linux-gcc-debug -R '^fluent.color-picker$' --output-on-failure。
- [x] 保存 RED/GREEN 输出、修改清单和 API 到任务报告，接受任务审查。

## Task 2: 颜色按钮弹层和确认对话框

**文件：** 新增 ZzFluentUI/widgets/include/ZzFluentUI/ZzColorPickerButton.h、ZzColorPickerDialog.h，对应 widgets/src 与 private 实现；更新 ZzFluentUI/CMakeLists.txt；新增 tests/ZzColorPickerSurfacesTest.cpp 与测试目标。

- [x] Button 继承 QToolButton，selectedColor()/setSelectedColor(QColor)、selectedColorChanged(const QColor&)、colorPicker() 返回稳定选择器供配置；Dialog 继承 QDialog，title/setTitle、currentColor/setCurrentColor、currentColorChanged、colorSelected、colorPicker、done(int)。二者使用 Fluent 外观，默认启用 alpha，组件按 parent 所有。
- [x] 先建可编译 API 壳和真实交互失败测试：点击按钮可显示选择器、修改仅通知一次、外部点击/Escape 恢复焦点；dialog reject 恢复初值且不提交、accept 提交未失焦 HEX。
```cpp
ZzColorPickerDialog dialog;
dialog.setCurrentColor(QColor("#0078d4"));
dialog.show();
dialog.colorPicker()->setCurrentColor(Qt::red);
dialog.reject();
QCOMPARE(dialog.currentColor(), QColor("#0078d4"));
```
- [x] 用 palette/style/theme 绘制色块按钮和双等宽 Fluent 对话框底部；可复用私有主题/弹层基础设施，避免复制颜色状态计算。QPointer 护卫跨外部信号访问，避免双通知、已销毁对象访问和嵌套结束。
- [x] 验证重复开关对象稳定、父窗口销毁、信号同步销毁/重入、屏幕边缘和 RTL 布局。构建 ZzColorPickerSurfacesTest，运行 fluent.color-picker-surfaces 与 fluent.color-picker。
- [x] 保存 RED/GREEN 与 API 报告，接受任务审查。

## Task 3: 独立示例、翻译、截图与文档

**文件：** 新增 examples/ZzPureToolsExample/ZzExampleColorPickerPage.cpp、ZzExampleColorPickerSmoke.{h,cpp}；修改页面工厂、路由目录、Showcase 声明/装配、CMakeLists、SmokeControllerPrivate、tests/ZzExampleWorkspaceSmokeTest.cpp、translations/ZzPureToolsExample_en.ts；修改 ZzFluentUI/tests/ZzFluentScreenshotTest.cpp，添加新基线；新增 docs/development/COLOR_PICKER_ZH.md。

- [x] 先为 route color-picker 在 carousel 后出现、真实页面可创建编写 workspace 失败测试。路由标题“颜色选择器(ColorPicker)”，数量 41。
- [x] 内嵌、按钮弹层、对话框三张演示卡；使用参考紫色 #944E9B（内嵌）、#FFB900（按钮）、#0078D4（对话框），配置 alpha、显示项、Box/Ring、RGBA/HSVA、自定义色板、禁用、RTL，提供重置和 API 示例。参考标准色板可作为示例数据，组件原默认保持不变。
- [x] 真正的操作烟测覆盖颜色输入、切换形状/表示、开关显示项、弹层开关、对话框确认/取消、重置；中英文文本均可读且布局不溢出。
- [x] 添加色谱、色板、通道、按钮、对话框三主题截图；100/125/150/200% DPR 验证新呈现，旧 Compact 相关基线不变。先看实际截图与参考布局，再接受基线。
- [x] 构建示例、相关测试、截图测试；运行 color-picker 两套、workspace-smoke、中英文 puretools-integration、architecture.public-headers；文档记录 API、兼容语义、资源无新增、未测 Windows/Qt6.8。
- [x] 保存任务报告，任务审查后进行整个改动的最终审查与针对性验证；回报完成但不提交。

## 验收记录

2026-10-09 完成三项实施与逐项审查，最终整体审查通过，无待修复发现。额外回归覆盖真实 HEX 失焦后的安全通知、HSV 透明度精度、弹层小屏幕滚动与鼠标事件，以及隐藏后恢复页签的实际几何。

- Linux Qt 6.11.1 / GCC 15.2：示例、核心与弹层测试、工作区测试、截图测试构建通过。
- 9 项相关 CTest 通过：颜色选择器两套、工作区、中英文示例集成、字体、公共头文件及两项组件依赖边界。
- 新增 72 组截图比较与旧 Compact 相关 12 组比较通过；覆盖浅色、深色、高对比度及 100/125/150/200% DPR。旧截图基线未修改。
- Xvfb 正常尺寸弹层测试通过；200×160、390×490 的屏幕范围与内容可达性检查通过。
- 中英文真实页面预览已人工检查；没有新增运行时图片或字体资源。
- 未测试 Windows / Qt 6.8；未提交或推送。使用接口参见 [颜色选择器指南](COLOR_PICKER_ZH.md)。
