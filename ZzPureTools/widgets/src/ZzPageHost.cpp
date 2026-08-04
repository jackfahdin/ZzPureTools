#include <ZzPureTools/ZzPageHost.h>

#include <utility>

#include "private/ZzPageHostPrivate.h"

namespace ZzPureTools {

ZzPageHost::ZzPageHost(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzPageHostPrivate>(this))
{
}

ZzPageHost::~ZzPageHost()
{
    d_ptr.reset();
}

ZzCore::ZzResult<void> ZzPageHost::activate(
    const ZzPageRegistration &registration)
{
    return d_ptr->activate(registration);
}

void ZzPageHost::deactivateCurrent() noexcept
{
    d_ptr->deactivateCurrent();
}

ZzCore::ZzResult<void> ZzPageHost::showFrameworkError(
    ZzRouteId failedRoute)
{
    return d_ptr->showFrameworkError(std::move(failedRoute));
}

ZzRouteId ZzPageHost::currentRoute() const
{
    return d_ptr->currentRoute();
}

ZzCore::ZzResult<void> ZzPageHost::setRecreatableCapacity(
    qsizetype capacity)
{
    return d_ptr->setRecreatableCapacity(capacity);
}

} // namespace ZzPureTools
