#include <ZzPureTools/ZzNavigationController.h>

#include <utility>

#include "private/ZzNavigationControllerPrivate.h"

namespace ZzPureTools {

ZzNavigationController::ZzNavigationController(
    ZzNavigationModel *model,
    ZzPageHost *pageHost,
    QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<ZzNavigationControllerPrivate>(
          this, model, pageHost))
{
}

ZzNavigationController::~ZzNavigationController() = default;

ZzCore::ZzResult<void> ZzNavigationController::setRegistrations(
    QList<ZzPageRegistration> registrations)
{
    return d_ptr->setRegistrations(std::move(registrations));
}

ZzCore::ZzResult<void> ZzNavigationController::navigate(
    ZzRouteId routeId)
{
    return d_ptr->navigate(std::move(routeId));
}

ZzCore::ZzResult<void> ZzNavigationController::goBack()
{
    return d_ptr->goBack();
}

bool ZzNavigationController::canGoBack() const noexcept
{
    return d_ptr->canGoBack();
}

ZzRouteId ZzNavigationController::currentRoute() const
{
    return d_ptr->currentRoute();
}

ZzCore::ZzResult<void> ZzNavigationController::setHistoryCapacity(
    qsizetype capacity)
{
    return d_ptr->setHistoryCapacity(capacity);
}

} // namespace ZzPureTools
