#include "ZzExampleWindowShell.h"

#include <exception>
#include <utility>

#include <QtCore/QString>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationWindow.h>

#include "ZzExampleWindowShellPrivate.h"

namespace ZzExample {

ZzCore::ZzResult<void> ZzExampleWindowShell::attach(
    ZzPureTools::ZzApplicationWindow &window,
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication &application)
{
    try {
        auto shell = std::unique_ptr<ZzExampleWindowShell>(
            new ZzExampleWindowShell(
                window, std::move(context), application));
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

ZzExampleWindowShell::ZzExampleWindowShell(
    ZzPureTools::ZzApplicationWindow &window,
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication &application)
    : QObject(&window)
    , d_ptr(std::make_unique<ZzExampleWindowShellPrivate>(
          this, &window, std::move(context), &application))
{
    setObjectName(QStringLiteral("zzExampleWindowShell"));
}

ZzExampleWindowShell::~ZzExampleWindowShell() = default;

} // namespace ZzExample
