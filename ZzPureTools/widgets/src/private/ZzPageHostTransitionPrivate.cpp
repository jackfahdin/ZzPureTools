#include "ZzPageHostTransitionPrivate.h"
#include "ZzPageHostPrivate.h"
#include <private/ZzPageTransitionPrivate.h>
#include <ZzFluentUI/ZzAnimatedStackedWidget.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzAnimationPolicy.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <QtCore/QEvent>
#include <QtCore/QScopeGuard>
#include <QtWidgets/QApplication>

namespace ZzPureTools {
namespace {
/** @brief 静止嵌套容器允许捕获；运动中的子快照不重复捕获。 */
bool zzCanCapture(QWidget *view)
{
    if (!ZzFluentUI::ZzPageTransitionPrivate::canCapture(view)) return false;
    auto nested = view->findChildren<ZzFluentUI::ZzAnimatedStackedWidget *>();
    if (auto *self = qobject_cast<ZzFluentUI::ZzAnimatedStackedWidget *>(view)) nested.prepend(self);
    for (auto *stack : nested) if (stack->isTransitionRunning()) return false;
    return true;
}
}

/** @brief 保存单段图像代次与非拥有的焦点记忆。 */
class ZzPageHostTransitionPrivate::ZzPrivate final
{
public:
    explicit ZzPrivate(ZzPageHost *owner) : q(owner), player(owner) { refresh(); }
    /** @brief 非 Fluent 样式只在环境变化时刷新回退快照。 */
    void refresh()
    {
        const auto palette = q->palette();
        fallback = std::make_shared<const ZzFluentUI::ZzThemeSnapshot>(ZzFluentUI::ZzThemeSnapshot::create(
            palette.color(QPalette::Window).lightness() < 128 ? ZzFluentUI::ZzThemeMode::Dark : ZzFluentUI::ZzThemeMode::Light,
            palette.color(QPalette::Highlight), 0, false));
    }
    ZzPageHost *q;
    ZzFluentUI::ZzPageTransitionPrivate player;
    std::shared_ptr<const ZzFluentUI::ZzThemeSnapshot> fallback;
    ZzFluentUI::ZzPageTransitionPrivate::Token token = 0;
    ZzRouteId sourceRoute;
    ZzRouteId targetRoute;
    bool focusOwned = false;
    bool settling = false;
};

ZzPageHostTransitionPrivate::ZzPageHostTransitionPrivate(ZzPageHost *owner) : d(std::make_unique<ZzPrivate>(owner))
{
    d->player.setFinishedCallback([this] { settle(); });
}
ZzPageHostTransitionPrivate::~ZzPageHostTransitionPrivate()
{
    d->player.setFinishedCallback({});
    d->player.cancel();
}
bool ZzPageHostTransitionPrivate::running() const noexcept { return d->player.running(); }
bool ZzPageHostTransitionPrivate::involves(const ZzRouteId &route) const
{
    return route == d->sourceRoute || route == d->targetRoute;
}
void ZzPageHostTransitionPrivate::invalidate()
{
    d->token = 0;
    d->player.cancel();
}
void ZzPageHostTransitionPrivate::prepare(QWidget *source, QWidget *target, const ZzRouteId &route,
    ZzPageHost::ZzTransitionDirection direction)
{
    const QPointer<ZzPageHost> guard(d->q);
    const QPointer<QWidget> outgoing(source), incoming(target);
    settle();
    if (!guard || !outgoing || !incoming) return;
    auto &host = *guard->d_ptr;
    if (host.mode == ZzPageHost::ZzTransitionMode::None || !guard->isVisible() || !guard->isEnabled()
        || host.showingError || host.disabledRoutes.contains(host.activeRoute.value())
        || host.disabledRoutes.contains(route.value()) || !zzCanCapture(outgoing) || !zzCanCapture(incoming)) return;
    const auto *style = qobject_cast<const ZzFluentUI::ZzFluentStyle *>(guard->style());
    const auto theme = style ? style->themeSnapshot() : d->fallback;
    const int duration = ZzFluentUI::ZzAnimationPolicy::adjustedDuration(host.duration < 0
        ? theme->duration(ZzFluentUI::ZzMotionToken::PageTransition) : host.duration, theme->reducedMotion(), false);
    if (duration <= 0) return;
    d->sourceRoute = host.activeRoute;
    d->targetRoute = route;
    bool reverse = direction == ZzPageHost::ZzTransitionDirection::Backward;
    if (host.mode == ZzPageHost::ZzTransitionMode::SlideHorizontal && guard->isRightToLeft()) reverse = !reverse;
    QWidget *focused = QApplication::focusWidget();
    const bool owned = focused && (focused == outgoing || outgoing->isAncestorOf(focused));
    if (owned) host.pages.at(host.activeRoute.value()).focus = focused;
    const auto token = d->player.beginCapture({guard->contentsRect(), guard->devicePixelRatioF(),
        guard->palette().color(QPalette::Window), static_cast<ZzFluentUI::ZzPageTransitionPrivate::Mode>(host.mode), reverse, duration});
    if (!guard || !token) return;
    d->token = token;
    bool captured = d->player.captureSource(token, outgoing);
    if (!guard) return;
    if (captured && incoming && incoming->parentWidget() == host.host) captured = d->player.captureTarget(token, incoming);
    else captured = false;
    if (!guard) return;
    if (!captured || !outgoing || outgoing->parentWidget() != host.host || !incoming || incoming->parentWidget() != host.host) {
        invalidate();
        return;
    }
    focused = QApplication::focusWidget();
    d->focusOwned = owned && focused && (focused == outgoing || outgoing->isAncestorOf(focused));
    if (d->focusOwned) guard->setFocus(Qt::OtherFocusReason);
}
void ZzPageHostTransitionPrivate::commit()
{
    const QPointer<ZzPageHost> guard(d->q);
    auto &host = *guard->d_ptr;
    if (!d->token) { settle(); return; }
    const QPointer<QWidget> view(host.currentView());
    if (!view) { settle(); return; }
    // 隐藏页面后再隐藏父容器，避免 Qt 在回调后遍历已经删除的兄弟页面。
    view->hide();
    if (!guard) return;
    if (!view || host.currentView() != view) { settle(); return; }
    host.host->hide();
    if (!guard) return;
    const bool played = d->player.play(d->token);
    if (!guard) return;
    if (!played) settle();
}
void ZzPageHostTransitionPrivate::settle()
{
    if (d->settling) return;
    const QPointer<ZzPageHost> guard(d->q);
    auto *state = guard->d_ptr.get();
    if (!state || state->destroying) return;
    const bool wasBusy = state->busy;
    state->busy = true;
    d->settling = true;
    const auto unlock = qScopeGuard([this, guard, state, wasBusy] {
        if (guard && guard->d_ptr.get() == state) { state->busy = wasBusy; d->settling = false; }
    });
    const bool restore = d->focusOwned && guard->hasFocus();
    d->focusOwned = false;
    d->sourceRoute = {};
    d->targetRoute = {};
    invalidate();
    if (!guard) return;
    state->synchronizePages();
    if (!guard || !restore || !guard->hasFocus() || !guard->isVisible() || !guard->isEnabled() || !guard->isActiveWindow()) return;
    const QPointer<QWidget> target(state->currentView());
    if (!target) return;
    QPointer<QWidget> focus;
    const auto found = state->pages.find(state->activeRoute.value());
    if (found != state->pages.end()) focus = found->second.focus;
    if (!focus || !(focus == target || target->isAncestorOf(focus)) || !focus->isVisible() || !focus->isEnabled()) {
        focus.clear();
        auto *candidate = target.data();
        do {
            if ((candidate == target || target->isAncestorOf(candidate)) && candidate->isVisible()
                && candidate->isEnabled() && (candidate->focusPolicy() & Qt::TabFocus)) { focus = candidate; break; }
            candidate = candidate->nextInFocusChain();
        } while (candidate != target);
    }
    if (focus) focus->setFocus(Qt::OtherFocusReason);
}
bool ZzPageHostTransitionPrivate::event(QEvent *event)
{
    const QPointer<ZzPageHost> guard(d->q);
    switch (event->type()) {
    case QEvent::Resize:
    case QEvent::ContentsRectChange:
    case QEvent::Hide:
    case QEvent::EnabledChange:
    case QEvent::WindowDeactivate:
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::FontChange:
    case QEvent::LanguageChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::DevicePixelRatioChange:
        settle();
        if (guard) d->refresh();
        return false;
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        if (running()) { event->accept(); return true; }
        break;
    default: break;
    }
    return false;
}
void ZzPageHostTransitionPrivate::observeCapture(ZzPageHost *host, std::function<void(bool)> callback)
{
    if (host && host->d_ptr && host->d_ptr->isOwnerThread())
        host->d_ptr->transition->d->player.setCaptureObserver(std::move(callback));
}
qsizetype ZzPageHostTransitionPrivate::snapshotBytes(const ZzPageHost *host)
{
    return host && host->d_ptr && host->d_ptr->isOwnerThread() ? host->d_ptr->transition->d->player.retainedBytes() : 0;
}
} // namespace ZzPureTools
