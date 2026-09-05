# 工作区公共 API 使用约定

本文面向直接集成 `ZzPureToolsFrame` 的 Qt 应用。工作区组件只协调界面结构和用户意图，
不依赖 SSH、终端、数据库或其他业务协议；应用层负责业务模型、Presenter、设置存储和
持久化策略。

## 最小装配

`ZzWorkspaceShell::create()` 要求宿主是 GUI 线程中的顶层 `QMainWindow`。可选的
`ZzFluentTitleBar` 必须已经是宿主的子孙控件。Shell 创建成功后，应用应保存返回的
`std::unique_ptr`，再把 `workspaceWidget()` 挂到宿主：

```cpp
auto result = ZzPureTools::ZzWorkspaceShell::create(
    &window, windowTitleBar);
if (!result) {
    return ZzCore::ZzResult<void>::failure(result.error());
}

workspaceShell_ = std::move(result).value();
window.setCentralWidget(workspaceShell_->workspaceWidget());
```

`workspaceWidget()`、`activityBar()`、`sidePane()`、`splitWorkspace()`、`bottomPane()`
和 `commandPalette()` 都是非拥有观察指针，生命周期不超过 Shell 和宿主窗口。

## 面板所有权

Side、Bottom 和 Dock 面板注册前，内容 `QWidget` 必须是 GUI 线程中的无父对象。注册成功
后，Shell 接管内容并负责销毁；注册失败时，调用方仍拥有原对象。需要移除面板时使用
`takePanel()`，成功返回同一指针且父对象为空：

```cpp
auto *content = new QWidget;
const auto id = ZzPureTools::ZzWorkspacePanelId(
    QStringLiteral("diagnostics"));
auto registered = workspaceShell_->registerSidePanel(
    id, QStringLiteral("Diagnostics"), {},
    ZzFluentUI::ZzActivityArea::RightSecondary, content);
if (!registered) {
    delete content;
    return ZzCore::ZzResult<void>::failure(registered.error());
}

auto removed = workspaceShell_->takePanel(id);
if (removed) {
    delete std::move(removed).value();
}
```

`registerSidePanelFactory()` 用于延迟创建不常用面板。工厂在首次显示或移除 Pending 面板
时调用；注册阶段不会调用。工厂失败返回错误且保留 Pending 状态，之后再次 `showPanel()`
可以重试。工厂必须在 GUI 线程返回无父 `QWidget`，不得捕获短生命周期的业务对象。

| 表面 | 注册接口 | 框架职责 | 应用职责 |
|---|---|---|---|
| 左右侧栏 | `registerSidePanel()` / `registerSidePanelFactory()` | Activity、显隐、宽度、布局 | 提供内容和业务快照 |
| 中央标签 | `tabWidget()->addTab()` | 标签顺序、固定、脏状态、关闭意图 | 决定保存、关闭和页面生命周期 |
| 底部工具区 | `registerBottomPanel()` | 面板显隐、尺寸和布局 | 提供日志、输出或诊断视图 |
| 原生停靠区 | `registerDockPanel()` | Fluent 标题栏和 Qt 停靠协议 | 决定停靠区域和内容销毁 |
| 固定入口 | `registerFixedActivityAction()` | 非拥有观察 `QAction` 状态 | 拥有 QAction 并执行实际命令 |

## 模型和意图

`ZzActivityBar`、`ZzCommandPalette` 和 `ZzExplorerPane` 只观察调用方提供的
`QAbstractItemModel`，不取得模型所有权，也不访问业务服务。用户操作通过
`activationRequested`、`collapseRequested`、`moveRequested` 或 `commandActivated` 信号
发出，应用层决定是否修改模型和执行命令。

`ZzWorkspaceShell` 负责把 Activity 行和 Side Pane 对齐，但不会替应用修改模型。固定
Activity 的 `QAction` 由应用层拥有，Shell 只在动作销毁或状态变化时移除、刷新对应入口。

## 标签、标题和布局

标签页状态通过 `ZzTabWidget` 的公开 API 设置：`setTabPinned()`、`setTabModified()`、
`setTabAttention()` 和 `setTabCloseEnabled()`。关闭、拖出和跨容器转移都只发出意图，
应用层负责保存数据、创建新窗口或销毁页面。

标题由 Shell 统一计算。应用使用 `setApplicationTitle()`、`setCustomTitle()` 和
`setTitleMode()`，不应让标题栏反向查找标签或业务模型。`setAlwaysOnTop()` 只在 GUI 线程
修改宿主真实窗口标志，并保持窗口可见性；标题栏按钮本身只发出请求信号。

`saveLayout()` 返回带版本和校验的字节串，`restoreLayout()` 负责校验并事务恢复。应用只
保存和恢复完整字节串，不应解析内部二进制格式，也不应在恢复失败后自行拼接部分状态。

## 多窗口协调

多窗口能力由 `ZzPureApplication` 持有的唯一
`ZzWorkspaceWindowCoordinator` 提供。首窗装配完成后，应用使用
`ZzWorkspaceWindowHandle` 登记窗口和对应的 `ZzWorkspaceShell`；协调器只保存非拥有
观察值，窗口本身仍由应用对象负责拥有：

