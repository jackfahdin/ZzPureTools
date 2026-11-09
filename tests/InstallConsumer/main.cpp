#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzModuleDescriptor.h>
#include <ZzPureTools/ZzModuleGraphBuilder.h>
#include <ZzPureTools/ZzModuleId.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ZzPureTools::ZzPageHost host;
    if (host.transitionMode() != ZzPureTools::ZzPageHost::ZzTransitionMode::None
        || !host.setTransitionMode(ZzPureTools::ZzPageHost::ZzTransitionMode::Fade)
        || !host.setTransitionDuration(200)) return 2;
    ZzPureTools::ZzPageRegistration page;
    page.routeId = ZzPureTools::ZzRouteId(QStringLiteral("sdk-page"));
    page.factory = [](QWidget *parent) {
        return ZzPureTools::ZzPageInstance::create(parent, new QWidget(parent),
            std::make_unique<QObject>(), std::make_unique<QObject>());
    };
    if (!host.setRouteTransitionEnabled(page.routeId, false)
        || host.isRouteTransitionEnabled(page.routeId)
        || !host.activate(page, ZzPureTools::ZzPageHost::ZzTransitionDirection::Backward)) return 3;
    ZzPureTools::ZzModuleId moduleId(
        QStringLiteral("install.consumer"));
    ZzPureTools::ZzRouteId routeId(QStringLiteral("home"));
    ZzPureTools::ZzModuleDescriptor descriptor{
        moduleId,
        QStringLiteral("1.0.0"),
        {}};
    ZzPureTools::ZzNavigationNode navigationNode{
        routeId,
        QStringLiteral("ZzInstallConsumer"),
        QStringLiteral("Home"),
        {}};
    navigationNode.sectionTranslationContext =
        QStringLiteral("ZzInstallConsumer");
    navigationNode.sectionSourceText = QStringLiteral("Workspace");
    navigationNode.badgeText = QStringLiteral("3");
    navigationNode.placement =
        ZzFluentUI::ZzNavigationPlacement::Primary;
    ZzPureTools::ZzModuleGraphBuilder moduleGraphBuilder;
    ZzPureTools::ZzApplicationBuilder applicationBuilder;

    if (!moduleId.isValid() || !routeId.isValid()
        || descriptor.id != moduleId
        || navigationNode.routeId != routeId
        || navigationNode.sectionSourceText
            != QStringLiteral("Workspace")
        || navigationNode.badgeText != QStringLiteral("3")
        || moduleGraphBuilder.isFrozen()
        || applicationBuilder.isFrozen()) {
        return 1;
    }
    return 0;
}
