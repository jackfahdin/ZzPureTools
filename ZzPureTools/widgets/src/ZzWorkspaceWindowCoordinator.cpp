#include <ZzPureTools/ZzWorkspaceWindowCoordinator.h>

#include <algorithm>
#include <exception>
#include <utility>

#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzWorkspaceShell.h>

#include "private/ZzWorkspaceWindowCoordinatorPrivate.h"

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzCoordinatorFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        code, std::move(message)));
}

[[nodiscard]] bool zzIsUnspecifiedOrValidSize(const QSize &size) noexcept
{
    return size == QSize() || size.isValid();
}

[[nodiscard]] bool zzIsUnspecifiedOrValidGeometry(
    const QRect &geometry) noexcept
{
    return geometry == QRect() || geometry.isValid();
}

[[nodiscard]] bool zzIsValidTitleMode(
    ZzWorkspaceTitleMode mode) noexcept
{
    switch (mode) {
    case ZzWorkspaceTitleMode::Application:
    case ZzWorkspaceTitleMode::CurrentTab:
    case ZzWorkspaceTitleMode::CurrentTabAndApplication:
    case ZzWorkspaceTitleMode::Custom:
        return true;
    }
    return false;
}

[[nodiscard]] bool zzIsValidClosePolicy(
    ZzWindowClosePolicy policy) noexcept
{
    switch (policy) {
    case ZzWindowClosePolicy::Allow:
    case ZzWindowClosePolicy::Deny:
    case ZzWindowClosePolicy::Delegate:
        return true;
    }
    return false;
}

[[nodiscard]] bool zzHasValidConfiguration(
    const ZzWorkspaceWindowConfiguration &configuration) noexcept
{
    if (!zzIsUnspecifiedOrValidSize(configuration.minimumSize)
        || !zzIsUnspecifiedOrValidSize(configuration.maximumSize)
        || !zzIsUnspecifiedOrValidGeometry(configuration.initialGeometry)
        || !zzIsValidTitleMode(configuration.titleMode)
        || !zzIsValidClosePolicy(configuration.closePolicy)) {
        return false;
    }
    if (configuration.minimumSize != QSize()
        && configuration.maximumSize != QSize()
        && (configuration.minimumSize.width() > configuration.maximumSize.width()
            || configuration.minimumSize.height()
                > configuration.maximumSize.height())) {
        return false;
    }
    return true;
}

void zzApplyPatch(
    ZzWorkspaceWindowConfiguration &configuration,
    const ZzWorkspaceWindowConfigurationPatch &patch)
{
    if (patch.title) configuration.title = *patch.title;
    if (patch.icon) configuration.icon = *patch.icon;
    if (patch.titleMode) configuration.titleMode = *patch.titleMode;
    if (patch.closePolicy) configuration.closePolicy = *patch.closePolicy;
    if (patch.alwaysOnTop) configuration.alwaysOnTop = *patch.alwaysOnTop;
    if (patch.minimumSize) configuration.minimumSize = *patch.minimumSize;
    if (patch.maximumSize) configuration.maximumSize = *patch.maximumSize;
    if (patch.initialGeometry) configuration.initialGeometry = *patch.initialGeometry;
}

[[nodiscard]] ZzWorkspaceWindowConfigurationPatch zzFullPatch(
    const ZzWorkspaceWindowConfiguration &configuration)
{
    return {configuration.title, configuration.icon, configuration.titleMode,
        configuration.closePolicy, configuration.alwaysOnTop,
        configuration.minimumSize, configuration.maximumSize,
        configuration.initialGeometry};
}

[[nodiscard]] QRect zzConvergedTearOffGeometry(
    const QPoint &position,
    QSize size)
{
    size = size.expandedTo(QSize(1, 1));
    QScreen *screen = QGuiApplication::screenAt(position);
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        return QRect(position - QPoint(size.width() / 2, size.height() / 2), size);
    }
    const QRect available = screen->availableGeometry();
    size.setWidth(std::min(size.width(), available.width()));
    size.setHeight(std::min(size.height(), available.height()));
    QRect geometry(position - QPoint(size.width() / 2, size.height() / 2), size);
    geometry.moveLeft(std::clamp(geometry.left(), available.left(),
        available.right() - geometry.width() + 1));
    geometry.moveTop(std::clamp(geometry.top(), available.top(),
        available.bottom() - geometry.height() + 1));
    return geometry;
}

void zzCloseStagedWindow(ZzApplicationWindow *window)
{
    if (window != nullptr) {
        window->close();
    }
}

} // namespace

