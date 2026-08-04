#include "ZzPageInstancePrivate.h"

#include <exception>
#include <utility>

#include <QtCore/QDebug>

namespace ZzPureTools {

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

    if (view) {
        static_cast<void>(QObject::disconnect(
            view.data(), nullptr, nullptr, nullptr));
    }
    if (presenter_) {
        static_cast<void>(QObject::disconnect(
            presenter_.get(), nullptr, nullptr, nullptr));
    }
    if (viewModel_) {
        static_cast<void>(QObject::disconnect(
            viewModel_.get(), nullptr, nullptr, nullptr));
    }

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