```cpp
auto *coordinator = application.workspaceWindowCoordinator();
ZzPureTools::ZzWorkspaceWindowHandle primary{
    &window, windowShell->workspaceShell()};
ZzPureTools::ZzWorkspaceWindowConfiguration configuration;
configuration.title = QStringLiteral("MyApplication");
configuration.closePolicy = ZzPureTools::ZzWindowClosePolicy::Delegate;
auto registered = coordinator->registerWindow(
    primary, configuration, true);
if (!registered) {
    qFatal("register primary workspace window failed");
}
```

后续窗口必须通过工厂创建。工厂需要完成与首窗相同的 Shell 装配，但不得预先创建
业务页面；协调器会在事务中迁移页面并在成功后显示窗口。配置来源可以选择默认值、
来源窗口或调用方显式覆盖：

```cpp
coordinator->setWindowFactory(
    [&application](const ZzPureTools::ZzWorkspaceWindowCreateOptions &options) {
        const auto created = application.createWindow(
            options.visibility);
        if (!created) {
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>
                ::failure(created.error());
        }
        auto *window = created.value();
        auto *shell = findWorkspaceShell(window); // 应用层观察函数
        if (shell == nullptr) {
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>
                ::failure(ZzCore::ZzError(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("window shell is not attached")));
        }
        return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success({
            window, shell});
    });

ZzPureTools::ZzWorkspaceWindowCreateOptions options;
options.configurationSource =
    ZzPureTools::ZzWorkspaceConfigurationSource::SourceWindow;
options.sourceWindow = &window;
options.visibility = ZzPureTools::ZzApplicationWindowVisibility::Deferred;
auto created = coordinator->createWindow(options);
```

标签撕出和程序化迁移都必须调用协调器或 `ZzSplitWorkspace` 的事务接口。撕出窗口在
迁移提交前保持不可见；页面指针、稳定 `pageId`、标签元数据和 `layoutKey` 会一并转移：

```cpp
const auto group = sourceShell->splitWorkspace()->activeGroupId();
auto tornOff = coordinator->tearOff(
    sourceShell->splitWorkspace(), group, tabIndex, options);

auto moved = sourceShell->splitWorkspace()->transferTabToWorkspace(
    group, tabIndex, targetShell->splitWorkspace(),
    targetShell->splitWorkspace()->activeGroupId());
```

关闭策略为 `Allow` 时，协调器按来源栈回收页面后关闭窗口；`Deny` 会拒绝关闭；
`Delegate` 首次请求只发出 `windowCloseApprovalRequested`，应用完成保存或确认后调用
`approveDelegatedClose()`，不得在信号回调中直接销毁页面或绕过协调器。关闭期间会冻结
公开迁移，找不到任何有效回收目标的页面通过 `orphanedPages` 返回给应用，应用必须继续
拥有这些页面，不能假定框架会销毁它们。

跨重启保存使用 `saveTopology()`，恢复前所有已登记工作区必须为空。解析器按稳定
`layoutKey` 创建 GUI 线程中的无父页面，每个键必须只解析一次：

```cpp
const auto state = coordinator->saveTopology();
if (state) {
    auto restored = coordinator->restoreTopology(
        state.value(),
        [](QStringView key) {
            return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                createPageForLayoutKey(key));
        });
}
```

`ZZWT` v2 保存窗口配置、几何、分屏树、页面顺序和来源栈；`ZZSW` v1 只能作为单窗口
兼容输入。拓扑保存要求每个页面都有非空且唯一的 `layoutKey`，因此临时页面在进入
持久化流程前必须先设置键。布局键只用于恢复定位，不应编码 QObject 指针、业务凭据或
跨进程句柄。恢复失败会回滚已创建窗口、页面和配置，调用方无需自行清理半成品。

## 安装包消费

应用可以链接 shared 或 static 安装包，公共头和目标名称保持一致：

```cmake
find_package(ZzPureToolsFrame 0.1 CONFIG REQUIRED)
target_link_libraries(MyApplication PRIVATE Zz::PureTools Zz::FluentUI)
```

在仓库中，`tests/InstallConsumer/Gui/main.cpp` 是完整 GUI 消费示例，
`ZzPureTools/tests/ZzWorkspacePublicApiTest.cpp` 是只使用公开头的工作区契约测试。Linux
本机可执行：

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON
cmake --build --preset linux-gcc-debug --target ZzWorkspacePublicApiTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools.workspace-public-api$' --output-on-failure
```

shared/static 安装消费还应运行 `install.consumer` 和 `architecture.public-headers`。
Windows MSVC、Windows Qt MinGW 与 macOS 的公共 API 只使用 Qt 公开的 Core、Gui 和
Widgets 接口；本机没有这些平台的运行证据时，不应把静态检查写成真机通过。

## 线程和生命周期清单

1. 所有 QWidget、QAction、模型和 Shell API 调用都在 GUI 线程完成。
2. `register*()` 成功前内容必须无父；失败时调用方继续拥有内容。
3. Shell、Activity Bar、命令面板和 Dock 不保存业务对象的裸指针。
4. 异步任务在应用业务层完成，结果以信号或不可变快照回到 GUI 线程。
5. 窗口销毁前先停止业务订阅，再释放 Presenter 和 Shell。
6. 任何公开指针都是非拥有观察值，不能跨宿主窗口或 Shell 保存。