ZzWorkspaceWindowCoordinator::ZzWorkspaceWindowCoordinator(
    QObject *applicationObject)
    : QObject(nullptr)
    , d_ptr(std::make_unique<ZzWorkspaceWindowCoordinatorPrivate>(this))
{
    Q_ASSERT(applicationObject != nullptr);
    Q_ASSERT(applicationObject == nullptr
        || QThread::currentThread() == applicationObject->thread());
}

ZzWorkspaceWindowCoordinator::~ZzWorkspaceWindowCoordinator()
{
    beginShutdown();
}

ZzCore::ZzResult<void> ZzWorkspaceWindowCoordinator::registerWindow(
    const ZzWorkspaceWindowHandle &handle,
    const ZzWorkspaceWindowConfiguration &configuration,
    bool primary)
{
    if (QThread::currentThread() != thread()) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator called from a non-owner thread"));
    }
    if (d_ptr->shuttingDown) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator is shutting down"));
    }
    if (!handle.isValid()) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace window handle must contain a window and shell"));
    }
    if (handle.window->thread() != thread() || handle.shell->thread() != thread()) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window and shell must use the coordinator thread"));
    }
    if (!zzHasValidConfiguration(configuration)) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace window configuration is invalid"));
    }

    const auto duplicate = std::find_if(
        d_ptr->records.cbegin(),
        d_ptr->records.cend(),
        [&handle](const auto &record) {
            return record.windowIdentity == handle.window.data()
                || record.shellIdentity == handle.shell.data();
        });
    if (duplicate != d_ptr->records.cend()) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window or shell is already registered"));
    }
    if (handle.shell->workspaceWidget() == nullptr
        || handle.shell->workspaceWidget()->window() != handle.window.data()) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace shell does not belong to the application window"));
    }
    if (handle.shell->splitWorkspace() == nullptr) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace shell has no split workspace"));
    }
    if (primary && std::any_of(
            d_ptr->records.cbegin(),
            d_ptr->records.cend(),
            [](const auto &record) { return record.primary; })) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("only one workspace window may be primary"));
    }

    try {
        d_ptr->records.reserve(d_ptr->records.size() + 1);
        ZzWorkspaceWindowCoordinatorPrivate::ZzWindowRecord record;
        record.window = handle.window;
        record.windowIdentity = handle.window.data();
        record.windowId = QUuid::createUuid();
        record.shell = handle.shell;
        record.shellIdentity = handle.shell.data();
        record.configuration = configuration;
        record.primary = primary;
        d_ptr->records.push_back(std::move(record));
    } catch (const std::exception &exception) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to reserve workspace window registration storage: %1")
                .arg(QString::fromLocal8Bit(exception.what())));
    } catch (...) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to reserve workspace window registration storage"));
    }

    auto &record = d_ptr->records.back();
    record.windowDestroyedConnection = QObject::connect(
        handle.window,
        &QObject::destroyed,
        this,
        [this, identity = record.windowIdentity] {
            d_ptr->removeRecordForObject(identity);
        });
    record.shellDestroyedConnection = QObject::connect(
        handle.shell,
        &QObject::destroyed,
        this,
        [this, identity = record.shellIdentity] {
            d_ptr->removeRecordForObject(identity);
        });
    handle.window->installEventFilter(this);
    handle.shell->installEventFilter(this);
    record.tearOffConnection = QObject::connect(
        handle.shell->splitWorkspace(),
        &ZzFluentUI::ZzSplitWorkspace::tabTearOffRequested,
        this,
        [this, workspace = QPointer<ZzFluentUI::ZzSplitWorkspace>(
                   handle.shell->splitWorkspace())](
            const ZzFluentUI::ZzTabGroupId &group,
            int index,
            const ZzFluentUI::ZzWorkspacePageId &,
            const QPoint &position,
            const QSize &recommended) {
            if (workspace.isNull()) {
                return;
            }
            ZzWorkspaceWindowCreateOptions options;
            options.configurationSource =
                ZzWorkspaceConfigurationSource::SourceWindow;
            options.sourceWindow = qobject_cast<ZzApplicationWindow *>(
                workspace->window());
            options.configuration.initialGeometry =
                zzConvergedTearOffGeometry(position, recommended);
            static_cast<void>(tearOff(workspace.data(), group, index, options));
        });
    record.transferConnection = QObject::connect(handle.shell->splitWorkspace(),
        &ZzFluentUI::ZzSplitWorkspace::tabTransferCommitted,
        this,
        [this, targetWindow = record.windowIdentity](
            ZzFluentUI::ZzSplitWorkspace *sourceWorkspace,
            const ZzFluentUI::ZzTabGroupId &sourceGroup,
            int sourceIndex,
            const ZzFluentUI::ZzTabGroupId &,
            QWidget *page,
            const ZzFluentUI::ZzWorkspacePageId &pageId,
            ZzFluentUI::ZzWorkspaceDropZone) {
            d_ptr->recordTransfer(targetWindow,
                sourceWorkspace,
                sourceGroup,
                sourceIndex,
                page,
                pageId);
        });
    record.activePageConnection = QObject::connect(
        handle.shell->splitWorkspace(),
        &ZzFluentUI::ZzSplitWorkspace::activePageChanged,
        this,
        [this, window = record.windowIdentity](
            QWidget *page, const ZzFluentUI::ZzWorkspacePageId &id) {
            Q_EMIT activePageChanged(window, page, id);
        });
    record.pageActivityConnection = QObject::connect(
        handle.shell->splitWorkspace(),
        &ZzFluentUI::ZzSplitWorkspace::pageActivityChanged,
        this,
        [this, window = record.windowIdentity](
            QWidget *page, const ZzFluentUI::ZzWorkspacePageId &id,
            bool modified, bool attention) {
            Q_EMIT pageActivityChanged(window, page, id, modified, attention);
        });
    handle.window->setCloseAcceptanceCallback(
        [coordinator = QPointer<ZzWorkspaceWindowCoordinator>(this),
            window = record.windowIdentity] {
            if (coordinator.isNull()) return true;
            const auto result = coordinator->d_ptr->closeWindow(window, true);
            return result || result.error().code() == ZzCore::ZzErrorCode::NotFound;
        });
    return ZzCore::ZzResult<void>::success();
}

