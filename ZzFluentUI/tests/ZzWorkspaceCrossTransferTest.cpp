#include <QtTest/QtTest>

#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzWorkspacePageId.h>

class ZzWorkspaceCrossTransferTest final : public QObject
{
    Q_OBJECT

private slots:
    void transfersPageAndIdentityAcrossWorkspaces()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        const auto sourceGroup = source.groupIds().constFirst();
        const auto targetGroup = target.groupIds().constFirst();
        auto *page = new QWidget;
        auto *sourceTabs = source.tabWidget(sourceGroup);
        QVERIFY(sourceTabs != nullptr);
        sourceTabs->addTab(page, QStringLiteral("Terminal"));
        sourceTabs->setPageTitle(0, QStringLiteral("Terminal"));
        sourceTabs->setTabPinned(0, true);
        QVERIFY(source.setPageLayoutKey(page, QStringLiteral("terminal/session-1")));

        const auto pageId = source.pageId(page);
        QVERIFY(pageId.isValid());
        const auto result = source.transferTabToWorkspace(
            sourceGroup,
            0,
            &target,
            targetGroup,
            -1,
            ZzFluentUI::ZzWorkspaceDropZone::Center);
        QVERIFY(result);
        QCOMPARE(target.pageForId(pageId), page);
        QCOMPARE(target.pageLayoutKey(page), QStringLiteral("terminal/session-1"));
        QVERIFY(!source.pageId(page).isValid());
        auto *targetTabs = target.tabWidget(targetGroup);
        QVERIFY(targetTabs != nullptr);
        QCOMPARE(targetTabs->widget(targetTabs->count() - 1), page);
        QVERIFY(targetTabs->isTabPinned(targetTabs->indexOf(page)));
    }

    void rejectsUnknownGroupsAndKeyConflicts()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        auto *page = new QWidget;
        source.tabWidget(source.groupIds().constFirst())->addTab(page, "Page");
        QVERIFY(source.setPageLayoutKey(page, QStringLiteral("same")));
        auto *other = new QWidget;
        target.tabWidget(target.groupIds().constFirst())->addTab(other, "Other");
        QVERIFY(target.setPageLayoutKey(other, QStringLiteral("same")));
        const auto result = source.transferTabToWorkspace(
            source.groupIds().constFirst(), 0, &target,
            target.groupIds().constFirst());
        QVERIFY(!result);
        QCOMPARE(source.pageForId(source.pageId(page)), page);
        QCOMPARE(target.pageLayoutKey(other), QStringLiteral("same"));
    }

    void preservesAllTabMetadataAndUpdatesActivity()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        const auto sourceGroup = source.groupIds().constFirst();
        const auto targetGroup = target.groupIds().constFirst();
        auto *page = new QWidget;
        auto *sourceTabs = source.tabWidget(sourceGroup);
        sourceTabs->addTab(page, QStringLiteral("Original"));
        sourceTabs->setTabToolTip(0, QStringLiteral("tip"));
        sourceTabs->setTabWhatsThis(0, QStringLiteral("what"));
        sourceTabs->setTabEnabled(0, false);
        sourceTabs->fluentTabBar()->setTabData(0, QStringLiteral("data"));
        sourceTabs->setTabPinned(0, true);
        sourceTabs->setTabModified(0, true);
        sourceTabs->setTabAttention(0, true);
        sourceTabs->setTabCloseEnabled(0, false);
        const auto id = source.pageId(page);
        QVERIFY(source.transferTabToWorkspace(sourceGroup, 0, &target, targetGroup));
        auto *targetTabs = target.tabWidget(targetGroup);
        const int index = targetTabs->indexOf(page);
        QCOMPARE(targetTabs->tabText(index), QStringLiteral("Original"));
        QCOMPARE(targetTabs->tabToolTip(index), QStringLiteral("tip"));
        QCOMPARE(targetTabs->tabWhatsThis(index), QStringLiteral("what"));
        QVERIFY(!targetTabs->isTabEnabled(index));
        QCOMPARE(targetTabs->fluentTabBar()->tabData(index).toString(), QStringLiteral("data"));
        QVERIFY(targetTabs->isTabPinned(index));
        QVERIFY(targetTabs->isTabModified(index));
        QVERIFY(targetTabs->hasTabAttention(index));
        QVERIFY(!targetTabs->isTabCloseEnabled(index));
        QCOMPARE(target.activeGroupId(), targetGroup);
        QCOMPARE(target.pageForId(id), page);
    }
};

QTEST_MAIN(ZzWorkspaceCrossTransferTest)
#include "ZzWorkspaceCrossTransferTest.moc"
