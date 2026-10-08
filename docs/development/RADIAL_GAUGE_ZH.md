# 径向仪表盘实现计划

目标：迁移 FluentUIStyle 的径向仪表盘完整展示，接在音频电平表后。
架构：ZzRadialGauge（QDial）、ZzMultiRadialGauge 和 ZzMultiProgressRing（QWidget）提供可复用绘制与数据接口；示例独立组织预设和属性页。
技术栈：C++20、Qt 6.8+ 公开 API、现有 Fluent 主题。沿用当前分支；用户验收后再提交。

## 全局约束

- 参考仓库 `/home/zz/Jackfahdin/github/FluentUIStyle` 只读；不访问或修改 `docs/research/`。
- 以 ExRadialGauge、ExMultiRadialGauge、ExMultiProgressRing 及 PageRadialGauge 的完整公开属性、几何与示例配置为准，类型前缀改为 Zz。
- 公共类在 ZzFluentUI 命名空间，显式属性与中文文档，私有状态采用 PIMPL；过长实现按绘制/状态拆分。
- 处理非有限数值、无效枚举、退化量程、极小尺寸、动态删除数据项；动画遵守隐藏、禁用及减少动态效果，显式颜色覆盖保留。
- 示例保留经典/进度/彩色区间三种配置、十个 ECharts 风格预设，以及单值仪表/得分环/多值仪表的实时属性页。预设网格适应应用内容宽度，避免固定五列裁切。
- 不引入 Qt 私有 API，不使用自画图标代替现有字体图标。构建目录串行使用。

## 任务 1：核心仪表控件

- [x] 先编写 `ZzFluentUI/tests/ZzRadialGaugeTest.cpp`，验证缺少实现的 RED。
- [x] 新建公共 `ZzRadialGauge.h`（含 Range）、`ZzMultiRadialGauge.h`（含 Item）、`ZzMultiProgressRing.h`（含 Item）以及对应 src/private 文件。
- [x] 单值保留 QDial 鼠标、键盘、滚轮和 tracking；圆弧起止角、主次刻度、数值/单位、指针、轴心、渐变、扫过扇区和彩色区间均可编辑。
- [x] 多值仪表支持独立颜色、偏移、可见性、重叠或同心进度；得分环支持多条环、中央详情与徽标。
- [x] 测试真实交互、禁用交互、动画中断与暂停、项目拥有权/删除、非法值及主题切换。接入库和测试 CMake，运行 `ZzRadialGaugeTest` / `fluent.radial-gauge` 并审查。

## 任务 2：示例与属性编辑

- [x] 新建 `examples/ZzPureToolsExample/ZzExampleRadialGaugePage.cpp`、预设与三类编辑器文件及私有助手头；保持每个文件职责聚焦。
- [x] 保留三种基础配置和公共数值联动；十个 ECharts 风格预设保留参数、颜色和类型，多值与得分环参与公共百分比控制。
- [x] 接入全部实时属性编辑、颜色编辑和重置；使用现有共享卡片/颜色按钮、ZzTabWidget 的 PivotSlide，并关闭新增/转移/撕出能力。
- [x] 接入 `radial-gauge` 路由、页面工厂、构建列表和英文翻译；更新工作区导航烟测为 36 项。
- [x] 实际窗口烟测验证公共数值联动、属性修改与重置、数据编辑；导出完整页面预览。

## 任务 3：外观与交付验证

- [x] 使用真实参考控件，在一致字体、尺寸、静态数值下对比浅色和深色；检查针尖、端帽、刻度间距、扫过渐变和徽标细节。
- [x] 新增三主题 × 四档 DPR 截图基线，覆盖三种基础结构、多值和得分环。
- [x] 构建示例与相关测试，运行核心、音频回归、四档截图、中英文集成和工作区烟测。
- [x] 独立审查并修复重要发现，记录验证结果与平台限制，不自动提交。

## 接入方式

链接 `ZzFluentUI`，按需包含 `ZzRadialGauge.h`、`ZzMultiRadialGauge.h` 或 `ZzMultiProgressRing.h`。

```cpp
auto *gauge = new ZzFluentUI::ZzRadialGauge(parent);
gauge->setRange(0, 100);
gauge->setScaleMode(ZzFluentUI::ZzRadialGauge::RangeScale);
gauge->addRange(0, 60, QColor("#21BCE2"));
gauge->addRange(60, 80, QColor("#FFB900"));
gauge->addRange(80, 100, QColor("#FF6475"));
gauge->setValue(70);

auto *rings = new ZzFluentUI::ZzMultiProgressRing(parent);
auto *cpu = rings->addItem(QStringLiteral("CPU"), 35, QColor("#5470C6"));
cpu->setValue(62);
```

单值仪表继承 `QDial`，支持鼠标、键盘、滚轮及 `tracking`；调用 `setInteractive(false)` 可作为只读显示。
`setValueAnimationDuration(0)` 关闭值动画；隐藏、禁用及减少动态效果也会停止动画并显示目标值。
多值控件拥有添加的数据项；`removeItem`/`clearItems` 延迟销毁数据项，`addItem` 支持在同类型控件间转移。
显式设置的颜色保留，未设置的颜色跟随主题。多值仪表的徽标文字默认自动选择黑/白对比色。

示例入口为“自定义控件 → 径向仪表盘”，路由 `radial-gauge`。
十种预设按窗口宽度自动换行；实时属性页提供单仪表、得分环、多值仪表三组编辑器。

## 验证记录（2026-10-08）

- Linux / GCC 15 / Qt 6.11.1：示例、工作区烟测、仪表行为测试和截图测试均构建成功。
- 仪表单元测试 18/18：实际输入、tracking、角度/量程、非法数值、小尺寸绘制、动画中断与连续动画、数据项转移/删除/重入、有效值通知。
- 最终 CTest 12/12：仪表、音频电平表、边框光束、四档 DPR 截图、中英文完整路由烟测、工作区烟测、公开头文件和组件依赖边界。
- 中英文应用完整路由运行均退出 0；浅色/深色完整页面和三类编辑器已导出。两个 integration 测试改为等待烟测自行完成，移除原先可能掩盖失败的 100ms 自动退出。
- 控件参考对照：六种组合在相同字体、尺寸、静态输入下比较真实 FluentUIStyle 控件；浅色差异 263/427440 像素，深色 271/427440，主要为徽标自动黑白文字及细小栅格差异，几何保持参考。
- 12 张新基线覆盖浅色、深色、高对比度 × DPR 1/1.25/1.5/2；未更新其它控件基线。
- 核心、页面定向复审及最终整项审查均通过，无遗留问题；对象接管/连续动画及主题色块问题已用真实失败→通过验证。
- 本机没有验证 Windows/MSVC；未运行 ASan。
