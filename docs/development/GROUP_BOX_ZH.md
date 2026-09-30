# 分组框（GroupBox）

使用 Qt 原生 `QGroupBox` 搭配 `ZzFluentStyle`，无需额外包装类。
参考 FluentUIStyle 的细线圆角边框与扁平分隔线，绘制实现在库内，所有使用该样式的项目均可复用。

| 模式 | Qt 接口 | 效果 |
| --- | --- | --- |
| 普通 | `setTitle()` | 标题、圆角细线边框，内部不额外填色 |
| 可勾选 | `setCheckable(true)` | 勾选框使用公共 Fluent 样式；取消勾选时禁用内部控件 |
| 禁用 | `setEnabled(false)` | 标题、勾选框和内部控件使用禁用状态 |
| 扁平 | `setFlat(true)` | 仅保留顶部细线，适合轻量分区 |
| 嵌套 | 布局中加入另一个 `QGroupBox` | 分层组织内容，所有权沿用 Qt |

```cpp
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QVBoxLayout>
#include <ZzFluentUI/ZzPushButton.h>

// 应用已安装 ZzFluentStyle。
auto *group = new QGroupBox(QStringLiteral("高级选项"), parent);
group->setCheckable(true);
auto *layout = new QVBoxLayout(group);
layout->addWidget(new ZzFluentUI::ZzPushButton(QStringLiteral("执行"), group));
```

边框对齐 FluentUIStyle 的 `frameColorStrong`：浅色使用约 60.63% 不透明度的黑色，
深色使用约 60.47% 不透明度的白色，普通框内缩 1.5px，默认圆角为 4px。
高对比模式使用调色板的按钮文字色，圆角仍读取本库主题令牌。
标题布局、助记键、焦点、勾选行为、子控件启用状态和无障碍交给 Qt；
不增加动画、定时器或额外容器控件。
边框独立绘制，标题、焦点及勾选指示器使用 `QCommonStyle` 公共流程。
边框裁剪参照 Fusion 的标题上半区规则，保留标题下方完整的上边线，
同时避免 Fusion 在系统高对比模式下绕过边框绘制、将扁平模式画成完整框。

Example 的“基础控件 → 分组框(GroupBox)”展示五种模式，支持在不同主题下观察。
普通/可勾选分组并排伸展，禁用/扁平分组放在第二行，嵌套示例独占一行。

## 实施范围与验证计划

1. `ZzFluentStyleTest.cpp` 补充普通/扁平、浅色/深色/高对比边框测试，先复现旧样式差异。
2. `ZzFluentStyle.cpp` 为 `CC_GroupBox` 使用公共组合绘制，将 `PE_FrameGroupBox` 委派给私有绘制；
   `ZzFluentStylePrivate.h/.cpp` 只负责主题化边框，不接管 Qt 的交互与布局。
3. `ZzExampleControlKind.h`、`ZzExampleRouteCatalog.cpp` 和
   `ZzExampleControlPagePrivate.h/.cpp` 注册并装配独立演示页，补充英文翻译。
4. `ZzExampleSmokeControllerPrivate.cpp` 验证页面五种状态及勾选对内部控件的影响，
   `ZzExampleWorkspaceSmokeTest.cpp` 更新路由总数。
5. 完整构建 `linux-gcc-debug`，运行常规及定向测试、四档截图，导出并查看真实页面。
   代码、文档及验证结果一并中文提交，不推送。

## 本机验证结果（2026-09-30）

- Linux、Qt 6.11.1、GCC 15.2，完整 Debug 构建及 217 项常规测试通过。
- `fluent.style`、中英文 Example 集成及工作区烟测共四项定向测试通过。
- 样式测试共 17 项通过；新增边框及基础样式绕过 primitive 的回归测试均先失败、后通过。
- 四档已有截图回归通过；单独导出并检查新分组框页面的真实窗口截图。
- 日志：`build/linux-gcc-debug/groupbox-gate.log`、`groupbox-screenshots.log`。
  页面预览：`build/linux-gcc-debug/groupbox-preview/group-box.png`。
- Windows/macOS、Qt 6.8、性能门禁及物理桌面人工验收未在本轮执行。

## 视觉修正（2026-09-30）

- 上一次使用过淡的通用边框色，并由公共组合绘制裁掉整个标题矩形，
  导致标题下方出现缺口；已有 primitive 测试未覆盖完整控件的组合绘制。
- 本次编译运行本机 FluentUIStyle，以相同 Qt 6.11.1 和字体对照浅/深色截图，
  对齐边框颜色、内缩及勾选框位置。参考项目源码保持不变。
  对照工具仅在自身进程中关闭参考样式因离屏色系 Unknown 而误启用的高对比分支。
- 完整控件的标题下边线回归已验证先失败、修正后通过；样式测试 18 项通过。
- 本次定向 CTest 共 8 项通过，包含样式测试、四档已有截图回归、中英文 Example
  集成及工作区烟测。已有截图套件不含 GroupBox 专用基线，其视觉另外通过上述对照图核查。
- 通常使用的 Debug Example 已重新编译，真实页面截图为
  `build/linux-gcc-debug/groupbox-preview/group-box.png`。
- 浅/深色同场景对照为 `build/groupbox-reference/comparison-light.png` 和
  `comparison-dark.png`。页面背景、复选框等继续使用本库公共样式，并未替换其他控件。
