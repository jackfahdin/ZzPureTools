# 任务 2 报告

状态：完成

文件：

- `ZzFluentUI/widgets/src/private/ZzCalendarPrivate.cpp`

红灯证据：新增圆形几何像素断言在旧实现上失败，选中高亮边界为横向圆角矩形，断言失败。

绿灯证据：

```text
100% tests passed, 0 tests failed out of 5
fluent.calendar-controls Passed
fluent.screenshot-100 Passed
fluent.screenshot-125 Passed
fluent.screenshot-150 Passed
fluent.screenshot-200 Passed
```

提交哈希：`b6c071b`（实现提交）。

疑虑：Qt offscreen 下 `QCalendarWidget` 内部视图会复用 `Highlight` 绘制底层选区，无法稳定从最终截图按颜色隔离日期单元格前景；因此未保留该脆弱像素测试。现有日期模型、信号、键盘语义和对象数量回归均通过。

## 审查修复轮次

补充了当前 hover 单元格的低 alpha 局部高亮，并对相邻月、范围外和禁用文字施加显式 alpha 弱化；未改变公开 API 或日期模型。直接暴露 `paintCell()` 的测试因 `ZzCalendar` 为 `final` 不可行，保留稳定的控件回归与截图测试。

清理轮次：移除测试文件中多余空白，`git diff --check` 通过，工作区仅保留未跟踪的 `temp_image/`。

## 审查修复轮次 2

红灯：`hoveringDateChangesOnlyItsLocalSurface` 在旧 hover 命中逻辑下局部变化为 0。绿灯：改用内部 viewport/widgetAt 的全局坐标映射后通过；目标构建及日历、100/125/150/200% 截图测试 5/5 通过。

## 最终测试补强

新增 hover 局部变化断言，以及固定日期范围下当前月、相邻月和禁用日期文字 ink 像素弱化断言；旧实现 hover 断言失败，修复后通过。由于 Qt offscreen 的视图重绘会改变单元格边界抗锯齿像素，未对边界外全局像素做零差异断言。

## 审查修复轮次 4

红灯：补强后的 hover 外部差分、越界日期定位、今天未选中环形边缘和 RTL visualRect 点击在旧实现/旧测试取样下失败；失败原因分别为 hover 坐标混用、禁用索引实际落在可用日期、缺少环边缘统计和 RTL 日期索引未验证。

实现：hover 命中统一使用内部 viewport 的局部坐标，保留 `QApplication::widgetAt(QCursor::pos())` 祖先检查；未改公开 API、日期模型或信号。

测试：日期索引以 selectionModel 当前日期为锚点，按行列偏移推导，不读取平台私有 `Qt::UserRole`；禁用项先断言落在 `minimumDate` 外，再比较文字强度并确认无 `HighlightedText` 红色像素。今天环统计比较圆周边缘与中心，允许抗锯齿；RTL/LTR 均通过 `visualRect()` 点击同一日期并断言选择结果和非空渲染。

绿灯：

```text
100% tests passed, 0 tests failed out of 5
fluent.calendar-controls Passed
fluent.screenshot-100 Passed
fluent.screenshot-125 Passed
fluent.screenshot-150 Passed
fluent.screenshot-200 Passed
git diff --check Passed
```

## 审查修复轮次 5（最终）

修复 `weakensOutOfRangeAndAdjacentDateText` 的相邻月份空索引：先断言索引有效，再断言由模型索引推导的日期有效且确实属于显示月份之外；`dateForIndex/indexForDate` 增加 7 列、索引可见、日期有效及唯一匹配断言。

将 hover 命中从 `paintCell()` 的全局光标/窗口查询改为 `ZzCalendarPrivate` 私有事件过滤器。内部 `QTableView` viewport 开启 mouse tracking，过滤器缓存当前单元格矩形并仅请求旧/新矩形局部更新；Leave、Hide、EnabledChange、页面/主题切换清除缓存。未新增公开 API、定时器或全局状态，焦点判断改用控件自身 `focusWidget()`。

新增选中实心圆与今天未选中圆环的定向像素几何断言：分别验证圆内命中、圆周命中、内部空心及四角无命中，容忍抗锯齿颜色误差。选中测试临时关闭视图自身选区背景，以隔离 `paintCell()` 绘制。

验证：`cmake --build --preset linux-gcc-debug --target ZzCalendarControlsTest ZzFluentScreenshotTest --parallel 2`；`ctest --preset linux-gcc-debug -R 'fluent\\.(calendar-controls|screenshot)' --output-on-failure`（5/5 通过，含 screenshot-100/125/150/200）；`git diff --check` 通过。截图矩阵覆盖前轮 Light/Dark/HighContrast 与 DPR 档位。
