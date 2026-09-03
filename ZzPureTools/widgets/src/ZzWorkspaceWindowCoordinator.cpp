#include <ZzPureTools/ZzWorkspaceWindowCoordinator.h>

#include <algorithm>
#include <exception>
#include <utility>

#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

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
        d_ptr->records.push_back({
            handle.window,
            handle.window.data(),
            handle.shell,
            handle.shell.data(),
            configuration,
            {},
            {},
            primary});
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
    return ZzCore::ZzResult<void>::success();
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
    Q_UNUSED(watched);
    Q_UNUSED(event);
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
