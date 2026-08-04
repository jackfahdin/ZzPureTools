#include "ZzPageHostPrivate.h"

#include <cstddef>
#include <exception>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzPageHost.h>

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzPageHostFailure(
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

[[nodiscard]] QSet<QObject *> zzDirectChildren(QWidget *parent)
{
    QSet<QObject *> children;
    const auto directChildren = parent->children();
    children.reserve(directChildren.size());
    for (auto *child : directChildren) {
        children.insert(child);
    }
    return children;
}

void zzDeleteNewDirectChildren(
    QWidget *parent,
    const QSet<QObject *> &childrenBefore) noexcept
{
    const auto childrenAfter = parent->children();
    for (auto *child : childrenAfter) {
        if (!childrenBefore.contains(child)) {
            delete child;
        }
    }
}

} // namespace

ZzPageHostPrivate::ZzPageHostPrivate(ZzPageHost *host)
    : q_ptr(host)
    , stack(new QStackedWidget(host))
    , frameworkErrorWidget(new QWidget(stack))
{
    Q_ASSERT(q_ptr != nullptr);
    if (q_ptr == nullptr) {
        std::terminate();
    }

    auto *hostLayout = new QVBoxLayout(q_ptr);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);
    hostLayout->addWidget(stack);

    auto *errorLayout = new QVBoxLayout(frameworkErrorWidget);
    errorLayout->setContentsMargins(24, 24, 24, 24);
    auto *errorLabel = new QLabel(
        QCoreApplication::translate(
            "ZzPageHost", "The page could not be opened."),
        frameworkErrorWidget);
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setWordWrap(true);
    errorLayout->addWidget(errorLabel);

    stack->addWidget(frameworkErrorWidget);
    stack->hide();
}

ZzPageHostPrivate::~ZzPageHostPrivate()
{
    stack->setEnabled(false);
    stack->blockSignals(true);
    for (auto &page : pages) {
        if (page.second.instance) {
            page.second.instance->prepareForDestruction();
        }
    }
    pages.clear();
}

ZzCore::ZzResult<void> ZzPageHostPrivate::activate(
    const ZzPageRegistration &registration)
{
    auto validation = validateRegistration(registration);
    if (!validation) {
        return validation;
    }

    const QString routeKey = registration.routeId.value();
    auto pageIterator = pages.find(routeKey);
    if (pageIterator != pages.end()
        && pageIterator->second.policy != registration.lifetime) {
        return zzPageHostFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("page lifetime policy changed for an existing route"),
            QStringLiteral("routeId=%1").arg(routeKey));
    }
    if (!showingFrameworkError && activeRoute == registration.routeId
        && pageIterator != pages.end()
        && pageIterator->second.instance
        && pageIterator->second.instance->view() != nullptr) {
        return ZzCore::ZzResult<void>::success();
    }

    if (pageIterator != pages.end()
        && (!pageIterator->second.instance
            || pageIterator->second.instance->view() == nullptr)) {
        removeFromRecreatableLru(routeKey);
        pages.erase(pageIterator);
        pageIterator = pages.end();
        if (!showingFrameworkError
            && activeRoute == registration.routeId) {
            activeRoute = {};
            stack->hide();
        }
    }
    if (pageIterator == pages.end()) {
        auto createResult = createPage(registration);
        if (!createResult) {
            return ZzCore::ZzResult<void>::failure(createResult.error());
        }

        ZzPageEntry entry;
        entry.policy = registration.lifetime;
        entry.instance = std::move(createResult).value();
        auto inserted = pages.emplace(routeKey, std::move(entry));
        Q_ASSERT(inserted.second);
        pageIterator = inserted.first;
        stack->addWidget(pageIterator->second.instance->view());
    }

    removeFromRecreatableLru(routeKey);
    deactivateCurrentUnchecked();
    stack->setCurrentWidget(pageIterator->second.instance->view());
    stack->show();
    activeRoute = registration.routeId;
    showingFrameworkError = false;
    return ZzCore::ZzResult<void>::success();
}

void ZzPageHostPrivate::deactivateCurrent() noexcept
{
    Q_ASSERT(isOwnerThread());
    if (!isOwnerThread()) {
        return;
    }
    deactivateCurrentUnchecked();
}

ZzCore::ZzResult<void> ZzPageHostPrivate::showFrameworkError(
    ZzRouteId failedRoute)
{
    if (!isOwnerThread()) {
        return zzPageHostFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("page host called from a non-owner thread"));
    }
    if (!failedRoute.isValid()) {
        return zzPageHostFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("failed page route must not be empty"));
    }

    deactivateCurrentUnchecked();
    stack->setCurrentWidget(frameworkErrorWidget);
    stack->show();
    activeRoute = std::move(failedRoute);
    showingFrameworkError = true;
    return ZzCore::ZzResult<void>::success();
}

ZzRouteId ZzPageHostPrivate::currentRoute() const
{
    Q_ASSERT(isOwnerThread());
    if (!isOwnerThread()) {
        return {};
    }
    return activeRoute;
}

