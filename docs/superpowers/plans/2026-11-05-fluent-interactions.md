# 截图可重复性与已有控件交互完善

用户已批准先解决截图差异，再依顺序完善开关拖动与面包屑溢出。本轮在本地 master
线性提交，不推送、不监控 CI、不接触 docs/research。使用 writing-plans 整理执行步骤，
按已有批准方案直接执行，测试与独立复审作为检查点。

## 设计

Qt 6.8 截图以固定 DejaVu Sans 10pt 的灰度抗锯齿作为参考，只在该版本测试设置
`QFont::NoSubpixelAntialias`；Qt 6.11 保持自身参考的默认字体策略。
不改系统 Fontconfig、产品字体、文字遮罩、基线或门限。
Qt 的屏幕子像素环境提示不能覆盖本机 Fontconfig 的字体级 RGB 设置，因此采用公开
QFont 策略约束测试字体，不增加外部配置依赖。

开关复用 QCheckBox 的点击、键盘、焦点和无障碍语义以及现有唯一动画对象。
只有旋钮上的左键按下可进入拖动候选；横向位移超过平台拖动距离的三分之一
（至少 2 个逻辑像素）后，旋钮跟随位置。释放按轨道中点确定状态，不要求鼠标仍在轨道内。
未超过阈值仍按普通点击处理，标签不启动拖动。RTL 反转位移方向；隐藏、禁用、丢失抓取、
窗口失活、失焦和几何方向改变取消手势，取消不发 released／clicked。
正常释放最多发出一次 toggled／released／clicked，
接收者同步删除控件时安全返回。减少动态效果保留直接操作反馈，只关闭自动补间。

面包屑保留 items／currentIndex／indexRequested。窄宽度优先保留末项，前面的项
放入省略按钮菜单，菜单仍发原逻辑索引。宽度恢复时恢复可见项。保留宽布局的现有分配，
避免无关布局变化；分隔符改用已有 Segoe 字体的 Chevron 并响应 RTL。Home／End／方向键
在可见按钮间移动，Enter／Space 激活；隐藏焦点项时转移到溢出按钮。主题、字体、DPR
改变后更新展示；信号回调不在通知后访问对象，重建保留上一轮延迟释放保护。

## 文件与步骤

- [x] `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`：复现原 32 项失败，固定字体策略；
  运行 Qt 6.8.3 与 Qt 6.11.1 四档截图，检查基线及阈值零变动，单独提交。
- [x] `ZzFluentUI/tests/ZzToggleSwitchTest.cpp`：先加拖回原侧、RTL、越界释放、
  未提交状态、取消、信号次数／同步销毁的失败用例。
- [x] `widgets/include/ZzFluentUI/ZzToggleSwitch.h`、`widgets/src/ZzToggleSwitch.cpp`、
  `widgets/src/private/ZzToggleSwitchPrivate.{h,cpp}`：增加输入状态，绘制和命中共用
  几何；通过上述测试、无障碍与原截图后单独提交。
- [x] `ZzFluentUI/tests/ZzNavigationControlsTest.cpp`：先加窄宽度溢出、恢复宽度、
  菜单原索引、RTL、键盘、菜单回调重建与销毁测试。
- [x] `widgets/include/ZzFluentUI/ZzBreadcrumbBar.h`、`widgets/src/ZzBreadcrumbBar.cpp`、
  `widgets/src/private/ZzBreadcrumbBarPrivate.{h,cpp}`：增加溢出菜单、布局更新与键盘
  事件过滤，使用已有字体资源；运行导航／无障碍／截图及架构审计，单独提交。
- [x] 更新上游跟进清单的完成状态与实测范围，独立复审，最后提交文档。

路径以仓库根为准，以上 `widgets/` 均位于 `ZzFluentUI/` 下。常用验证命令：

```bash
cmake --build build/spectrum-qt683-release --target ZzToggleSwitchTest ZzNavigationControlsTest ZzFluentScreenshotTest --parallel 2
ctest --test-dir build/spectrum-qt683-release -R '^fluent\.(toggle-switch|navigation-controls|accessibility|screenshot-.*)$' --output-on-failure --parallel 2
ctest --test-dir build/spectrum-qt683-release -R '^architecture\.(complete-audit|documentation-audit)$' --output-on-failure
```

Qt 6.11.1 使用 `build/linux-gcc-debug` 执行相同目标与测试。新增行为先单独运行新用例，
确认旧实现因缺少该行为失败，再实现并复测；通过后才执行较广的回归。

## 完成记录

- `ecaa7f2`：Qt 6.8 截图字体灰度抗锯齿；两版 Qt 四档截图均通过。
- `af57e62`：开关拖动；两版 Qt 回归均通过，包含中断取消、按钮组与同步删除／重入。
- `dd09944`：面包屑；两版 Qt 导航和无障碍均通过，包含实际菜单键盘激活。
- 开关、导航、无障碍各版合计 3/3 CTest 通过；完整架构审计与文档审计 2/2 通过。
- 独立复审发现的组通知、中断复活、冻结动画和回调重入问题已分别回归并修复。
- 没有修改第三方源码、截图基线／遮罩／门限或性能门禁；未推送及监控 CI。
- Linux offscreen 不替代 Windows／macOS 原生交互验收；弹层、TeachingTip、Expander 留待后续。
