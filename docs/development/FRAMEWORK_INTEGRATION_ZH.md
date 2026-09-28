# ZzPureToolsFrame 应用接入指南

本文说明其他 Qt 应用如何把 `ZzPureToolsFrame` 作为通用桌面框架使用。框架不依赖
SSH、SFTP、终端、数据库或任何具体业务协议；这些能力必须由应用自己的业务层提供。

## 分层边界

| 层 | 负责内容 | 不负责内容 |
|---|---|---|
| `ZzWindowKit` | 无边框窗口、系统按钮命中和平台窗口行为 | 页面导航、业务状态、网络连接 |
| `ZzFluentUI` | Fluent 主题、绘制、输入、标签、侧栏、命令面板和停靠面板 | 业务模型、持久化、网络请求 |
| `ZzPureTools` | 应用生命周期、窗口创建、路由、页面宿主和工作区协调 | 具体领域对象和协议实现 |
| 应用层 | Presenter/ViewModel、设置存储、业务服务和错误策略 | 修改框架内部状态机 |

UI 只通过信号或公开意图接口通知应用层。Presenter/ViewModel 将业务状态转换为页面
需要的快照，页面不直接读取服务、数据库或设置存储。

## 最小窗口装配

默认中央区域是无标签堆叠页面，接入见 [中央堆叠页面](STACKED_WORKSPACE_ZH.md)。
下文含标签撕出与拓扑恢复的示例显式选择 `Tabbed` 模式；普通堆叠应用不需要这些步骤。

应用通常从 `ZzPureApplication` 和 `ZzApplicationBuilder` 开始，在窗口设置回调中创建
工作区。下面的代码展示核心顺序，页面注册和错误处理应保留在应用自己的启动模块中。
工作区指针应保存到应用自己的窗口装配对象中，不能让局部 `unique_ptr` 在回调返回时销毁：

以下为装配结构示意，省略 `include` 和应用层的 `findWindowShell()` 实现：

```cpp
class MyWindowShell final : public QObject
{
public:
    // 这是应用层自定义的生命周期方法，不属于 ZzPureToolsFrame。
    void attachWorkspace(
        std::unique_ptr<ZzPureTools::ZzWorkspaceShell> workspace)
    {
        workspace_ = std::move(workspace);
    }

    QWidget *workspaceWidget() const noexcept
    {
        return workspace_ != nullptr ? workspace_->workspaceWidget() : nullptr;
    }

private:
    std::unique_ptr<ZzPureTools::ZzWorkspaceShell> workspace_;
};

auto setupResult = builder.setWindowSetupCallback(
    [](ZzPureTools::ZzApplicationWindow &window) {
        auto shellResult = ZzPureTools::ZzWorkspaceShell::create(
            &window, window.titleBar(), ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
        if (!shellResult) {
            return ZzCore::ZzResult<void>::failure(shellResult.error());
        }

        auto shell = std::move(shellResult).value();
        shell->setApplicationTitle(QStringLiteral("MyApplication"));
        shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);

        // 由应用自己的窗口装配对象保存 shell，并在完成注册后挂载根控件。
        // 这里的 workspaceOwner 是应用层对象，不是框架提供的 API。
        auto *workspaceOwner = findWindowShell(window);
        workspaceOwner->attachWorkspace(std::move(shell));
        window.setCentralWidget(workspaceOwner->workspaceWidget());
        return ZzCore::ZzResult<void>::success();
    });
```

上例中的 `findWindowShell()` 和 `workspaceOwner` 只用于表达应用层生命周期管理，
不是框架 API。实际项目可以参考 Example 的 `ZzExampleWindowShell`，让装配对象以窗口
为父对象，并在初始化阶段保存 `std::unique_ptr<ZzWorkspaceShell>`。

## 多窗口工作区

首窗和后续窗口由同一个 `ZzWorkspaceWindowCoordinator` 管理。首窗完成 Shell 装配后
登记为 `primary`，随后设置窗口工厂。工厂只负责创建并返回新的窗口和 Shell；业务页面
由 `tearOff()` 或 `restoreTopology()` 事务迁移，不能在工厂中预先添加：

