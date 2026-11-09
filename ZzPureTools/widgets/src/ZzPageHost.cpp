#include <ZzPureTools/ZzPageHost.h>
#include <QtCore/QEvent>
#include <QtCore/QScopeGuard>
#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include "private/ZzPageHostPrivate.h"

namespace ZzPureTools {
namespace {
/** @brief 析构期间拒绝重入的公共错误。 */
ZzCore::ZzResult<void> zzUnavailable()
{
    return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::InvalidState, QStringLiteral("page host is unavailable")));
}
ZzCore::ZzResult<void> zzInvalid()
{
    return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::InvalidArgument, QStringLiteral("invalid transition configuration")));
}
}
ZzPageHost::ZzPageHost(QWidget *parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    d_ptr = std::make_unique<ZzPageHostPrivate>(this);
}
ZzPageHost::~ZzPageHost() { d_ptr.reset(); }
ZzCore::ZzResult<void> ZzPageHost::activate(const ZzPageRegistration &registration)
{
    return activate(registration, ZzTransitionDirection::Forward);
}
ZzCore::ZzResult<void> ZzPageHost::activate(const ZzPageRegistration &registration, ZzTransitionDirection direction)
{
    return d_ptr ? d_ptr->activate(registration, direction) : zzUnavailable();
}
void ZzPageHost::deactivateCurrent() noexcept { if (d_ptr) d_ptr->deactivateCurrent(); }
ZzCore::ZzResult<void> ZzPageHost::showFrameworkError(ZzRouteId route)
{
    return d_ptr ? d_ptr->showFrameworkError(std::move(route)) : zzUnavailable();
}
ZzRouteId ZzPageHost::currentRoute() const
{
    return d_ptr && d_ptr->isOwnerThread() ? d_ptr->activeRoute : ZzRouteId{};
}
ZzCore::ZzResult<void> ZzPageHost::setRecreatableCapacity(qsizetype capacity)
{
    return d_ptr ? d_ptr->setRecreatableCapacity(capacity) : zzUnavailable();
}
ZzPageHost::ZzTransitionMode ZzPageHost::transitionMode() const noexcept
{
    return d_ptr && d_ptr->isOwnerThread() ? d_ptr->mode : ZzTransitionMode::None;
}
int ZzPageHost::transitionDuration() const noexcept
{
    return d_ptr && d_ptr->isOwnerThread() ? d_ptr->duration : -1;
}
bool ZzPageHost::isTransitionRunning() const noexcept { return false; }
bool ZzPageHost::isRouteTransitionEnabled(const ZzRouteId &route) const
{
    return d_ptr && d_ptr->isOwnerThread() && route.isValid() && !d_ptr->disabledRoutes.contains(route.value());
}
ZzCore::ZzResult<void> ZzPageHost::setTransitionMode(ZzTransitionMode mode)
{
    if (!d_ptr) return zzUnavailable();
    auto valid = d_ptr->validateOperation();
    if (!valid) return valid;
    if (mode < ZzTransitionMode::None || mode > ZzTransitionMode::Fade) return zzInvalid();
    d_ptr->mode = mode;
    d_ptr->settle();
    return ZzCore::ZzResult<void>::success();
}
ZzCore::ZzResult<void> ZzPageHost::setTransitionDuration(int milliseconds)
{
    if (!d_ptr) return zzUnavailable();
    auto valid = d_ptr->validateOperation();
    if (!valid) return valid;
    if (milliseconds < -1 || milliseconds > 10000) return zzInvalid();
    d_ptr->duration = milliseconds;
    d_ptr->settle();
    return ZzCore::ZzResult<void>::success();
}
ZzCore::ZzResult<void> ZzPageHost::setRouteTransitionEnabled(const ZzRouteId &route, bool enabled)
{
    if (!d_ptr) return zzUnavailable();
    auto valid = d_ptr->validateOperation();
    if (!valid) return valid;
    if (!route.isValid()) return zzInvalid();
    if (enabled) d_ptr->disabledRoutes.remove(route.value());
    else d_ptr->disabledRoutes.insert(route.value());
    return ZzCore::ZzResult<void>::success();
}
QSize ZzPageHost::sizeHint() const { return d_ptr ? d_ptr->hint(false) : QWidget::sizeHint(); }
QSize ZzPageHost::minimumSizeHint() const { return d_ptr ? d_ptr->hint(true) : QWidget::minimumSizeHint(); }
bool ZzPageHost::event(QEvent *event)
{
    const QPointer<ZzPageHost> guard(this);
    const bool handled = QWidget::event(event);
    if (!guard) return handled;
    if (d_ptr && !d_ptr->destroying && (event->type() == QEvent::Resize || event->type() == QEvent::ContentsRectChange))
        d_ptr->synchronizePages();
    return handled;
}
} // namespace ZzPureTools
