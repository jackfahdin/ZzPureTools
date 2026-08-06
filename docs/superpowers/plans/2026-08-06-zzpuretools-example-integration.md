# ZzPureToolsExample 端到端集成实施计划

> 文档状态：已确认，实施中
>
> 确认日期：2026-08-06
>
> 目标仓库：`/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro`
>
> 旧版参考：仓库父目录中的 `ZzPureToolsExample`

## 1. 目标与边界

新增正式的 `ZzPureToolsExample`，以旧版示例的可见功能、页面结构和操作路径为参考，完成 ZzCore、ZzWindowKit、ZzFluentUI、ZzPureTools 与 ZzLog 的端到端装配。

本示例同时承担以下责任：

- 展示无边框窗口、Fluent 主题、应用导航、页面生命周期和多窗口隔离。
- 证明 View、ViewModel、Presenter 与应用服务之间没有反向依赖。
- 提供 Linux 物理桌面最终效果验收入口。
- 为 Windows MSVC/MinGW 和 macOS 提供可编译、可静态审计的同一源码入口。

现有 `ZzWindowKitDemo`、`ZzFluentFoundationDemo`、`ZzFluentControlsGallery` 和 `ZzPureToolsDemo` 保留，继续承担单组件 smoke 和故障隔离。新示例不替代这些目标，也不默认安装到 SDK 发布包。

旧版 API、全局 EventBus、全局撤销栈、服务定位器、字体图标枚举、Windows DXGI 私有实现和无授权素材不迁移。

## 2. 框架增量契约

### 2.1 窗口装配回调

新增公开类型：

```cpp
namespace ZzPureTools {

using ZzWindowSetupCallback = std::function<ZzCore::ZzResult<void>(
    ZzApplicationWindow &window)>;

} // namespace ZzPureTools
```

`ZzApplicationBuilder::setWindowSetupCallback()` 只允许成功设置一次。空回调、重复设置、Builder 冻结或移动后调用必须返回明确错误。

回调执行顺序固定为：

1. 创建 `ZzApplicationWindow` 和基础 QWidget 子树。
2. 创建窗口独占导航模型、页面宿主和导航控制器。
3. 调用窗口装配回调。
4. 附加 ZzWindowKit 并使用最终标题栏交互控件配置 chrome。
5. 执行首次导航。
6. 把完整窗口提交给 `ZzPureApplication`，随后才允许显示。

回调由应用保存，对首窗和后续 `createWindow()` 创建的全部窗口生效。回调失败时不得显示或收养半初始化窗口；已经创建的 Qt 子对象按窗口所有权树清理。

### 2.2 窗口壳层

`ZzApplicationWindow` 增加以下 GUI 线程观察接口：

- `ZzFluentTitleBar *titleBar() const noexcept`
- `ZzNavigationPane *navigationPane() const noexcept`

标题栏使用 `QMainWindow::setMenuWidget()` 放在标准工具栏、Dock、中央区和状态栏之上。应用装配回调使用继承的 `addToolBar()`、`addDockWidget()` 和 `setStatusBar()`，不访问 private layout。

### 2.3 双向历史

`ZzNavigationController` 增加：

- `goForward()`
- `canGoForward()`
- `historyStateChanged(bool canGoBack, bool canGoForward)`

直接成功导航时，旧当前路由进入 back 栈并清空 forward 栈；成功后退时当前路由进入 forward 栈；成功前进时当前路由进入 back 栈。页面创建或切换失败时两个栈和当前路由均不得变化。历史容量同时约束两个方向，每个方向都裁剪最旧项，零值禁用历史。

## 3. 应用结构

### 3.1 Composition root

`main.cpp` 只负责：

1. 准备 ZzWindowKit。
2. 创建 `ZzPureApplication`。
3. 创建共享的 `ZzExampleApplicationContext`。
4. 注册 `ZzExampleApplicationModule`、页面 factory、导航节点和窗口装配回调。
5. 构建并运行应用。

`ZzExampleApplicationContext` 由 `std::shared_ptr` 共享，包含设置存储、任务执行器、活动模型和只读应用能力，不包含 QWidget。页面 factory 可以捕获它完成构造注入。

`ZzExampleApplicationModule` 管理 ZzLog、`ZzQtLogBridge`、任务停止和最终设置同步。模块停止协议保持幂等，ZzLog 初始化失败必须通过 `ZzResult` 阻止应用构建。

### 3.2 展示边界

- View 只创建控件、设置可访问文本、显示 ViewModel 状态并发出用户意图。
- ViewModel 只保存展示值或实现 `QAbstractItemModel`，不访问设置、日志、文件或窗口。
- Presenter 连接 View 意图与注入的主题、导航、任务、设置和活动接口。
- UI 源码禁止 include ZzLog、ZzQtSettingsStore 或平台 API。
- 复杂 View、Window Shell 与 Presenter 使用四文件 Pimpl；简单值模型和无状态 factory 使用两文件结构。

### 3.3 窗口壳层

每个窗口由独立 `ZzExampleWindowShell` 装配：

- 顶部命令栏：返回、前进、页面搜索、主题切换、新建窗口和窗口菜单。
- 右侧 Dock：活动日志与更新内容。
- 状态栏：当前路由、任务状态和平台名称。
- 关闭守卫：取消、最小化和确认关闭。

多窗口共享主题、设置和活动数据；导航控制器、历史、页面实例、Dock 状态、关闭守卫和 ZzWindowAgent 必须逐窗口隔离。

活动 Dock 不观察 spdlog sink。Presenter 将同一条结构化活动同时追加到有界活动模型并写入 ZzLog，避免给日志热路径增加 GUI 回调。

## 4. 页面与路由

| 路由 | 展示位置 | 生命周期 | 旧版对应 | 新版内容 |
|---|---|---|---|---|
| `home` | Workspace | Persistent | `T_Home` | 主视觉、快捷卡片、最近活动 |
| `controls` | Controls | Recreatable | `T_BaseComponents` | 按钮、输入、选择、开关、进度、日期和滚轮 |
| `cards` | Controls | Recreatable | `T_Card` | ActionCard、ImageCard、Carousel、LCD |
| `list-view` | Data views | Recreatable | `T_ListView` | 列表 delegate、筛选和选择 |
| `table-view` | Data views | Recreatable | `T_TableView` | 排序、列调整和模型更新 |
| `tree-view` | Data views | Recreatable | `T_TreeView` | 层级模型、展开和选择 |
| `navigation` | Interaction | Recreatable | `T_Navigation` | Breadcrumb、Tab、标签转移和历史 |
| `feedback` | Interaction | Recreatable | `T_Popup` | Menu、Dialog、MessageBar、SuggestBox |
| `icons` | Resources | Recreatable | `T_Icon` | 本地示例图标和 Qt 标准图标搜索 |
| `platform` | System | Persistent | `T_ZzScreen` | 屏幕、DPI、窗口状态和 WindowKit 能力 |
| `settings` | Footer | Persistent | `T_Setting` | 主题、日志等级、Dock 与窗口设置 |
| `about` | Footer | Persistent | `T_About` | 版本、编译器、许可证和第三方信息 |

导航继续使用平面值模型。旧版仅用于压力演示的多层 `TEST_EXPAND_NODE` 不迁移；分区使用 section metadata，设置和关于使用 footer placement。

## 5. 视觉、资源与本地化

- 高还原指页面结构、导航层级、控件组合、Dock、状态栏和主要操作路径，不复制旧版实现。
- 旧版动漫图片、来源不明的控制截图和两个约 45 MiB GIF 禁止进入新仓库。
- 首页和卡片使用原创轻量位图，总资源预算不超过 8 MiB，不在运行时下载。
- 示例 SVG 图标只属于示例资源，不形成新的生产图标枚举或字体依赖。
- 每个非代码资源记录来源、作者、许可证、用途和 SHA-256。
- 中文为源码语言；英文 translator 仅在系统 locale 为英文时由 Builder 加载。
- Light、Dark、HighContrast、LTR/RTL 和 100% 至 200% DPR 必须保持可操作且无文字重叠。

## 6. 验收与提交

每个逻辑批次通过对应测试后立即提交，提交标题为中文简述，正文为中文修改细节。禁止把多个未验证批次堆积到单个提交。

自动验证必须覆盖：

- 窗口装配回调顺序、失败回滚和后续窗口复用。
- back/forward、搜索导航、失败不修改历史和窗口隔离。
- 页面生命周期、设置持久化、主题切换、任务取消和模块关闭。
- 全部路由可达，工具栏、Dock、状态栏和关闭守卫可操作。
- Linux shared/static、GCC/Clang、ASan、clang-tidy 和完整 CTest。
- Windows MSVC/MinGW 与 macOS 的 CMake 和源码静态审计。
- 四档 DPR 与 Light、Dark、HighContrast 的确定性截图。
- 启动、空闲、页面切换、主题切换和大模型滚动继续满足现有性能门禁。

Linux 物理桌面最终验收以 `ZzPureToolsExample` 为主入口，检查窗口拖动、系统按钮、主题、搜索、双向导航、Dock、多窗口、DPI、平台能力与关闭流程。
