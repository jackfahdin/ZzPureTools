#include "ZzPageHostPrivate.h"

#include <exception>
#include <utility>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QScopeGuard>
#include <QtCore/QThread>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

namespace ZzPureTools {
namespace {
template<typename T = void>
ZzCore::ZzResult<T> zzFailure(ZzCore::ZzErrorCode code, const QString &message)
{
    return ZzCore::ZzResult<T>::failure(ZzCore::ZzError(code, message));
}
ZzCore::ZzResult<void> zzLost()
{
    return zzFailure(ZzCore::ZzErrorCode::InvalidState, QStringLiteral("page or host was destroyed during activation"));
}
bool zzValidPolicy(ZzPageLifetimePolicy policy)
{
    return policy == ZzPageLifetimePolicy::Persistent || policy == ZzPageLifetimePolicy::WhileActive
        || policy == ZzPageLifetimePolicy::Recreatable;
}
/** @brief 清理 factory 新增子对象，避免删除回调使快照内其他裸指针失效。 */
void zzCleanup(QPointer<QWidget> parent, const QSet<QObject *> &before)
{
    if (!parent) return;
    QList<QPointer<QObject>> added;
    for (auto *child : parent->children()) {
        if (!before.contains(child)) added.append(child);
    }
    for (const auto &child : added) {
        if (!parent) return;
        if (child && child->parent() == parent) delete child.data();
    }
}
}

ZzPageHostPrivate::ZzPageHostPrivate(ZzPageHost *owner)
    : q(owner), host(new QWidget(owner)), errorView(new QWidget(host))
{
    host->installEventFilter(this);
    auto *layout = new QVBoxLayout(errorView);
    layout->setContentsMargins(24, 24, 24, 24);
    auto *label = new QLabel(QCoreApplication::translate("ZzPageHost", "The page could not be opened."), errorView);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    layout->addWidget(label);
    errorView->hide();
    host->hide();
}

ZzPageHostPrivate::~ZzPageHostPrivate()
{
    destroying = true;
    busy = true;
    host->removeEventFilter(this);
    for (auto &[key, entry] : pages) {
        Q_UNUSED(key);
        QObject::disconnect(entry.destroyed);
        if (entry.instance && entry.instance->view()) entry.instance->view()->removeEventFilter(this);
    }
    // 摘出全部所有权，取消回调不再碰到正在析构的 map。
    auto dying = std::move(pages);
    for (auto &[key, entry] : dying) {
        Q_UNUSED(key);
        if (entry.instance) entry.instance->prepareForDestruction();
    }
}

bool ZzPageHostPrivate::isOwnerThread() const noexcept { return QThread::currentThread() == q->thread(); }
ZzCore::ZzResult<void> ZzPageHostPrivate::validateOperation() const
{
    if (!isOwnerThread() || busy || destroying) return zzFailure(
        ZzCore::ZzErrorCode::InvalidState, QStringLiteral("page host is busy or called from a non-owner thread"));
    return ZzCore::ZzResult<void>::success();
}
QWidget *ZzPageHostPrivate::currentView() const
{
    if (showingError) return errorView;
    const auto found = pages.find(activeRoute.value());
    if (found == pages.end() || !found->second.instance) return nullptr;
    auto *view = found->second.instance->view();
    return view && view->parentWidget() == host ? view : nullptr;
}
QSize ZzPageHostPrivate::hint(bool minimum) const
{
    QSize size = minimum ? errorView->minimumSizeHint() : errorView->sizeHint();
    for (const auto &[key, entry] : pages) {
        Q_UNUSED(key);
        if (auto *view = entry.instance->view()) size = size.expandedTo(minimum ? view->minimumSizeHint() : view->sizeHint());
    }
    const auto margins = q->contentsMargins();
    return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}
void ZzPageHostPrivate::synchronizePages()
{
    const QPointer<ZzPageHost> guard(q);
    const bool wasBusy = busy;
    busy = true;
    const auto unlock = qScopeGuard([this, guard, wasBusy] {
        if (guard && guard->d_ptr.get() == this) busy = wasBusy;
    });
    host->setGeometry(q->contentsRect());
    if (!guard) return;
    QList<QPointer<QWidget>> snapshot{errorView};
    for (const auto &[key, entry] : pages) {
        Q_UNUSED(key);
        snapshot.append(entry.instance->view());
    }
    for (const auto &view : snapshot) {
        if (view && view != currentView()) view->hide();
        if (!guard) return;
    }
    QPointer<QWidget> target(currentView());
    if (target) {
        target->setGeometry(host->rect());
        if (!guard) return;
        if (target && target == currentView()) target->show();
        if (!guard) return;
        if (target && target == currentView()) host->show();
    } else {
        host->hide();
    }
    if (guard) q->updateGeometry();
}
void ZzPageHostPrivate::settle() { synchronizePages(); }

ZzCore::ZzResult<void> ZzPageHostPrivate::activate(const ZzPageRegistration &registration, Direction direction)
{
    auto valid = validateOperation();
    if (!valid) return valid;
    if (!registration.routeId.isValid() || !registration.factory || !zzValidPolicy(registration.lifetime)
        || (direction != Direction::Forward && direction != Direction::Backward))
        return zzFailure(ZzCore::ZzErrorCode::InvalidArgument, QStringLiteral("invalid page registration or direction"));
    const QPointer<ZzPageHost> guard(q);
    busy = true;
    const auto unlock = qScopeGuard([this, guard] { if (guard && guard->d_ptr.get() == this) busy = false; });
    // factory 可能销毁持有注册表的 controller，不能跨回调使用调用者的引用。
    const auto requested = registration;
    const QString key = requested.routeId.value();
    auto found = pages.find(key);
    if (found != pages.end() && found->second.policy != requested.lifetime)
        return zzFailure(ZzCore::ZzErrorCode::InvalidArgument, QStringLiteral("page lifetime policy changed for an existing route"));
    if (!showingError && activeRoute == requested.routeId && currentView())
        return ZzCore::ZzResult<void>::success();
    if (found != pages.end() && (!found->second.instance->view()
        || found->second.instance->view()->parentWidget() != host)) {
        erasePage(key);
        if (!guard) return zzLost();
    }
    std::unique_ptr<ZzPageInstance> created;
    QPointer<QWidget> target;
    found = pages.find(key);
    if (found == pages.end()) {
        auto result = createPage(requested);
        if (!guard) return zzLost();
        if (!result) return ZzCore::ZzResult<void>::failure(result.error());
        created = std::move(result).value();
        target = created->view();
    } else target = found->second.instance->view();
    settle();
    if (!guard) return zzLost();
    if (!target || target->parentWidget() != host) return zzLost();
    if (created) {
        ZzPageEntry entry;
        entry.policy = requested.lifetime;
        entry.instance = std::move(created);
        entry.destroyed = QObject::connect(target, &QObject::destroyed, this, [this, key] {
            if (!showingError && activeRoute.value() == key) activeRoute = {};
            // QWidget 在 QObject 析构清空 QPointer 前发 destroyed；此时销毁
            // PageInstance 会再次 delete 正在析构的 View。仅在观察指针清空后清理。
            QMetaObject::invokeMethod(this, [this, key] {
                const auto item = pages.find(key);
                if (item != pages.end() && !item->second.instance->view()) pageLost(key);
            }, Qt::QueuedConnection);
        });
        target->installEventFilter(this);
        pages.emplace(key, std::move(entry));
    }
    lru.remove(key);
    leave();
    if (!guard) return zzLost();
    if (!target || target->parentWidget() != host || !pages.contains(key)) return zzLost();
    activeRoute = requested.routeId;
    showingError = false;
    synchronizePages();
    if (!guard) return zzLost();
    if (!target || currentView() != target) return zzLost();
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<std::unique_ptr<ZzPageInstance>> ZzPageHostPrivate::createPage(const ZzPageRegistration &registration)
{
    using Pointer = std::unique_ptr<ZzPageInstance>;
    QPointer<QWidget> parent(host);
    QSet<QObject *> before;
    for (auto *child : parent->children()) before.insert(child);
    try {
        auto result = registration.factory(parent);
        if (!parent) return zzFailure<Pointer>(ZzCore::ZzErrorCode::InvalidState, QStringLiteral("page host was destroyed by factory"));
        if (!result) {
            auto error = result.error();
            zzCleanup(parent, before);
            return ZzCore::ZzResult<Pointer>::failure(error);
        }
        auto instance = std::move(result).value();
        if (!instance || !instance->view() || instance->view()->parentWidget() != parent) {
            instance.reset();
            zzCleanup(parent, before);
            return zzFailure<Pointer>(ZzCore::ZzErrorCode::InvalidState, QStringLiteral("page factory returned an invalid page instance"));
        }
        return ZzCore::ZzResult<Pointer>::success(std::move(instance));
    } catch (const std::exception &exception) {
        zzCleanup(parent, before);
        return ZzCore::ZzResult<Pointer>::failure(ZzCore::ZzError(ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("page factory threw an exception"), QString::fromLocal8Bit(exception.what())));
    } catch (...) {
        zzCleanup(parent, before);
        return zzFailure<Pointer>(ZzCore::ZzErrorCode::Unknown, QStringLiteral("page factory threw an unknown exception"));
    }
}
void ZzPageHostPrivate::erasePage(const QString &key)
{
    const auto found = pages.find(key);
    if (found == pages.end()) return;
    QObject::disconnect(found->second.destroyed);
    auto dying = std::move(found->second.instance);
    if (dying && dying->view()) dying->view()->removeEventFilter(this);
    pages.erase(found);
    lru.remove(key);
    if (dying) dying->prepareForDestruction();
}
void ZzPageHostPrivate::pageLost(const QString &key)
{
    if (destroying) return;
    const QPointer<ZzPageHost> guard(q);
    const bool wasBusy = busy;
    busy = true;
    const auto unlock = qScopeGuard([this, guard, wasBusy] { if (guard && guard->d_ptr.get() == this) busy = wasBusy; });
    if (!showingError && activeRoute.value() == key) activeRoute = {};
    erasePage(key);
    if (guard) settle();
}
void ZzPageHostPrivate::leave()
{
    const QPointer<ZzPageHost> guard(q);
    const QString key = activeRoute.value();
    const bool wasError = showingError;
    const QPointer<QWidget> outgoing(currentView());
    activeRoute = {};
    showingError = false;
    // 先逐页隐藏，避免 QWidget::hideChildren 使用可能被回调删除的兄弟裸指针。
    if (outgoing) outgoing->hide();
    if (!guard) return;
    host->hide();
    if (!guard || wasError || key.isEmpty()) return;
    const auto found = pages.find(key);
    if (found == pages.end()) return;
    const auto policy = found->second.policy;
    if (policy == ZzPageLifetimePolicy::WhileActive || (policy == ZzPageLifetimePolicy::Recreatable && capacity == 0)) {
        erasePage(key);
    } else if (policy == ZzPageLifetimePolicy::Recreatable) {
        lru.remove(key);
        lru.push_back(key);
        evict();
    }
}
void ZzPageHostPrivate::evict()
{
    const QPointer<ZzPageHost> guard(q);
    while (lru.size() > static_cast<std::size_t>(capacity)) {
        const QString key = lru.front();
        lru.pop_front();
        erasePage(key);
        if (!guard) return;
    }
}
void ZzPageHostPrivate::deactivateCurrent() noexcept
{
    if (!validateOperation()) return;
    const QPointer<ZzPageHost> guard(q);
    busy = true;
    const auto unlock = qScopeGuard([this, guard] { if (guard && guard->d_ptr.get() == this) busy = false; });
    leave();
}
ZzCore::ZzResult<void> ZzPageHostPrivate::showFrameworkError(ZzRouteId route)
{
    auto valid = validateOperation();
    if (!valid) return valid;
    if (!route.isValid()) return zzFailure(ZzCore::ZzErrorCode::InvalidArgument, QStringLiteral("failed page route must not be empty"));
    const QPointer<ZzPageHost> guard(q);
    busy = true;
    const auto unlock = qScopeGuard([this, guard] { if (guard && guard->d_ptr.get() == this) busy = false; });
    leave();
    if (!guard) return zzLost();
    activeRoute = std::move(route);
    showingError = true;
    synchronizePages();
    return guard ? ZzCore::ZzResult<void>::success() : zzLost();
}
ZzCore::ZzResult<void> ZzPageHostPrivate::setRecreatableCapacity(qsizetype count)
{
    auto valid = validateOperation();
    if (!valid) return valid;
    if (count < 0) return zzFailure(ZzCore::ZzErrorCode::InvalidArgument, QStringLiteral("recreatable page capacity must not be negative"));
    const QPointer<ZzPageHost> guard(q);
    busy = true;
    const auto unlock = qScopeGuard([this, guard] { if (guard && guard->d_ptr.get() == this) busy = false; });
    capacity = count;
    evict();
    return guard ? ZzCore::ZzResult<void>::success() : zzLost();
}
bool ZzPageHostPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (destroying) return false;
    if (event->type() == QEvent::LayoutRequest) q->updateGeometry();
    if (watched != host && event->type() == QEvent::ParentChange) {
        auto *view = qobject_cast<QWidget *>(watched);
        if (view && view->parentWidget() != host) {
            QString key;
            for (const auto &[route, entry] : pages) {
                if (entry.instance->view() == view) { key = route; break; }
            }
            const QPointer<QObject> guard(watched);
            if (!key.isEmpty()) pageLost(key);
            return guard.isNull();
        }
    }
    return false;
}
} // namespace ZzPureTools
