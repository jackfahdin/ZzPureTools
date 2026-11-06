# Expander 连续卡片适配计划

沿用已批准的上游跟进范围与顺序，本地 master 线性提交，文档最后提交。

## 设计与范围

在现有单内容、向下展开 API 上完善绘制，不新增上游多内容或方向接口。
外层绘制覆盖标题和当前可见内容的连续圆角卡片，连接处只绘制一条分隔线；
标题保留 QToolButton 的键盘、焦点、无障碍语义，使用自绘文本与尾端 Segoe
Chevron，RTL 镜像位置，悬停／按下只突出箭头区域。主题颜色与圆角沿用 token。
沿用单一高度动画，箭头随同一动画进度转动，减少动态效果直接进入最终态。

相比逐个标题／内容独立画面板，外层统一绘制避免连接边双描边和圆角缝隙；
相比整体替换上游控件，保留现有 API 与动画契约，缩小回归范围。

## 执行

- [x] 在 `ZzFluentUI/tests/ZzExpanderTest.cpp` 增加连续表面、局部悬停、
  RTL、禁用、动画中间态回归，运行并确认旧实现失败。
- [x] 修改 `widgets/src/ZzExpander.cpp` 及 `widgets/src/private/ZzExpanderPrivate.{h,cpp}`，
  在公开头声明 paintEvent；绘制与现有动画复用主题和字体资源。
- [x] 两套 Qt 编译运行 Expander、无障碍、四档截图和 X11，独立复审后提交代码。
  先检查实际图及差异范围；确认本轮设计变化后，仅更新对应总览 PNG，
  不修改遮罩、差异门限或性能基线，不用容差掩盖问题。
- [x] 最后更新上游对照文档、验证证据及未移植能力，单独提交文档。

## 当前验证结果

- Qt 6.8.3 Release／Qt 6.11.1 Debug 的 Expander 与无障碍各 2/2 通过。
- 两版 Qt 的 X11/Xvfb 各 21 行通过；Qt 6.11.1 的 150% 绘制专项 8 行通过。
- 架构与文档审计 2/2 通过，独立复审无剩余阻塞项。
- 内容在动画中删除后箭头收敛到最终状态；标题助记符测量、绘制和省略沿用 Qt 语义。
  两项均先复现失败再修复。原有内容所有权、快速反转和单动画对象预算保持通过。
- 更新基线前，两版 Qt 的四档截图均仅总览三种主题与旧外观不符，
  其余每份 118 行通过。失败属于截图差异门禁，不是性能门限失败。
- 2026-11-06 恢复会话后重新构建并复现上述结果，逐档审查两版 Qt 三主题实际图。
  Expander 和下方布局位移区域之外的差异约为 0.090%～0.150%，低于原 0.5% 门限。
  该比例由原截图比较生成的 diff 像素数及日志比例估算，仅用于诊断，没有新增测试遮罩。
- 使用既有更新模式且仅选择 `rendersThemes`，生成 Qt 6.8／6.11 各四档三主题
  共 24 张总览 PNG；版本目录、图片尺寸、比较代码、遮罩和门限保持不变。
  更新后两版 Qt 的 Expander、无障碍、四档截图各 6/6 通过。
- 本轮重新运行两版 Qt X11 各 21 行、Qt 6.11.1 的 150% 绘制专项 8 行，均通过。
  架构／文档审计 2/2 通过，代码与全部参考图独立复审无阻塞项。
- 三主题视觉对照：本机构建产物 `build/expander-review/comparison.png`；
  原始 actual／diff 位于各构建目录 `ZzFluentUI/tests/reports/fluent-screenshots/`。
- 全缩放审查图：`build/expander-review/qt-6.8-all-scales.png` 和
  `build/expander-review/qt-6.11-all-scales.png`；旧图校验值、差异范围记录在
  `build/expander-review/baseline-review.json`，旧图本地副本位于同目录 `previous-baselines/`。
- 代码、回归测试与参考图已提交为 `bd4ad25`；本地 master 线性提交，未推送。
  当前结论限于本轮测试集合，不代表已运行整个仓库测试或 Windows／macOS 实机验收。

验证（另一套目录为 `build/linux-gcc-debug`）：

```bash
cmake --build build/spectrum-qt683-release --target ZzExpanderTest ZzFluentScreenshotTest --parallel 2
ctest --test-dir build/spectrum-qt683-release -R '^fluent\.(expander|accessibility|screenshot-.*)$' --output-on-failure --parallel 2
ctest --test-dir build/spectrum-qt683-release -R '^architecture\.(complete-audit|documentation-audit)$' --output-on-failure
```
