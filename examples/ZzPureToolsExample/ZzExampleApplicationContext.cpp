#include "ZzExampleApplicationContext.h"

#include <exception>
#include <utility>

#include <QtCore/QString>

#include <ZzCore/ZzApplicationPaths.h>
#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzCore/ZzSettingsStore.h>
#include <ZzCore/ZzTaskExecutor.h>

#include "ZzExampleApplicationContextPrivate.h"

namespace ZzExample {

ZzCore::ZzResult<std::shared_ptr<ZzExampleApplicationContext>>
ZzExampleApplicationContext::create()
{
    const ZzCore::ZzApplicationPaths paths(
        QStringLiteral("Jackfahdin"),
        QStringLiteral("ZzPureToolsExample"));
    auto directoriesResult = paths.ensureDirectories();
    if (!directoriesResult) {
        return ZzCore::ZzResult<std::shared_ptr<
            ZzExampleApplicationContext>>::failure(
            directoriesResult.error());
    }

    try {
        return ZzCore::ZzResult<std::shared_ptr<
            ZzExampleApplicationContext>>::success(
            std::shared_ptr<ZzExampleApplicationContext>(
                new ZzExampleApplicationContext(paths)));
    } catch (const std::exception &exception) {
        return ZzCore::ZzResult<std::shared_ptr<
            ZzExampleApplicationContext>>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to create example application context"),
            QString::fromLocal8Bit(exception.what())));
    } catch (...) {
        return ZzCore::ZzResult<std::shared_ptr<
            ZzExampleApplicationContext>>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to create example application context")));
    }
}

ZzExampleApplicationContext::ZzExampleApplicationContext(
    const ZzCore::ZzApplicationPaths &paths)
    : d_ptr(std::make_unique<ZzExampleApplicationContextPrivate>(paths))
{
}

ZzExampleApplicationContext::~ZzExampleApplicationContext() = default;

ZzCore::ZzSettingsStore &
ZzExampleApplicationContext::settingsStore() noexcept
{
    return d_ptr->settings;
}

ZzCore::ZzTaskExecutor &
ZzExampleApplicationContext::taskExecutor() noexcept
{
    return d_ptr->tasks;
}

const ZzCore::ZzApplicationPaths &
ZzExampleApplicationContext::paths() const noexcept
{
    return d_ptr->paths;
}

const QString &ZzExampleApplicationContext::platformName() const noexcept
{
    return d_ptr->platform;
}

} // namespace ZzExample
