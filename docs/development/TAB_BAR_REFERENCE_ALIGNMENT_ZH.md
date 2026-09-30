# TabBar 原版细节对齐计划

目标：按 FluentUIStyle 原始 PageTab 与绘制源码修正已确认的差异；采用 Qt 公开 API，保留 RTL、键盘、禁用、高对比度、减少动态效果及快速切换保障。
参考项目只读，不改动 docs/research，不提交或推送。本次延续工作区中已导入的 Segoe 字体。

## 任务与验收

- [x] 示例页：恢复五组卡片、14 px 粗体小标题、12 px 灰色说明；Pivot 15 px 粗体，五个原版标签与字形；三组自定义胶囊，Pill 关闭按钮、Capsule 内容页、导航内容页与 220 ms 纵向切换。
- [x] 字体和关闭按钮：保留原 TTF，提供 25/30 字形画布比例的 Gallery 图标；普通图标使用主题原色，关闭按钮绘制 Segoe U+E894，覆盖启用、禁用、悬停及高 DPI。
- [x] 样式布局：普通图文间距恢复 Qt 4 px 布局，WinUI3 专用布局，Navigation 前导 5 px；保留非方形按钮与 RTL 兼容。选中背景、文字颜色和指示条位置对照原图修正。
- [x] 动效：Pivot Grow、Segmented Slide 的进度曲线以及 Pivot Stretch 的分阶段拉伸/回收/回弹对齐源码；保持中断时从当前状态继续和减少动态效果行为。
- [x] 验证：先增加能抓住关闭按钮和几何/动效破坏的聚焦测试，再实现。构建示例与测试，使用原始 PageTab 源码生成浅深色并排截图，核对四档 DPR 和高对比度。对照截图通过后才更新自身基线。

## 文件边界

示例页仅改 `examples/ZzPureToolsExample/ZzExampleTabBarsPage.cpp` 及必要的示例翻译。
字体接口在 `ZzSegoeIconFont.h/.cpp`；关闭按钮在 `ZzFluentStyle.cpp`；标签绘制和动画在 `ZzTabBarStylePrivate.cpp`。
验证放在 `ZzIconFontTest.cpp`、`ZzTabControlsTest.cpp`、`ZzFluentScreenshotTest.cpp`。
本地原源码对照工具位于忽略的 `build/groupbox-reference-src/tab-details.cpp`，输出 `build/tab-bar-details`。

## 验证记录（2026-09-30）

- 构建示例及相关测试成功；字体 SHA-256 与参考 TTF 完全相同。
- 10 项 CTest 全部通过：字体、标签交互、选择指示条、四档 DPR 截图、中文/英文示例集成、工作区烟测。
- 浅深色完整页和标签栏并排图已人工核对；仅更新三主题 × 四档 DPR 的 TabBar 外观基线。
- 原始示例页本地交互检查覆盖导航切换、连续重定向、隐藏、缩放、减少动态效果、RTL、Pill/Capsule 关闭及动画清理。
- 独立审查发现的窄标签指示条消失、宽按钮遮挡图标及非方形图标拉伸均已修复，新增测试先失败再通过；高对比度选中关闭叉号另有对比度回归。
- 验证环境为 Linux / GCC 15 / Qt 6.11.1；Windows / MSVC 与 Qt 6.8 未实机验证。未提交或推送。
