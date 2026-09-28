# 日期选择 Fluent 外观改进实施计划

> 面向 AI 代理：按已获用户确认的设计执行，先失败测试再实现，完成后独立审查并提交。沿用当前主工作区和本机 Debug 构建，便于用户直接验收。

**目标：** 日期选择入口、年月导航、星期表头、日期网格和弹层形成统一 Fluent 外观。

**架构：** 保留 `QDateEdit + QCalendarWidget` 的日期引擎、原生导航和弹出事务。日历私有实现统一装配导航与绘制，选择器私有实现处理弹层表面；Example 只展示组件。参考 FluentUIStyle 的排布，使用本项目主题与公开 Qt API，不引入参考项目依赖。

**技术栈：** Qt 6.8+、C++20、QtTest；本机 Qt 6.11.1、GCC 15、linux-gcc-debug。

## 已确认设计

- 年月位于逻辑起始侧，翻月按钮位于尾侧；RTL 镜像。月份菜单、年份编辑继续由 Qt 管理。
- 导航取消整条强调色背景；弹层采用圆角细边框和统一背景、内边距。
- 选中日期为强调色圆形，今天为圆环，悬停为柔和底色；星期与周末色根据 locale、firstDayOfWeek 正确映射。
- 尺寸根据字体测量，保留大字体和高 DPI 可读性，不采用固定字体像素值。
- 禁用、范围外日期、键盘焦点、Esc/外部关闭恢复、Enter/鼠标提交沿用现有契约。
- 不增加切页动画、定时器或逐帧资源解析。内部 Qt 子控件查找封装在 private，并检查查找结果。

## 单一交付任务：日历整体呈现与原生交互回归

**修改文件：**

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzCalendar.h`、`widgets/src/ZzCalendar.cpp`：日历表面绘制、自然尺寸与状态刷新。
- `ZzFluentUI/widgets/src/private/ZzCalendarPrivate.h/.cpp`：导航装配、按钮绘制、配色和字体尺寸、日期状态绘制。
- `ZzFluentUI/widgets/src/private/ZzCalendarPickerPrivate.cpp`：去除弹层原生方形框，保留已有确认/取消事务。
- `ZzFluentUI/widgets/src/ZzFluentStyle.cpp`：日历内部网格不使用普通列表的选择指示槽，避免星期名称被截断。
- `ZzFluentUI/tests/ZzCalendarControlsTest.cpp`：导航位置、翻月/年编辑、星期排列、主题、尺寸和弹层回归。
- `examples/ZzPureToolsExample/ZzExampleControlPagePrivate.cpp`：日期输入与独立日历同时展示、日期联动。
- `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp` 和受影响 Linux 基线：日历三主题与四档 DPR 视觉验证。

- [x] 先补充导航方向、导航背景、月份/年份操作和字体增大后的内容容纳测试。未改实现时确认导航仍为旧布局、旧背景，测试失败。

  ```cpp
  QVERIFY(month->geometry().right() < previous->geometry().left());
  QTest::mouseClick(next, Qt::LeftButton);
  QCOMPARE(calendar.monthShown(), 9);
  QCOMPARE(calendar.selectedDate(), QDate(2026, 8, 5));
  ```

- [x] 重排原生导航按钮、修正内部按钮绘制；日历表面使用主题圆角/边框，星期颜色使用实际星期格式，不硬编码第五第六列。
- [x] 让日期圆形按单元格空间/字体尺寸缩放；选中与今天也响应悬停，禁用不产生交互反馈。
- [x] 弹层去除多余边框，保留 Qt 的定位和已有事务，反复打开不增加子对象。
- [x] Example 展示输入与日历，增加实际 popup/日历状态截图，人工查看三主题及 200% 图。
- [x] 构建和定向测试：

  ```bash
  cmake --build build/linux-gcc-debug --target ZzCalendarControlsTest ZzFluentScreenshotTest ZzPureToolsExample --parallel 2
  ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^(fluent.calendar-controls|example.puretools-integration)$'
  ```

- [x] 只更新审阅过的受影响基线，关闭更新变量后复验；运行本机最小门禁、公开头和视觉令牌检查，独立审查。
- [x] 一笔中文功能提交，记录实际环境、测试结果和未执行项；本轮不推送。不修改 `docs/research/`。

## 交付与验证记录

日期外观改动位于库内，Example 增加与输入值联动的独立日历。重排仅移动翻月按钮并调整弹簧比例，保留 Qt 年份编辑使用的占位对象，不替换日期模型和弹出事务。独立 popup 显式同步输入控件的布局方向。字体、地区和星期文本在样式刷新或尺寸查询时处理，绘制复用日期文本缓存，不增加动画或定时器。

本机环境：Ubuntu 26.04.1 LTS、Qt 6.11.1、GCC 15.2.0、CMake 4.3.3，preset 为 `linux-gcc-debug`。

- 本机最小门禁通过：完整构建与 216 项普通 CTest（包含公开头、架构、视觉令牌和 Example 集成）。
- 定向正则 `^fluent\.(calendar-controls|combo-box-controls|spin-box-controls|text-input-controls)$`，4 项通过。
- 日历 QtTest：24 passed、0 failed、1 skipped（包括初始化和清理）。唯一跳过项是 offscreen 平台不支持的键盘打开 popup；鼠标打开、Enter 提交、取消恢复、范围、年月编辑、RTL、大字体和对象数量测试通过。
- 控件、工作区、Example 的 100%、125%、150%、200% 截图共 12 项 CTest 通过。仅更新 12 张受影响的总览基线，新增三主题与浅色 RTL 的 16 张真实日历 popup 基线，关闭更新模式后复验，未放宽阈值。
- 已查看 Example 日期页、三主题总览、三主题 popup、200% popup 及 RTL 图。截图的悬停使用局部鼠标事件，避免全局光标污染其他场景。
- 已完成独立代码审查，未留下阻断问题。导航字体的静态疑点经实际像素测试和 Qt 的字体继承实现核查，不构成本机缺陷；显式传入字体仅明确绘制契约。

**待验证：** Windows、macOS、Qt 6.8 的实际构建/运行和截图，ASan/UBSan、clang-tidy、专项性能、Linux 物理桌面交互。Qt 6.8 专用总览基线没有用 Qt 6.11 截图冒充更新，须在对应环境重新核查与生成；本轮本机通过不代表远端发布门禁已经通过。

直接运行 `build/linux-gcc-debug/examples/ZzPureToolsExample/ZzPureToolsExample`，进入“基础控件 → 日期选择(CalendarPicker)”验收输入、弹层和联动日历。