```cpp
auto *coordinator = application.workspaceWindowCoordinator();
auto registration = coordinator->registerWindow(
    {&window, windowShell->workspaceShell()}, primaryConfiguration, true);
if (!registration) {
    return registration;
}

coordinator->setWindowFactory(
    [&application](const ZzPureTools::ZzWorkspaceWindowCreateOptions &options) {
        const auto created = application.createWindow(options.visibility);
        if (!created) {
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>
                ::failure(created.error());
        }
        auto *window = created.value();
        auto *shell = findWorkspaceShell(window);
        if (shell == nullptr) {
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>
                ::failure(ZzCore::ZzError(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("window shell is not attached")));
        }
        return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success({
            window, shell});
    });
```

用户从标签栏撕出页面时，连接 `ZzSplitWorkspace::tabTearOffRequested` 或直接调用
协调器的 `tearOff()`。调用方应使用 `Deferred` 创建选项，让窗口在页面迁移、布局和
配置审计成功后才显示：

```cpp
ZzPureTools::ZzWorkspaceWindowCreateOptions options;
options.configurationSource =
    ZzPureTools::ZzWorkspaceConfigurationSource::SourceWindow;
options.sourceWindow = &window;
options.visibility = ZzPureTools::ZzApplicationWindowVisibility::Deferred;
const auto result = coordinator->tearOff(
    workspace->splitWorkspace(), groupId, tabIndex, options);
```

窗口可以通过 `ZzWorkspaceWindowConfigurationPatch` 独立覆盖标题、图标、最小尺寸、
置顶和关闭策略；不设置覆盖时才继承来源窗口配置。`Allow`、`Deny` 和 `Delegate` 的
关闭语义由协调器统一执行。对 `Delegate` 窗口监听
`windowCloseApprovalRequested`，完成业务保存后调用 `approveDelegatedClose()`；不要
在 `windowAboutToClose` 或 `orphanedPages` 信号中删除仍需回收的页面。

跨窗口程序化迁移使用 `transferTabToWorkspace()`。该调用在 GUI 线程中完成双边事务，
失败时页面、标签顺序、活动页和稳定身份均保持不变：

```cpp
const auto moved = sourceWorkspace->transferTabToWorkspace(
    sourceGroup, sourceIndex, targetWorkspace, targetGroup);
if (!moved) {
    logWorkspaceError(moved.error());
}
```

持久化时使用 `saveTopology()` 保存 `ZZWT` v2 字节；恢复前必须保证已登记工作区没有
业务页面，并通过 `ZzWorkspacePageResolver` 按 `layoutKey` 返回全新的无父 QWidget：

```cpp
const auto snapshot = coordinator->saveTopology();
if (snapshot) {
    const auto restored = coordinator->restoreTopology(
        snapshot.value(),
        [](QStringView key) {
            return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                createPageForLayoutKey(key));
        });
}
```

`layoutKey` 是应用层稳定页面类型或文档标识，必须非空、唯一且不包含 QObject 指针、
网络凭据或跨进程句柄。`ZZSW` v1 仅用于单窗口兼容导入；恢复失败由框架回滚临时窗口、
页面和配置，应用只需记录错误并保留原拓扑。

## 注册工作区表面

`ZzWorkspaceShell` 只接管无父对象的 QWidget，注册成功后负责其父对象生命周期。应用层
仍然拥有业务模型和 Presenter：

```cpp
auto sessions = shell->registerSidePanelFactory(
    ZzPureTools::ZzWorkspacePanelId(QStringLiteral("sessions")),
    tr("Sessions"),
    ZzFluentUI::ZzIconDescriptor::fromSvgResource(
        QStringLiteral(":/icons/sessions.svg")),
    ZzFluentUI::ZzActivityArea::LeftPrimary,
    [sessionModel] {
        return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
            createSessionPanel(sessionModel));
    });
```

常用表面与所有权规则如下：

