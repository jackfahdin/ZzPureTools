#include "ZzExampleWindowShell.h"

#include <exception>
#include <utility>

#include <QtCore/QString>
#include <QtCore/QEvent>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationWindow.h>

#include "ZzExampleWindowShellPrivate.h"

namespace ZzExample {

ZzCore::ZzResult<void> ZzExampleWindowShell::attach(
    ZzPureTools::ZzApplicationWindow &window,
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication &application,
    bool closeGuardEnabled)
{
    try {
        auto shell = std::unique_ptr<ZzExampleWindowShell>(
            new ZzExampleWindowShell(
                window,
                std::move(context),
                application,
                closeGuardEnabled));
        auto initialized = shell->d_ptr->initialize();
        if (!initialized) {
            return initialized;
        }
        ZzExampleWindowShell *const attachedShell = shell.release();
        if (attachedShell == nullptr) {
            return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("failed to retain example window shell")));
        }
        return ZzCore::ZzResult<void>::success();
    } catch (const std::exception &exception) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to create example window shell"),
            QString::fromLocal8Bit(exception.what())));
    } catch (...) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to create example window shell")));
    }
}

ZzExampleWindowShell *ZzExampleWindowShell::attachedTo(
    ZzPureTools::ZzApplicationWindow &window) noexcept
{
    return window.findChild<ZzExampleWindowShell *>(
        QStringLiteral("zzExampleWindowShell"),
        Qt::FindDirectChildrenOnly);
}

bool ZzExampleWindowShell::isActivityDockVisible() const noexcept
{
    return d_ptr->isActivityDockVisible();
}

void ZzExampleWindowShell::setActivityDockVisible(bool visible)
{
    d_ptr->setActivityDockVisible(visible);
}

ZzExampleWindowShell::ZzExampleWindowShell(
    ZzPureTools::ZzApplicationWindow &window,
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication &application,
    bool closeGuardEnabled)
    : QObject(&window)
    , d_ptr(std::make_unique<ZzExampleWindowShellPrivate>(
          this,
          &window,
          std::move(context),
          &application,
          closeGuardEnabled))
{
    setObjectName(QStringLiteral("zzExampleWindowShell"));
}

ZzExampleWindowShell::~ZzExampleWindowShell() = default;

bool ZzExampleWindowShell::eventFilter(
    QObject *watched,
    QEvent *event)
{
    if (d_ptr->filterWindowEvent(watched, event)) {
        return true;
    }
    return QObject::eventFilter(watched, event);
}

} // namespace ZzExample
