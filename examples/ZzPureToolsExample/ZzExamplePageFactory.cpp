#include "ZzExamplePageFactory.h"

#include <optional>
#include <utility>

#include <QtCore/QDebug>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtGui/QFont>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>

#include "ZzExampleApplicationContext.h"
#include "ZzExampleGalleryPage.h"
#include "ZzExampleNavigationPresenter.h"

namespace ZzExample {

namespace {

/** @brief 为已实现路由解析 Gallery 页面类型。 */
[[nodiscard]] std::optional<ZzExampleGalleryPage::ZzPageKind>
zzGalleryPageKind(const ZzPureTools::ZzRouteId &routeId)
{
    if (routeId.value() == QStringLiteral("home")) {
        return ZzExampleGalleryPage::ZzPageKind::Home;
    }
    if (routeId.value() == QStringLiteral("controls")) {
        return ZzExampleGalleryPage::ZzPageKind::Controls;
    }
    return std::nullopt;
}

/** @brief 创建 Gallery View、导航 Presenter 和空展示模型。 */
[[nodiscard]] ZzCore::ZzResult<std::unique_ptr<ZzPureTools::ZzPageInstance>>
zzCreateGalleryPage(
    ZzExampleGalleryPage::ZzPageKind kind,
    const QString &title,
    QWidget *pageParent)
{
    auto *window = qobject_cast<ZzPureTools::ZzApplicationWindow *>(
        pageParent->window());
    if (window == nullptr || window->navigationController() == nullptr) {
        return ZzCore::ZzResult<std::unique_ptr<
            ZzPureTools::ZzPageInstance>>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral(
                "gallery page requires an application window navigation controller")));
    }

    auto viewModel = std::make_unique<QObject>();
    viewModel->setProperty("title", title);
    auto view = std::make_unique<ZzExampleGalleryPage>(
        kind, title, pageParent);
    const QPointer<ZzPureTools::ZzApplicationWindow> guardedWindow(window);
    auto presenter = std::make_unique<ZzExampleNavigationPresenter>(
        view.get(),
        [guardedWindow](const ZzPureTools::ZzRouteId &routeId) {
            if (guardedWindow.isNull()) {
                qWarning().noquote()
                    << "ZzPureToolsExample navigation ignored: window destroyed";
                return;
            }
            auto *navigation = guardedWindow->navigationController();
            if (navigation == nullptr) {
                qWarning().noquote()
                    << "ZzPureToolsExample navigation ignored:"
                    << "controller unavailable";
                return;
            }
            const auto result = navigation->navigate(routeId);
            if (!result) {
                qWarning().noquote()
                    << "ZzPureToolsExample gallery navigation failed:"
                    << result.error().technicalMessage()
                    << result.error().context();
            }
        });

    QWidget *const viewObserver = view.release();
    return ZzPureTools::ZzPageInstance::create(
        pageParent,
        viewObserver,
        std::move(viewModel),
        std::move(presenter));
}

} // namespace

ZzCore::ZzResult<std::unique_ptr<ZzPureTools::ZzPageInstance>>
ZzExamplePageFactory::createPlaceholder(
    const ZzPureTools::ZzRouteId &routeId,
    QString title,
    const std::shared_ptr<ZzExampleApplicationContext> &context,
    QWidget *pageParent)
{
    if (!routeId.isValid() || title.trimmed().isEmpty()
        || !context || pageParent == nullptr) {
        return ZzCore::ZzResult<std::unique_ptr<
            ZzPureTools::ZzPageInstance>>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("placeholder page requires route, title, context and parent")));
    }

    title = title.trimmed();
    if (const auto kind = zzGalleryPageKind(routeId); kind.has_value()) {
        return zzCreateGalleryPage(*kind, title, pageParent);
    }

    const QString platform = context->platformName();
    auto viewModel = std::make_unique<QObject>();
    viewModel->setProperty("routeId", routeId.value());
    viewModel->setProperty("title", title);
    viewModel->setProperty("platform", platform);

    auto presenter = std::make_unique<QObject>();
    presenter->setProperty("contextReady", true);

    auto view = std::make_unique<QWidget>(pageParent);
    view->setObjectName(
        QStringLiteral("zzExamplePage_%1").arg(routeId.value()));
    view->setAccessibleName(title);
    auto *layout = new QVBoxLayout(view.get());
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(title, view.get());
    titleLabel->setObjectName(QStringLiteral("zzExamplePageTitle"));
    titleLabel->setAccessibleName(title);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 6.0);
    titleFont.setWeight(QFont::DemiBold);
    titleLabel->setFont(titleFont);

    auto *stateLabel = new QLabel(
        QStringLiteral("%1 | %2").arg(routeId.value(), platform),
        view.get());
    stateLabel->setObjectName(QStringLiteral("zzExamplePageState"));
    stateLabel->setTextInteractionFlags(
        Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);

    layout->addWidget(titleLabel);
    layout->addWidget(stateLabel);
    layout->addStretch(1);

    QWidget *const viewObserver = view.release();
    return ZzPureTools::ZzPageInstance::create(
        pageParent,
        viewObserver,
        std::move(viewModel),
        std::move(presenter));
}

} // namespace ZzExample
