#pragma once

#include <cstdint>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QVector>

#include <ZzCore/ZzResult.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzPureTools/ZzWorkspaceActivityId.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>

class QAction;
class QAbstractListModel;
class QMainWindow;
class QModelIndex;
class QWidget;

namespace ZzFluentUI {
class ZzActivityBar;
class ZzBottomPane;
class ZzCommandPalette;
class ZzDockPanel;
class ZzFluentTitleBar;
class ZzSidePane;
class ZzSplitWorkspace;
class ZzTabWidget;
}

namespace ZzPureTools {

class ZzWorkspaceShell;
class ZzWorkspaceLayoutTransactionPrivate;
class ZzWorkspaceNavigationIntegrationTransactionPrivate;

/** @brief 管理工作区稳定对象、面板注册表和布局恢复事务。 */
class ZzWorkspaceShellPrivate final
{
public:
    enum class ZzTransactionKind : std::uint8_t
    {
        None,
        LayoutRestore,
        ActivityMove,
        SideActivation,
        NavigationIntegration
    };

    enum class ZzPanelKind : std::uint8_t
    {
        Side,
        Bottom,
        Dock
    };

    /** @brief 区分可选择 Side Panel 与不可选择固定动作模型行。 */
    enum class ZzActivityRowKind : std::uint8_t
    {
        SidePanel,
        FixedAction
    };

    /** @brief 描述延迟 Side 内容尚未创建、创建中或已被接管。 */
    enum class ZzMaterializationState : std::uint8_t
    {
        Pending,
        Materializing,
        Ready
    };

    struct ZzPanelRecord final
    {
        ZzWorkspacePanelId id;
        QString title;
        ZzFluentUI::ZzIconDescriptor icon;
        ZzPanelKind kind = ZzPanelKind::Side;
        ZzFluentUI::ZzActivityArea activityArea =
            ZzFluentUI::ZzActivityArea::LeftPrimary;
        Qt::DockWidgetArea dockArea = Qt::NoDockWidgetArea;
        QPointer<QWidget> content;
        QWidget *contentIdentity = nullptr;
        QPointer<QWidget> contentOwner;
        QWidget *contentOwnerIdentity = nullptr;
        ZzWorkspacePanelFactory factory;
        ZzMaterializationState materialization =
            ZzMaterializationState::Ready;
        std::uint64_t registrationGeneration = 0;
        QPointer<QObject> dock;
        ZzFluentUI::ZzDockPanel *dockIdentity = nullptr;
        QMetaObject::Connection contentDestroyedConnection;
        bool registrationInProgress = false;
        bool removalInProgress = false;
    };

    struct ZzSideLayoutEntry final
    {
        ZzWorkspacePanelId id;
        ZzFluentUI::ZzActivityArea area =
            ZzFluentUI::ZzActivityArea::LeftPrimary;
        int order = 0;
    };

    /** @brief 非拥有地观察固定动作及其模型同步连接。 */
    struct ZzFixedActivityRecord final
    {
        ZzWorkspaceActivityId id;
        QPointer<QAction> action;
        QAction *actionIdentity = nullptr;
        QMetaObject::Connection destroyedConnection;
        QMetaObject::Connection changedConnection;
        bool registrationInProgress = false;
        bool actionChangePending = false;
    };

    struct ZzActivityRowSnapshot final
    {
        ZzWorkspacePanelId id;
        QString title;
        ZzFluentUI::ZzIconDescriptor icon;
        ZzFluentUI::ZzActivityArea area =
            ZzFluentUI::ZzActivityArea::LeftPrimary;
        int badge = 0;
        int order = 0;
    };

    /** @brief 创建固定工作区对象并连接同步信号。 */
    ZzWorkspaceShellPrivate(
        ZzWorkspaceShell *publicObject,
        QMainWindow *host,
        ZzFluentUI::ZzFluentTitleBar *titleBar);

    /** @brief 在宿主仍存活时同步移除并销毁 Shell 对象。 */
    ~ZzWorkspaceShellPrivate();

