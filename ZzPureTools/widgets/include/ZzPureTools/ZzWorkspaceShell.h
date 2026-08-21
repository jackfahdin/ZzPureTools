#pragma once

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/Qt>

#include <ZzCore/ZzResult.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzPureTools/ZzPureToolsExport.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>

class QMainWindow;
class QWidget;

namespace ZzFluentUI {
class ZzActivityBar;
class ZzCommandPalette;
class ZzFluentTitleBar;
class ZzSidePane;
class ZzTabWidget;
}

namespace ZzPureTools {

class ZzWorkspaceShellPrivate;

/**
 * @brief 协调宿主窗口中的 Fluent 工作区组件、面板、标题和布局。
 *
 * Shell 不替调用方设置 QMainWindow centralWidget，也不创建 WindowAgent。
 * create() 返回的 unique_ptr 拥有 Shell；内部 QWidget 挂入 host 对象树。
 */
class ZZ_PURE_TOOLS_EXPORT ZzWorkspaceShell final : public QObject
{
    Q_DISABLE_COPY_MOVE(ZzWorkspaceShell)

public:
    /**
     * @brief 校验宿主和可选标题栏后创建工作区。
     * @param host 非空、当前 GUI 线程中的顶层 QMainWindow。
     * @param titleBar 可空；非空时必须是 host 的同线程后代。
     * @return 成功时返回独占 Shell，失败时不创建工作区对象。
     */
    [[nodiscard]] static ZzCore::ZzResult<std::unique_ptr<ZzWorkspaceShell>>
    create(
        QMainWindow *host,
        ZzFluentUI::ZzFluentTitleBar *titleBar = nullptr);

    /** @brief 移除 Shell 创建的 Dock 和工作区，host 已销毁时安全结束。 */
    ~ZzWorkspaceShell() override;

    /** @brief 返回待由调用方挂载的工作区根控件。 */
    [[nodiscard]] QWidget *workspaceWidget() const noexcept;

    /** @brief 返回中央标签控件。 */
    [[nodiscard]] ZzFluentUI::ZzTabWidget *tabWidget() const noexcept;

    /** @brief 返回工作区内覆盖式命令面板。 */
    [[nodiscard]] ZzFluentUI::ZzCommandPalette *commandPalette() const noexcept;

    /** @brief 返回指定物理侧的 Activity Bar。 */
    [[nodiscard]] ZzFluentUI::ZzActivityBar *activityBar(
        ZzFluentUI::ZzSidePaneEdge edge) const noexcept;

    /** @brief 返回指定物理侧的 Side Pane。 */
    [[nodiscard]] ZzFluentUI::ZzSidePane *sidePane(
        ZzFluentUI::ZzSidePaneEdge edge) const noexcept;

    /** @brief 校验后接管无父对象内容，并注册到对应侧栏分组。 */
    [[nodiscard]] ZzCore::ZzResult<void> registerSidePanel(
        const ZzWorkspacePanelId &id,
        QString title,
        ZzFluentUI::ZzIconDescriptor icon,
        ZzFluentUI::ZzActivityArea area,
        QWidget *content);

    /** @brief 校验后创建原生 Dock 并接管无父对象内容。 */
    [[nodiscard]] ZzCore::ZzResult<void> registerDockPanel(
        const ZzWorkspacePanelId &id,
        QString title,
        ZzFluentUI::ZzIconDescriptor icon,
        Qt::DockWidgetArea area,
        QWidget *content);

    /** @brief 从 Side Pane 或 Dock 中移除面板并归还无父对象内容。 */
    [[nodiscard]] ZzCore::ZzResult<QWidget *> takePanel(
        const ZzWorkspacePanelId &id);

    /** @brief 展开/折叠 Side Pane，或显示/隐藏 Dock。 */
    [[nodiscard]] ZzCore::ZzResult<void> showPanel(
        const ZzWorkspacePanelId &id,
        bool visible = true);

    /** @brief 更新 Side Panel 活动入口的非负徽标值。 */
    [[nodiscard]] ZzCore::ZzResult<void> setPanelBadge(
        const ZzWorkspacePanelId &id,
        int value);

    /** @brief 返回当前应用标题。 */
    [[nodiscard]] QString applicationTitle() const;

    /** @brief 设置应用标题并刷新当前标题策略。 */
    void setApplicationTitle(QString title);

    /** @brief 返回当前显式自定义标题。 */
    [[nodiscard]] QString customTitle() const;

    /** @brief 设置自定义标题并刷新当前标题策略。 */
    void setCustomTitle(QString title);

    /** @brief 返回当前标题策略。 */
    [[nodiscard]] ZzWorkspaceTitleMode titleMode() const noexcept;

    /** @brief 设置标题策略并同步宿主和可选 Fluent 标题栏。 */
    void setTitleMode(ZzWorkspaceTitleMode mode);

    /** @brief 返回宿主当前真实置顶标志。 */
    [[nodiscard]] bool isAlwaysOnTop() const noexcept;

    /** @brief 修改宿主置顶标志并保持原可见性和窗口状态。 */
    [[nodiscard]] ZzCore::ZzResult<void> setAlwaysOnTop(bool alwaysOnTop);

    /** @brief 保存带版本、长度和 SHA-256 校验的工作区布局。 */
    [[nodiscard]] ZzCore::ZzResult<QByteArray> saveLayout() const;

    /** @brief 事务恢复不超过 1 MiB 的工作区布局。 */
    [[nodiscard]] ZzCore::ZzResult<void> restoreLayout(
        const QByteArray &state);

private:
    ZzWorkspaceShell(
        QMainWindow *host,
        ZzFluentUI::ZzFluentTitleBar *titleBar);

    std::unique_ptr<ZzWorkspaceShellPrivate> d_ptr;
};

} // namespace ZzPureTools
