#include <memory>

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
#include <ZzPureTools/ZzWorkspaceActivityId.h>
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

} // namespace

class ZzWorkspacePublicApiTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableWorkspaceSurfaces()
    {
        QMainWindow host;
        ZzFluentUI::ZzFluentTitleBar titleBar(&host);
        auto created = ZzPureTools::ZzWorkspaceShell::create(
            &host, &titleBar);
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
        auto created = ZzPureTools::ZzWorkspaceShell::create(&host);
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
        auto created = ZzPureTools::ZzWorkspaceShell::create(&host);
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
            &host, &titleBar);
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

QTEST_MAIN(ZzWorkspacePublicApiTest)
#include "ZzWorkspacePublicApiTest.moc"
