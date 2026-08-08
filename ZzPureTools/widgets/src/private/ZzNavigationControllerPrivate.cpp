#include "ZzNavigationControllerPrivate.h"

#include <utility>

#include <QtCore/QDebug>
#include <QtCore/QThread>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzNavigationModel.h>
#include <ZzPureTools/ZzPageHost.h>

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzNavigationFailure(
    ZzCore::ZzErrorCode code,
    QString message,
    QString context = {})
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        code, std::move(message), std::move(context)));
}

[[nodiscard]] bool zzIsValidPolicy(
    ZzPageLifetimePolicy policy) noexcept
{
    switch (policy) {
    case ZzPageLifetimePolicy::Persistent:
    case ZzPageLifetimePolicy::WhileActive:
    case ZzPageLifetimePolicy::Recreatable:
        return true;
    }
    return false;
}

} // namespace

ZzNavigationControllerPrivate::ZzNavigationControllerPrivate(
    ZzNavigationController *controller,
    ZzNavigationModel *navigationModel,
    ZzPageHost *pageHost)
    : q_ptr(controller)
    , model(navigationModel)
    , host(pageHost)
{
    Q_ASSERT(q_ptr != nullptr);
    if (q_ptr == nullptr) {
        std::terminate();
    }
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());
    Q_ASSERT(navigationModel == nullptr
             || navigationModel->thread() == q_ptr->thread());
    Q_ASSERT(pageHost == nullptr
             || pageHost->thread() == q_ptr->thread());
}

ZzNavigationControllerPrivate::~ZzNavigationControllerPrivate() = default;

ZzCore::ZzResult<void> ZzNavigationControllerPrivate::setRegistrations(
    QList<ZzPageRegistration> newRegistrations)
{
    auto valid = validateOperation();
    if (!valid) {
        return valid;
    }
    if (registrationsSet) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("page registrations can only be set once"));
    }
    registrationsSet = true;

    std::map<QString, ZzPageRegistration> staged;
    for (qsizetype index = 0; index < newRegistrations.size(); ++index) {
        auto &registration = newRegistrations[index];
        if (!registration.routeId.isValid()) {
            return zzNavigationFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("page registration route must not be empty"),
                QStringLiteral("index=%1").arg(index));
        }
        if (!zzIsValidPolicy(registration.lifetime)) {
            return zzNavigationFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("page registration lifetime is invalid"),
                QStringLiteral("routeId=%1")
                    .arg(registration.routeId.value()));
        }
        if (!registration.factory) {
            return zzNavigationFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("page registration factory must not be empty"),
                QStringLiteral("routeId=%1")
                    .arg(registration.routeId.value()));
        }

        const QString routeKey = registration.routeId.value();
        auto inserted = staged.emplace(
            routeKey, std::move(registration));
        if (!inserted.second) {
            return zzNavigationFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("page registration route must be unique"),
                QStringLiteral("routeId=%1").arg(routeKey));
        }
    }

    registrations.swap(staged);
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzNavigationControllerPrivate::navigate(
    const ZzRouteId &routeId)
{
    auto valid = validateOperation();
    if (!valid) {
        return valid;
    }
    if (!registrationsSet) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("page registrations have not been set"));
    }
    if (!routeId.isValid()) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("navigation route must not be empty"));
    }

    const auto registrationIterator = registrations.find(routeId.value());
    if (registrationIterator == registrations.end()) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("navigation route is not registered"),
            QStringLiteral("routeId=%1").arg(routeId.value()));
    }

    const ZzRouteId oldRoute = host->currentRoute();
    const bool oldWasFrameworkError = showingFrameworkError;
    if (!oldWasFrameworkError && oldRoute == routeId) {
        return ZzCore::ZzResult<void>::success();
    }
    const bool oldCanGoBack = !backHistory.isEmpty();
    const bool oldCanGoForward = !forwardHistory.isEmpty();

    auto activation = host->activate(registrationIterator->second);
    if (activation) {
        if (!oldWasFrameworkError && oldRoute.isValid()
            && oldRoute != routeId) {
            appendHistory(backHistory, oldRoute);
            forwardHistory.clear();
        } else if (oldWasFrameworkError && !backHistory.isEmpty()
                   && backHistory.constLast() == routeId) {
            backHistory.removeLast();
        }
        showingFrameworkError = false;
        Q_EMIT q_ptr->currentRouteChanged(routeId);
        notifyHistoryState(oldCanGoBack, oldCanGoForward);
        return ZzCore::ZzResult<void>::success();
    }

    const ZzCore::ZzError &error = activation.error();
    if (!oldWasFrameworkError && oldRoute.isValid()) {
        appendHistory(backHistory, oldRoute);
        forwardHistory.clear();
    }
    auto frameworkResult = host->showFrameworkError(routeId);
    if (frameworkResult) {
        showingFrameworkError = true;
        Q_EMIT q_ptr->currentRouteChanged(routeId);
    } else {
        qWarning().noquote()
            << "failed to show navigation framework error page:"
            << frameworkResult.error().technicalMessage()
            << frameworkResult.error().context();
    }
    reportNavigationFailure(error);
    notifyHistoryState(oldCanGoBack, oldCanGoForward);
    return ZzCore::ZzResult<void>::failure(error);
}

