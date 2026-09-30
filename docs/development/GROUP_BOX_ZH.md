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

边框颜色和圆角使用主题令牌，高对比模式保留清晰边界。
标题布局、助记键、焦点、勾选行为、子控件启用状态和无障碍交给 Qt；
不增加动画、定时器或额外容器控件。
组合绘制通过 `QCommonStyle` 公共流程调用本库的边框与勾选指示器，
避免 Fusion 在系统高对比模式下绕过边框绘制、将扁平模式画成完整框。

Example 的“基础控件 → 分组框(GroupBox)”展示五种模式，支持在不同主题下观察。

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
