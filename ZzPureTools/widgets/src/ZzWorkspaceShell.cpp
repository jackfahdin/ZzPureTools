#include <ZzPureTools/ZzWorkspaceShell.h>

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

#include "private/ZzWorkspaceShellPrivate.h"

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzWorkspaceCreateFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<ZzValue>::failure(
        ZzCore::ZzError(code, std::move(message)));
}

[[nodiscard]] bool zzIsDescendantOf(
    const QObject *object,
    const QObject *ancestor) noexcept
{
    for (const QObject *current = object;
         current != nullptr;
         current = current->parent()) {
        if (current == ancestor) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool zzIsShellThread(const QObject *object) noexcept
{
    return object != nullptr
        && object->thread() == QThread::currentThread();
}

} // namespace

ZzCore::ZzResult<std::unique_ptr<ZzWorkspaceShell>>
ZzWorkspaceShell::create(
    QMainWindow *host,
    ZzFluentUI::ZzFluentTitleBar *titleBar)
{
    if (host == nullptr) {
        return zzWorkspaceCreateFailure<std::unique_ptr<ZzWorkspaceShell>>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace host must not be null"));
    }
    if (!zzIsShellThread(host)
        || QCoreApplication::instance() == nullptr
        || QCoreApplication::instance()->thread() != QThread::currentThread()) {
        return zzWorkspaceCreateFailure<std::unique_ptr<ZzWorkspaceShell>>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host must be used on the GUI thread"));
    }
    if (host->parentWidget() != nullptr || !host->isWindow()) {
        return zzWorkspaceCreateFailure<std::unique_ptr<ZzWorkspaceShell>>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace host must be a top-level QMainWindow"));
    }
    if (titleBar != nullptr
        && (!zzIsShellThread(titleBar)
            || !zzIsDescendantOf(titleBar, host))) {
        return zzWorkspaceCreateFailure<std::unique_ptr<ZzWorkspaceShell>>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace title bar must be a host descendant"));
    }
    return ZzCore::ZzResult<std::unique_ptr<ZzWorkspaceShell>>::success(
        std::unique_ptr<ZzWorkspaceShell>(
            new ZzWorkspaceShell(host, titleBar)));
}

ZzWorkspaceShell::ZzWorkspaceShell(
    QMainWindow *host,
    ZzFluentUI::ZzFluentTitleBar *titleBar)
    : QObject(nullptr)
    , d_ptr(std::make_unique<ZzWorkspaceShellPrivate>(
          this, host, titleBar))
{
}

ZzWorkspaceShell::~ZzWorkspaceShell() = default;

QWidget *ZzWorkspaceShell::workspaceWidget() const noexcept
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return nullptr;
    }
    return d_ptr->workspaceRoot.data();
}

ZzFluentUI::ZzTabWidget *ZzWorkspaceShell::tabWidget() const noexcept
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return nullptr;
    }
    if (d_ptr->splitWorkspace == nullptr) {
        return nullptr;
    }
    return d_ptr->splitWorkspace->tabWidget(
        d_ptr->splitWorkspace->activeGroupId());
}

ZzFluentUI::ZzSplitWorkspace *
ZzWorkspaceShell::splitWorkspace() const noexcept
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return nullptr;
    }
    return d_ptr->splitWorkspace.data();
}

ZzFluentUI::ZzBottomPane *ZzWorkspaceShell::bottomPane() const noexcept
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return nullptr;
    }
    return d_ptr->bottomPane.data();
}

ZzFluentUI::ZzCommandPalette *
ZzWorkspaceShell::commandPalette() const noexcept
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return nullptr;
    }
    return d_ptr->palette.data();
}

ZzFluentUI::ZzActivityBar *ZzWorkspaceShell::activityBar(
    ZzFluentUI::ZzSidePaneEdge edge) const noexcept
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return nullptr;
    }
    return edge == ZzFluentUI::ZzSidePaneEdge::Left
        ? d_ptr->leftActivityBar.data() : d_ptr->rightActivityBar.data();
}

ZzFluentUI::ZzSidePane *ZzWorkspaceShell::sidePane(
    ZzFluentUI::ZzSidePaneEdge edge) const noexcept
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return nullptr;
    }
    return edge == ZzFluentUI::ZzSidePaneEdge::Left
        ? d_ptr->leftSidePane.data() : d_ptr->rightSidePane.data();
}

