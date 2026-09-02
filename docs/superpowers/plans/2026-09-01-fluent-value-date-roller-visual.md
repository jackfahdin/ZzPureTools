# ZzFluentUI 数值、日期与滚轮视觉统一实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans）逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 在不改变公开 API、Qt 原生交互语义和 Picker 事务行为的前提下，统一 `ZzSpinBox`、`ZzDoubleSpinBox`、`ZzCalendarPicker`、`ZzCalendar`、`ZzRollerPicker` 与 `ZzRoller` 的 Fluent 表面、状态、几何和高 DPI 表现。

**架构：** 继续由 `ZzFluentStylePrivate` 负责数值输入框的公共绘制和子控件几何，由 `ZzCalendarPrivate` 与 `ZzRoller` 负责各自内容绘制；两个 Picker 只负责输入表面、弹层布局和事务，不复制日期或滚轮模型。颜色从现有 `ZzThemeSnapshot`/`QPalette` 读取，DPR 使用设备像素换算，所有状态变化只触发必要的重绘。

**技术栈：** Qt 6.8+ Widgets、C++20、现有 `ZzFluentStyle`、`ZzThemeController`、Qt Test/CTest、现有 clang-tidy 和 benchmark 门禁。

---

### 任务 1：统一整数与浮点数值输入表面

**文件：**
- 修改：`ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp` 中 `drawSpinBox()`、`spinBoxSubControlRect()`、`hitTestSpinBox()`。
- 测试：`ZzFluentUI/tests/ZzSpinBoxControlsTest.cpp`。

- [x] **步骤 1：编写失败的行为和几何测试**

在 `ZzSpinBoxControlsTest` 中增加以下断言：`ZzSpinBox` 和 `ZzDoubleSpinBox` 在 36px 高度下的编辑区与上下按钮矩形互不重叠；按钮宽度在 28px 以内且两个矩形均可命中；RTL 下按钮位于视觉左侧；焦点状态只改变渲染像素而不改变 `sizeHint()`。使用现有 `QStyleOptionSpinBox` 调用样式的 `subControlRect()` 和 `hitTestComplexControl()`，并渲染浅色、深色、高对比三种 palette，断言图像包含非背景像素。

- [x] **步骤 2：运行测试确认失败**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzSpinBoxControlsTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\\.spin-box-controls$' --output-on-failure
```

预期：新增的按钮命中、RTL 几何或焦点渲染断言至少一项失败，记录失败断言后再实现。

- [x] **步骤 3：编写最小实现代码**

在 `drawSpinBox()` 中按启用、悬停、按下和 `stepEnabled` 选择局部填充与 glyph 颜色；使用 `devicePixelRatioF()` 将焦点底线和 glyph 线宽换算到约一个设备像素。保持编辑区由 `spinBoxSubControlRect()` 计算，并通过 `QStyle::visualRect()` 映射 RTL。按钮命中区域沿用同一矩形，禁止在绘制中修改控件尺寸或创建对象。

- [x] **步骤 4：运行测试确认通过**

重复步骤 2，预期 `fluent.spin-box-controls` 全部通过，并且 `findChildren<QAbstractAnimation*>` 与 `findChildren<QTimer*>` 数量保持原值。

- [x] **步骤 5：Commit**

```bash
git add ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp ZzFluentUI/tests/ZzSpinBoxControlsTest.cpp
git commit -m "fix(数值控件): 统一输入表面与步进命中"
```

### 任务 2：优化日历日期单元格和高 DPI 状态

**文件：**
- 修改：`ZzFluentUI/widgets/src/private/ZzCalendarPrivate.cpp` 的 `paintCell()`；必要时仅调整 `ZzFluentUI/widgets/src/ZzCalendar.cpp` 的刷新路径。
- 测试：`ZzFluentUI/tests/ZzCalendarControlsTest.cpp`、`ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`。

- [x] **步骤 1：编写失败的视觉状态测试**

增加可控日期（今天、当前月、相邻月、范围外）的渲染辅助函数，在三种主题和 DPR 1/2 模拟尺寸下断言：选中单元格中心像素接近 `QPalette::Highlight`，今天未选中时存在环形边缘像素，相邻月文本 alpha 低于当前月文本，禁用日期不使用 `HighlightedText`，RTL 只改变布局不改变日期选择结果。检查日历重复渲染前后子对象数量不变。

- [x] **步骤 2：运行测试确认失败**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzCalendarControlsTest ZzFluentScreenshotTest --parallel 2
ctest --preset linux-gcc-debug -R 'fluent\\.(calendar-controls|screenshot)' --output-on-failure
```

