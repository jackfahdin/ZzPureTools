#include <ZzPureTools/ZzApplicationRuntime.h>

#include <utility>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include "private/ZzApplicationRuntimePrivate.h"

namespace ZzPureTools {

ZzApplicationRuntime::ZzApplicationRuntime(
    std::vector<std::unique_ptr<ZzApplicationModule>> modules)
    : d_ptr(std::make_unique<ZzApplicationRuntimePrivate>(
          std::move(modules)))
{
}

ZzApplicationRuntime::~ZzApplicationRuntime()
{
    if (d_ptr) {
        d_ptr->requestStop();
        d_ptr->stop();
    }
}

ZzApplicationRuntime::ZzApplicationRuntime(
    ZzApplicationRuntime &&other) noexcept = default;

ZzApplicationRuntime &ZzApplicationRuntime::operator=(
    ZzApplicationRuntime &&other) noexcept
{
    if (this != &other) {
        if (d_ptr) {
            d_ptr->requestStop();
            d_ptr->stop();
        }
        d_ptr = std::move(other.d_ptr);
    }
    return *this;
}

ZzCore::ZzResult<void> ZzApplicationRuntime::start()
{
    if (!d_ptr) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application runtime has been moved")));
    }
    return d_ptr->start();
}

void ZzApplicationRuntime::requestStop() noexcept
{
    if (d_ptr) {
        d_ptr->requestStop();
    }
}

void ZzApplicationRuntime::stop() noexcept
{
    if (d_ptr) {
        d_ptr->stop();
    }
}

bool ZzApplicationRuntime::isRunning() const noexcept
{
    return d_ptr && d_ptr->isRunning();
}

qsizetype ZzApplicationRuntime::moduleCount() const noexcept
{
    return d_ptr ? d_ptr->moduleCount() : 0;
}

} // namespace ZzPureTools