ZzCore::ZzResult<void> ZzNavigationControllerPrivate::goBack()
{
    auto valid = validateOperation();
    if (!valid) {
        return valid;
    }
    if (backHistory.isEmpty()) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("navigation history is empty"));
    }

    const ZzRouteId targetRoute = backHistory.constLast();
    const auto registrationIterator = registrations.find(targetRoute.value());
    if (registrationIterator == registrations.end()) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("navigation history route is not registered"),
            QStringLiteral("routeId=%1").arg(targetRoute.value()));
    }

    const bool oldCanGoBack = !backHistory.isEmpty();
    const bool oldCanGoForward = !forwardHistory.isEmpty();
    const ZzRouteId oldRoute = host->currentRoute();
    const bool oldWasFrameworkError = showingFrameworkError;
    auto activation = host->activate(registrationIterator->second);
    if (!activation) {
        reportNavigationFailure(activation.error());
        return activation;
    }

    backHistory.removeLast();
    if (!oldWasFrameworkError && oldRoute.isValid()) {
        appendHistory(forwardHistory, oldRoute);
    }
    showingFrameworkError = false;
    Q_EMIT q_ptr->currentRouteChanged(targetRoute);
    notifyHistoryState(oldCanGoBack, oldCanGoForward);
    return ZzCore::ZzResult<void>::success();
}

bool ZzNavigationControllerPrivate::canGoBack() const noexcept
{
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());
    return QThread::currentThread() == q_ptr->thread()
        && !model.isNull() && !host.isNull()
        && model->thread() == q_ptr->thread()
        && host->thread() == q_ptr->thread()
        && !backHistory.isEmpty();
}

ZzCore::ZzResult<void> ZzNavigationControllerPrivate::goForward()
{
    auto valid = validateOperation();
    if (!valid) {
        return valid;
    }
    if (forwardHistory.isEmpty()) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("navigation forward history is empty"));
    }

    const ZzRouteId targetRoute = forwardHistory.constLast();
    const auto registrationIterator = registrations.find(targetRoute.value());
    if (registrationIterator == registrations.end()) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("navigation forward route is not registered"),
            QStringLiteral("routeId=%1").arg(targetRoute.value()));
    }

    const bool oldCanGoBack = !backHistory.isEmpty();
    const bool oldCanGoForward = !forwardHistory.isEmpty();
    const ZzRouteId oldRoute = host->currentRoute();
    const bool oldWasFrameworkError = showingFrameworkError;
    auto activation = host->activate(registrationIterator->second);
    if (!activation) {
        reportNavigationFailure(activation.error());
        return activation;
    }

    forwardHistory.removeLast();
    if (!oldWasFrameworkError && oldRoute.isValid()) {
        appendHistory(backHistory, oldRoute);
    }
    showingFrameworkError = false;
    Q_EMIT q_ptr->currentRouteChanged(targetRoute);
    notifyHistoryState(oldCanGoBack, oldCanGoForward);
    return ZzCore::ZzResult<void>::success();
}

bool ZzNavigationControllerPrivate::canGoForward() const noexcept
{
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());
    return QThread::currentThread() == q_ptr->thread()
        && !model.isNull() && !host.isNull()
        && model->thread() == q_ptr->thread()
        && host->thread() == q_ptr->thread()
        && !forwardHistory.isEmpty();
}

ZzRouteId ZzNavigationControllerPrivate::currentRoute() const
{
    auto valid = validateOperation();
    if (!valid) {
        return {};
    }
    return host->currentRoute();
}

ZzCore::ZzResult<void> ZzNavigationControllerPrivate::setHistoryCapacity(
    qsizetype capacity)
{
    auto valid = validateOperation();
    if (!valid) {
        return valid;
    }
    if (capacity < 0) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("navigation history capacity must not be negative"));
    }

    historyCapacity = capacity;
    const bool oldCanGoBack = !backHistory.isEmpty();
    const bool oldCanGoForward = !forwardHistory.isEmpty();
    for (auto *history : {&backHistory, &forwardHistory}) {
        const qsizetype excess = history->size() - historyCapacity;
        if (excess > 0) {
            history->remove(0, excess);
        }
    }
    notifyHistoryState(oldCanGoBack, oldCanGoForward);
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void>
ZzNavigationControllerPrivate::validateOperation() const
{
    if (QThread::currentThread() != q_ptr->thread()) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("navigation controller called from a non-owner thread"));
    }
    if (model.isNull() || host.isNull()) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("navigation controller dependencies are unavailable"));
    }
    if (model->thread() != q_ptr->thread()
        || host->thread() != q_ptr->thread()) {
        return zzNavigationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("navigation dependencies belong to another thread"));
    }
    return ZzCore::ZzResult<void>::success();
}

void ZzNavigationControllerPrivate::appendHistory(
    QList<ZzRouteId> &history,
    const ZzRouteId &routeId)
{
    if (historyCapacity == 0 || !routeId.isValid()) {
        return;
    }
    history.append(routeId);
    const qsizetype excess = history.size() - historyCapacity;
    if (excess > 0) {
        history.remove(0, excess);
    }
}

void ZzNavigationControllerPrivate::notifyHistoryState(
    bool oldCanGoBack,
    bool oldCanGoForward)
{
    const bool newCanGoBack = !backHistory.isEmpty();
    const bool newCanGoForward = !forwardHistory.isEmpty();
    if (oldCanGoBack != newCanGoBack
        || oldCanGoForward != newCanGoForward) {
        Q_EMIT q_ptr->historyStateChanged(
            newCanGoBack, newCanGoForward);
    }
}

void ZzNavigationControllerPrivate::reportNavigationFailure(
    const ZzCore::ZzError &error)
{
    qWarning().noquote()
        << "page navigation failed:"
        << error.technicalMessage()
        << error.context();
    Q_EMIT q_ptr->navigationFailed(error);
}

} // namespace ZzPureTools