void ZzWorkspaceWindowCoordinator::setWindowFactory(
    ZzWorkspaceWindowFactory factory)
{
    if (QThread::currentThread() != thread() || d_ptr->shuttingDown) {
        return;
    }
    d_ptr->windowFactory = std::move(factory);
}

ZzCore::ZzResult<void> ZzWorkspaceWindowCoordinator::applyConfiguration(
    ZzApplicationWindow *window,
    const ZzWorkspaceWindowConfigurationPatch &patch)
{
    if (QThread::currentThread() != thread()) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator called from a non-owner thread"));
    }
    const auto iterator = std::find_if(d_ptr->records.begin(), d_ptr->records.end(),
        [window](const auto &record) { return record.windowIdentity == window; });
    if (window == nullptr || iterator == d_ptr->records.end()) {
        return zzCoordinatorFailure<void>(
            window == nullptr ? ZzCore::ZzErrorCode::InvalidArgument : ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("workspace window is not registered"));
    }
    ZzWorkspaceWindowConfiguration updated = iterator->configuration;
    zzApplyPatch(updated, patch);
    if (!zzHasValidConfiguration(updated)) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace window configuration is invalid"));
    }
    if (d_ptr->closeTransactionActive(*iterator)) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window close transaction is active"));
    }
    // 置顶是配置同步中唯一会报告失败的操作，必须在写入其他表面前预检。
    auto alwaysOnTopApplied = iterator->shell->setAlwaysOnTop(
        updated.alwaysOnTop);
    if (!alwaysOnTopApplied) {
        return alwaysOnTopApplied;
    }
    if (iterator->configuration.closePolicy != updated.closePolicy) {
        d_ptr->invalidateCloseRequest(*iterator);
    }
    window->setWindowIcon(updated.icon);
    window->setMaximumSize(updated.maximumSize == QSize()
        ? QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX)
        : updated.maximumSize);
    window->setMinimumSize(updated.minimumSize == QSize()
        ? QSize(0, 0)
        : updated.minimumSize);
    if (updated.initialGeometry != QRect()) {
        window->setGeometry(updated.initialGeometry);
    }
    iterator->shell->setApplicationTitle(updated.title);
    iterator->shell->setTitleMode(updated.titleMode);
    iterator->configuration = std::move(updated);
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<ZzWorkspaceWindowHandle>
ZzWorkspaceWindowCoordinator::createWindow(
    const ZzWorkspaceWindowCreateOptions &options)
{
    QElapsedTimer auditTimer;
    auditTimer.start();
    if (QThread::currentThread() != thread() || d_ptr->shuttingDown) {
        return zzCoordinatorFailure<ZzWorkspaceWindowHandle>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator is not accepting new windows"));
    }
    if (!d_ptr->windowFactory) {
        return zzCoordinatorFailure<ZzWorkspaceWindowHandle>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window factory is not configured"));
    }
    if (options.visibility != ZzApplicationWindowVisibility::Visible
        && options.visibility != ZzApplicationWindowVisibility::Deferred) {
        return zzCoordinatorFailure<ZzWorkspaceWindowHandle>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace window visibility is invalid"));
    }
    ZzWorkspaceWindowConfiguration resolved;
    switch (options.configurationSource) {
    case ZzWorkspaceConfigurationSource::CoordinatorDefaults:
    case ZzWorkspaceConfigurationSource::Explicit:
        break;
    case ZzWorkspaceConfigurationSource::SourceWindow: {
        const auto source = configuration(options.sourceWindow.data());
        if (!source) return ZzCore::ZzResult<ZzWorkspaceWindowHandle>::failure(source.error());
        resolved = source.value();
        break;
    }
    default:
        return zzCoordinatorFailure<ZzWorkspaceWindowHandle>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace configuration source is invalid"));
    }
    zzApplyPatch(resolved, options.configuration);
    if (!zzHasValidConfiguration(resolved)) {
        return zzCoordinatorFailure<ZzWorkspaceWindowHandle>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace window configuration is invalid"));
    }
    ZzCore::ZzResult<ZzWorkspaceWindowHandle> created =
        zzCoordinatorFailure<ZzWorkspaceWindowHandle>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("workspace window factory did not return a result"));
    try {
        created = d_ptr->windowFactory(options);
    } catch (const std::exception &exception) {
        return zzCoordinatorFailure<ZzWorkspaceWindowHandle>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("workspace window factory threw an exception: %1")
                .arg(QString::fromLocal8Bit(exception.what())));
    } catch (...) {
        return zzCoordinatorFailure<ZzWorkspaceWindowHandle>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("workspace window factory threw an exception"));
    }
    if (!created) return ZzCore::ZzResult<ZzWorkspaceWindowHandle>::failure(created.error());
    const auto handle = created.value();
    if (!handle.isValid()) {
        zzCloseStagedWindow(handle.window.data());
        return zzCoordinatorFailure<ZzWorkspaceWindowHandle>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace window factory returned an invalid handle"));
    }
    const auto registered = registerWindow(handle, {});
    if (!registered) {
        zzCloseStagedWindow(handle.window.data());
        return ZzCore::ZzResult<ZzWorkspaceWindowHandle>::failure(registered.error());
    }
    const auto configured = applyConfiguration(handle.window.data(), zzFullPatch(resolved));
    if (!configured) {
        static_cast<void>(unregisterWindow(handle.window.data()));
        zzCloseStagedWindow(handle.window.data());
        return ZzCore::ZzResult<ZzWorkspaceWindowHandle>::failure(configured.error());
    }
    if (options.visibility == ZzApplicationWindowVisibility::Visible) {
        handle.window->show();
        if (options.activate) {
            handle.window->raise();
            handle.window->activateWindow();
        }
    }
    const auto createdRecord = std::find_if(d_ptr->records.cbegin(),
        d_ptr->records.cend(),
        [&handle](const auto &value) {
            return value.windowIdentity == handle.window.data();
        });
    if (createdRecord != d_ptr->records.cend()) {
        d_ptr->writeWindowAudit(QStringLiteral("window.create"),
            createdRecord->windowId,
            QStringLiteral("commit"),
            auditTimer,
            true);
    }
    return ZzCore::ZzResult<ZzWorkspaceWindowHandle>::success(handle);
}

