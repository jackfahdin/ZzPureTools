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
