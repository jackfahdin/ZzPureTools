#include "ZzExamplePerformanceController.h"

#include <memory>

#include "ZzExamplePerformanceControllerPrivate.h"

namespace ZzExample {

ZzExamplePerformanceController::ZzExamplePerformanceController(
    ZzPureTools::ZzPureApplication &application,
    const QElapsedTimer &processTimer)
    : d_ptr(std::make_unique<ZzExamplePerformanceControllerPrivate>(
          &application,
          &processTimer))
{
}

ZzExamplePerformanceController::~ZzExamplePerformanceController() = default;

bool ZzExamplePerformanceController::isEnabled() const noexcept
{
    return d_ptr->isEnabled();
}

void ZzExamplePerformanceController::windowAttached(
    ZzPureTools::ZzApplicationWindow &window)
{
    d_ptr->windowAttached(window);
}

} // namespace ZzExample
