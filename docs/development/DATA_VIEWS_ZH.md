# 数据视图外观

## 外观与实现

以只读参考项目 `FluentUIStyle/fluentui3style/fluentui3style.cpp` 的
`CE_ItemViewItem`、`PE_PanelItemViewRow`、`PE_IndicatorBranch`、
`CE_HeaderSection` 和 `CT_ItemViewItem` 为依据。

- 列表：基础行高 32，浅色黑色 4% 悬停 / 5.5% 选中，深色白色
  6.05% 悬停 / 4.19% 选中；内缩 2 的圆角背板、leading 指示条。
- 表格：行选择使用强调色透明填充和贯穿可见列的边框，编辑时只保留填充；
  单元格选择保持单元格语义，不额外绘制列表指示条。表头为平面填充和细分隔线。
- 树：圆角行背景贯穿分支和内容区，使用折角箭头，支持无根装饰、RTL、
  多列和交替行；选中指示条位于行 leading 侧，默认层级缩进 30。
- 普通 `QStyledItemDelegate` 和 `ZzFluentItemDelegate` 复用库级样式。
  专用导航、组合框弹出列表、日历继续使用原有处理路径。
- 保留模型 BackgroundRole / ForegroundRole、复选框命中、编辑器布局与键盘焦点；
  高对比模式使用 palette Highlight / HighlightedText。
- 默认交替行使用 Window 背景，显式设置的 AlternateBase 优先；此处理仅在
  数据视图内部生效，不修改应用调色板。弹出建议列表保持原有外观。

落点：私有 `ZzDataViewStylePrivate` 承担数据视图几何与绘制；
`ZzFluentStyle` 路由绘制、内容尺寸与表格悬停事件；delegate 复用该路由。
三个 Example 数据页使用紧凑密度、滚动和合理列宽，不加入样式表补丁。

验证顺序：先运行真实视图回归，确认跨列选中断缝及无装饰树问题；实现后运行
数据视图、delegate、选择动画、导航、日历、组合框与 Example 冒烟测试。
使用相同 Qt、字体和数据生成参考 / 当前浅深色对照图，检查实际像素结果。

## 验证记录

Linux、GCC 15、Qt 6.11.1 下 Example 构建成功，16 项 CTest 全部通过：
数据视图、delegate、选择动画、标准控件、导航面板、导航控件、日历、组合框、
侧面板、中英文 Example、工作区冒烟及 100% / 125% / 150% / 200% 截图。

新增回归覆盖隐藏和移动列后的 LTR / RTL 行边框、无根装饰与深层窄列树、
模型背景覆盖、整行悬停与离开、编辑时边框隐藏、高对比文字与树箭头、复选框命中。
基础截图和标准控件广度截图中包含的数据视图外观已更新，共 24 张基线；
其余卡片、标签页、搜索建议等基线保持不变。

本地人工对照产物：`build/data-view-reference/comparison-light.png`、
`build/data-view-reference/comparison-dark.png`。
真实页面截图位于 `build/data-view-preview/{list,table,tree}-view.png`。
参考项目只读，未修改其源码。未在 Windows、macOS 或 Qt 6.8 实机运行。