ZzCore::ZzResult<void> ZzWorkspaceWindowCoordinator::tearOff(
    ZzFluentUI::ZzSplitWorkspace *sourceWorkspace,
    const ZzFluentUI::ZzTabGroupId &sourceGroup,
    int sourceIndex,
    const ZzWorkspaceWindowCreateOptions &options)
{
    QElapsedTimer auditTimer;
    auditTimer.start();
    if (QThread::currentThread() != thread() || d_ptr->shuttingDown) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator is not accepting tear-off requests"));
    }
    if (sourceWorkspace == nullptr || sourceIndex < 0) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("tear-off source workspace or index is invalid"));
    }
    const auto sourceRecord = std::find_if(d_ptr->records.cbegin(), d_ptr->records.cend(),
        [sourceWorkspace](const auto &record) {
            return record.shell && record.shell->splitWorkspace() == sourceWorkspace;
        });
    if (sourceRecord == d_ptr->records.cend()) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("tear-off source workspace is not registered"));
    }
    const QUuid sourceWindowId = sourceRecord->windowId;
    ZzWorkspaceWindowCreateOptions staged = options;
    staged.visibility = ZzApplicationWindowVisibility::Deferred;
    if (staged.configurationSource == ZzWorkspaceConfigurationSource::CoordinatorDefaults) {
        staged.configurationSource = ZzWorkspaceConfigurationSource::SourceWindow;
        staged.sourceWindow = sourceRecord->window;
    }
    if (!staged.configuration.initialGeometry) {
        staged.configuration.initialGeometry = sourceRecord->window->geometry();
    }
    const auto created = createWindow(staged);
    if (!created) {
        d_ptr->writeWindowAudit(QStringLiteral("window.tear_off"),
            sourceWindowId,
            QStringLiteral("create"),
            auditTimer,
            false);
        return ZzCore::ZzResult<void>::failure(created.error());
    }
    const auto &handle = created.value();
    const auto targetRecord = std::find_if(d_ptr->records.cbegin(),
        d_ptr->records.cend(),
        [&handle](const auto &value) {
            return value.windowIdentity == handle.window.data();
        });
    const QUuid targetWindowId = targetRecord != d_ptr->records.cend()
                                     ? targetRecord->windowId
                                     : sourceWindowId;
    const auto groups = handle.shell->splitWorkspace()->groupIds();
    auto transferred = sourceWorkspace->transferTabToWorkspace(sourceGroup, sourceIndex,
        handle.shell->splitWorkspace(), groups.constFirst());
    if (!transferred) {
        d_ptr->writeWindowAudit(QStringLiteral("window.tear_off"),
            targetWindowId,
            QStringLiteral("transfer"),
            auditTimer,
            false);
        static_cast<void>(unregisterWindow(handle.window.data()));
        zzCloseStagedWindow(handle.window.data());
        return transferred;
    }
    handle.window->show();
    if (options.activate) {
        handle.window->raise();
        handle.window->activateWindow();
    }
    d_ptr->writeWindowAudit(QStringLiteral("window.tear_off"),
        targetWindowId,
        QStringLiteral("commit"),
        auditTimer,
        true);
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceWindowCoordinator::closeWindow(
    ZzApplicationWindow *window)
{
    return d_ptr->closeWindow(window);
}

