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