预期：新增的选中圆形、今天环和相邻月对比度断言在当前绘制实现上失败或无法区分状态。

- [x] **步骤 3：编写最小实现代码**

在 `paintCell()` 中根据单元格短边计算不超过单元格的圆形几何，使用 `Highlight`/`HighlightedText` 绘制选中态，使用一设备像素的 `QPen` 绘制今天环和焦点内环；普通悬停使用低 alpha 局部填充；相邻月、范围外和禁用日期只调整文字颜色。绘制前保存并恢复 painter 状态，禁止修改日期模型。

- [x] **步骤 4：运行测试确认通过**

重复步骤 2，预期所有日历行为、截图和对象预算断言通过，且 `selectionChanged` 信号次数不变。

- [x] **步骤 5：Commit**

```bash
git add ZzFluentUI/widgets/src/private/ZzCalendarPrivate.cpp ZzFluentUI/widgets/src/ZzCalendar.cpp ZzFluentUI/tests/ZzCalendarControlsTest.cpp ZzFluentUI/tests/ZzFluentScreenshotTest.cpp
git commit -m "fix(日历控件): 完善日期选中与今天状态"
```

### 任务 3：统一滚轮中心带、渐隐和悬停

**文件：**
- 修改：`ZzFluentUI/widgets/src/ZzRoller.cpp` 的 `paintEvent()` 和现有行几何辅助逻辑。
- 测试：`ZzFluentUI/tests/ZzRollerControlsTest.cpp`。

- [x] **步骤 1：编写失败的滚轮视觉测试**

为固定项目集合渲染滚轮，分别断言中心行存在低饱和 `Highlight` 背景、中心上下行文字 alpha 按距离递减、悬停只影响单行、长文本按稳定宽度省略、RTL 和主题切换不改变行矩形。记录滚轮渲染前后的动画和定时器对象数。

- [x] **步骤 2：运行测试确认失败**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzRollerControlsTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\\.roller-controls$' --output-on-failure
```

预期：中心带颜色、渐隐曲线或悬停局部性的新断言失败。

- [x] **步骤 3：编写最小实现代码**

在 `paintEvent()` 中以 `SC_SpinBoxEditField` 为内容矩形，按固定行高计算行矩形；中心行使用主题强调色的低饱和 alpha，距离中心的文字逐级降低 alpha，悬停只填充命中行。保持现有离散键盘、鼠标、滚轮和 wrapping 逻辑，不添加动画、计时器或像素偏移状态。

- [x] **步骤 4：运行测试确认通过**

重复步骤 2，预期行为、主题、RTL、无障碍和对象预算测试全部通过。

- [x] **步骤 5：Commit**

```bash
git add ZzFluentUI/widgets/src/ZzRoller.cpp ZzFluentUI/tests/ZzRollerControlsTest.cpp
git commit -m "fix(滚轮控件): 统一中心选中带与渐隐"
```

### 任务 4：统一日期与滚轮 Picker 弹层

**文件：**
- 修改：`ZzFluentUI/widgets/src/private/ZzCalendarPickerPrivate.cpp`；`ZzFluentUI/widgets/src/private/ZzRollerPickerPrivate.cpp` 中 `ZzRollerPickerPopup`、`preparePopupGeometry()`、`rebuildRollers()`。
- 测试：`ZzFluentUI/tests/ZzCalendarControlsTest.cpp`、`ZzFluentUI/tests/ZzRollerControlsTest.cpp`、`ZzFluentUI/tests/ZzPopupSurfacesTest.cpp`。

- [x] **步骤 1：编写失败的弹层契约测试**

打开两个 Picker 并渲染 popup，断言 popup 宽度不小于触发按钮和列最小宽度，边框四角在四个角附近存在完整连续像素，列间只存在一条低对比分隔线，按钮顺序遵循 RTL；确认、取消、Escape、外部隐藏分别保持提交或恢复快照。重复打开关闭不得新增 popup、roller 或动画对象。

- [x] **步骤 2：运行测试确认失败**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzCalendarControlsTest ZzRollerControlsTest ZzPopupSurfacesTest --parallel 2
ctest --preset linux-gcc-debug -R 'fluent\\.(calendar-controls|roller-controls|popup-surfaces)' --output-on-failure
```

