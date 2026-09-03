#include <ZzPureTools/ZzWorkspaceWindowCoordinator.h>

#include <algorithm>
#include <exception>
#include <limits>
#include <utility>

#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzLog/ZzLog.h>
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

class ZzScopedBoolean final
{
public:
    explicit ZzScopedBoolean(bool &value, bool replacement = true) noexcept
        : value_(value)
        , previous_(value)
    {
        value_ = replacement;
    }

    ~ZzScopedBoolean() { value_ = previous_; }

    Q_DISABLE_COPY_MOVE(ZzScopedBoolean)

private:
    bool &value_;
    bool previous_;
};

void zzWriteWindowAudit(QStringView event,
    const QUuid &windowId,
    QStringView phase,
    const QElapsedTimer &timer,
    bool success) noexcept
{
    if (!ZzLog::shouldLog(ZzLog::ZzLogLevel::Info) || windowId.isNull()) {
        return;
    }
    const QByteArray message =
        QStringLiteral("%1 window_id=%2 phase=%3 elapsed_us=%4 result=%5")
            .arg(event, windowId.toString(QUuid::WithoutBraces), phase)
            .arg(std::max<qint64>(0, timer.nsecsElapsed() / 1000))
            .arg(
                success ? QStringLiteral("success") : QStringLiteral("failure"))
            .toUtf8();
    ZzLog::writeText(ZzLog::ZzLogLevel::Info,
        std::string_view(
            message.constData(), static_cast<std::size_t>(message.size())));
}