ZzCore::ZzResult<void> ZzWorkspaceWindowCoordinator::approveDelegatedClose(
    ZzApplicationWindow *window)
{
    return d_ptr->approveDelegatedClose(window);
}

ZzCore::ZzResult<void> ZzWorkspaceWindowCoordinator::unregisterWindow(
    ZzApplicationWindow *window)
{
    if (QThread::currentThread() != thread()) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator called from a non-owner thread"));
    }
    if (window == nullptr) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace window must not be null"));
    }
    const auto iterator = std::find_if(
        d_ptr->records.cbegin(),
        d_ptr->records.cend(),
        [window](const auto &record) {
            return record.windowIdentity == window;
        });
    if (iterator == d_ptr->records.cend()) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("workspace window is not registered"));
    }
    if (d_ptr->closeTransactionActive(*iterator)) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window close transaction is active"));
    }
    d_ptr->removeRecord(static_cast<std::size_t>(
        std::distance(d_ptr->records.cbegin(), iterator)));
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<ZzWorkspaceWindowConfiguration>
ZzWorkspaceWindowCoordinator::configuration(ZzApplicationWindow *window) const
{
    if (QThread::currentThread() != thread()) {
        return zzCoordinatorFailure<ZzWorkspaceWindowConfiguration>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator called from a non-owner thread"));
    }
    if (window == nullptr) {
        return zzCoordinatorFailure<ZzWorkspaceWindowConfiguration>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace window must not be null"));
    }
    const auto iterator = std::find_if(
        d_ptr->records.cbegin(),
        d_ptr->records.cend(),
        [window](const auto &record) {
            return record.windowIdentity == window;
        });
    if (iterator == d_ptr->records.cend()) {
        return zzCoordinatorFailure<ZzWorkspaceWindowConfiguration>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("workspace window is not registered"));
    }
    return ZzCore::ZzResult<ZzWorkspaceWindowConfiguration>::success(
        iterator->configuration);
}

bool ZzWorkspaceWindowCoordinator::eventFilter(
    QObject *watched,
    QEvent *event)
{
    return d_ptr->eventFilter(watched, event);
}

void ZzWorkspaceWindowCoordinator::beginShutdown() noexcept
{
    if (d_ptr->shuttingDown) {
        return;
    }
    d_ptr->shuttingDown = true;
    d_ptr->clear();
}

} // namespace ZzPureTools
