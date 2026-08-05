#include <cstdlib>
#include <memory>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include "ZzDemoModule.h"
#include "ZzDemoPageFactory.h"

int main(int argc, char *argv[])
{
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return EXIT_FAILURE;
    }

    ZzPureTools::ZzPureApplication application(argc, argv);
    ZzPureTools::ZzApplicationBuilder builder;
    if (!builder.addModule(std::make_unique<ZzDemoModule>())) {
        return EXIT_FAILURE;
    }

    const ZzPureTools::ZzRouteId homeRoute(QStringLiteral("home"));
    const ZzPureTools::ZzRouteId detailsRoute(
        QStringLiteral("details"));

    ZzPureTools::ZzPageRegistration home;
    home.routeId = homeRoute;
    home.lifetime = ZzPureTools::ZzPageLifetimePolicy::Persistent;
    home.factory = &ZzDemoPageFactory::createHome;
    if (!builder.addPage(std::move(home))) {
        return EXIT_FAILURE;
    }

    ZzPureTools::ZzPageRegistration details;
    details.routeId = detailsRoute;
    details.lifetime = ZzPureTools::ZzPageLifetimePolicy::Recreatable;
    details.factory = &ZzDemoPageFactory::createDetails;
    if (!builder.addPage(std::move(details))) {
        return EXIT_FAILURE;
    }

    ZzPureTools::ZzNavigationNode homeNode{
        homeRoute,
        QStringLiteral("ZzPureToolsDemo"),
        QStringLiteral("Home"),
        {}};
    if (!builder.addNavigationNode(std::move(homeNode))) {
        return EXIT_FAILURE;
    }

    ZzPureTools::ZzNavigationNode detailsNode{
        detailsRoute,
        QStringLiteral("ZzPureToolsDemo"),
        QStringLiteral("Details"),
        {}};
    if (!builder.addNavigationNode(std::move(detailsNode))) {
        return EXIT_FAILURE;
    }

    if (!builder.setInitialRoute(homeRoute)
        || !builder.build(application)) {
        return EXIT_FAILURE;
    }

    const auto secondWindowResult = application.createWindow();
    if (!secondWindowResult) {
        application.beginShutdown();
        return EXIT_FAILURE;
    }

    bool timeoutValid = false;
    const int timeout = qEnvironmentVariableIntValue(
        "ZZ_PURETOOLS_DEMO_AUTO_CLOSE_MS",
        &timeoutValid);
    if (timeoutValid && timeout > 0) {
        QTimer::singleShot(
            timeout,
            &application,
            &QCoreApplication::quit);
    }

    return application.exec();
}
