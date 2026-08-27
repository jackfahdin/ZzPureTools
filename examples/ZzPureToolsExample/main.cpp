#include <cstdlib>
#include <memory>
#include <string_view>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#if defined(ZZ_EXAMPLE_PERFORMANCE_BENCHMARKS)
#include <QtCore/QElapsedTimer>
#endif
#include <QtCore/QLocale>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtGui/QFont>
#include <QtWidgets/QApplication>

#include <ZzCore/ZzError.h>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include "ZzExampleApplicationContext.h"
#include "ZzExampleApplicationModule.h"
#include "ZzExamplePageFactory.h"
#if defined(ZZ_EXAMPLE_PERFORMANCE_BENCHMARKS)
#include "ZzExamplePerformanceController.h"
#endif
#include "ZzExampleRouteCatalog.h"
#include "ZzExampleSmokeController.h"
#include "ZzExampleWindowShell.h"

namespace {

/** @brief 将路由表中的 UTF-8 常量转换为 Qt 字符串。 */
[[nodiscard]] QString zzFromUtf8(std::string_view text)
{
    return QString::fromUtf8(
        text.data(), static_cast<qsizetype>(text.size()));
}

/** @brief 向启动日志输出一个稳定技术错误。 */
void zzReportStartupFailure(
    const char *stage,
    const ZzCore::ZzError &error)
{
    qCritical().noquote()
        << stage << error.technicalMessage() << error.context();
}

} // namespace

int main(int argc, char *argv[])
{
#if defined(ZZ_EXAMPLE_PERFORMANCE_BENCHMARKS)
    QElapsedTimer processTimer;
    processTimer.start();
#endif
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        zzReportStartupFailure("WindowKit bootstrap failed:", bootstrap.error());
        return EXIT_FAILURE;
    }

    ZzPureTools::ZzPureApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Jackfahdin"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ZzPureToolsExample"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    bool timeoutValid = false;
    const int timeout = qEnvironmentVariableIntValue(
        "ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS", &timeoutValid);
    const bool autoCloseRequested = timeoutValid && timeout > 0;
    const bool commandLineSmokeRequested =
        QCoreApplication::arguments().contains(
            QStringLiteral("--smoke-test"));
    const bool smokeRequested =
        autoCloseRequested || commandLineSmokeRequested;
#if defined(ZZ_EXAMPLE_PERFORMANCE_BENCHMARKS)
    const bool performanceMode = !qEnvironmentVariable(
        "ZZ_PURETOOLS_EXAMPLE_PERFORMANCE_SCENARIO").trimmed().isEmpty();
    const bool smokeMode = smokeRequested && !performanceMode;
    const bool testMode = smokeMode || performanceMode;
#else
    const bool smokeMode = smokeRequested;
    const bool testMode = smokeMode;
#endif
    if (testMode) {
        QStandardPaths::setTestModeEnabled(true);
    }
    const bool screenshotMode = smokeMode && qEnvironmentVariable(
        "ZZ_PURETOOLS_EXAMPLE_SMOKE_SCENARIO")
                                    == QStringLiteral("screenshot");
    if (screenshotMode) {
        QLocale::setDefault(QLocale::c());
        QApplication::setLayoutDirection(Qt::LeftToRight);
        QApplication::setFont(QFont(QStringLiteral("DejaVu Sans"), 10));
    }

    auto contextResult =
        ZzExample::ZzExampleApplicationContext::create();
    if (!contextResult) {
        zzReportStartupFailure(
            "application context failed:", contextResult.error());
        return EXIT_FAILURE;
    }
    auto context = std::move(contextResult).value();
    auto smokeController = std::make_shared<
        ZzExample::ZzExampleSmokeController>(
        smokeMode, application, context);
#if defined(ZZ_EXAMPLE_PERFORMANCE_BENCHMARKS)
    auto performanceController = std::make_shared<
        ZzExample::ZzExamplePerformanceController>(
        application, processTimer);