预期：至少有 popup 角点封闭、宽度下限或 RTL 按钮布局断言失败。

- [x] **步骤 3：编写最小实现代码**

让 `ZzCalendarPicker` 仅设置统一输入表面所需的 frame、palette 和下拉指示属性；在 `ZzRollerPickerPopup::paintEvent()` 中只绘制一次完整菜单面板，使用现有 Fluent 样式 token，保留单层阴影。`preparePopupGeometry()` 按列内容、最小宽度和触发按钮宽度取最大值，并使用 `QStyle::visualRect()` 处理 RTL；不改变事务回调顺序。

- [x] **步骤 4：运行测试确认通过**

重复步骤 2，预期 popup 表面、事务、布局、主题和对象预算测试通过。

- [x] **步骤 5：Commit**

```bash
git add ZzFluentUI/widgets/src/private/ZzCalendarPickerPrivate.cpp ZzFluentUI/widgets/src/private/ZzRollerPickerPrivate.cpp ZzFluentUI/tests/ZzCalendarControlsTest.cpp ZzFluentUI/tests/ZzRollerControlsTest.cpp ZzFluentUI/tests/ZzPopupSurfacesTest.cpp
git commit -m "fix(选择器): 统一日期与滚轮弹层表面"
```

### 任务 5：补充跨主题、高 DPI、RTL 和性能回归

**文件：**
- 修改：`ZzFluentUI/tests/ZzSpinBoxControlsTest.cpp`、`ZzFluentUI/tests/ZzCalendarControlsTest.cpp`、`ZzFluentUI/tests/ZzRollerControlsTest.cpp`、`ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`、`ZzFluentUI/tests/ZzBasicControlsBenchmark.cpp`。

- [x] **步骤 1：编写失败的组合回归测试**

在现有测试 fixture 中循环浅色、深色、高对比和左右布局，增加 DPR 2 渲染分支；对六个组件记录 `findChildren<QObject*>`、`QAbstractAnimation`、`QTimer` 数量，并在 24 次主题/字体/Palette 更新后比较计数。截图断言确保图像非空、非单色、无裁切。

- [x] **步骤 2：运行测试确认失败**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentScreenshotTest --parallel 2
ctest --test-dir build/linux-gcc-debug \\
  -R '^fluent\\.screenshot-(100|125|150|200)$' --output-on-failure
ctest --test-dir build/linux-gcc-benchmarks \\
  -R '^benchmark\\.(fluent-basic-controls|fluent-theme-switch)$' \\
  --output-on-failure