    [[nodiscard]] ZzCore::ZzResult<void> registerSidePanel(
        const ZzWorkspacePanelId &id,
        const QString &title,
        ZzFluentUI::ZzIconDescriptor icon,
        ZzFluentUI::ZzActivityArea area,
        QWidget *content,
        bool withinNavigationIntegration = false);
    [[nodiscard]] ZzCore::ZzResult<void> registerSidePanelFactory(
        const ZzWorkspacePanelId &id,
        const QString &title,
        ZzFluentUI::ZzIconDescriptor icon,
        ZzFluentUI::ZzActivityArea area,
        ZzWorkspacePanelFactory factory);
    [[nodiscard]] ZzCore::ZzResult<void> registerFixedActivityAction(
        const ZzWorkspaceActivityId &id,
        const QString &title,
        ZzFluentUI::ZzIconDescriptor icon,
        ZzFluentUI::ZzActivityArea area,
        QAction *action);
    /** @brief 将已创建的无父 Side 内容事务接管到其逻辑区域。 */
    [[nodiscard]] ZzCore::ZzResult<void> adoptSidePanelContent(
        const ZzWorkspacePanelId &id,
        std::uint64_t registrationGeneration,
        QWidget *content,
        bool activate);
    [[nodiscard]] ZzCore::ZzResult<std::unique_ptr<QWidget>>
    createPendingSidePanelContent(const ZzWorkspacePanelId &id);
    /** @brief 调用 Pending factory 并在失败时恢复可重试状态。 */
    [[nodiscard]] ZzCore::ZzResult<void> materializeSidePanel(
        const ZzWorkspacePanelId &id);
    [[nodiscard]] ZzCore::ZzResult<void> registerDockPanel(
        const ZzWorkspacePanelId &id,
        const QString &title,
        ZzFluentUI::ZzIconDescriptor icon,
        Qt::DockWidgetArea area,
        QWidget *content);
    /** @brief 预占全局标识后事务接管中央底部面板内容。 */
    [[nodiscard]] ZzCore::ZzResult<void> registerBottomPanel(
        const ZzWorkspacePanelId &id,
        const QString &title,
        const ZzFluentUI::ZzIconDescriptor &icon,
        QWidget *content);
    [[nodiscard]] ZzCore::ZzResult<QWidget *> takePanel(
        const ZzWorkspacePanelId &id,
        bool withinNavigationIntegration = false);
    [[nodiscard]] ZzCore::ZzResult<void> integrateApplicationNavigation(
        const ZzWorkspacePanelId &panelId,
        const QString &panelTitle,
        ZzFluentUI::ZzIconDescriptor icon,
        ZzFluentUI::ZzActivityArea area,
        const QString &centralTabTitle);
    [[nodiscard]] ZzCore::ZzResult<void> showPanel(
        const ZzWorkspacePanelId &id,
        bool visible);
    [[nodiscard]] ZzCore::ZzResult<void> setPanelBadge(
        const ZzWorkspacePanelId &id,
        int value);
    [[nodiscard]] ZzCore::ZzResult<void> setAlwaysOnTop(bool alwaysOnTop);
    [[nodiscard]] ZzCore::ZzResult<QByteArray> saveLayout() const;
    [[nodiscard]] ZzCore::ZzResult<void> restoreLayout(
        const QByteArray &state);

    /** @brief 根据当前标签和策略同步宿主及标题栏文本。 */
    void refreshTitle();

    /** @brief 重新绑定当前活动标签组的展示与当前页信号。 */
    void refreshActiveTabConnections();

    /** @brief 更新当前页标题观察连接并刷新标题。 */
    void refreshCurrentTabConnection();

    /** @brief 为注册内容建立销毁清理连接。 */
    void connectPanelContentDestroyed(
        const ZzWorkspacePanelId &id,
        QWidget *content);

    /** @brief 清理被外部销毁内容对应的注册与界面状态。 */
    void handlePanelContentDestroyed(
        const ZzWorkspacePanelId &id,
        QWidget *contentIdentity);

    /** @brief 回滚尚未提交的面板注册。 */
    void rollbackPanelRegistration(
        const ZzWorkspacePanelId &id,
        QWidget *contentIdentity);

    /** @brief 清理移除事务中已失效的注册，不夺回第三方内容。 */
    void cleanupInterruptedPanelRemoval(
        const ZzWorkspacePanelId &id,
        QWidget *contentIdentity,
        std::uint64_t registrationGeneration);

    /** @brief 解除 Dock 当前内容，并只在容器稳定为空时销毁容器。 */
    [[nodiscard]] bool cleanupDockPanel(
        ZzFluentUI::ZzDockPanel *dockIdentity);

    /** @brief 析构时同步收敛任意有限回调链中的待清理 Dock。 */
    void cleanupPendingDockPanelForDestruction(
        ZzPanelRecord expected);

    /** @brief 保留事务记录，并在当前同步回调退出后重试移除清理。 */
    void scheduleInterruptedPanelRemovalCleanup(
        const ZzWorkspacePanelId &id,
        QWidget *contentIdentity,
        std::uint64_t registrationGeneration);

    /** @brief 将 Activity 激活或折叠意图分派给 Side Panel 或固定动作。 */
    void activateActivity(const QModelIndex &sourceIndex, bool collapse);

    /** @brief QAction 销毁后按活动标识和动作身份移除固定入口。 */
    void handleFixedActivityActionDestroyed(
        const ZzWorkspaceActivityId &id,
        QAction *actionIdentity);

    /** @brief QAction 状态改变后刷新对应活动标识的模型行 flags。 */
    void handleFixedActivityActionChanged(
        const ZzWorkspaceActivityId &id,
        QAction *actionIdentity);