void zzWritePageAudit(QStringView event,
    const ZzFluentUI::ZzWorkspacePageId &pageId,
    const QUuid &sourceWindowId,
    const QUuid &targetWindowId,
    QStringView phase,
    const QElapsedTimer &timer,
    bool success) noexcept
{
    if (!ZzLog::shouldLog(ZzLog::ZzLogLevel::Info) || !pageId.isValid()
        || sourceWindowId.isNull() || targetWindowId.isNull()) {
        return;
    }
    const QByteArray message =
        QStringLiteral("%1 page_id=%2 source_window_id=%3 target_window_id=%4 "
                       "phase=%5 elapsed_us=%6 result=%7")
            .arg(event,
                pageId.toString(),
                sourceWindowId.toString(QUuid::WithoutBraces),
                targetWindowId.toString(QUuid::WithoutBraces),
                phase)
            .arg(std::max<qint64>(0, timer.nsecsElapsed() / 1000))
            .arg(
                success ? QStringLiteral("success") : QStringLiteral("failure"))
            .toUtf8();
    ZzLog::writeText(ZzLog::ZzLogLevel::Info,
        std::string_view(
            message.constData(), static_cast<std::size_t>(message.size())));
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
            QElapsedTimer auditTimer;
            auditTimer.start();
            if (d_ptr->reclaiming || sourceWorkspace == nullptr
                || page == nullptr)
                return;
            const auto source = std::find_if(d_ptr->records.cbegin(),
                d_ptr->records.cend(),
                [sourceWorkspace](const auto &value) {
                    return value.shell
                           && value.shell->splitWorkspace() == sourceWorkspace;
                });
            if (source == d_ptr->records.cend()) return;
            const auto target = std::find_if(d_ptr->records.cbegin(),
                d_ptr->records.cend(),
                [targetWindow](const auto &value) {
                    return value.windowIdentity == targetWindow;
                });
            if (target == d_ptr->records.cend()) return;
            auto found = std::find_if(d_ptr->pageOrigins.begin(),
                d_ptr->pageOrigins.end(),
                [page](const auto &value) { return value.page == page; });
            if (found == d_ptr->pageOrigins.end()) {
                d_ptr->pageOrigins.push_back({page, {}});
                found = std::prev(d_ptr->pageOrigins.end());
            }
            const auto existing = std::find_if(found->origins.cbegin(),
                found->origins.cend(),
                [&target](const auto &origin) {
                    return origin.windowId == target->windowId;
                });
            if (existing != found->origins.cend()) {
                found->origins.erase(existing, found->origins.end());
            } else {
                found->origins.push_back(
                    {source->windowId, sourceGroup, sourceIndex});
            }
            zzWritePageAudit(QStringLiteral("page.transfer"),
                pageId,
                source->windowId,
                target->windowId,
                QStringLiteral("commit"),
                auditTimer,
                true);
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
    // 置顶是配置同步中唯一会报告失败的操作，必须在写入其他表面前预检。
    const auto alwaysOnTopApplied = iterator->shell->setAlwaysOnTop(
        updated.alwaysOnTop);
    if (!alwaysOnTopApplied) {
        return alwaysOnTopApplied;
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
        zzWriteWindowAudit(QStringLiteral("window.create"),
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
        zzWriteWindowAudit(QStringLiteral("window.tear_off"),
            sourceWindowId,
            QStringLiteral("create"),
            auditTimer,
            false);
        return ZzCore::ZzResult<void>::failure(created.error());
    }
    const auto handle = created.value();
    const auto targetRecord = std::find_if(d_ptr->records.cbegin(),
        d_ptr->records.cend(),
        [&handle](const auto &value) {
            return value.windowIdentity == handle.window.data();
        });
    const QUuid targetWindowId = targetRecord != d_ptr->records.cend()
                                     ? targetRecord->windowId
                                     : sourceWindowId;
    const auto groups = handle.shell->splitWorkspace()->groupIds();
    const auto transferred = sourceWorkspace->transferTabToWorkspace(sourceGroup, sourceIndex,
        handle.shell->splitWorkspace(), groups.constFirst());
    if (!transferred) {
        zzWriteWindowAudit(QStringLiteral("window.tear_off"),
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
    zzWriteWindowAudit(QStringLiteral("window.tear_off"),
        targetWindowId,
        QStringLiteral("commit"),
        auditTimer,
        true);
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceWindowCoordinator::closeWindow(
    ZzApplicationWindow *window)
{
    if (QThread::currentThread() != thread() || d_ptr->shuttingDown) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator is not accepting "
                           "close requests"));
    }
    auto record = std::find_if(d_ptr->records.begin(),
        d_ptr->records.end(),
        [window](const auto &value) { return value.windowIdentity == window; });
    if (window == nullptr || record == d_ptr->records.end()) {
        return zzCoordinatorFailure<void>(
            window == nullptr ? ZzCore::ZzErrorCode::InvalidArgument
                              : ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("workspace window is not registered"));
    }
    if (record->configuration.closePolicy == ZzWindowClosePolicy::Deny) {
        return ZzCore::ZzResult<void>::success();
    }
    if (record->configuration.closePolicy == ZzWindowClosePolicy::Delegate
        && !record->delegatedCloseApproved) {
        if (!record->delegatedClosePending) {
            record->delegatedClosePending = true;
            Q_EMIT windowCloseApprovalRequested(window);
        }
        return ZzCore::ZzResult<void>::success();
    }
    record->delegatedCloseApproved = false;
    const QUuid closingWindowId = record->windowId;
    auto *const workspace = record->shell->splitWorkspace();
    QList<QWidget *> pages;
    for (const auto &group : workspace->groupIds()) {
        auto *tabs = workspace->tabWidget(group);
        for (int index = 0; tabs != nullptr && index < tabs->count(); ++index) {
            pages.push_back(tabs->widget(index));
        }
    }
    const bool hasFallback = std::any_of(d_ptr->records.cbegin(),
        d_ptr->records.cend(),
        [window](const auto &value) {
            return value.windowIdentity != window && !value.window.isNull();
        });
    if (!pages.isEmpty() && !hasFallback) {
        Q_EMIT orphanedPages(pages);
        return ZzCore::ZzResult<void>::success();
    }
    struct Move final
    {
        QPointer<QWidget> page;
        ZzFluentUI::ZzTabGroupId group;
        int index = -1;
        QUuid targetWindowId;
        ZzFluentUI::ZzTabGroupId targetGroup;
        int targetIndex = -1;
    };
    std::vector<Move> moves;
    for (const auto &group : workspace->groupIds()) {
        auto *tabs = workspace->tabWidget(group);
        for (int index = 0; tabs != nullptr && index < tabs->count(); ++index) {
            auto *page = tabs->widget(index);
            auto target = d_ptr->records.end();
            ZzFluentUI::ZzTabGroupId targetGroup;
            int targetIndex = -1;
            const auto history = std::find_if(d_ptr->pageOrigins.begin(),
                d_ptr->pageOrigins.end(),
                [page](const auto &value) { return value.page == page; });
            if (history != d_ptr->pageOrigins.end()) {
                for (auto origin = history->origins.rbegin();
                    origin != history->origins.rend();
                    ++origin) {
                    const auto candidate = std::find_if(d_ptr->records.begin(),
                        d_ptr->records.end(),
                        [&origin, window](const auto &value) {
                            return value.windowId == origin->windowId
                                   && value.windowIdentity != window
                                   && !value.window.isNull()
                                   && !value.shell.isNull();
                        });
                    if (candidate != d_ptr->records.end()) {
                        target = candidate;
                        targetGroup =
                            candidate->shell->splitWorkspace()->tabWidget(
                                origin->group)
                                    != nullptr
                                ? origin->group
                                : candidate->shell->splitWorkspace()
                                      ->activeGroupId();
                        targetIndex =
                            candidate->shell->splitWorkspace()->tabWidget(
                                origin->group)
                                    != nullptr
                                ? origin->index
                                : -1;
                        break;
                    }
                }
            }
            if (target == d_ptr->records.end()) {
                const auto fallback = std::find_if(d_ptr->records.begin(),
                    d_ptr->records.end(),
                    [window](const auto &value) {
                        return value.primary && value.windowIdentity != window
                               && !value.window.isNull()
                               && !value.shell.isNull();
                    });
                const auto first =
                    fallback != d_ptr->records.end()
                        ? fallback
                        : std::find_if(d_ptr->records.begin(),
                              d_ptr->records.end(),
                              [window](const auto &value) {
                                  return value.windowIdentity != window
                                         && !value.window.isNull()
                                         && !value.shell.isNull();
                              });
                if (first != d_ptr->records.end()) {
                    target = first;
                    targetGroup =
                        first->shell->splitWorkspace()->activeGroupId();
                }
            }
            if (target == d_ptr->records.end()) {
                Q_EMIT orphanedPages(pages);
                return ZzCore::ZzResult<void>::success();
            }
            moves.push_back({page,
                group,
                index,
                target->windowId,
                targetGroup,
                targetIndex});
        }
    }

    for (std::size_t moveIndex = 0; moveIndex < moves.size(); ++moveIndex) {
        const auto &move = moves.at(moveIndex);
        const auto target = std::find_if(d_ptr->records.cbegin(),
            d_ptr->records.cend(),
            [&move](const auto &value) {
                return value.windowId == move.targetWindowId
                       && !value.window.isNull() && !value.shell.isNull();
            });
        auto *const sourceTabs = workspace->tabWidget(move.group);
        auto *const targetWorkspace = target != d_ptr->records.cend()
                                          ? target->shell->splitWorkspace()
                                          : nullptr;
        auto *const targetTabs =
            targetWorkspace != nullptr
                ? targetWorkspace->tabWidget(move.targetGroup)
                : nullptr;
        if (move.page.isNull() || sourceTabs == nullptr
            || sourceTabs->indexOf(move.page) < 0 || targetTabs == nullptr) {
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("page reclaim target capacity changed"));
        }
        const auto pageId = workspace->pageId(move.page);
        if (!pageId.isValid()
            || targetWorkspace->pageForId(pageId) != nullptr) {
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("page reclaim identity already exists"));
        }
        const QString layoutKey = workspace->pageLayoutKey(move.page);
        if (!layoutKey.isEmpty()) {
            bool keyExists = false;
            for (const auto &targetGroup : targetWorkspace->groupIds()) {
                auto *const existingTabs =
                    targetWorkspace->tabWidget(targetGroup);
                for (int index = 0;
                    existingTabs != nullptr && index < existingTabs->count();
                    ++index) {
                    if (targetWorkspace->pageLayoutKey(
                            existingTabs->widget(index))
                        == layoutKey) {
                        keyExists = true;
                        break;
                    }
                }
                if (keyExists) break;
            }
            if (keyExists) {
                return zzCoordinatorFailure<void>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("page reclaim layout key already exists"));
            }
        }
        const auto additions = static_cast<int>(std::count_if(moves.cbegin(),
            moves.cbegin() + static_cast<std::ptrdiff_t>(moveIndex),
            [&move](const auto &planned) {
                return planned.targetWindowId == move.targetWindowId
                       && planned.targetGroup == move.targetGroup;
            }));
        if (targetTabs->count()
            > std::numeric_limits<int>::max() - additions - 1) {
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("page reclaim target capacity exceeded"));
        }
    }

    std::vector<Move> completed;
    try {
        completed.reserve(moves.size());
    } catch (...) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::Unknown,
            QStringLiteral(
                "failed to reserve page reclaim transaction storage"));
    }
    {
        ZzScopedBoolean reclaiming(d_ptr->reclaiming);
        for (const auto &move : moves) {
            QElapsedTimer reclaimTimer;
            reclaimTimer.start();
            const auto pageId = !move.page.isNull()
                                    ? workspace->pageId(move.page)
                                    : ZzFluentUI::ZzWorkspacePageId{};
            const auto target = std::find_if(d_ptr->records.begin(),
                d_ptr->records.end(),
                [&move](const auto &value) {
                    return value.windowId == move.targetWindowId
                           && !value.window.isNull() && !value.shell.isNull();
                });
            auto *const sourceTabs = workspace->tabWidget(move.group);
            auto *const targetWorkspace = target != d_ptr->records.end()
                                              ? target->shell->splitWorkspace()
                                              : nullptr;
            auto *const targetTabs =
                targetWorkspace != nullptr
                    ? targetWorkspace->tabWidget(move.targetGroup)
                    : nullptr;
            const int current = sourceTabs != nullptr && !move.page.isNull()
                                    ? sourceTabs->indexOf(move.page)
                                    : -1;
            int targetIndex = move.targetIndex;
            if (targetIndex >= 0 && targetTabs != nullptr) {
                const auto earlierAtSlot =
                    static_cast<int>(std::count_if(completed.cbegin(),
                        completed.cend(),
                        [&move](const auto &planned) {
                            return planned.targetWindowId == move.targetWindowId
                                   && planned.targetGroup == move.targetGroup
                                   && planned.targetIndex >= 0
                                   && planned.targetIndex <= move.targetIndex;
                        }));
                targetIndex = std::clamp(
                    targetIndex + earlierAtSlot, 0, targetTabs->count());
            }
            const auto transferred =
                targetWorkspace != nullptr
                    ? workspace->transferTabToWorkspace(move.group,
                          current,
                          targetWorkspace,
                          move.targetGroup,
                          targetIndex)
                    : zzCoordinatorFailure<void>(
                          ZzCore::ZzErrorCode::InvalidState,
                          QStringLiteral("page reclaim target disappeared"));
            if (transferred) {
                zzWritePageAudit(QStringLiteral("page.reclaim"),
                    pageId,
                    closingWindowId,
                    move.targetWindowId,
                    QStringLiteral("commit"),
                    reclaimTimer,
                    true);
                completed.push_back(move);
                continue;
            }
            zzWritePageAudit(QStringLiteral("page.reclaim"),
                pageId,
                closingWindowId,
                move.targetWindowId,
                QStringLiteral("transfer"),
                reclaimTimer,
                false);
            bool restored = true;
            for (auto rollback = completed.rbegin();
                rollback != completed.rend();
                ++rollback) {
                const auto rollbackTarget = std::find_if(d_ptr->records.begin(),
                    d_ptr->records.end(),
                    [&rollback](const auto &value) {
                        return value.windowId == rollback->targetWindowId
                               && !value.window.isNull()
                               && !value.shell.isNull();
                    });
                auto *const rollbackWorkspace =
                    rollbackTarget != d_ptr->records.end()
                        ? rollbackTarget->shell->splitWorkspace()
                        : nullptr;
                auto *const rollbackTabs =
                    rollbackWorkspace != nullptr
                        ? rollbackWorkspace->tabWidget(rollback->targetGroup)
                        : nullptr;
                auto *const originalTabs =
                    workspace->tabWidget(rollback->group);
                const int rollbackIndex =
                    rollbackTabs != nullptr && !rollback->page.isNull()
                        ? rollbackTabs->indexOf(rollback->page)
                        : -1;
                const int originalIndex =
                    originalTabs != nullptr
                        ? std::clamp(rollback->index, 0, originalTabs->count())
                        : -1;
                if (rollbackIndex < 0 || originalIndex < 0
                    || !rollbackWorkspace->transferTabToWorkspace(
                        rollback->targetGroup,
                        rollbackIndex,
                        workspace,
                        rollback->group,
                        originalIndex)) {
                    restored = false;
                }
            }
            return restored
                       ? transferred
                       : zzCoordinatorFailure<void>(
                             ZzCore::ZzErrorCode::InvalidState,
                             QStringLiteral("page reclaim rollback failed"));
        }
    }

    for (const auto &move : moves) {
        const auto history = std::find_if(d_ptr->pageOrigins.begin(),
            d_ptr->pageOrigins.end(),
            [&move](const auto &value) { return value.page == move.page; });
        if (history == d_ptr->pageOrigins.end()) continue;
        const auto targetOrigin = std::find_if(history->origins.begin(),
            history->origins.end(),
            [&move](const auto &origin) {
                return origin.windowId == move.targetWindowId;
            });
        if (targetOrigin != history->origins.end()) {
            history->origins.erase(targetOrigin, history->origins.end());
        }
    }
    Q_EMIT windowAboutToClose(window, pages);
    record = std::find_if(d_ptr->records.begin(),
        d_ptr->records.end(),
        [&closingWindowId](
            const auto &value) { return value.windowId == closingWindowId; });
    if (record == d_ptr->records.end() || record->window.isNull()) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window disappeared before close commit"));
    }
    record->closeBypass = true;
    if (!d_ptr->handlingCloseEvent) {
        record->window->close();
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceWindowCoordinator::approveDelegatedClose(
    ZzApplicationWindow *window)
{
    if (QThread::currentThread() != thread() || d_ptr->shuttingDown) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator is not accepting "
                           "close approvals"));
    }
    auto record = std::find_if(d_ptr->records.begin(),
        d_ptr->records.end(),
        [window](const auto &value) { return value.windowIdentity == window; });
    if (record == d_ptr->records.end() || !record->delegatedClosePending
        || record->configuration.closePolicy != ZzWindowClosePolicy::Delegate) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window has no delegated close pending"));
    }
    record->delegatedClosePending = false;
    record->delegatedCloseApproved = true;
    return closeWindow(window);
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
    if (event == nullptr || event->type() != QEvent::Close) {
        return false;
    }
    const auto iterator = std::find_if(d_ptr->records.begin(),
        d_ptr->records.end(),
        [watched](
            const auto &record) { return record.windowIdentity == watched; });
    if (iterator != d_ptr->records.cend() && iterator->closeBypass) {
        iterator->closeBypass = false;
        return false;
    }
    if (iterator != d_ptr->records.cend()
        && iterator->configuration.closePolicy == ZzWindowClosePolicy::Deny) {
        event->ignore();
        return true;
    }
    if (iterator != d_ptr->records.cend()
        && iterator->configuration.closePolicy
               == ZzWindowClosePolicy::Delegate) {
        if (!iterator->delegatedClosePending) {
            iterator->delegatedClosePending = true;
            Q_EMIT windowCloseApprovalRequested(iterator->windowIdentity);
        }
        event->ignore();
        return true;
    }
    if (iterator != d_ptr->records.cend()) {
        event->ignore();
        const auto result = [&] {
            ZzScopedBoolean handlingCloseEvent(d_ptr->handlingCloseEvent);
            return closeWindow(iterator->windowIdentity);
        }();
        const auto committed = std::find_if(d_ptr->records.begin(),
            d_ptr->records.end(),
            [watched](const auto &record) {
                return record.windowIdentity == watched;
            });
        if (result && committed != d_ptr->records.end()
            && committed->closeBypass) {
            committed->closeBypass = false;
            return false;
        }
        return true;
    }
    return false;
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
