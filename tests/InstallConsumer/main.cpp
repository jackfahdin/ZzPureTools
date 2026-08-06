#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzModuleDescriptor.h>
#include <ZzPureTools/ZzModuleGraphBuilder.h>
#include <ZzPureTools/ZzModuleId.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzRouteId.h>

int main()
{
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
