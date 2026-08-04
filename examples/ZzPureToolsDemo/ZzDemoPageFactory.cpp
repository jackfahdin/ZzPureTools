#include "ZzDemoPageFactory.h"

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

namespace {

/** @brief 创建一个异常安全且不访问业务服务的只读展示页面。 */
[[nodiscard]] ZzCore::ZzResult<std::unique_ptr<
    ZzPureTools::ZzPageInstance>> zzCreatePage(
    QWidget *pageParent,
    const char *titleSource,
    const char *stateSource)
{
    auto viewModel = std::make_unique<QObject>();
    auto presenter = std::make_unique<QObject>();
    auto view = std::make_unique<QWidget>(pageParent);
    auto *layout = new QVBoxLayout(view.get());
    auto *title = new QLabel(
        QCoreApplication::translate("ZzPureToolsDemo", titleSource),
        view.get());
    auto *state = new QLabel(
        QCoreApplication::translate("ZzPureToolsDemo", stateSource),
        view.get());
    title->setObjectName(QStringLiteral("zzDemoPageTitle"));
    state->setObjectName(QStringLiteral("zzDemoPageState"));
    state->setTextInteractionFlags(Qt::TextSelectableByKeyboard
                                   | Qt::TextSelectableByMouse);
    layout->addWidget(title);
    layout->addWidget(state);
    layout->addStretch(1);

    QWidget *const viewObserver = view.release();
    return ZzPureTools::ZzPageInstance::create(
        pageParent,
        viewObserver,
        std::move(viewModel),
        std::move(presenter));
}

} // namespace

ZzCore::ZzResult<std::unique_ptr<ZzPureTools::ZzPageInstance>>
ZzDemoPageFactory::createHome(QWidget *pageParent)
{
    return zzCreatePage(
        pageParent,
        "Home",
        "Ready for navigation");
}

ZzCore::ZzResult<std::unique_ptr<ZzPureTools::ZzPageInstance>>
ZzDemoPageFactory::createDetails(QWidget *pageParent)
{
    return zzCreatePage(
        pageParent,
        "Details",
        "Read-only presentation state");
}
