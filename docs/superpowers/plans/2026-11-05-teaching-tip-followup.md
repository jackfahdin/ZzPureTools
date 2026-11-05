# TeachingTip 交互与展开适配

沿用已批准的上游跟进顺序，在本地 master 线性提交，不推送、不改截图基线或门限。
使用 brainstorming 确定控件边界、TDD 复现行为、独立复审验证生命周期。

## 设计

保留现有 Qt::Tool 顶层窗口与公开 API。上游子控件自动显示问题先通过测试确认是否
适用；本地确有目标祖先移动／裁剪监听缺失、关闭焦点恢复缺失，以及属性信号回调
同步销毁后继续访问的问题。先解决这些基础行为，独立提交，再完善展开动效。

目标可见区域逐层与祖先矩形相交；监听完整祖先链，目标完全滚出视口时关闭。
轻触关闭仍保留点击目标不关闭、Action 不自动关闭的现有契约。应用事件过滤器若
关闭回调删除了事件接收者，应吞掉当前事件，避免 Qt 继续派发给已销毁对象。
仅提示内部持有焦点并主动关闭时恢复打开前的可用控件；外部点击不抢回焦点。
属性通知后的后续操作使用 QPointer，并在状态完成后发出定位信号。

展开效果采用有限时长、可复用动画驱动的快照缩放，围绕指向目标的边缘展开；
动画期间保持布局尺寸，结束后恢复真实子控件，隐藏、重开及主题变化不能遗留快照。
减少动态效果直接展示最终状态，保持现有最终外观和截图基线。

## 步骤

- [x] 在 `ZzFluentUI/tests/ZzTeachingTipTest.cpp` 添加父窗口不自动显示、祖先跟随／
  裁剪、焦点恢复及信号接收者同步销毁的回归，确认新增问题在旧实现失败。
- [x] 修改 `widgets/src/ZzTeachingTip.cpp`、`widgets/src/private/ZzTeachingTipPrivate.{h,cpp}`
  的目标监听、焦点与回调保护；两版 Qt TeachingTip 测试通过并复审后单独提交。
- [x] 添加缩放阶段、结束恢复、快速反转、减少动态效果回归，按测试实现快照展开。
- [x] Qt 6.8.3 / 6.11.1 构建并运行 TeachingTip、无障碍和四档截图；补 xcb/Xvfb
  交互检查。运行架构、文档审计并独立复审，单独提交动效。
- [x] 最后更新上游跟进文档，说明完成项、原生平台验证边界和后续 Expander 优先级。

## 完成证据

- 基础交互提交：`9662ed1`；展开动效提交：`23d4051`。
- 父窗口不自动显示在旧实现即通过；祖先移动、焦点恢复、取消显示、通知删除、
  内容析构重入等新增问题先复现失败／崩溃，再修复。四方向缩放中间态也先失败再通过。
- Qt 6.8.3 Release／Qt 6.11.1 Debug 的 TeachingTip、无障碍、四档截图各 6/6 通过；
  X11/Xvfb 的 TeachingTip 各 32 行通过；Qt 6.11.1 的 150% 动画专项 7 行通过。
- 架构与文档审计 2/2 通过；独立复审发现的焦点竞态、内容重挂与动画恢复重入均已修复并补测。
- 本地 master 线性提交，不推送；未修改第三方、截图基线、遮罩和门限。

验证命令以 `build/spectrum-qt683-release` 为例，另一套使用 `build/linux-gcc-debug`：

```bash
cmake --build build/spectrum-qt683-release --target ZzTeachingTipTest ZzFluentScreenshotTest --parallel 2
ctest --test-dir build/spectrum-qt683-release -R '^fluent\.(teaching-tip|accessibility|screenshot-.*)$' --output-on-failure --parallel 2
ctest --test-dir build/spectrum-qt683-release -R '^architecture\.(complete-audit|documentation-audit)$' --output-on-failure
```