ZzCore::ZzResult<void> ZzWorkspaceShell::registerSidePanel(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    QWidget *content)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return zzWorkspaceCreateFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace operation requires its GUI thread"));
    }
    return d_ptr->registerSidePanel(
        id, title, std::move(icon), area, content);
}

ZzCore::ZzResult<void> ZzWorkspaceShell::registerDockPanel(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    Qt::DockWidgetArea area,
    QWidget *content)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return zzWorkspaceCreateFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace operation requires its GUI thread"));
    }
    return d_ptr->registerDockPanel(
        id, title, std::move(icon), area, content);
}

ZzCore::ZzResult<void> ZzWorkspaceShell::registerBottomPanel(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    QWidget *content)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return zzWorkspaceCreateFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace operation requires its GUI thread"));
    }
    return d_ptr->registerBottomPanel(
        id, title, std::move(icon), content);
}

ZzCore::ZzResult<QWidget *> ZzWorkspaceShell::takePanel(
    const ZzWorkspacePanelId &id)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return zzWorkspaceCreateFailure<QWidget *>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace operation requires its GUI thread"));
    }
    return d_ptr->takePanel(id);
}

ZzCore::ZzResult<void> ZzWorkspaceShell::showPanel(
    const ZzWorkspacePanelId &id,
    bool visible)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return zzWorkspaceCreateFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace operation requires its GUI thread"));
    }
    return d_ptr->showPanel(id, visible);
}

ZzCore::ZzResult<void> ZzWorkspaceShell::setPanelBadge(
    const ZzWorkspacePanelId &id,
    int value)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return zzWorkspaceCreateFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace operation requires its GUI thread"));
    }
    return d_ptr->setPanelBadge(id, value);
}

QString ZzWorkspaceShell::applicationTitle() const
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return {};
    }
    return d_ptr->applicationTitle;
}

void ZzWorkspaceShell::setApplicationTitle(QString title)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return;
    }
    d_ptr->applicationTitle = std::move(title);
    d_ptr->refreshTitle();
}

QString ZzWorkspaceShell::customTitle() const
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return {};
    }
    return d_ptr->customTitle;
}

void ZzWorkspaceShell::setCustomTitle(QString title)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return;
    }
    d_ptr->customTitle = std::move(title);
    d_ptr->refreshTitle();
}

ZzWorkspaceTitleMode ZzWorkspaceShell::titleMode() const noexcept
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return ZzWorkspaceTitleMode::Application;
    }
    return d_ptr->titleMode;
}

void ZzWorkspaceShell::setTitleMode(ZzWorkspaceTitleMode mode)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return;
    }
    switch (mode) {
    case ZzWorkspaceTitleMode::Application:
    case ZzWorkspaceTitleMode::CurrentTab:
    case ZzWorkspaceTitleMode::CurrentTabAndApplication:
    case ZzWorkspaceTitleMode::Custom:
        d_ptr->titleMode = mode;
        d_ptr->refreshTitle();
        return;
    }
}

bool ZzWorkspaceShell::isAlwaysOnTop() const noexcept
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return false;
    }
    return d_ptr->host != nullptr
        && d_ptr->host->windowFlags().testFlag(Qt::WindowStaysOnTopHint);
}

ZzCore::ZzResult<void> ZzWorkspaceShell::setAlwaysOnTop(bool alwaysOnTop)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return zzWorkspaceCreateFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace operation requires its GUI thread"));
    }
    return d_ptr->setAlwaysOnTop(alwaysOnTop);
}

ZzCore::ZzResult<QByteArray> ZzWorkspaceShell::saveLayout() const
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return zzWorkspaceCreateFailure<QByteArray>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace operation requires its GUI thread"));
    }
    return d_ptr->saveLayout();
}

ZzCore::ZzResult<void> ZzWorkspaceShell::restoreLayout(
    const QByteArray &state)
{
    Q_ASSERT(zzIsShellThread(this));
    if (!zzIsShellThread(this)) {
        return zzWorkspaceCreateFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace operation requires its GUI thread"));
    }
    return d_ptr->restoreLayout(state);
}

} // namespace ZzPureTools
