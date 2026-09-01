# 任务 4 报告：统一日期与滚轮 Picker 弹层

## 测试记录

- 红灯阶段：基线提交 `d9d6e11` 执行指定三项测试，现有契约已全部通过，未复现简报所述角点/宽度/RTL 失败（本任务未改动测试源文件）。
- 绿灯阶段：实现后重新执行构建与测试，`fluent.popup-surfaces`、`fluent.roller-controls`、`fluent.calendar-controls` 全部通过。
- `git diff --check`：通过。

## 变更文件

- `ZzFluentUI/widgets/src/private/ZzCalendarPickerPrivate.cpp`
- `ZzFluentUI/widgets/src/private/ZzRollerPickerPrivate.cpp`

## 设计选择

`ZzCalendarPicker` 仅关闭输入框及其内部编辑器 frame、同步输入 palette，并保留 QDateEdit 原生日期模型、日历 popup 和下拉行为。`ZzRollerPickerPopup::paintEvent()` 显式设置完整区域后单次调用 `PE_PanelMenu`，由 Fluent 样式统一绘制连续面板、边框和单层阴影。`preparePopupGeometry()` 使用列内容字体宽度加内边距、列最小宽度、布局间距及触发按钮宽度取最大值；定位通过 `QStyle::visualRect()` 对 RTL 进行镜像，屏幕边界约束和上下翻转逻辑保持原有行为。事务回调、公开 API、对象复用均未改变。

## 提交

`f94bf33 fix(选择器): 统一日期与滚轮弹层表面`

## 第 1 轮审查修复

- 在 `ZzRollerControlsTest::commitsAndRollsBackReusablePopup` 增加实际 popup 宽度、列最小宽度、子 Roller frame、RTL popup/按钮几何及事务回滚断言。
- `rebuildRollers()` 将每列设为无 frame；popup 面板绘制后按实际 Roller 几何绘制唯一低对比分隔线。
- 移除构造阶段对内部 QLineEdit 的显式 palette 复制，避免主题切换后锁死 palette。
- 红灯：新增 RTL/事务断言初次运行因取消计数预期未更新而失败（实际 5，旧预期 4）。
- 绿灯命令及结果：构建三测试目标成功；`ctest --preset linux-gcc-debug -R 'fluent\\.(calendar-controls|roller-controls|popup-surfaces)' --output-on-failure`，3/3 通过；`git diff --check` 通过。
