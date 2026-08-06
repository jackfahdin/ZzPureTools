#include "ZzPageInstancePrivate.h"

#include <array>
#include <exception>
#include <utility>

#include <QtCore/QDebug>

namespace ZzPureTools {

namespace {

/** @brief 仅断开页面展示对象之间的连接，保留外部 destroyed 观察者。 */
void zzDisconnectPresentationObjects(
    QWidget *view,
    QObject *viewModel,
    QObject *presenter) noexcept
{
    const std::array<QObject *, 3> objects{
        view, viewModel, presenter};
    for (QObject *sender : objects) {
        if (sender == nullptr) {
            continue;
        }
        for (QObject *receiver : objects) {
            if (receiver != nullptr) {
                static_cast<void>(QObject::disconnect(
                    sender, nullptr, receiver, nullptr));
            }
        }
    }
}

} // namespace

ZzPageInstancePrivate::ZzPageInstancePrivate(
    QWidget *pageView,
    std::unique_ptr<QObject> viewModel,
    std::unique_ptr<QObject> presenter)
    : view(pageView)
    , viewModel_(std::move(viewModel))
    , presenter_(std::move(presenter))
{
}

void ZzPageInstancePrivate::addCancellation(
    ZzPageInstance::ZzCancelCallback callback)
{
    if (!callback) {
        return;
    }
    if (prepared_) {
        invokeCancellation(callback);
        return;
    }
    cancellations_.push_back(std::move(callback));
}

void ZzPageInstancePrivate::prepareForDestruction() noexcept
{
    if (prepared_) {
        return;
    }
    prepared_ = true;

    if (view) {
        view->setEnabled(false);
    }

    for (auto &callback : cancellations_) {
        invokeCancellation(callback);
    }
    cancellations_.clear();

    zzDisconnectPresentationObjects(
        view.data(), viewModel_.get(), presenter_.get());

    delete view.data();
    view.clear();
    presenter_.reset();
    viewModel_.reset();
}

void ZzPageInstancePrivate::invokeCancellation(
    ZzPageInstance::ZzCancelCallback &callback) noexcept
{
    try {
        callback();
    } catch (const std::exception &exception) {
        qWarning().noquote()
            << "page cancellation callback threw an exception:"
            << exception.what();
    } catch (...) {
        qWarning().noquote()
            << "page cancellation callback threw an unknown exception";
    }
}

} // namespace ZzPureTools
