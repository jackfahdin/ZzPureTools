# 任务 3 报告

状态：完成

文件：
- `ZzFluentUI/widgets/src/ZzRoller.cpp`
- `ZzFluentUI/tests/ZzRollerControlsTest.cpp`

实现：行矩形以 `SC_SpinBoxEditField` 顶部为稳定几何原点；中心行使用低透明度主题 `Highlight`，保留距离递减文字 alpha 与单行 hover；未增加动画、定时器、公开 API 或运行时依赖。

测试证据：
- 红灯：新增 `rendersSubtleCenterBandAndStableRows` 后，中心带断言失败（1 failed, 8 passed）。
- 绿灯：`cmake --build --preset linux-gcc-debug --target ZzRollerControlsTest --parallel 2` 成功。
- 绿灯：`ctest --preset linux-gcc-debug -R '^fluent\\.roller-controls$' --output-on-failure`：1/1 passed，100%。

提交：`a81dee5`

疑虑：透明背景渲染下逐行 alpha 的像素断言仍依赖现有渐隐实现；本任务新增测试覆盖中心带、稳定行高和 hover 单行局部性。

## 审查修复

补齐 `sizeHint()` 的样式上下边界，绘制裁剪到 `SC_SpinBoxEditField`，并让 `rowOffsetAt()` 使用相同内容区原点和固定行范围，避免末行裁剪及命中偏移。

追加实测样式编辑域的 9 行闭合断言（LTR/RTL）、距离 1..3 的文字亮度递减断言，以及固定宽度长文本省略和 RTL 尺寸稳定断言。

红灯证据：9 行编辑域闭合断言失败；修复后绿灯：目标 CTest 1/1 通过，100%。

审查修复提交：`2fedb11`

## 定向复审第 1 轮修复

按 `task-3-rereview.md` 补强测试并修正样式适配：
- 文字 alpha 使用同背景下“有文字/文字色等于背景”的实际渲染差分，比较距离 1..3 的字形信号，覆盖背景合成；
- 长文本验证 `elidedText` 确实产生省略标记，并确认绘制像素位于实际编辑域内；
- RTL 切换后重新初始化 `QStyleOptionSpinBox`，比较编辑域的垂直原点、宽高；
- `sizeHint()` 使用 `PM_SpinBoxFrameWidth` 推导上下边界，移除固定 `+4`；
- 中心带在多个背景点按 Highlight alpha=48 的合成结果验证，并确认相邻背景差异。

验证：`cmake --build --preset linux-gcc-debug --target ZzRollerControlsTest --parallel 2` 成功；`ctest --preset linux-gcc-debug -R '^fluent\\.roller-controls$' --output-on-failure` 1/1 通过（100%）；`git diff --check` 通过。此前遗留的定向测试进程已终止，未修改 `temp_image/`。

## 定向复审第 2 轮修复

将渐隐测试改为 9 行完全相同的 `same` 文本，以同一字号和字形在白色背景下进行实际“有字/空白文字色”渲染差分，比较距离 1..3 的合成文字信号。长文本测试改为同编辑域、同字体的长短文本实际图像对照，检测长文本编辑域尾部的字形像素多于短文本，证明省略结果确实绘制且未越界；未复算生产省略字符串。

验证命令：
- `cmake --build --preset linux-gcc-debug --target ZzRollerControlsTest --parallel 2`：成功。
- `QT_QPA_PLATFORM=offscreen build/linux-gcc-debug/ZzFluentUI/tests/ZzRollerControlsTest fadesRowsAndElidesWithoutGeometryDrift -v1`：3 passed, 0 failed。
- `git diff --check`：通过。
- `ctest --preset linux-gcc-debug -R '^fluent\\.roller-controls$' --output-on-failure`：1/1 passed, 100%。