ZzCore::ZzResult<void> ZzPageHostPrivate::setRecreatableCapacity(
    qsizetype capacity)
{
    if (!isOwnerThread()) {
        return zzPageHostFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("page host called from a non-owner thread"));
    }
    if (capacity < 0) {
        return zzPageHostFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("recreatable page capacity must not be negative"));
    }

    recreatableCapacity = capacity;
    evictRecreatablePages();
    return ZzCore::ZzResult<void>::success();
}

bool ZzPageHostPrivate::isOwnerThread() const noexcept
{
    return QThread::currentThread() == q_ptr->thread();
}

ZzCore::ZzResult<void> ZzPageHostPrivate::validateRegistration(
    const ZzPageRegistration &registration) const
{
    if (!isOwnerThread()) {
        return zzPageHostFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("page host called from a non-owner thread"));
    }
    if (!registration.routeId.isValid()) {
        return zzPageHostFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("page route must not be empty"));
    }
    if (!zzIsValidPolicy(registration.lifetime)) {
        return zzPageHostFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("page lifetime policy is invalid"),
            QStringLiteral("routeId=%1")
                .arg(registration.routeId.value()));
    }
    if (!registration.factory) {
        return zzPageHostFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("page factory must not be empty"),
            QStringLiteral("routeId=%1")
                .arg(registration.routeId.value()));
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<std::unique_ptr<ZzPageInstance>>
ZzPageHostPrivate::createPage(const ZzPageRegistration &registration)
{
    using ZzPagePointer = std::unique_ptr<ZzPageInstance>;

    const auto childrenBefore = zzDirectChildren(stack);
    try {
        auto factoryResult = registration.factory(stack);
        if (!factoryResult) {
            const auto error = factoryResult.error();
            zzDeleteNewDirectChildren(stack, childrenBefore);
            return ZzCore::ZzResult<ZzPagePointer>::failure(error);
        }

        auto instance = std::move(factoryResult).value();
        if (!instance || instance->view() == nullptr
            || instance->view()->parentWidget() != stack) {
            instance.reset();
            zzDeleteNewDirectChildren(stack, childrenBefore);
            return zzPageHostFailure<ZzPagePointer>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("page factory returned an invalid page instance"),
                QStringLiteral("routeId=%1")
                    .arg(registration.routeId.value()));
        }
        return ZzCore::ZzResult<ZzPagePointer>::success(
            std::move(instance));
    } catch (const std::exception &exception) {
        zzDeleteNewDirectChildren(stack, childrenBefore);
        return zzPageHostFailure<ZzPagePointer>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("page factory threw an exception"),
            QStringLiteral("routeId=%1; exception=%2")
                .arg(
                    registration.routeId.value(),
                    QString::fromLocal8Bit(exception.what())));
    } catch (...) {
        zzDeleteNewDirectChildren(stack, childrenBefore);
        return zzPageHostFailure<ZzPagePointer>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("page factory threw an unknown exception"),
            QStringLiteral("routeId=%1; exception=unknown")
                .arg(registration.routeId.value()));
    }
}

void ZzPageHostPrivate::deactivateCurrentUnchecked() noexcept
{
    stack->hide();
    if (!activeRoute.isValid()) {
        showingFrameworkError = false;
        return;
    }
    if (showingFrameworkError) {
        activeRoute = {};
        showingFrameworkError = false;
        return;
    }

    const QString routeKey = activeRoute.value();
    auto pageIterator = pages.find(routeKey);
    activeRoute = {};
    if (pageIterator == pages.end()) {
        return;
    }

    switch (pageIterator->second.policy) {
    case ZzPageLifetimePolicy::Persistent:
        break;
    case ZzPageLifetimePolicy::WhileActive:
        pageIterator->second.instance->prepareForDestruction();
        pages.erase(pageIterator);
        break;
    case ZzPageLifetimePolicy::Recreatable:
        if (recreatableCapacity == 0) {
            pageIterator->second.instance->prepareForDestruction();
            pages.erase(pageIterator);
        } else {
            removeFromRecreatableLru(routeKey);
            recreatableLru.push_back(routeKey);
            evictRecreatablePages();
        }
        break;
    }
}

void ZzPageHostPrivate::removeFromRecreatableLru(
    const QString &routeKey) noexcept
{
    recreatableLru.remove(routeKey);
}

void ZzPageHostPrivate::evictRecreatablePages() noexcept
{
    const auto capacity = static_cast<std::size_t>(recreatableCapacity);
    while (recreatableLru.size() > capacity) {
        const QString routeKey = std::move(recreatableLru.front());
        recreatableLru.pop_front();
        auto pageIterator = pages.find(routeKey);
        if (pageIterator == pages.end()
            || pageIterator->second.policy
                != ZzPageLifetimePolicy::Recreatable) {
            continue;
        }
        pageIterator->second.instance->prepareForDestruction();
        pages.erase(pageIterator);
    }
}

} // namespace ZzPureTools
