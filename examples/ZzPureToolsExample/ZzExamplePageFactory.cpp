#include "ZzExamplePageFactory.h"

#include <utility>

#include <QtCore/QObject>
#include <QtGui/QFont>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include "ZzExampleApplicationContext.h"

namespace ZzExample {

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
