#include <memory>
#include <type_traits>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzCore/ZzResult.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzWorkspaceActivityId.h>
#include <ZzPureTools/ZzWorkspaceWindowConfiguration.h>
#include <ZzPureTools/ZzWorkspaceWindowCreateOptions.h>
#include <ZzPureTools/ZzWorkspaceWindowFactory.h>
#include <ZzPureTools/ZzWorkspaceWindowHandle.h>
#include <ZzPureTools/ZzWorkspaceWindowCoordinator.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>

namespace {

[[nodiscard]] ZzFluentUI::ZzIconDescriptor zzTestIcon()
{
    return ZzFluentUI::ZzIconDescriptor::fromFontIcon(
        ZzFluentUI::ZzFontIcon::PuzzlePiece);
}

[[nodiscard]] ZzCore::ZzResult<std::unique_ptr<QWidget>> zzFactoryFailure()
{
    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::failure(
        ZzCore::ZzError(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral("intentional factory failure")));
}

[[nodiscard]] ZzPureTools::ZzPageRegistration zzHandleTestPage()
{
    ZzPureTools::ZzPageRegistration page;
    page.routeId = ZzPureTools::ZzRouteId(QStringLiteral("handle"));
    page.lifetime = ZzPureTools::ZzPageLifetimePolicy::WhileActive;
    page.factory = [](QWidget *parent)
        -> ZzCore::ZzResult<std::unique_ptr<ZzPureTools::ZzPageInstance>> {
        return ZzPureTools::ZzPageInstance::create(
            parent,
            new QWidget(parent),
            std::make_unique<QObject>(),
            std::make_unique<QObject>());
    };
    return page;
}

[[nodiscard]] ZzPureTools::ZzPureApplication &zzApplication()
{
    auto *const application =
        qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
    Q_ASSERT(application != nullptr);
    return *application;
}

} // namespace

class ZzWorkspacePublicApiTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultsWorkspaceWindowConfiguration()
    {
        const ZzPureTools::ZzWorkspaceWindowConfiguration configuration;

        QVERIFY(configuration.title.isEmpty());
        QVERIFY(configuration.icon.isNull());
        QCOMPARE(configuration.titleMode,
                 ZzPureTools::ZzWorkspaceTitleMode::Application);
        QCOMPARE(configuration.closePolicy,
                 ZzPureTools::ZzWindowClosePolicy::Allow);
        QVERIFY(!configuration.alwaysOnTop);
        QVERIFY(configuration.minimumSize.isEmpty());
        QVERIFY(configuration.maximumSize.isEmpty());
        QVERIFY(configuration.initialGeometry.isEmpty());
    }

    void keepsWorkspaceWindowConfigurationPatchFieldValues()
    {
        ZzPureTools::ZzWorkspaceWindowConfigurationPatch patch;
        const QIcon icon = QIcon::fromTheme(QStringLiteral("document-new"));

        patch.title = QStringLiteral("New workspace");
        patch.icon = icon;
        patch.titleMode = ZzPureTools::ZzWorkspaceTitleMode::Custom;
        patch.closePolicy = ZzPureTools::ZzWindowClosePolicy::Delegate;
        patch.alwaysOnTop = true;
        patch.minimumSize = QSize(320, 240);
        patch.maximumSize = QSize(1920, 1080);
        patch.initialGeometry = QRect(20, 30, 1280, 720);

        QCOMPARE(*patch.title, QStringLiteral("New workspace"));
        QCOMPARE(*patch.icon, icon);
        QCOMPARE(*patch.titleMode,
                 ZzPureTools::ZzWorkspaceTitleMode::Custom);
        QCOMPARE(*patch.closePolicy,
                 ZzPureTools::ZzWindowClosePolicy::Delegate);
        QVERIFY(*patch.alwaysOnTop);
        QCOMPARE(*patch.minimumSize, QSize(320, 240));
        QCOMPARE(*patch.maximumSize, QSize(1920, 1080));
        QCOMPARE(*patch.initialGeometry, QRect(20, 30, 1280, 720));
    }

    void exposesWorkspaceWindowCreateDefaults()
    {
        const ZzPureTools::ZzWorkspaceWindowCreateOptions options;

        QCOMPARE(options.configurationSource,
                 ZzPureTools::ZzWorkspaceConfigurationSource::
                     CoordinatorDefaults);
        QVERIFY(options.sourceWindow.isNull());
        QVERIFY(!options.configuration.title.has_value());
        QCOMPARE(options.visibility,
                 ZzPureTools::ZzApplicationWindowVisibility::Visible);
        QVERIFY(options.activate);
    }

    void acceptsWorkspaceWindowCreationEnumValues()
    {
        ZzPureTools::ZzWorkspaceWindowCreateOptions options;

        options.configurationSource =
            ZzPureTools::ZzWorkspaceConfigurationSource::SourceWindow;
        options.visibility =
            ZzPureTools::ZzApplicationWindowVisibility::Deferred;
        QCOMPARE(options.configurationSource,
                 ZzPureTools::ZzWorkspaceConfigurationSource::SourceWindow);
        QCOMPARE(options.visibility,
                 ZzPureTools::ZzApplicationWindowVisibility::Deferred);

        options.configurationSource =
            ZzPureTools::ZzWorkspaceConfigurationSource::Explicit;
        QCOMPARE(options.configurationSource,
                 ZzPureTools::ZzWorkspaceConfigurationSource::Explicit);
    }

    void observesWorkspaceWindowHandleLifetime()
    {
        ZzPureTools::ZzWorkspaceWindowHandle handle;
        QVERIFY(!handle.isValid());

        auto &application = zzApplication();
        std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell;
        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(builder.addPage(zzHandleTestPage()));
        QVERIFY(builder.addNavigationNode({
            ZzPureTools::ZzRouteId(QStringLiteral("handle")),
            QStringLiteral("ZzWorkspacePublicApiTest"),
            QStringLiteral("Handle"),
            {}}));
        QVERIFY(builder.setInitialRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("handle"))));
        QVERIFY(builder.setWindowSetupCallback(
            [&shell](ZzPureTools::ZzApplicationWindow &window) {
                auto created = ZzPureTools::ZzWorkspaceShell::create(&window, nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
                if (!created) {
                    return ZzCore::ZzResult<void>::failure(created.error());
                }
                shell = std::move(created).value();
                window.setCentralWidget(shell->workspaceWidget());
                return ZzCore::ZzResult<void>::success();
            }));
        const auto buildResult = builder.build(application);
        QVERIFY2(buildResult,
            buildResult ? "" : qPrintable(buildResult.error().technicalMessage()));

        ZzPureTools::ZzApplicationWindow *window = nullptr;
        for (QWidget *widget : application.topLevelWidgets()) {
            window = qobject_cast<ZzPureTools::ZzApplicationWindow *>(widget);
            if (window != nullptr) {
                break;
            }
        }
        QVERIFY(window != nullptr);
        QVERIFY(shell != nullptr);
        handle.window = window;
        handle.shell = shell.get();
        QVERIFY(handle.isValid());

        shell.reset();
        QVERIFY(!handle.isValid());

        application.beginShutdown();
        QVERIFY(!handle.isValid());
        QVERIFY(handle.window.isNull());
        QVERIFY(handle.shell.isNull());
    }

    void keepsWorkspaceWindowValueTypesCopyableAndMovable()
    {
        ZzPureTools::ZzWorkspaceWindowCreateOptions original;
        original.configuration.title = QStringLiteral("Copied workspace");
        original.visibility =
            ZzPureTools::ZzApplicationWindowVisibility::Deferred;
        original.activate = false;

        const auto copied = original;
        auto moved = std::move(original);

        QCOMPARE(copied.configuration.title.value(),
                 QStringLiteral("Copied workspace"));
        QCOMPARE(moved.configuration.title.value(),
                 QStringLiteral("Copied workspace"));
        QCOMPARE(moved.visibility,
                 ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(!moved.activate);
    }

    void registersWorkspaceWindowMetatypes()
    {
        QVERIFY(qMetaTypeId<ZzPureTools::ZzWindowClosePolicy>() > 0);
        QVERIFY(qMetaTypeId<ZzPureTools::ZzWorkspaceConfigurationSource>() > 0);
        QVERIFY(qMetaTypeId<ZzPureTools::ZzApplicationWindowVisibility>() > 0);
        QVERIFY(qMetaTypeId<ZzPureTools::ZzWorkspaceWindowConfiguration>() > 0);
        QVERIFY(qMetaTypeId<ZzPureTools::ZzWorkspaceWindowConfigurationPatch>()
                > 0);
        QVERIFY(qMetaTypeId<ZzPureTools::ZzWorkspaceWindowCreateOptions>() > 0);
        QVERIFY(qMetaTypeId<ZzPureTools::ZzWorkspaceWindowHandle>() > 0);
    }

    void exposesWorkspaceWindowFactoryContracts()
    {
        ZzPureTools::ZzWorkspaceWindowFactory factory =
            [](const ZzPureTools::ZzWorkspaceWindowCreateOptions &options) {
                ZzPureTools::ZzWorkspaceWindowHandle handle;
                handle.window = options.sourceWindow;
                return ZzCore::ZzResult<
                    ZzPureTools::ZzWorkspaceWindowHandle>::success(handle);
            };
        ZzPureTools::ZzWorkspacePageResolver resolver =
            [](QStringView route) {
                if (route.isEmpty()) {
                    return zzFactoryFailure();
                }
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            };

        const auto created = factory({});
        QVERIFY(created);
        QVERIFY(!created.value().isValid());

        auto page = resolver(QStringView(u"overview"));
        QVERIFY(page);
        QVERIFY(page.value() != nullptr);
    }

    void exposesTopologyPersistenceContracts()
    {
        using SaveResult = decltype(std::declval<
            const ZzPureTools::ZzWorkspaceWindowCoordinator &>()
                                        .saveTopology());
        using RestoreResult = decltype(std::declval<
            ZzPureTools::ZzWorkspaceWindowCoordinator &>()
                                          .restoreTopology(
                                              std::declval<const QByteArray &>(),
                                              std::declval<const ZzPureTools::
                                                               ZzWorkspacePageResolver &>()));
        static_assert(std::is_same_v<SaveResult,
                                     ZzCore::ZzResult<QByteArray>>);
        static_assert(std::is_same_v<RestoreResult,
                                     ZzCore::ZzResult<void>>);
        QVERIFY(true);
    }

    void exposesStableWorkspaceSurfaces()
    {
        QMainWindow host;
        ZzFluentUI::ZzFluentTitleBar titleBar(&host);
        auto created = ZzPureTools::ZzWorkspaceShell::create(
            &host, &titleBar, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
        QVERIFY(created);
        auto shell = std::move(created).value();

        QVERIFY(shell->workspaceWidget() != nullptr);
        QCOMPARE(shell->workspaceWidget()->parentWidget(), &host);
        QVERIFY(shell->splitWorkspace() != nullptr);
        QVERIFY(shell->tabWidget() != nullptr);
        QVERIFY(shell->bottomPane() != nullptr);
        QVERIFY(shell->commandPalette() != nullptr);
        QVERIFY(shell->activityBar(ZzFluentUI::ZzSidePaneEdge::Left)
                != nullptr);
        QVERIFY(shell->activityBar(ZzFluentUI::ZzSidePaneEdge::Right)
                != nullptr);
        QVERIFY(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                != nullptr);
        QVERIFY(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Right)
                != nullptr);

        auto *const leftBar = shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QCOMPARE(leftBar->model(), shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right)->model());
        QCOMPARE(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)->mode(),
                 ZzFluentUI::ZzSidePaneMode::Single);
    }

    void registersAndReturnsOwnedPanels()
    {
        QMainWindow host;
        auto created = ZzPureTools::ZzWorkspaceShell::create(&host, nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
        QVERIFY(created);
        auto shell = std::move(created).value();

        auto *const side = new QWidget;
        auto sideResult = shell->registerSidePanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("side")),
            QStringLiteral("Side"), zzTestIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side);
        QVERIFY(sideResult);
        QVERIFY(side->parentWidget() != nullptr);

        auto *const bottom = new QWidget;
        auto bottomResult = shell->registerBottomPanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("bottom")),
            QStringLiteral("Bottom"), zzTestIcon(), bottom);
        QVERIFY(bottomResult);
        QVERIFY(bottom->parentWidget() != nullptr);

        auto *const dock = new QWidget;
        auto dockResult = shell->registerDockPanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("dock")),
            QStringLiteral("Dock"), zzTestIcon(),
            Qt::RightDockWidgetArea, dock);
        QVERIFY(dockResult);
        QVERIFY(dock->parentWidget() != nullptr);

        auto invalid = shell->registerSidePanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("invalid")),
            QStringLiteral("Invalid"), zzTestIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side);
        QVERIFY(!invalid);
        QCOMPARE(invalid.error().code(), ZzCore::ZzErrorCode::InvalidState);

        auto takenSide = shell->takePanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("side")));
        QVERIFY(takenSide);
        QCOMPARE(takenSide.value(), side);
        QCOMPARE(side->parent(), nullptr);
        delete std::move(takenSide).value();

        auto takenBottom = shell->takePanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("bottom")));
        QVERIFY(takenBottom);
        QCOMPARE(takenBottom.value(), bottom);
        QCOMPARE(bottom->parent(), nullptr);
        delete std::move(takenBottom).value();

        auto takenDock = shell->takePanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("dock")));
        QVERIFY(takenDock);
        QCOMPARE(takenDock.value(), dock);
        QCOMPARE(dock->parent(), nullptr);
        delete std::move(takenDock).value();
    }

    void keepsFactoryLazyAndRetryable()
    {
        QMainWindow host;
        auto created = ZzPureTools::ZzWorkspaceShell::create(&host, nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
        QVERIFY(created);
        auto shell = std::move(created).value();

        int calls = 0;
        bool fail = true;
        const auto id = ZzPureTools::ZzWorkspacePanelId(
            QStringLiteral("deferred"));
        auto registered = shell->registerSidePanelFactory(
            id, QStringLiteral("Deferred"), zzTestIcon(),
            ZzFluentUI::ZzActivityArea::RightSecondary,
            [&calls, &fail] {
                ++calls;
                if (fail) {
                    return zzFactoryFailure();
                }
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            });
        QVERIFY(registered);
        QCOMPARE(calls, 0);

        auto firstShow = shell->showPanel(id);
        QVERIFY(!firstShow);
        QCOMPARE(firstShow.error().code(), ZzCore::ZzErrorCode::Backend);
        QCOMPARE(calls, 1);

        fail = false;
        auto secondShow = shell->showPanel(id);
        QVERIFY(secondShow);
        QCOMPARE(calls, 2);

        auto taken = shell->takePanel(id);
        QVERIFY(taken);
        QCOMPARE(taken.value()->parent(), nullptr);
        delete std::move(taken).value();
        QCOMPARE(calls, 2);
    }

    void roundTripsLayoutAndTitleState()
    {
        QMainWindow host;
        ZzFluentUI::ZzFluentTitleBar titleBar(&host);
        auto created = ZzPureTools::ZzWorkspaceShell::create(
            &host, &titleBar, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
        QVERIFY(created);
        auto shell = std::move(created).value();
        host.setCentralWidget(shell->workspaceWidget());

        shell->setApplicationTitle(QStringLiteral("Application"));
        shell->setCustomTitle(QStringLiteral("Custom"));
        shell->setTitleMode(ZzPureTools::ZzWorkspaceTitleMode::Custom);
        QCOMPARE(shell->applicationTitle(), QStringLiteral("Application"));
        QCOMPARE(shell->customTitle(), QStringLiteral("Custom"));
        QCOMPARE(shell->titleMode(), ZzPureTools::ZzWorkspaceTitleMode::Custom);

        const auto sideId = ZzPureTools::ZzWorkspacePanelId(
            QStringLiteral("layout-side"));
        auto *const side = new QWidget;
        QVERIFY(shell->registerSidePanel(
            sideId, QStringLiteral("Layout side"), zzTestIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side));
        QVERIFY(shell->showPanel(sideId));
        QVERIFY(!shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                     ->isCollapsed());

        auto *const tab = new QWidget;
        const int tabIndex = shell->tabWidget()->addTab(
            tab, QStringLiteral("Overview"));
        QVERIFY(tabIndex >= 0);
        shell->tabWidget()->setTabPinned(tabIndex, true);
        shell->tabWidget()->setTabModified(tabIndex, true);
        QVERIFY(shell->tabWidget()->isTabPinned(tabIndex));
        QVERIFY(shell->tabWidget()->isTabModified(tabIndex));

        auto saved = shell->saveLayout();
        QVERIFY(saved);
        QVERIFY(!saved.value().isEmpty());

        shell->setTitleMode(ZzPureTools::ZzWorkspaceTitleMode::Application);
        shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
            ->setCollapsed(true);
        auto restored = shell->restoreLayout(saved.value());
        QVERIFY(restored);
        QCOMPARE(shell->titleMode(), ZzPureTools::ZzWorkspaceTitleMode::Custom);
        QVERIFY(shell->tabWidget()->isTabPinned(0));
        QVERIFY(shell->tabWidget()->isTabModified(0));
        QVERIFY(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                    ->isCollapsed()
                == false);

        auto takenSide = shell->takePanel(sideId);
        QVERIFY(takenSide);
        QCOMPARE(takenSide.value(), side);
        QCOMPARE(side->parent(), nullptr);
        delete std::move(takenSide).value();

        auto invalid = shell->restoreLayout(QByteArrayLiteral("invalid"));
        QVERIFY(!invalid);
        QCOMPARE(invalid.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
    }
};

int main(int argc, char *argv[])
{
    ZzPureTools::ZzPureApplication application(argc, argv);
    ZzWorkspacePublicApiTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzWorkspacePublicApiTest.moc"
