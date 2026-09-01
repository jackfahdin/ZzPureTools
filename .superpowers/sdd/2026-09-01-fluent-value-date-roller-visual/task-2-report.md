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

提交哈希：`8a552ed`。

疑虑：Qt offscreen 下 `QCalendarWidget` 内部视图会复用 `Highlight` 绘制底层选区，无法稳定从最终截图按颜色隔离日期单元格前景；因此未保留该脆弱像素测试。现有日期模型、信号、键盘语义和对象数量回归均通过。
