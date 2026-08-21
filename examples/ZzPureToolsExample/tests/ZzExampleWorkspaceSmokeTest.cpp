#include <memory>
#include <vector>

#include <QtCore/QAbstractItemModel>
#include <QtGui/QStandardItemModel>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QListView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>

#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>

#include "ZzExampleActivityModel.h"
#include "ZzExampleSessionModel.h"
#include "ZzExampleWorkspaceContent.h"

namespace {

[[nodiscard]] ZzPureTools::ZzWorkspacePanelId zzPanelId(
    const char *value)
{
    return ZzPureTools::ZzWorkspacePanelId(QString::fromLatin1(value));
}

[[nodiscard]] ZzFluentUI::ZzIconDescriptor zzIcon()
{
    return ZzFluentUI::ZzIconDescriptor{};
}

[[nodiscard]] QListView *zzPrimaryActivityView(
    ZzFluentUI::ZzActivityBar *bar)
{
    return bar->findChild<QListView *>(
        QStringLiteral("zzActivityPrimaryView"));
}

} // namespace

class ZzExampleWorkspaceSmokeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publicWorkspaceRunsSshStyleScenario()
    {
        QMainWindow host;
        host.resize(1100, 720);
        auto shellResult = ZzPureTools::ZzWorkspaceShell::create(&host);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        host.setCentralWidget(shell->workspaceWidget());

        ZzExample::ZzExampleSessionModel sessions;
        ZzExample::ZzExampleActivityModel activities;
        shell->commandPalette()->setModel(sessions.commandModel());

        auto sessionPanel =
            ZzExample::ZzExampleWorkspaceContent::createSessionPanel(
                &sessions);
        QVERIFY(sessionPanel != nullptr);
        QWidget *const sessionPanelRaw = sessionPanel.get();
        QVERIFY(shell->registerSidePanel(
            zzPanelId("sessions"), QStringLiteral("会话"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            sessionPanel.get()));
        [[maybe_unused]] QWidget *const adoptedSessionPanel =
            sessionPanel.release();

        auto terminal =
            ZzExample::ZzExampleWorkspaceContent::createTerminalPage(
                QStringLiteral("本地终端"));
        QVERIFY(terminal != nullptr);
        QWidget *const terminalRaw = terminal.get();
        QCOMPARE(
            shell->tabWidget()->addTab(
                terminal.release(), QStringLiteral("本地终端")),
            0);

        struct ZzDockFixture final
        {
            const char *id;
            QString title;
            std::unique_ptr<QWidget> content;
        };
        std::vector<ZzDockFixture> docks;
        docks.emplace_back(ZzDockFixture{
            "sftp", QStringLiteral("SFTP"),
            ZzExample::ZzExampleWorkspaceContent::createSftpPanel()});
        docks.emplace_back(ZzDockFixture{
            "log", QStringLiteral("日志"),
            ZzExample::ZzExampleWorkspaceContent::createActivityLogPanel(
                &activities)});
        docks.emplace_back(ZzDockFixture{
            "properties", QStringLiteral("属性"),
            ZzExample::ZzExampleWorkspaceContent::createPropertiesPanel()});
        docks.emplace_back(ZzDockFixture{
            "tasks", QStringLiteral("任务"),
            ZzExample::ZzExampleWorkspaceContent::createTasksPanel()});
        for (auto &dock : docks) {
            QVERIFY(dock.content != nullptr);
            QVERIFY(shell->registerDockPanel(
                zzPanelId(dock.id), dock.title, zzIcon(),
                Qt::BottomDockWidgetArea, dock.content.get()));
            [[maybe_unused]] QWidget *const adoptedDockContent =
                dock.content.release();
        }

        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        auto *leftBar = shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *activityView = zzPrimaryActivityView(leftBar);
        QVERIFY(activityView != nullptr);
        shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
            ->setCollapsed(true);
        QTest::mouseClick(
            activityView->viewport(), Qt::LeftButton, Qt::NoModifier,
            activityView->visualRect(
                activityView->model()->index(0, 0)).center());
        QCOMPARE(
            shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                ->currentWidget(),
            sessionPanelRaw);
        QVERIFY(!shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                     ->isCollapsed());

        QSignalSpy commandSpy(
            shell->commandPalette(),
            &ZzFluentUI::ZzCommandPalette::commandActivated);
        shell->commandPalette()->open();
        shell->commandPalette()->setQuery(QStringLiteral("新建终端"));
        QTest::keyClick(
            shell->commandPalette()->searchEdit(), Qt::Key_Return);
        QCOMPARE(commandSpy.count(), 1);
        QCOMPARE(
            sessions.commandId(
                commandSpy.first().at(0).value<QModelIndex>()),
            ZzExample::ZzExampleCommandId::NewTerminal);

        auto foreignModel = std::make_unique<QStandardItemModel>();
        auto *foreignItem = new QStandardItem(QStringLiteral("外来命令"));
        foreignItem->setData(
            static_cast<int>(ZzExample::ZzExampleCommandId::ShowTasks),
            Qt::UserRole + 0x520);
        foreignModel->appendRow(foreignItem);
        QVERIFY(
            sessions.commandId({})
            == ZzExample::ZzExampleCommandId::NewTerminal);
        QVERIFY(
            sessions.commandId(foreignModel->index(0, 0))
            == ZzExample::ZzExampleCommandId::NewTerminal);
        QVERIFY(
            sessions.commandId(sessions.commandModel()->index(0, 1))
            == ZzExample::ZzExampleCommandId::NewTerminal);

        auto secondTerminal =
            ZzExample::ZzExampleWorkspaceContent::createTerminalPage(
                QStringLiteral("测试终端"));
        const int secondIndex = shell->tabWidget()->addTab(
            secondTerminal.release(), QStringLiteral("测试终端"));
        QCOMPARE(shell->tabWidget()->count(), 2);
        QWidget *const closedPage = shell->tabWidget()->widget(secondIndex);
        shell->tabWidget()->removeTab(secondIndex);
        delete closedPage;
        QCOMPARE(shell->tabWidget()->count(), 1);

        auto *sftpDock = host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:sftp"));
        QVERIFY(sftpDock != nullptr);
        sftpDock->setFloating(true);
        QVERIFY(sftpDock->isFloating());

        shell->setApplicationTitle(QStringLiteral("ZzPureToolsExample"));
        shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);
        shell->tabWidget()->setPageTitle(
            terminalRaw, QStringLiteral("已连接"));
        QCOMPARE(
            host.windowTitle(),
            QStringLiteral("已连接 - ZzPureToolsExample"));

        shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
            ->setPaneWidth(360);
        auto saved = shell->saveLayout();
        QVERIFY(saved);
        shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
            ->setPaneWidth(220);
        sftpDock->setFloating(false);
        QVERIFY(shell->restoreLayout(saved.value()));
        QCOMPARE(
            shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                ->paneWidth(),
            360);
        QVERIFY(sftpDock->isFloating());

        auto missingPanel = shell->showPanel(
            zzPanelId("not-registered"));
        QVERIFY(!missingPanel);
    }
};

QTEST_MAIN(ZzExampleWorkspaceSmokeTest)
#include "ZzExampleWorkspaceSmokeTest.moc"
