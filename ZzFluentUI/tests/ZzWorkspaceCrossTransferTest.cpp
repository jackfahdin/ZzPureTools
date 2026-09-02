#include <QtTest/QtTest>

#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtWidgets/QApplication>

#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzWorkspacePageId.h>
#include <QtTest/QSignalSpy>

class ZzWorkspaceCrossTransferTest final : public QObject
{
    Q_OBJECT

private slots:
    void transfersAcrossPhysicalEdgeZones()
    {
        const ZzFluentUI::ZzWorkspaceDropZone zones[] = {
            ZzFluentUI::ZzWorkspaceDropZone::Left,
            ZzFluentUI::ZzWorkspaceDropZone::Top,
            ZzFluentUI::ZzWorkspaceDropZone::Right,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom};
        for (const auto zone : zones) {
            ZzFluentUI::ZzSplitWorkspace source;
            ZzFluentUI::ZzSplitWorkspace target;
            const auto sg = source.groupIds().constFirst();
            const auto tg = target.groupIds().constFirst();
            auto *page = new QWidget;
            source.tabWidget(sg)->addTab(page, QStringLiteral("edge"));
            QVERIFY(source.transferTabToWorkspace(sg, 0, &target, tg, -1, zone));
            bool found = false;
            for (const auto &id : target.groupIds())
                found = found || target.tabWidget(id)->indexOf(page) == 0;
            QVERIFY(found);
            QCOMPARE(target.groupIds().size(), 2);
        }
    }

