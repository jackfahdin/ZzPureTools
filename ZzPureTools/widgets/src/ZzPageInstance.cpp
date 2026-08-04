#include <ZzPureTools/ZzPageInstance.h>

#include <utility>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include "private/ZzPageInstancePrivate.h"

namespace ZzPureTools {

namespace {

using ZzPageInstancePointer = std::unique_ptr<ZzPageInstance>;

[[nodiscard]] ZzCore::ZzResult<ZzPageInstancePointer>
zzPageInstanceFailure(QString message)
{
    return ZzCore::ZzResult<ZzPageInstancePointer>::failure(
        ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidArgument,
            std::move(message)));
}

void zzDeleteRejectedView(QWidget *pageParent, QWidget *view) noexcept
{
    if (view != nullptr && view != pageParent) {
        delete view;
    }
}

} // namespace

ZzCore::ZzResult<std::unique_ptr<ZzPageInstance>> ZzPageInstance::create(
    QWidget *pageParent,
    QWidget *view,
    std::unique_ptr<QObject> viewModel,
    std::unique_ptr<QObject> presenter)
{
    const auto reject = [&](QString message) {
        // 先解除无效的双重所有权，避免 View 的 QObject 子树与 unique_ptr 重复释放。
        if (presenter) {
            presenter->setParent(nullptr);
        }
        if (viewModel) {
            viewModel->setParent(nullptr);
        }
        presenter.reset();
        viewModel.reset();
        zzDeleteRejectedView(pageParent, view);
        return zzPageInstanceFailure(std::move(message));
    };

    if (pageParent == nullptr) {
        return reject(
            QStringLiteral("page parent must not be null"));
    }
    if (view == nullptr) {
        return reject(
            QStringLiteral("page view must not be null"));
    }
    if (view == pageParent || view->parentWidget() != pageParent) {
        return reject(
            QStringLiteral("page view must be a direct child of page parent"));
    }
    if (!viewModel) {
        return reject(
            QStringLiteral("page view model must not be null"));
    }
    if (!presenter) {
        return reject(
            QStringLiteral("page presenter must not be null"));
    }
    if (viewModel->parent() != nullptr) {
        return reject(QStringLiteral(
            "page view model must not have a QObject parent"));
    }
    if (presenter->parent() != nullptr) {
        return reject(QStringLiteral(
            "page presenter must not have a QObject parent"));
    }

    auto instance = ZzPageInstancePointer(new ZzPageInstance(
        view, std::move(viewModel), std::move(presenter)));
    return ZzCore::ZzResult<ZzPageInstancePointer>::success(
        std::move(instance));
}

ZzPageInstance::ZzPageInstance(
    QWidget *view,
    std::unique_ptr<QObject> viewModel,
    std::unique_ptr<QObject> presenter)
    : d_ptr(std::make_unique<ZzPageInstancePrivate>(
          view, std::move(viewModel), std::move(presenter)))
{
}

ZzPageInstance::~ZzPageInstance()
{
    d_ptr->prepareForDestruction();
}

QWidget *ZzPageInstance::view() const noexcept
{
    return d_ptr->view.data();
}

void ZzPageInstance::addCancellation(ZzCancelCallback callback)
{
    d_ptr->addCancellation(std::move(callback));
}

void ZzPageInstance::prepareForDestruction() noexcept
{
    d_ptr->prepareForDestruction();
}

} // namespace ZzPureTools