| 表面 | 注册接口 | 适合内容 | 内容所有者 |
|---|---|---|---|
| 左右侧栏 | `registerSidePanel()` / `registerSidePanelFactory()` | 会话、文件、属性、任务、导航 | `ZzWorkspaceShell` 注册成功后接管 |
| 中央堆叠页面（默认） | `stackWidget()->addWidget()` | 任意应用页面 | `QStackedWidget` 接管页面；移除不销毁 |
| 中央标签（显式 Tabbed） | `tabWidget()->addTab()` | 文档、终端、编辑器、预览 | `ZzTabWidget` 按 Qt 标签页规则管理 |
| 底部工具区 | `registerBottomPanel()` | 日志、输出、诊断、任务结果 | `ZzWorkspaceShell` 注册成功后接管 |
| 原生停靠区 | `registerDockPanel()` | 需要自由浮动或重新停靠的工具 | `ZzDockPanel`/`QMainWindow` 管理 |
| 固定动作 | `registerFixedActivityAction()` | 设置、帮助等稳定入口 | `QAction` 由应用层拥有，Shell 非拥有观察 |

应用导航面板使用 `integrateApplicationNavigation()`。该操作会把已有
`ZzNavigationPane` 放入侧栏，把 `ZzPageHost` 放入中央堆叠页面（显式 Tabbed 模式才是固定标签）；调用方不应再复制一套
导航模型，也不应把设置页面伪装成普通路由。

## 命令和标签

`ZzCommandPalette` 只消费应用提供的平面 `QAbstractItemModel`。模型行应该包含稳定的
命令标识、展示文本和可选快捷键；`commandActivated` 发出后由应用层执行真正动作。
命令面板不应直接打开网络连接、修改设置或访问业务对象。

标签页同样只处理页面展示状态：固定、脏状态、注意提示、关闭意图和标题同步。关闭或
转移信号交给应用层决定是否保存、销毁或创建新的窗口。

## 业务适配示例

以 SSH 客户端为例，SSH 只是一种应用业务实现，不应进入框架模块：

```text
SshApplication
  ├─ SshSessionService / SshConnectionModel
  ├─ SshWorkspacePresenter
  └─ ZzWorkspaceShell
       ├─ Sessions SidePanel       <- 展示会话快照
       ├─ Files SidePanel          <- 展示 SFTP 快照
       ├─ Terminal ZzTabWidget     <- 展示终端 View
       ├─ Properties SidePanel     <- 展示当前会话属性
       └─ Output ZzBottomPane      <- 展示日志和命令输出
```

其他应用可以用本地文件管理器、数据库客户端、监控工具或编辑器替换这些面板，而无需
修改 `ZzFluentUI` 或 `ZzPureTools`。如果某个能力只服务于一个业务协议，应放在独立的
应用库或适配模块中，通过 Presenter 和公开工作区接口连接。

## 生命周期与线程要求

1. 所有 QWidget、QAction 和模型注册都在 GUI 线程完成。
2. `register*()` 成功前，内容必须是无父对象；失败时调用方仍拥有内容。
3. Shell 不保存业务对象的裸指针，不从全局容器查找服务。
4. 异步任务在业务层完成，结果通过信号或不可变快照回到 GUI 线程。
5. 窗口销毁前先停止业务订阅，再释放 Presenter 和 Shell；不要在控件析构函数中执行网络
   请求或阻塞等待。
6. 布局通过 `saveLayout()` 和 `restoreLayout()` 持久化。应用只保存返回的版本化字节，
   不要自行解析内部 JSON/二进制格式。

## 验证清单

接入一个新应用时，至少验证以下内容：

- 业务库不被 `ZzFluentUI`、`ZzPureTools` 或 `ZzWindowKit` 链接。
- UI 源码没有直接访问数据库、网络客户端、设置存储或领域模型。
- 每个面板、标签和命令都有稳定标识，重复注册能返回 `ZzResult` 错误。
- 面板移动、布局恢复、窗口关闭和业务服务停止顺序都有自动化测试。
- Linux 先完成真实桌面验收，再由 Windows 和 macOS 做对应工具链构建与人工检查。