    void mapsTabTearOffToWorkspaceSignal()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto group = workspace.groupIds().constFirst();
        auto *tabs = workspace.tabWidget(group);
        auto *page = new QWidget;
        tabs->addTab(page, QStringLiteral("tear"));
        QSignalSpy spy(&workspace, &ZzFluentUI::ZzSplitWorkspace::tabTearOffRequested);
        Q_EMIT tabs->tearOffRequested(0, page, QPoint(10, 20));
        QCOMPARE(spy.size(), 1);
        QVERIFY(spy.at(0).at(2).value<ZzFluentUI::ZzWorkspacePageId>().isValid());
        QVERIFY(spy.at(0).at(4).toSize().isValid());
    }

    void edgeTransferPreservesMetadataAndRtlGeometry()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        target.setLayoutDirection(Qt::RightToLeft);
        const auto sg = source.groupIds().constFirst();
        const auto tg = target.groupIds().constFirst();
        auto *page = new QWidget;
        source.tabWidget(sg)->addTab(page, QStringLiteral("meta"));
        source.tabWidget(sg)->setTabPinned(0, true);
        QVERIFY(source.setPageLayoutKey(page, QStringLiteral("edge/key")));
        const auto id = source.pageId(page);
        QVERIFY(source.transferTabToWorkspace(sg, 0, &target, tg, -1, ZzFluentUI::ZzWorkspaceDropZone::Right));
        QVERIFY(target.pageForId(id) == page);
        QCOMPARE(target.pageLayoutKey(page), QStringLiteral("edge/key"));
        QVERIFY(target.groupIds().size() == 2);
    }
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

    void rejectsInvalidArgumentsAndAuditsOnce()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        const auto sg = source.groupIds().constFirst();
        const auto tg = target.groupIds().constFirst();
        auto *page = new QWidget;
        source.tabWidget(sg)->addTab(page, QStringLiteral("Page"));
        QSignalSpy spy(&target, &ZzFluentUI::ZzSplitWorkspace::tabTransferCommitted);
        QVERIFY(!source.transferTabToWorkspace({}, 0, &target, tg));
        QVERIFY(!source.transferTabToWorkspace(sg, -1, &target, tg));
        QVERIFY(!source.transferTabToWorkspace(sg, 0, &target, {}, 0));
        QVERIFY(source.transferTabToWorkspace(sg, 0, &target, tg));
        QCOMPARE(spy.size(), 1);
    }

    void rollsBackBothSidesWhenCommitNotificationMutatesState()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        const auto sourceGroup = source.groupIds().constFirst();
        const auto targetGroup = target.groupIds().constFirst();
        auto *sourceTabs = source.tabWidget(sourceGroup);
        auto *targetTabs = target.tabWidget(targetGroup);
        auto *first = new QWidget;
        auto *moving = new QWidget;
        auto *targetExisting = new QWidget;
        sourceTabs->addTab(first, QStringLiteral("First"));
        sourceTabs->addTab(moving, QStringLiteral("Moving"));
        targetTabs->addTab(targetExisting, QStringLiteral("Existing"));
        sourceTabs->setTabToolTip(0, QStringLiteral("first-tip"));
        sourceTabs->setTabPinned(0, true);
        sourceTabs->setTabModified(1, true);
        targetTabs->setTabAttention(0, true);
        QVERIFY(source.setPageLayoutKey(first, QStringLiteral("source:first")));
        QVERIFY(source.setPageLayoutKey(moving, QStringLiteral("source:moving")));
        QVERIFY(target.setPageLayoutKey(targetExisting, QStringLiteral("target:existing")));
        sourceTabs->setCurrentWidget(first);
        targetTabs->setCurrentWidget(targetExisting);
        QVERIFY(source.setActiveGroup(sourceGroup));
        QVERIFY(target.setActiveGroup(targetGroup));

        const auto beforeSourcePages = QList<QWidget *> {first, moving};
        const auto beforeTargetPages = QList<QWidget *> {targetExisting};
        const auto movingId = source.pageId(moving);
        bool injected = false;
        connect(&target, &ZzFluentUI::ZzSplitWorkspace::tabTransferCommitted,
                &target, [&](ZzFluentUI::ZzSplitWorkspace *,
                             const ZzFluentUI::ZzTabGroupId &,
                             const ZzFluentUI::ZzTabGroupId &,
                             QWidget *, const ZzFluentUI::ZzWorkspacePageId &,
                             ZzFluentUI::ZzWorkspaceDropZone) {
                    if (injected) {
                        return;
                    }
                    injected = true;
                    sourceTabs->setTabText(sourceTabs->indexOf(first), QStringLiteral("mutated"));
                    sourceTabs->setCurrentWidget(moving);
                    targetTabs->setTabText(targetTabs->indexOf(targetExisting), QStringLiteral("mutated"));
                    targetTabs->setCurrentWidget(targetExisting);
                });

        const auto result = source.transferTabToWorkspace(
            sourceGroup, 1, &target, targetGroup);
        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(injected);
        QCOMPARE(sourceTabs->count(), 2);
        QCOMPARE(targetTabs->count(), 1);
        QCOMPARE(sourceTabs->widget(0), first);
        QCOMPARE(sourceTabs->widget(1), moving);
        QCOMPARE(targetTabs->widget(0), targetExisting);
        QCOMPARE(sourceTabs->currentWidget(), first);
        QCOMPARE(targetTabs->currentWidget(), targetExisting);
        QCOMPARE(sourceTabs->tabText(0), QStringLiteral("First"));
        QCOMPARE(sourceTabs->tabToolTip(0), QStringLiteral("first-tip"));
        QVERIFY(sourceTabs->isTabPinned(0));
        QVERIFY(sourceTabs->isTabModified(1));
        QVERIFY(targetTabs->hasTabAttention(0));
        QCOMPARE(source.pageLayoutKey(first), QStringLiteral("source:first"));
        QCOMPARE(source.pageLayoutKey(moving), QStringLiteral("source:moving"));
        QCOMPARE(target.pageLayoutKey(targetExisting), QStringLiteral("target:existing"));
        QCOMPARE(source.pageForId(movingId), moving);
        QVERIFY(target.pageForId(movingId) == nullptr);
        QCOMPARE(source.activeGroupId(), sourceGroup);
        QCOMPARE(target.activeGroupId(), targetGroup);
        const QList<QWidget *> afterSourcePages {
            sourceTabs->widget(0), sourceTabs->widget(1)};
        const QList<QWidget *> afterTargetPages {targetTabs->widget(0)};
        QCOMPARE(afterSourcePages, beforeSourcePages);
        QCOMPARE(afterTargetPages, beforeTargetPages);
    }

    void rejectsCurrentChangedDestructionWithoutDereference()
    {
        auto *source = new ZzFluentUI::ZzSplitWorkspace;
        auto *target = new ZzFluentUI::ZzSplitWorkspace;
        QPointer<ZzFluentUI::ZzSplitWorkspace> sourceGuard(source);
        QPointer<ZzFluentUI::ZzSplitWorkspace> targetGuard(target);
        const auto sourceGroup = source->groupIds().constFirst();
        const auto targetGroup = target->groupIds().constFirst();
        auto *sourceTabs = source->tabWidget(sourceGroup);
        auto *targetTabs = target->tabWidget(targetGroup);
        auto *moving = new QWidget;
        sourceTabs->addTab(moving, QStringLiteral("Moving"));
        sourceTabs->addTab(new QWidget, QStringLiteral("Retained"));
        bool destroyed = false;
        connect(sourceTabs, &QTabWidget::currentChanged, sourceTabs,
                [&, targetTabs](int) {
                    if (!destroyed) {
                        destroyed = true;
                        delete targetTabs;
                    }
                });

        const auto result = source->transferTabToWorkspace(
            sourceGroup, 0, target, targetGroup);
        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(destroyed);
        QVERIFY(sourceGuard);
        QVERIFY(targetGuard);
        QVERIFY(source->tabWidget(sourceGroup) != nullptr);
        QCOMPARE(source->tabWidget(sourceGroup)->indexOf(moving), 0);
        QVERIFY(target->tabWidget(targetGroup) == nullptr);
        delete source;
        delete target;
    }

    void leavesThirdPartyTakeoverUntouched()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        ZzFluentUI::ZzTabWidget thirdParty;
        const auto sourceGroup = source.groupIds().constFirst();
        const auto targetGroup = target.groupIds().constFirst();
        auto *sourceTabs = source.tabWidget(sourceGroup);
        auto *targetTabs = target.tabWidget(targetGroup);
        auto *moving = new QWidget;
        sourceTabs->addTab(moving, QStringLiteral("Moving"));
        sourceTabs->addTab(new QWidget, QStringLiteral("Retained"));
        bool claimed = false;
        connect(sourceTabs, &QTabWidget::currentChanged, sourceTabs,
                [&, moving](int) {
                    if (!claimed && sourceTabs->indexOf(moving) < 0) {
                        claimed = true;
                        thirdParty.addTab(moving, QStringLiteral("Claimed"));
                    }
                });

        const auto result = source.transferTabToWorkspace(
            sourceGroup, 0, &target, targetGroup);
        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(claimed);
        QCOMPARE(thirdParty.indexOf(moving), 0);
        QVERIFY(sourceTabs->indexOf(moving) < 0);
        QVERIFY(targetTabs->indexOf(moving) < 0);
    }

    void rejectsCallsFromAnotherThread()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        const auto sourceGroup = source.groupIds().constFirst();
        const auto targetGroup = target.groupIds().constFirst();
        source.tabWidget(sourceGroup)->addTab(new QWidget, QStringLiteral("Moving"));
        QThread worker;
        QObject invoker;
        invoker.moveToThread(&worker);
        worker.start();
        bool failed = false;
        QMetaObject::invokeMethod(&invoker, [&] {
            const auto result = source.transferTabToWorkspace(
                sourceGroup, 0, &target, targetGroup);
            failed = !result && result.error().code() == ZzCore::ZzErrorCode::InvalidArgument;
        }, Qt::BlockingQueuedConnection);
        worker.quit();
        worker.wait();
        QVERIFY(failed);
    }

    void delegatesTabWidgetCrossHostTransfer()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        const auto sourceGroup = source.groupIds().constFirst();
        const auto targetGroup = target.groupIds().constFirst();
        auto *page = new QWidget;
        auto *sourceTabs = source.tabWidget(sourceGroup);
        auto *targetTabs = target.tabWidget(targetGroup);
        sourceTabs->addTab(page, QStringLiteral("Delegated"));
        const auto id = source.pageId(page);
        QSignalSpy committed(&target,
            &ZzFluentUI::ZzSplitWorkspace::tabTransferCommitted);
        QVERIFY(sourceTabs->transferTabTo(targetTabs, 0));
        QCOMPARE(target.pageForId(id), page);
        QCOMPARE(committed.size(), 1);
    }

    void transfersToRequestedNonTailSlot()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        const auto sourceGroup = source.groupIds().constFirst();
        const auto targetGroup = target.groupIds().constFirst();
        auto *moving = new QWidget;
        auto *before = new QWidget;
        auto *after = new QWidget;
        source.tabWidget(sourceGroup)->addTab(moving, "Moving");
        auto *targetTabs = target.tabWidget(targetGroup);
        targetTabs->addTab(before, "Before");
        targetTabs->addTab(after, "After");
        const auto id = source.pageId(moving);
        QVERIFY(source.transferTabToWorkspace(sourceGroup, 0, &target,
                                               targetGroup, 1));
        QCOMPARE(targetTabs->widget(0), before);
        QCOMPARE(targetTabs->widget(1), moving);
        QCOMPARE(targetTabs->widget(2), after);
        QCOMPARE(target.pageForId(id), moving);
    }

    void thirdPartyTakeoverCleansWorkspaceRegistration()
    {
        ZzFluentUI::ZzSplitWorkspace source;
        ZzFluentUI::ZzSplitWorkspace target;
        ZzFluentUI::ZzTabWidget thirdParty;
        const auto sourceGroup = source.groupIds().constFirst();
        const auto targetGroup = target.groupIds().constFirst();
        auto *moving = new QWidget;
        auto *sourceTabs = source.tabWidget(sourceGroup);
        sourceTabs->addTab(moving, "Moving");
        QVERIFY(source.setPageLayoutKey(moving, "takeover"));
        const auto id = source.pageId(moving);
        connect(sourceTabs, &QTabWidget::currentChanged, sourceTabs,
                [&, moving](int) {
                    if (sourceTabs->indexOf(moving) < 0
                        && thirdParty.indexOf(moving) < 0)
                        thirdParty.addTab(moving, "Claimed");
                });
        const auto result = source.transferTabToWorkspace(sourceGroup, 0,
                                                           &target, targetGroup);
        QVERIFY(!result);
        QVERIFY(source.pageForId(id) == nullptr);
        QVERIFY(target.pageForId(id) == nullptr);
        QCOMPARE(source.pageLayoutKey(moving), QString());
        QCOMPARE(target.pageLayoutKey(moving), QString());
        delete moving;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

QTEST_MAIN(ZzWorkspaceCrossTransferTest)
#include "ZzWorkspaceCrossTransferTest.moc"
