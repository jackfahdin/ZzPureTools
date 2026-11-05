# 弹层和 ComboBox 细节适配计划

按已批准的上游跟进第 4 项执行，继续本地 master 线性提交、不推送。
使用 brainstorming 核对方案、writing-plans 记录步骤、TDD 实现与独立复审收尾。

## 证据与设计

同 Qt 6.11.1、DejaVu Sans 10pt、DPR 1 实测：框架 ComboBox 位于 (200,190)，
尺寸 220×36，下拉窗口位于 (200,225)，即压住底边一个像素；框架菜单和下拉窗口
均没有启用透明背景，真实 grab 的圆角外仍不透明。上游 `fb227d1` 下拉窗口有
6px 自绘阴影留白，窗口位于 (196,223)，可见面板从 y=229 开始，间隔为 3px。

选择保留 Qt 的窗口与屏幕避让，在公开样式接口中修复透明背景、锚点矩形及菜单栏
对齐。另一方案是复制上游整套软件阴影和定位动画，但需改变窗口尺寸、内容边距与
滚动视口，且与平台阴影重复；仅复制偏移量也会造成错误。此轮保留平台阴影与现有
内容布局，不改变截图基线、遮罩或门限，不修改上游源码，不加入 Qt 私有头。

ComboBox 使用 `SC_ComboBoxListBoxPopup` 返回垂直扩展的锚点，预留 3px 间隔并
计入 QRect 包含末像素的语义；Qt 决定向上／向下、实际高度、横向和屏幕裁剪。
菜单栏高亮的现有水平内缩成为绘制与定位共用常量，仅将关联菜单对齐该边缘；
上下文菜单和子菜单保持 Qt 定位。原生 Wayland 的位置由合成器控制，不强制搬动。
仅标准菜单和 ComboBox 直接所属的 Popup 容器启用透明背景，样式撤销恢复原属性。

## 执行步骤

- [x] 读取上游 `8f3ba68` 与当前 Qt 定位源码；构建同 Qt 对照工具并采样真实窗口。
  工具及 PNG 位于忽略的 `build/popup-followup-src`、`build/popup-followup`。
- [x] 在 `ZzFluentUI/tests/ZzPopupSurfacesTest.cpp` 添加真实菜单透明、样式撤销、
  菜单栏 LTR／RTL 对齐用例；在 `ZzComboBoxControlsTest.cpp` 添加真实下拉窗口
  透明、上下弹出间距、屏幕边缘、长列表和重复打开测试，先运行确认失败。
- [x] 在 `widgets/src/private/ZzPopupStylePrivate.{h,cpp}` 集中标准弹层适配，
  由 `ZzFluentStyle.cpp` 的 polish／unpolish／eventFilter 与 subControlRect 接入；
  `ZzFluentStylePrivate.cpp` 共用菜单栏内缩常量。注册源文件到 `ZzFluentUI/CMakeLists.txt`。
- [x] 构建并运行 Qt 6.8.3 与 6.11.1：

  ```bash
  cmake --build build/spectrum-qt683-release --target ZzPopupSurfacesTest ZzComboBoxControlsTest ZzFluentScreenshotTest --parallel 2
  ctest --test-dir build/spectrum-qt683-release -R '^fluent\.(popup-surfaces|combo-box-controls|screenshot-.*)$' --output-on-failure --parallel 2
  ```

  Qt 6.11 使用 `build/linux-gcc-debug`；保留选择、滚动、键盘和无障碍既有回归。
- [x] 独立复审，运行完整架构和文档审计，修复阻塞问题后提交代码。
- [x] 最后更新 `docs/development/FLUENT_UPSTREAM_FOLLOWUP_2026-11-05_ZH.md`
  的实测数据与原生平台验证边界，单独提交文档。

## 完成记录

代码提交 `7266f7d`。两版 Qt 弹层／组合框 CTest 各 2/2，四档截图各 4/4，
架构与文档审计 2/2 通过；Qt 6.11.1 的 xcb/Xvfb 菜单 20 行、组合框 34 行通过。
未改动截图基线、遮罩、性能门限或第三方源码；架构颜色豁免仅随新增 include 移动行号。

复审要求保留 Qt 已选的 `menu->screen()`、通过 QAction 关联查找活动菜单栏，
并排除 ComboBox 自定义同父弹层，均已处理。后两项新增回归先失败再通过。
容器名称识别由两个 Qt 版本的真实弹窗测试约束，不会在 polish 中懒创建 view。
软件阴影未移植；多屏、Wayland、Windows／macOS 仍保留明确的实机验证边界。
