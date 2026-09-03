#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtTest/QTest>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzErrorCode.h>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceWindowCoordinator.h>
#include <ZzPureTools/ZzWorkspaceWindowCreateOptions.h>

namespace {

[[nodiscard]] ZzPureTools::ZzPureApplication &zzApplication()
{
    auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
    Q_ASSERT(application != nullptr);
    return *application;
}

[[nodiscard]] ZzPureTools::ZzPageRegistration zzPage()
{
    ZzPureTools::ZzPageRegistration page;
    page.routeId = ZzPureTools::ZzRouteId(QStringLiteral("home"));
    page.lifetime = ZzPureTools::ZzPageLifetimePolicy::WhileActive;
    page.factory = [](QWidget *parent) -> ZzCore::ZzResult<std::unique_ptr<
        ZzPureTools::ZzPageInstance>> {
        return ZzPureTools::ZzPageInstance::create(
            parent,
            new QWidget(parent),
            std::make_unique<QObject>(),
            std::make_unique<QObject>());
    };
    return page;
}

[[nodiscard]] bool zzBuildApplication(
    ZzPureTools::ZzPureApplication &application)
{
    ZzPureTools::ZzApplicationBuilder builder;
    const ZzPureTools::ZzNavigationNode node{
        ZzPureTools::ZzRouteId(QStringLiteral("home")),
        QStringLiteral("ZzWorkspaceWindowCoordinatorTest"),
        QStringLiteral("Home"),
        {}};
    return builder.addPage(zzPage())
        && builder.addNavigationNode(node)
        && builder.setInitialRoute(ZzPureTools::ZzRouteId(QStringLiteral("home")))
        && builder.build(application);
}

[[nodiscard]] ZzPureTools::ZzApplicationWindow *zzOnlyWindow(
    ZzPureTools::ZzPureApplication &application)
{
    ZzPureTools::ZzApplicationWindow *result = nullptr;
    for (QWidget *widget : application.topLevelWidgets()) {
        auto *window = qobject_cast<ZzPureTools::ZzApplicationWindow *>(widget);
        if (window == nullptr) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = window;
    }
    return result;
}

[[nodiscard]] ZzCore::ZzResult<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>>
zzCreateShell(ZzPureTools::ZzApplicationWindow *window)
{
    return ZzPureTools::ZzWorkspaceShell::create(window);
}

[[nodiscard]] ZzPureTools::ZzWorkspaceWindowConfiguration zzConfiguration()
{
    ZzPureTools::ZzWorkspaceWindowConfiguration configuration;
    configuration.title = QStringLiteral("Workspace A");
    configuration.minimumSize = QSize(320, 240);
    configuration.maximumSize = QSize(1440, 900);
    configuration.initialGeometry = QRect(-20, -40, 1280, 720);
    return configuration;
}

} // namespace