    /** @brief 将 Activity 拖放意图交给不可变移动事务。 */
    void moveSidePanel(
        const QModelIndex &sourceIndex,
        ZzFluentUI::ZzActivityArea targetArea,
        int targetRow);

    /** @brief 捕获当前 Activity model 的全局行顺序和区域。 */
    [[nodiscard]] QVector<ZzSideLayoutEntry> activityRows() const;

    /** @brief 捕获一行完整 Activity 数据，供同步信号后原位恢复。 */
    [[nodiscard]] std::optional<ZzActivityRowSnapshot> activityRowSnapshot(
        const ZzWorkspacePanelId &id) const;

    /** @brief 以标准结构信号移除指定 Activity 行。 */
    [[nodiscard]] bool removeActivityRow(const ZzWorkspacePanelId &id);

    /** @brief 以标准结构信号恢复完整 Activity 行。 */
    [[nodiscard]] bool restoreActivityRow(
        const ZzActivityRowSnapshot &snapshot);

    /** @brief 更新 Activity 行区域并发出 dataChanged。 */
    [[nodiscard]] bool setActivityRowArea(
        const ZzWorkspacePanelId &id,
        ZzFluentUI::ZzActivityArea area);

    /** @brief 以单次 model reset 替换完整 Activity 行投影。 */
    [[nodiscard]] bool replaceActivityRows(
        const QVector<ZzSideLayoutEntry> &rows);

    /** @brief 将含 FixedAction 的区域投影行换算为纯 SidePanel 插入行。 */
    [[nodiscard]] int sidePanelTargetRow(
        ZzFluentUI::ZzActivityArea area,
        int projectionRow) const noexcept;

    /** @brief 隐藏没有已注册内容的边缘，并收起其 Side Pane。 */
    void syncSideEdgeVisibility();

    /** @brief 返回所有面板与固定动作共享域中是否已有稳定字符串。 */
    [[nodiscard]] bool hasRegisteredStableId(
        const QString &value) const noexcept;

    [[nodiscard]] int fixedActivityIndex(
        const ZzWorkspaceActivityId &id) const noexcept;
    [[nodiscard]] int fixedActivityIndex(
        const ZzWorkspaceActivityId &id,
        QAction *actionIdentity) const noexcept;
    [[nodiscard]] int indexOf(const ZzWorkspacePanelId &id) const noexcept;
    [[nodiscard]] int stablePanelIndex(
        const ZzPanelRecord &expected) const noexcept;
    [[nodiscard]] ZzWorkspacePanelId currentSideId(
        ZzFluentUI::ZzSidePane *pane) const;
    ZzWorkspaceShell *const q_ptr;
    QPointer<QMainWindow> host;
    QPointer<QObject> hostObject;
    QPointer<ZzFluentUI::ZzFluentTitleBar> titleBar;
    QPointer<QWidget> workspaceRoot;
    QPointer<ZzFluentUI::ZzActivityBar> leftActivityBar;
    QPointer<ZzFluentUI::ZzActivityBar> rightActivityBar;
    QPointer<ZzFluentUI::ZzSidePane> leftSidePane;
    QPointer<ZzFluentUI::ZzSidePane> rightSidePane;
    QPointer<QWidget> centerHost;
    QPointer<ZzFluentUI::ZzSplitWorkspace> splitWorkspace;
    QPointer<ZzFluentUI::ZzBottomPane> bottomPane;
    QPointer<ZzFluentUI::ZzTabWidget> activeTabs;
    QPointer<ZzFluentUI::ZzCommandPalette> palette;
    QPointer<QAbstractListModel> activityModel;
    QVector<ZzPanelRecord> panels;
    QVector<ZzFixedActivityRecord> fixedActivities;
    ZzWorkspacePanelId leftCurrentPanel;
    ZzWorkspacePanelId rightCurrentPanel;
    QString applicationTitle;
    QString customTitle;
    ZzWorkspaceTitleMode titleMode = ZzWorkspaceTitleMode::Application;
    std::uint64_t nextPanelRegistrationGeneration = 0;
    std::uint64_t titleRefreshGeneration = 0;
    int sideEdgeVisibilitySyncDepth = 0;
    bool leftPaneExpanded = false;
    bool rightPaneExpanded = false;
    bool applicationNavigationIntegrated = false;
    ZzTransactionKind transactionKind = ZzTransactionKind::None;
    QMetaObject::Connection activeTabChangedConnection;
    QMetaObject::Connection activeTabPresentationConnection;
    QMetaObject::Connection currentTabTitleConnection;
    QMetaObject::Connection navigationTabPinnedConnection;
    QMetaObject::Connection navigationTabCloseConnection;

    friend class ZzWorkspaceLayoutTransactionPrivate;
    friend class ZzWorkspaceNavigationIntegrationTransactionPrivate;
};

} // namespace ZzPureTools
