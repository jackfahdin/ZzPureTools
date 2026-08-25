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
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
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

        auto filesPanel =
            ZzExample::ZzExampleWorkspaceContent::createSftpPanel();
        QVERIFY(filesPanel != nullptr);
        QWidget *const filesPanelRaw = filesPanel.get();
        QVERIFY(shell->registerSidePanel(
            zzPanelId("files"), QStringLiteral("文件"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            filesPanel.get()));
        [[maybe_unused]] QWidget *const adoptedFilesPanel =
            filesPanel.release();

        auto propertiesPanel =
            ZzExample::ZzExampleWorkspaceContent::createPropertiesPanel();
        QVERIFY(propertiesPanel != nullptr);
        QWidget *const propertiesPanelRaw = propertiesPanel.get();
        QVERIFY(shell->registerSidePanel(
            zzPanelId("properties"), QStringLiteral("属性"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            propertiesPanel.get()));
        [[maybe_unused]] QWidget *const adoptedPropertiesPanel =
            propertiesPanel.release();

        auto tasksPanel =
            ZzExample::ZzExampleWorkspaceContent::createTasksPanel();
        QVERIFY(tasksPanel != nullptr);
        QWidget *const tasksPanelRaw = tasksPanel.get();
        QVERIFY(shell->registerSidePanel(
            zzPanelId("tasks"), QStringLiteral("任务"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightSecondary,
            tasksPanel.get()));
        [[maybe_unused]] QWidget *const adoptedTasksPanel =
            tasksPanel.release();

        auto terminalTool =
            ZzExample::ZzExampleWorkspaceContent::createTerminalPage(
                QStringLiteral("本地终端"));
        QVERIFY(terminalTool != nullptr);
        QWidget *const terminalToolRaw = terminalTool.get();
        QVERIFY(shell->registerBottomPanel(
            zzPanelId("terminal"), QStringLiteral("终端"), zzIcon(),
            terminalTool.get()));
        [[maybe_unused]] QWidget *const adoptedTerminalTool =
            terminalTool.release();

        auto problemsTool =
            ZzExample::ZzExampleWorkspaceContent::createProblemsPanel();
        QVERIFY(problemsTool != nullptr);
        QVERIFY(shell->registerBottomPanel(
            zzPanelId("problems"), QStringLiteral("问题"), zzIcon(),
            problemsTool.get()));
        [[maybe_unused]] QWidget *const adoptedProblemsTool =
            problemsTool.release();

        auto outputTool =
            ZzExample::ZzExampleWorkspaceContent::createOutputPanel(
                &activities);
        QVERIFY(outputTool != nullptr);
        QVERIFY(shell->registerBottomPanel(
            zzPanelId("output"), QStringLiteral("输出"), zzIcon(),
            outputTool.get()));
        [[maybe_unused]] QWidget *const adoptedOutputTool =
            outputTool.release();

        auto terminalPage =
            ZzExample::ZzExampleWorkspaceContent::createTerminalPage(
                QStringLiteral("编辑终端"));
        QVERIFY(terminalPage != nullptr);
        QWidget *const terminalPageRaw = terminalPage.get();
        QCOMPARE(shell->tabWidget()->addTab(
            terminalPage.release(), QStringLiteral("编辑终端")), 0);
        auto sftpPage =
            ZzExample::ZzExampleWorkspaceContent::createSftpPanel();
        QVERIFY(sftpPage != nullptr);
        QWidget *const sftpPageRaw = sftpPage.get();
        QCOMPARE(shell->tabWidget()->addTab(
            sftpPage.release(), QStringLiteral("SFTP")), 1);
        QVERIFY(shell->splitWorkspace()->setPageLayoutKey(
            terminalPageRaw, QStringLiteral("example-terminal")));
        QVERIFY(shell->splitWorkspace()->setPageLayoutKey(
            sftpPageRaw, QStringLiteral("example-sftp")));

        const auto firstGroup = shell->splitWorkspace()->groupIds().constFirst();
        QVERIFY(shell->splitWorkspace()->moveTabToDropZone(
            firstGroup, 0, firstGroup, ZzFluentUI::ZzWorkspaceDropZone::Right));
        QCOMPARE(shell->splitWorkspace()->groupIds().size(), 2);
        const auto terminalGroup = [&shell, &firstGroup, terminalPageRaw] {
            for (const auto &group : shell->splitWorkspace()->groupIds()) {
                auto *const tabs = shell->splitWorkspace()->tabWidget(group);
                if (tabs != nullptr && tabs->indexOf(terminalPageRaw) >= 0) {
                    return group;
                }
            }
            return decltype(firstGroup){};
        }();
        QVERIFY(terminalGroup.isValid());
        shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
            ->setPaneWidth(360);
        shell->bottomPane()->setPaneHeight(260);
        const auto savedLayout = shell->saveLayout();
        QVERIFY(savedLayout);
        shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
            ->setPaneWidth(220);
        shell->bottomPane()->setPaneHeight(180);
        QVERIFY(shell->restoreLayout(savedLayout.value()));
        QCOMPARE(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                     ->paneWidth(), 360);
        QCOMPARE(shell->bottomPane()->paneHeight(), 260);

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
        QCOMPARE(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                     ->visibleWidgets().size(), 2);
        QCOMPARE(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Right)
                     ->visibleWidgets().size(), 2);
        QCOMPARE(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                     ->visibleWidgets(),
            QList<QWidget *>({sessionPanelRaw, filesPanelRaw}));
        QCOMPARE(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Right)
                     ->visibleWidgets(),
            QList<QWidget *>({propertiesPanelRaw, tasksPanelRaw}));
        QTest::mouseClick(
            activityView->viewport(), Qt::LeftButton, Qt::NoModifier,
            activityView->visualRect(
                activityView->model()->index(0, 0)).center());
        QVERIFY(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                     ->isCollapsed());

        QVERIFY(shell->showPanel(zzPanelId("terminal")));
        QCOMPARE(shell->bottomPane()->currentWidget(), terminalToolRaw);
        QVERIFY(!shell->bottomPane()->isCollapsed());
        shell->bottomPane()->setCollapsed(true);
        QVERIFY(shell->bottomPane()->isCollapsed());

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

        shell->setApplicationTitle(QStringLiteral("ZzPureToolsExample"));
        shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);
        const auto otherGroup = terminalGroup == firstGroup
            ? shell->splitWorkspace()->groupIds().constLast() : firstGroup;
        QVERIFY(shell->splitWorkspace()->setActiveGroup(otherGroup));
        QVERIFY(shell->splitWorkspace()->setActiveGroup(terminalGroup));
        QCOMPARE(shell->tabWidget()->currentWidget(), terminalPageRaw);
        shell->tabWidget()->setPageTitle(
            terminalPageRaw, QStringLiteral("已连接"));
        QCOMPARE(
            host.windowTitle(),
            QStringLiteral("已连接 - ZzPureToolsExample"));

        auto missingPanel = shell->showPanel(
            zzPanelId("not-registered"));
        QVERIFY(!missingPanel);
    }
};

QTEST_MAIN(ZzExampleWorkspaceSmokeTest)
#include "ZzExampleWorkspaceSmokeTest.moc"
