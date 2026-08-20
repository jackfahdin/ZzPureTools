#include "ZzWindowAgentPrivate.h"

#include <array>
#include <exception>
#include <utility>

#include <QtCore/QPoint>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtCore/QString>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzWindowKit/ZzWindowAgent.h>

namespace ZzWindowKit {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzWindowFailure(
    ZzCore::ZzErrorCode code,
    QString message,
    QString context = {})
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        code, std::move(message), std::move(context)));
}

[[nodiscard]] bool zzIsActiveState(ZzWindowAgentState state) noexcept
{
    return state == ZzWindowAgentState::Attached
        || state == ZzWindowAgentState::Configured;
}

} // namespace

ZzWindowAgentPrivate::ZzWindowAgentPrivate(
    ZzWindowAgent *agent,
    std::unique_ptr<ZzWindowBackend> windowBackend)
    : q_ptr(agent)
    , backend(std::move(windowBackend))
{
    Q_ASSERT(q_ptr != nullptr);
    Q_ASSERT(backend != nullptr);
    if (q_ptr == nullptr || backend == nullptr) {
        std::terminate();
    }
}

ZzWindowAgentPrivate::~ZzWindowAgentPrivate()
{
    QObject::disconnect(hostDestroyedConnection);
}

ZzCore::ZzResult<void> ZzWindowAgentPrivate::attach(QWidget *window)
{
    if (state != ZzWindowAgentState::Detached) {
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("window agent can only attach once"));
    }
    if (window == nullptr) {
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("window must not be null"));
    }
    if (QThread::currentThread() != q_ptr->thread()) {
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("window agent called from a non-owner thread"));
    }
    if (window->thread() != q_ptr->thread()) {
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("window belongs to a different thread"));
    }
    if (!window->isWindow()) {
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("window must be a top-level QWidget"));
    }

    auto result = backend->attach(window);
    if (!result) {
        state = ZzWindowAgentState::Failed;
        return result;
    }

    host = window;
    hostDestroyedConnection = QObject::connect(
        window,
        &QObject::destroyed,
        q_ptr,
        [this] {
            host.clear();
            state = ZzWindowAgentState::Invalidated;
        });
    state = ZzWindowAgentState::Attached;
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWindowAgentPrivate::configureChrome(
    const ZzWindowChromeConfiguration &configuration)
{
    auto active = validateActiveOperation(
        QStringLiteral("configure chrome"));
    if (!active) {
        return active;
    }

    auto validation = validateChrome(configuration);
    if (!validation) {
        return validation;
    }

    auto result = backend->configureChrome(configuration);
    if (!result) {
        state = ZzWindowAgentState::Failed;
        return result;
    }
    state = ZzWindowAgentState::Configured;
    return ZzCore::ZzResult<void>::success();
}

ZzWindowCapabilities ZzWindowAgentPrivate::capabilities() const noexcept
{
    if (QThread::currentThread() != q_ptr->thread()
        || !zzIsActiveState(state) || host.isNull()) {
        return {};
    }
    return backend->capabilities();
}

ZzCore::ZzResult<ZzWindowApplyState>
ZzWindowAgentPrivate::setBackdrop(ZzWindowBackdrop backdrop)
{
    auto active = validateActiveOperation(QStringLiteral("set backdrop"));
    if (!active) {
        return ZzCore::ZzResult<ZzWindowApplyState>::failure(
            active.error());
    }
    return backend->setBackdrop(backdrop);
}

ZzCore::ZzResult<ZzWindowApplyState>
ZzWindowAgentPrivate::setColorScheme(ZzWindowColorScheme colorScheme)
{
    auto active = validateActiveOperation(
        QStringLiteral("set color scheme"));
    if (!active) {
        return ZzCore::ZzResult<ZzWindowApplyState>::failure(
            active.error());
    }
    return backend->setColorScheme(colorScheme);
}

ZzCore::ZzResult<void> ZzWindowAgentPrivate::showSystemMenu(
    const QPoint &globalPosition)
{
    auto active = validateActiveOperation(
        QStringLiteral("show system menu"));
    if (!active) {
        return active;
    }
    return backend->showSystemMenu(globalPosition);
}

ZzCore::ZzResult<void> ZzWindowAgentPrivate::validateChrome(
    const ZzWindowChromeConfiguration &configuration) const
{
    if (configuration.titleBar == nullptr) {
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("title bar must not be null"));
    }
    if (configuration.titleBar->thread() != q_ptr->thread()) {
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("title bar belongs to a different thread"));
    }
    if (host.isNull() || !host->isAncestorOf(configuration.titleBar)) {
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("title bar must be a host descendant"));
    }

    const std::array<QWidget *, 4> systemWidgets{
        configuration.windowIcon,
        configuration.minimizeButton,
        configuration.maximizeButton,
        configuration.closeButton};
    QSet<QWidget *> uniqueWidgets;
    for (auto *widget : systemWidgets) {
        if (widget == nullptr) {
            continue;
        }
        if (widget->thread() != q_ptr->thread()
            || !configuration.titleBar->isAncestorOf(widget)) {
            return zzWindowFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral(
                    "system widget must be a title bar descendant"));
        }
        if (uniqueWidgets.contains(widget)) {
            return zzWindowFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("chrome widgets must not contain duplicates"));
        }
        uniqueWidgets.insert(widget);
    }

    const auto &hitTestVisibleWidgets = configuration.interactiveWidgets;
    for (auto *widget : hitTestVisibleWidgets) {
        if (widget == nullptr) {
            return zzWindowFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("hit-test visible widget must not be null"));
        }
        if (widget->thread() != q_ptr->thread()
            || !configuration.titleBar->isAncestorOf(widget)) {
            return zzWindowFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral(
                    "hit-test visible widget must be a title bar descendant"));
        }
        if (uniqueWidgets.contains(widget)) {
            return zzWindowFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("chrome widgets must not contain duplicates"));
        }
        uniqueWidgets.insert(widget);
    }

    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWindowAgentPrivate::validateActiveOperation(
    QString operation)
{
    if (QThread::currentThread() != q_ptr->thread()) {
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("window agent called from a non-owner thread"),
            std::move(operation));
    }
    if (!zzIsActiveState(state)) {
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("window agent is not attached"),
            std::move(operation));
    }
    if (host.isNull()) {
        state = ZzWindowAgentState::Invalidated;
        return zzWindowFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("attached window has been destroyed"),
            std::move(operation));
    }
    return ZzCore::ZzResult<void>::success();
}

} // namespace ZzWindowKit