/** @brief 验证工作区窗口登记、配置快照和生命周期边界。 */
class ZzWorkspaceWindowCoordinatorTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void registrationStoresConfigurationSnapshot()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator != nullptr);
        QCOMPARE(coordinator, application.workspaceWindowCoordinator());
        auto configuration = zzConfiguration();

        const auto registered = coordinator->registerWindow(
            {window, shell.get()}, configuration, true);
        configuration.title = QStringLiteral("mutated after registration");
        const auto stored = coordinator->configuration(window);

        QVERIFY(registered);
        QVERIFY(stored);
        QCOMPARE(stored.value().title, QStringLiteral("Workspace A"));
        QCOMPARE(stored.value().minimumSize, QSize(320, 240));
        QCOMPARE(stored.value().maximumSize, QSize(1440, 900));
        QCOMPARE(stored.value().initialGeometry, QRect(-20, -40, 1280, 720));
        application.beginShutdown();
    }

    void registrationValidatesConfigurationWithoutMutation()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator != nullptr);
        struct ZzInvalidConfigurationCase final
        {
            ZzPureTools::ZzWorkspaceWindowConfiguration configuration;
        };
        auto negativeSize = zzConfiguration();
        negativeSize.minimumSize = QSize(-1, 240);
        auto invertedBounds = zzConfiguration();
        invertedBounds.minimumSize = QSize(800, 600);
        invertedBounds.maximumSize = QSize(640, 480);
        auto negativeMaximumSize = zzConfiguration();
        negativeMaximumSize.maximumSize = QSize(1440, -1);
        auto negativeGeometry = zzConfiguration();
        negativeGeometry.initialGeometry = QRect(20, 40, -1, 720);
        auto invalidTitleMode = zzConfiguration();
        invalidTitleMode.titleMode =
            static_cast<ZzPureTools::ZzWorkspaceTitleMode>(42);
        auto invalidClosePolicy = zzConfiguration();
        invalidClosePolicy.closePolicy =
            static_cast<ZzPureTools::ZzWindowClosePolicy>(42);
        const std::vector<ZzInvalidConfigurationCase> invalidCases{
            {negativeSize},
            {invertedBounds},
            {negativeMaximumSize},
            {negativeGeometry},
            {invalidTitleMode},
            {invalidClosePolicy}};

        for (const auto &testCase : invalidCases) {
            const auto rejected = coordinator->registerWindow(
                {window, shell.get()}, testCase.configuration);
            const auto missing = coordinator->configuration(window);
            QVERIFY(!rejected);
            QCOMPARE(
                rejected.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
            QVERIFY(!missing);
            QCOMPARE(missing.error().code(), ZzCore::ZzErrorCode::NotFound);
        }

        const ZzPureTools::ZzWorkspaceWindowConfiguration defaults;
        const auto registered = coordinator->registerWindow(
            {window, shell.get()}, defaults);
        const auto stored = coordinator->configuration(window);

        QVERIFY(registered);
        QVERIFY(stored);
        QCOMPARE(stored.value().minimumSize, QSize());
        QCOMPARE(stored.value().maximumSize, QSize());
        QCOMPARE(stored.value().initialGeometry, QRect());
        application.beginShutdown();
    }

    void registrationRejectsDuplicateWindowAndShell()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        QVERIFY(first != nullptr);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondResult);
        auto *second = secondResult.value();
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(second);
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({first, firstShell.get()}, zzConfiguration()));

        const auto duplicateWindow = coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration());
        const auto duplicateShell = coordinator->registerWindow(
            {second, firstShell.get()}, zzConfiguration());

        QVERIFY(!duplicateWindow);
        QCOMPARE(duplicateWindow.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(!duplicateShell);
        QCOMPARE(duplicateShell.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(!coordinator->configuration(second));
        Q_UNUSED(secondShell);
        application.beginShutdown();
    }

    void registrationAllowsOnlyOnePrimaryWindow()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        QVERIFY(first != nullptr);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondResult);
        auto *second = secondResult.value();
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(second);
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration(), true));

        const auto rejected = coordinator->registerWindow(
            {second, secondShell.get()}, zzConfiguration(), true);
        const auto accepted = coordinator->registerWindow(
            {second, secondShell.get()}, zzConfiguration());

        QVERIFY(!rejected);
        QCOMPARE(rejected.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(accepted);
        application.beginShutdown();
    }

    void registrationRejectsWrongHostAndForeignThread()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        QVERIFY(first != nullptr);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondResult);
        auto *second = secondResult.value();
        auto foreignShellResult = zzCreateShell(first);
        QVERIFY(foreignShellResult);
        auto foreignShell = std::move(foreignShellResult).value();
        auto ownShellResult = zzCreateShell(second);
        QVERIFY(ownShellResult);
        auto ownShell = std::move(ownShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();

        const auto wrongHost = coordinator->registerWindow(
            {second, foreignShell.get()}, zzConfiguration());
        bool rejectedInWorker = false;
        ZzCore::ZzErrorCode workerCode = ZzCore::ZzErrorCode::None;
        std::thread worker([&] {
            const auto result = coordinator->registerWindow(
                {second, ownShell.get()}, zzConfiguration());
            rejectedInWorker = !result;
            if (!result) {
                workerCode = result.error().code();
            }
        });
        worker.join();

        QVERIFY(!wrongHost);
        QCOMPARE(wrongHost.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QVERIFY(rejectedInWorker);
        QCOMPARE(workerCode, ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(!coordinator->configuration(second));
        application.beginShutdown();
    }

    void destroyedWindowOrShellUnregistersItsRecord()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, zzConfiguration()));

        shell.reset();
        QCoreApplication::processEvents();
        const auto afterShellDestroyed = coordinator->configuration(window);

        QVERIFY(!afterShellDestroyed);
        QCOMPARE(afterShellDestroyed.error().code(), ZzCore::ZzErrorCode::NotFound);
        application.beginShutdown();
    }

    void destroyedWindowUnregistersItsRecordBeforeApplicationErase()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, zzConfiguration()));
        auto *destroyedIdentity = window;
        QPointer<ZzPureTools::ZzApplicationWindow> destroyedWindow(window);

        QVERIFY(window->close());
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 0; }));
        const auto afterWindowDestroyed = coordinator->configuration(
            destroyedIdentity);

        QVERIFY(destroyedWindow.isNull());
        QVERIFY(!afterWindowDestroyed);
        QCOMPARE(
            afterWindowDestroyed.error().code(),
            ZzCore::ZzErrorCode::NotFound);
        application.beginShutdown();
    }

    void unregisterOnlyRemovesCoordinatorState()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, zzConfiguration()));

        const auto unregistered = coordinator->unregisterWindow(window);
        const auto missing = coordinator->configuration(window);

        QVERIFY(unregistered);
        QVERIFY(window != nullptr);
        QCOMPARE(application.windowCount(), 1);
        QVERIFY(!missing);
        QCOMPARE(missing.error().code(), ZzCore::ZzErrorCode::NotFound);
        application.beginShutdown();
    }
};

int main(int argc, char *argv[])
{
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return 1;
    }
    ZzPureTools::ZzPureApplication application(argc, argv);
    ZzWorkspaceWindowCoordinatorTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzWorkspaceWindowCoordinatorTest.moc"
