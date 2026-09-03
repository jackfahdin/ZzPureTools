#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtGui/QPixmap>
#include <QtTest/QTest>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzErrorCode.h>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

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
#include <ZzPureTools/ZzWorkspaceWindowFactory.h>

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

    void createWindowRequiresFactoryAndAppliesConfiguration()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *source = zzOnlyWindow(application);
        QVERIFY(source != nullptr);
        auto shellResult = zzCreateShell(source);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({source, shell.get()}, zzConfiguration(), true));
        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> createdShells;

        const auto missingFactory = coordinator->createWindow();
        QVERIFY(!missingFactory);
        QCOMPARE(missingFactory.error().code(), ZzCore::ZzErrorCode::InvalidState);

        coordinator->setWindowFactory([&application, &createdShells](const auto &) {
            auto result = application.createWindow(ZzPureTools::ZzApplicationWindowVisibility::Deferred);
            if (!result) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(result.error());
            }
            auto *window = result.value();
            auto createdShell = ZzPureTools::ZzWorkspaceShell::create(window);
            if (!createdShell) {
                window->close();
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(createdShell.error());
            }
            auto newShell = std::move(createdShell).value();
            auto *workspace = newShell->workspaceWidget();
            window->setCentralWidget(workspace);
            auto *shellObserver = newShell.get();
            createdShells.push_back(std::move(newShell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success({window, shellObserver});
        });
        ZzPureTools::ZzWorkspaceWindowCreateOptions options;
        options.configurationSource = ZzPureTools::ZzWorkspaceConfigurationSource::SourceWindow;
        options.sourceWindow = source;
        options.visibility = ZzPureTools::ZzApplicationWindowVisibility::Deferred;
        options.configuration.title = QStringLiteral("Child");
        options.configuration.minimumSize = QSize(500, 400);
        options.configuration.alwaysOnTop = true;
        const auto created = coordinator->createWindow(options);
        QVERIFY(created);
        QVERIFY(created.value().window != nullptr);
        QCOMPARE(created.value().window->windowTitle(), QStringLiteral("Child"));
        QVERIFY(!created.value().window->isVisible());
        QVERIFY(coordinator->configuration(created.value().window));
        const auto createdConfiguration = coordinator->configuration(created.value().window);
        QCOMPARE(createdConfiguration.value().minimumSize, QSize(500, 400));
        QVERIFY(createdConfiguration.value().alwaysOnTop);
        source->setWindowTitle(QStringLiteral("Mutated source"));
        QCOMPARE(created.value().window->windowTitle(), QStringLiteral("Child"));
        createdShells.clear();
        shell.reset();
        application.beginShutdown();
    }

    void registeredWorkspaceSignalTearsOffTheSamePageTransactionally()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *source = zzOnlyWindow(application);
        QVERIFY(source != nullptr);
        auto shellResult = zzCreateShell(source);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        source->setCentralWidget(shell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({source, shell.get()}, zzConfiguration(), true));
        auto *workspace = shell->splitWorkspace();
        const auto sourceGroup = workspace->groupIds().constFirst();
        auto *const page = new QWidget;
        workspace->tabWidget(sourceGroup)->addTab(page, QStringLiteral("Tear off"));

        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> createdShells;
        QPointer<ZzPureTools::ZzApplicationWindow> targetWindow;
        ZzPureTools::ZzApplicationWindowVisibility factoryVisibility =
            ZzPureTools::ZzApplicationWindowVisibility::Visible;
        coordinator->setWindowFactory(
            [&application, &createdShells, &targetWindow, &factoryVisibility](
                const ZzPureTools::ZzWorkspaceWindowCreateOptions &options) {
                factoryVisibility = options.visibility;
                auto created = application.createWindow(options.visibility);
                if (!created) {
                    return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                        created.error());
                }
                targetWindow = created.value();
                auto targetShellResult = ZzPureTools::ZzWorkspaceShell::create(
                    targetWindow.data());
                if (!targetShellResult) {
                    return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                        targetShellResult.error());
                }
                auto targetShell = std::move(targetShellResult).value();
                targetWindow->setCentralWidget(targetShell->workspaceWidget());
                auto *shellObserver = targetShell.get();
                createdShells.push_back(std::move(targetShell));
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                    {targetWindow, shellObserver});
            });

        Q_EMIT workspace->tabTearOffRequested(
            sourceGroup, 0, workspace->pageId(page), QPoint(100, 100), QSize(420, 300));

        QCOMPARE(factoryVisibility, ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(targetWindow->isVisible());
        QCOMPARE(workspace->tabWidget(sourceGroup)->indexOf(page), -1);
        QCOMPARE(createdShells.front()->splitWorkspace()->tabWidget(
            createdShells.front()->splitWorkspace()->groupIds().constFirst())->indexOf(page), 0);
        QVERIFY(createdShells.front()->splitWorkspace()->isAncestorOf(page));
        const auto targetConfiguration = coordinator->configuration(targetWindow.data());
        QVERIFY(targetConfiguration);
        QCOMPARE(targetConfiguration.value().title, QStringLiteral("Workspace A"));

        const auto failed = coordinator->tearOff(workspace, sourceGroup, 9);
        QVERIFY(!failed);
        QCOMPARE(failed.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(workspace->tabWidget(sourceGroup)->indexOf(page), -1);
        createdShells.clear();
        shell.reset();
        application.beginShutdown();
    }

    void factoryAndTransferFailuresLeaveSourceAndCoordinatorUnchanged()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *source = zzOnlyWindow(application);
        QVERIFY(source != nullptr);
        auto shellResult = zzCreateShell(source);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        source->setCentralWidget(shell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({source, shell.get()}, zzConfiguration(), true));
        auto *workspace = shell->splitWorkspace();
        const auto sourceGroup = workspace->groupIds().constFirst();
        auto *const page = new QWidget;
        workspace->tabWidget(sourceGroup)->addTab(page, QStringLiteral("Source"));

        coordinator->setWindowFactory([](const auto &) {
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success({});
        });
        const auto invalidHandle = coordinator->createWindow();
        QVERIFY(!invalidHandle);
        QCOMPARE(invalidHandle.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(application.windowCount(), 1);
        QCOMPARE(workspace->tabWidget(sourceGroup)->indexOf(page), 0);

        coordinator->setWindowFactory([](const auto &) {
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                ZzCore::ZzError(ZzCore::ZzErrorCode::Unknown,
                    QStringLiteral("factory failure")));
        });
        const auto factoryFailure = coordinator->createWindow();
        QVERIFY(!factoryFailure);
        QCOMPARE(factoryFailure.error().code(), ZzCore::ZzErrorCode::Unknown);
        QCOMPARE(application.windowCount(), 1);
        QCOMPARE(workspace->tabWidget(sourceGroup)->indexOf(page), 0);

        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> createdShells;
        QPointer<ZzPureTools::ZzApplicationWindow> stagedWindow;
        coordinator->setWindowFactory([&application, &createdShells, &stagedWindow](const auto &) {
            auto created = application.createWindow(ZzPureTools::ZzApplicationWindowVisibility::Deferred);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(created.error());
            }
            stagedWindow = created.value();
            auto createdShell = ZzPureTools::ZzWorkspaceShell::create(stagedWindow.data());
            if (!createdShell) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(createdShell.error());
            }
            auto targetShell = std::move(createdShell).value();
            stagedWindow->setCentralWidget(targetShell->workspaceWidget());
            auto *shellObserver = targetShell.get();
            createdShells.push_back(std::move(targetShell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                {stagedWindow, shellObserver});
        });
        const auto failedTransfer = coordinator->tearOff(workspace, sourceGroup, 3);
        QVERIFY(!failedTransfer);
        QCOMPARE(failedTransfer.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(workspace->tabWidget(sourceGroup)->indexOf(page), 0);
        QVERIFY(QTest::qWaitFor([&application] { return application.windowCount() == 1; }));
        QVERIFY(coordinator->configuration(source));
        createdShells.clear();
        shell.reset();
        application.beginShutdown();
    }

    void factoryExceptionReturnsUnknownWithoutCoordinatorMutation()
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
        coordinator->setWindowFactory([](const auto &) -> ZzCore::ZzResult<
            ZzPureTools::ZzWorkspaceWindowHandle> {
            throw std::runtime_error("factory exception");
        });

        const auto created = coordinator->createWindow();

        QVERIFY(!created);
        QCOMPARE(created.error().code(), ZzCore::ZzErrorCode::Unknown);
        QCOMPARE(application.windowCount(), 1);
        QVERIFY(coordinator->configuration(window));
        shell.reset();
        application.beginShutdown();
    }

    void tearOffRejectsForeignThreadAndShutdownBeforeStateAccess()
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
        auto *workspace = shell->splitWorkspace();
        const auto group = workspace->groupIds().constFirst();
        ZzCore::ZzErrorCode foreignCode = ZzCore::ZzErrorCode::None;
        bool foreignFailed = false;
        std::thread worker([&] {
            const auto result = coordinator->tearOff(workspace, group, 0);
            foreignFailed = !result;
            if (!result) {
                foreignCode = result.error().code();
            }
        });
        worker.join();
        QVERIFY(foreignFailed);
        QCOMPARE(foreignCode, ZzCore::ZzErrorCode::InvalidState);

        application.beginShutdown();
        const auto afterShutdown = coordinator->tearOff(nullptr, {}, 0);
        QVERIFY(!afterShutdown);
        QCOMPARE(afterShutdown.error().code(), ZzCore::ZzErrorCode::InvalidState);
    }

    void applyConfigurationPreflightsAlwaysOnTopBeforeChangingRealSurfaces()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QPixmap iconPixmap(1, 1);
        iconPixmap.fill(Qt::red);
        const QIcon iconBefore(iconPixmap);
        window->setWindowIcon(iconBefore);
        window->setMaximumSize(QSize(1200, 900));
        window->setMinimumSize(QSize(320, 240));
        window->setGeometry(QRect(40, 50, 640, 480));
        shell->setApplicationTitle(QStringLiteral("Before"));
        shell->setTitleMode(ZzPureTools::ZzWorkspaceTitleMode::Application);
        QVERIFY(shell->setAlwaysOnTop(false));
        ZzPureTools::ZzWorkspaceWindowConfiguration before;
        before.title = shell->applicationTitle();
        before.icon = window->windowIcon();
        before.titleMode = shell->titleMode();
        before.alwaysOnTop = shell->isAlwaysOnTop();
        before.minimumSize = window->minimumSize();
        before.maximumSize = window->maximumSize();
        before.initialGeometry = window->geometry();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, before));

        ZzPureTools::ZzWorkspaceWindowConfigurationPatch patch;
        QPixmap changedIconPixmap(1, 1);
        changedIconPixmap.fill(Qt::blue);
        patch.icon = QIcon(changedIconPixmap);
        patch.minimumSize = QSize(400, 300);
        patch.maximumSize = QSize(1000, 800);
        patch.initialGeometry = QRect(120, 130, 700, 500);
        patch.title = QStringLiteral("Changed");
        patch.titleMode = ZzPureTools::ZzWorkspaceTitleMode::Custom;
        patch.alwaysOnTop = true;
        bool callbackEntered = false;
        ZzCore::ZzErrorCode callbackError = ZzCore::ZzErrorCode::None;
        auto *tabs = shell->splitWorkspace()->tabWidget(
            shell->splitWorkspace()->activeGroupId());
        const QMetaObject::Connection connection = QObject::connect(
            tabs, &QTabWidget::currentChanged, window, [&](int) {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                const auto applied = coordinator->applyConfiguration(window, patch);
                QVERIFY(!applied);
                callbackError = applied.error().code();
            });

        const auto integrated = shell->integrateApplicationNavigation(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("navigation")),
            QStringLiteral("Navigation"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            QStringLiteral("Pages"));
        QObject::disconnect(connection);

        QVERIFY(callbackEntered);
        QCOMPARE(callbackError, ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(integrated);
        QCOMPARE(window->geometry(), before.initialGeometry);
        QCOMPARE(window->minimumSize(), before.minimumSize);
        QCOMPARE(window->maximumSize(), before.maximumSize);
        QCOMPARE(window->windowIcon().cacheKey(), before.icon.cacheKey());
        QCOMPARE(shell->applicationTitle(), before.title);
        QCOMPARE(shell->titleMode(), before.titleMode);
        QCOMPARE(shell->isAlwaysOnTop(), before.alwaysOnTop);
        const auto stored = coordinator->configuration(window);
        QVERIFY(stored);
        QCOMPARE(stored.value().initialGeometry, before.initialGeometry);
        QCOMPARE(stored.value().minimumSize, before.minimumSize);
        QCOMPARE(stored.value().maximumSize, before.maximumSize);
        QCOMPARE(stored.value().icon.cacheKey(), before.icon.cacheKey());
        QCOMPARE(stored.value().title, before.title);
        QCOMPARE(stored.value().titleMode, before.titleMode);
        QCOMPARE(stored.value().alwaysOnTop, before.alwaysOnTop);
        shell.reset();
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
