# 基础控件独立演示页实现计划

> 面向 AI 代理：使用 executing-plans 按以下清单实施；用户已批准独立页面和中英文名称，直接完成实现与验证。

**目标：** 将 Example 基础控件长页拆为独立路由，每个控件的变体在同页展示，导航采用“按钮(PushButton)”格式。

**架构：** 保留 Activity Bar、树导航与中央堆叠宿主。路由目录记录控件类型；页面工厂按类型创建纯展示页。使用既有 Recreatable 生命周期与有界缓存，不预建全部页面。

**技术栈：** Qt 6.8+ Widgets、C++20、CMake presets。

## 范围与顺序

参考 `/home/zz/Jackfahdin/github/FluentUIStyle/Examples/Gallery/mainwindow.ui` 的基础控件布局行序：Button、RadioButton/CheckBox、LineEdit、ComboBox、SpinBox、TimeEdit、Slider/Progress。

基础控件依次为 PushButton、IconButton、ToolButton、RadioButton、CheckBox、ToggleSwitch、LineEdit、PlainTextEdit、ComboBox、MultiSelectComboBox、SpinBox、DoubleSpinBox、CalendarPicker、RollerPicker、Slider、ProgressBar、ProgressRing。MessageBar、InfoBadge 放入反馈与状态分区；已有选择指示条对比保留为交互下的单独页面。其他大类暂不拆分。

## 实施清单

- [x] 路由与页面：创建 `ZzExampleControlKind.h`、`ZzExampleControlPage.h/.cpp`、`ZzExampleControlPagePrivate.h/.cpp`；修改 `ZzExampleRouteCatalog.h/.cpp` 与 `ZzExamplePageFactory.cpp`。每个控件演示基本/禁用/可编辑或忙碌等适用状态，设置区只控制本页预览。
- [x] 清理与集成：修改 `ZzExampleGalleryPage.h`、`ZzExampleGalleryPagePrivate.h/.cpp`，移除混合 Controls 页，保留首页和独立指示条演示；首页入口改为按钮页。更新 `CMakeLists.txt`、英语翻译、使用旧 controls 路由的烟测和性能轮转路由。
- [x] 验证：更新烟测以按独立路由检查实际内容，验证按钮切换、进度数值联动及非当前控件不混入页面。执行 `cmake --build build/linux-gcc-debug --target ZzPureToolsExample ZzExampleWorkspaceSmokeTest --parallel 2` 和 `ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^example\.'`。
- [x] 视觉：检查按钮、输入、日期与进度页；更新 Example 导航变化涉及的四档 DPR 首页截图基线及 SHA-256，关闭更新模式复验。保留截图阈值。
- [x] 提交：检查差异与文件范围，中文简述加详细正文提交；不推送，不触碰 `docs/research/`。

## 验收边界

导航条目使用完整中文名和英文控件名，点击进入对应页面，无外层标签。窄窗口内容可滚动，长标题可换行。基础控件之间的变体不跨页创建；辅助输入只用于调节当前控件。主题沿用全局设置。Linux Debug 实测，Windows/macOS 本轮仅静态检查。

## 完成记录

- Linux GCC 15 / Qt 6.11.1 Debug：Example 与工作区烟测目标编译通过。
- `ctest --test-dir build/linux-gcc-debug --parallel 2 --output-on-failure -R '^(example\.|fluent\.(calendar-controls|roller-controls)$)'`：18/18 通过，包含关闭截图更新模式后的四档 DPR 比较、中英文路由、多窗口及工作区验证。
- 已导出全部 29 个路由窗口，检查按钮、组合框、日期选择和进度条页面；首页检查 100% 三主题与 200% Light。12 张基线 SHA-256 已同步。
- 独立代码审查未发现阻塞问题；按审查建议保留原生强调色按钮与局部强调色开关演示。
- Windows/macOS 仅静态检查；本轮未执行全性能矩阵，未调整性能或截图阈值。