#endif

    ZzPureTools::ZzApplicationBuilder builder;
    if (QLocale::system().language() == QLocale::English) {
        auto translatorResult = builder.addTranslatorResource(
            QStringLiteral(
                ":/translations/ZzPureToolsExample_en.qm"));
        if (!translatorResult) {
            zzReportStartupFailure(
                "translator registration failed:",
                translatorResult.error());
            return EXIT_FAILURE;
        }
    }
    auto moduleResult = builder.addModule(
        std::make_unique<ZzExample::ZzExampleApplicationModule>(context));
    if (!moduleResult) {
        zzReportStartupFailure("module registration failed:", moduleResult.error());
        return EXIT_FAILURE;
    }

    for (const auto &route : ZzExample::ZzExampleRouteCatalog::routes()) {
        const ZzPureTools::ZzRouteId routeId(
            zzFromUtf8(route.routeId));
        const QString title = zzFromUtf8(route.title);

        ZzPureTools::ZzPageRegistration registration;
        registration.routeId = routeId;
        registration.lifetime = route.lifetime;
        registration.factory =
            [context, routeId, title, &application](QWidget *pageParent) {
                return ZzExample::ZzExamplePageFactory::createPage(
                    routeId,
                    QCoreApplication::translate(
                        "ZzPureToolsExample",
                        title.toUtf8().constData()),
                    context,
                    application,
                    pageParent);
            };
        auto pageResult = builder.addPage(std::move(registration));
        if (!pageResult) {
            zzReportStartupFailure(
                "page registration failed:", pageResult.error());
            return EXIT_FAILURE;
        }

        ZzPureTools::ZzNavigationNode node{
            routeId,
            QStringLiteral("ZzPureToolsExample"),
            title,
            {}};
        if (!route.section.empty()) {
            node.sectionTranslationContext =
                QStringLiteral("ZzPureToolsExample");
            node.sectionSourceText = zzFromUtf8(route.section);
        }
        node.placement = route.placement;
        auto nodeResult = builder.addNavigationNode(std::move(node));
        if (!nodeResult) {
            zzReportStartupFailure(
                "navigation registration failed:", nodeResult.error());
            return EXIT_FAILURE;
        }
    }

    auto initialRouteResult = builder.setInitialRoute(
        ZzPureTools::ZzRouteId(QStringLiteral("home")));
    if (!initialRouteResult) {
        zzReportStartupFailure(
            "initial route failed:", initialRouteResult.error());
        return EXIT_FAILURE;
    }

    auto setupResult = builder.setWindowSetupCallback(
        [context, &application, smokeController
#if defined(ZZ_EXAMPLE_PERFORMANCE_BENCHMARKS)
         , performanceController
#endif
        ](
            ZzPureTools::ZzApplicationWindow &window) {
            bool closeGuardEnabled = smokeController->closeGuardEnabled();
#if defined(ZZ_EXAMPLE_PERFORMANCE_BENCHMARKS)
            closeGuardEnabled = closeGuardEnabled
                && !performanceController->isEnabled();
#endif
            auto shellResult = ZzExample::ZzExampleWindowShell::attach(
                window,
                context,
                application,
                closeGuardEnabled);
            if (!shellResult) {
                return shellResult;
            }
            smokeController->windowAttached(window);
#if defined(ZZ_EXAMPLE_PERFORMANCE_BENCHMARKS)
            performanceController->windowAttached(window);
#endif
            return ZzCore::ZzResult<void>::success();
        });
    if (!setupResult) {
        zzReportStartupFailure(
            "window setup registration failed:", setupResult.error());
        return EXIT_FAILURE;
    }

    auto buildResult = builder.build(application);
    if (!buildResult) {
        zzReportStartupFailure("application build failed:", buildResult.error());
        return EXIT_FAILURE;
    }

    if (qEnvironmentVariableIsSet(
            "ZZ_PURETOOLS_EXAMPLE_EXPECT_ENGLISH")
        && QCoreApplication::translate(
               "ZzPureToolsExample", "首页")
            != QStringLiteral("Home")) {
        qCritical().noquote()
            << "English example translation was not loaded";
        return EXIT_FAILURE;
    }

    if (smokeMode && autoCloseRequested) {
        QTimer::singleShot(
            timeout, &application, &QCoreApplication::quit);
    }
    return application.exec();
}
