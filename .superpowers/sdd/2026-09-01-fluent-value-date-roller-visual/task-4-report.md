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

## 第 4 轮复审修复

- Roller 测试在 LTR/RTL 两个方向使用实际 popup/trigger global geometry、`QStyle::visualRect()` 和 availableGeometry clamp 校验镜像锚点；按视觉 x 排序实际 Roller 几何，验证 1px gap、预期 divider x、连续 Midlight 线色，并扫描确认没有额外同色连续竖线。
- Roller 宽度测试读取 popup layout 实际 contentsMargins、rollerHost/所有 Roller union，使用 64/72 最小宽度及明显长文本确认内容完整位于 margin 内容区且 popup 宽度不小于触发器。
- CalendarPicker 增加原生 calendar popup 的 show/geometry/渲染表面、日期提交、Escape 与外部 hide 恢复快照、重复打开关闭对象预算和运行时 palette 跟随断言。offscreen 平台不保证稳定像素细节，本轮仅断言可见性、几何、可渲染非透明表面、事务和对象计数。
- 两个 Picker 分别统计 popup、Roller/calendar 子对象、动画和 QTimer 数量，重复 show/hide/accept/cancel 后计数保持不变。
- 生产实现最小调整：`ZzCalendarPickerPrivate` 作为 QObject 安装事件过滤器，记录 popup 打开日期；鼠标选择视为提交，Escape/外部 hide 且未提交时恢复日期快照。未改变 QDateEdit 日期模型、公开 API 或信号顺序；无新增动画、QTimer、全局状态。

### 第 4 轮红灯

命令：

```bash
cmake --build --preset linux-gcc-debug --target ZzCalendarControlsTest ZzRollerControlsTest --parallel 2
ctest --preset linux-gcc-debug -R 'fluent\\.(calendar-controls|roller-controls)' --output-on-failure
```

新增断言首次运行失败：RTL/LTR 按钮方向与实际方向未匹配、RTL 列按创建顺序计算 gap 得到负值；Calendar 原生 popup Escape 不恢复日期快照。

### 第 4 轮绿灯

命令及结果：

```bash
cmake --build --preset linux-gcc-debug --target ZzCalendarControlsTest ZzRollerControlsTest ZzPopupSurfacesTest --parallel 2
ctest --preset linux-gcc-debug -R 'fluent\\.(calendar-controls|roller-controls|popup-surfaces)' --output-on-failure
git diff --check
```

构建成功；CTest `fluent.popup-surfaces`、`fluent.roller-controls`、`fluent.calendar-controls` 3/3 通过；`git diff --check` 无输出且退出码为 0。

第 4 轮提交前补充收紧 Calendar 鼠标提交判定：仅 popup 内实际 `QTableView` 日期网格的 release 视为确认，月份导航或其它 popup 点击不会阻止外部隐藏恢复快照；该修正后 `fluent.calendar-controls` 复测通过。

## 第 1 轮审查修复

- 在 `ZzRollerControlsTest::commitsAndRollsBackReusablePopup` 增加实际 popup 宽度、列最小宽度、子 Roller frame、RTL popup/按钮几何及事务回滚断言。
- `rebuildRollers()` 将每列设为无 frame；popup 面板绘制后按实际 Roller 几何绘制唯一低对比分隔线。
- 移除构造阶段对内部 QLineEdit 的显式 palette 复制，避免主题切换后锁死 palette。
- 红灯：新增 RTL/事务断言初次运行因取消计数预期未更新而失败（实际 5，旧预期 4）。
- 绿灯命令及结果：构建三测试目标成功；`ctest --preset linux-gcc-debug -R 'fluent\\.(calendar-controls|roller-controls|popup-surfaces)' --output-on-failure`，3/3 通过；`git diff --check` 通过。

## 第 2 轮审查修复

- 将滚轮列布局间距设为明确 1px，使分隔线落在列间空隙而不被子控件覆盖；保留 popup 单次面板绘制和低对比线。
- Roller 测试渲染实际 popup，依据实际 Roller 几何检查宽度与连续竖向分隔线；Calendar 测试增加无 frame 输入表面断言。
- 红灯：新增像素/RTL 契约首次运行因取消计数预期未同步而失败；修正后绿灯。
- 绿灯：构建 `ZzCalendarControlsTest`、`ZzRollerControlsTest`、`ZzPopupSurfacesTest` 成功；ctest 3/3 通过；`git diff --check` 通过。

## 第 3 轮审查修复

- 分隔线测试改为使用实际 Roller `geometry()` 与 popup 映射坐标，精确验证 1px gap、预期 x 的连续 Midlight 线色及列边界位置。
- 宽度测试改为检查 Roller bounding rect 完整位于 popup 内，并纳入实际列宽与间隙；Calendar 内部 QLineEdit 查找改为强制断言。
- 红灯：新增严格分隔线断言初次运行验证候选 gap；绿灯：`ctest --preset linux-gcc-debug -R 'fluent\\.(calendar-controls|roller-controls)' --output-on-failure` 2/2 通过，构建成功，`git diff --check` 通过。
