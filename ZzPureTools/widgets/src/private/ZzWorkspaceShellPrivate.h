#pragma once

#include <cstdint>

#include <QtCore/QByteArray>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QVector>

#include <ZzCore/ZzResult.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>

class QAbstractListModel;
class QMainWindow;
class QModelIndex;
class QWidget;

namespace ZzFluentUI {
class ZzActivityBar;
class ZzCommandPalette;
class ZzDockPanel;
class ZzFluentTitleBar;
class ZzSidePane;
class ZzTabWidget;
}

namespace ZzPureTools {

class ZzWorkspaceShell;

/** @brief 管理工作区稳定对象、面板注册表和布局恢复事务。 */
class ZzWorkspaceShellPrivate final
{
public:
    enum class ZzPanelKind : std::uint8_t
    {
        Side,
        Dock
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
        QPointer<ZzFluentUI::ZzDockPanel> dock;
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

    struct ZzLayoutState final
    {
        QByteArray qtState;
        bool leftCollapsed = false;
        int leftWidth = 280;
        bool rightCollapsed = false;
        int rightWidth = 280;
        ZzWorkspacePanelId leftCurrent;
        ZzWorkspacePanelId rightCurrent;
        QVector<ZzSideLayoutEntry> sideEntries;
        int currentTabIndex = -1;
        ZzWorkspaceTitleMode titleMode = ZzWorkspaceTitleMode::Application;
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
        QWidget *content);
    [[nodiscard]] ZzCore::ZzResult<void> registerDockPanel(
        const ZzWorkspacePanelId &id,
        const QString &title,
        ZzFluentUI::ZzIconDescriptor icon,
        Qt::DockWidgetArea area,
        QWidget *content);
    [[nodiscard]] ZzCore::ZzResult<QWidget *> takePanel(
        const ZzWorkspacePanelId &id);
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

    /** @brief 将 Activity 激活或折叠意图应用到对应 Side Panel。 */
    void activateSidePanel(const QModelIndex &sourceIndex, bool collapse);

    /** @brief 隐藏没有已注册内容的边缘，并收起其 Side Pane。 */
    void syncSideEdgeVisibility();

    [[nodiscard]] int indexOf(const ZzWorkspacePanelId &id) const noexcept;
    [[nodiscard]] ZzWorkspacePanelId currentSideId(
        ZzFluentUI::ZzSidePane *pane) const;
    [[nodiscard]] ZzLayoutState captureLayoutState() const;
    [[nodiscard]] bool applyShellLayout(const ZzLayoutState &state);

    ZzWorkspaceShell *const q_ptr;
    QPointer<QMainWindow> host;
    QPointer<ZzFluentUI::ZzFluentTitleBar> titleBar;
    QPointer<QWidget> workspaceRoot;
    QPointer<ZzFluentUI::ZzActivityBar> leftActivityBar;
    QPointer<ZzFluentUI::ZzActivityBar> rightActivityBar;
    QPointer<ZzFluentUI::ZzSidePane> leftSidePane;
    QPointer<ZzFluentUI::ZzSidePane> rightSidePane;
    QPointer<ZzFluentUI::ZzTabWidget> tabs;
    QPointer<ZzFluentUI::ZzCommandPalette> palette;
    QPointer<QAbstractListModel> activityModel;
    QVector<ZzPanelRecord> panels;
    QString applicationTitle;
    QString customTitle;
    ZzWorkspaceTitleMode titleMode = ZzWorkspaceTitleMode::Application;
    QMetaObject::Connection currentTabTitleConnection;
};

} // namespace ZzPureTools