```

预期：DPR 2 或高对比组合分支至少暴露一个颜色、像素或对象预算缺口。

- [x] **步骤 3：编写最小实现代码**

只修正前述生产绘制中的 token、alpha、DPR 和布局计算；测试辅助代码复用现有渲染函数，不引入测试专用公共控件或全局状态。

- [x] **步骤 4：运行测试确认通过**

重复步骤 2，并确认所有组件对象计数在循环前后相同。

- [x] **步骤 5：Commit**

```bash
git add ZzFluentUI/tests/ZzSpinBoxControlsTest.cpp ZzFluentUI/tests/ZzCalendarControlsTest.cpp ZzFluentUI/tests/ZzRollerControlsTest.cpp ZzFluentUI/tests/ZzFluentScreenshotTest.cpp ZzFluentUI/tests/ZzBasicControlsBenchmark.cpp
git commit -m "test(控件视觉): 增加主题高 DPI 与对象回归"
```

### 任务 6：执行完整 Linux 验收和静态检查

**文件：**
- 不新增源码文件；根据前述任务产生的改动执行构建和门禁。

- [x] **步骤 1：配置并构建 Debug**

运行：

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --parallel 2
```

预期：ZzFluentUI、测试和 `ZzPureToolsExample` 全部编译通过，且无新增 `-Werror` 警告。

- [x] **步骤 2：运行相关 CTest**

运行：

```bash
ctest --preset linux-gcc-debug -R 'fluent\\.(spin-box-controls|calendar-controls|roller-controls|screenshot|popup-surfaces)' --output-on-failure
```

预期：所有相关测试通过。

- [x] **步骤 3：运行 clang-tidy 目标和基准**

运行：

```bash
cmake --build --preset linux-clang-tidy-release \\
  --target ZzClangTidy --parallel 2
ctest --test-dir build/linux-gcc-benchmarks \\
  -R '^benchmark\\.(fluent-basic-controls|fluent-theme-switch)$' \\
  --output-on-failure
```

预期：clang-tidy 无新增诊断，基准门禁保持现有阈值内。

- [x] **步骤 4：提交验收结果**

```bash
git status --short
git log -6 --oneline
```

确认工作区只保留未跟踪的 `temp_image/`，然后以中文提交最终测试和文档状态：

```bash
git add docs/superpowers/plans/2026-09-01-fluent-value-date-roller-visual.md
git commit -m "docs(控件视觉): 记录实现计划与验收门禁"
```

## 规格覆盖度自检

- 统一数值输入表面、步进按钮和命中区域：任务 1。
- 日期选中、今天、相邻月、禁用和高 DPI：任务 2。
- 滚轮中心带、渐隐、悬停、固定几何和无动画：任务 3。
- 两个 Picker 的表面、弹层几何、RTL 和事务：任务 4。
- 浅色、深色、高对比、DPR 1/2、RTL、截图和对象图：任务 5。
- Linux 构建、CTest、clang-tidy、benchmark 和工作区验收：任务 6。

计划不包含公共 API、业务模型、字体资源、平台私有插件或参考项目运行时依赖，因此与规格的非目标保持一致。

## 执行证据（2026-09-02）

- 任务 1～4 已分别由历史提交完成；本次提交补齐了嵌套输入表面、
  标签页原生外框和对应 Linux 基线的最后收口。
- 任务 5 使用现有 Fluent 控件测试、四档 DPR 截图和对象预算覆盖，
  未新增重复测试；`fluent.screenshot-100/125/150/200` 通过 4/4。
- 任务 6 显式配置并构建 `linux-gcc-debug` 成功；选择器和弹层相关
  CTest 通过 8/8；`benchmark.fluent-basic-controls` 与
  `benchmark.fluent-theme-switch` 通过 2/2。
- `linux-clang-tidy-release` 的 `ZzClangTidy` 分析完成 279 个文件，
  退出码为 0。原计划把该目标放在 GCC Debug preset 下，已按实际
  CMake 目标定义修正为 Clang-Tidy preset。
- 本机 Qt 环境为 `/home/zz/Qt/6.11.1/gcc_64`；缺少 Vulkan 头文件只
  产生 CMake 的可选依赖提示，不影响本次构建和测试。
