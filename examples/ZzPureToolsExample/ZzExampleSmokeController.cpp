#include "ZzExampleSmokeController.h"

#include <utility>

#include "ZzExampleSmokeControllerPrivate.h"

namespace ZzExample {

ZzExampleSmokeController::ZzExampleSmokeController(
    bool enabled,
    ZzPureTools::ZzPureApplication &application,
    std::shared_ptr<ZzExampleApplicationContext> context)
    : d_ptr(std::make_unique<ZzExampleSmokeControllerPrivate>(
          enabled,
          &application,
          std::move(context)))
{
}

ZzExampleSmokeController::~ZzExampleSmokeController() = default;

bool ZzExampleSmokeController::closeGuardEnabled() const noexcept
{
    return d_ptr->closeGuardEnabled();
}

void ZzExampleSmokeController::windowAttached(
    ZzPureTools::ZzApplicationWindow &window)
{
    d_ptr->windowAttached(window);
}

} // namespace ZzExample
