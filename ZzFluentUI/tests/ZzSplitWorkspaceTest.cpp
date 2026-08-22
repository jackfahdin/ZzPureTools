#include <QtCore/QAbstractAnimation>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QSplitter>

#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>

namespace {

bool zzFocusBelongsTo(
    QWidget *focused,
    const ZzFluentUI::ZzTabWidget *tabs)
{
    return focused != nullptr && tabs != nullptr
        && (focused == tabs || tabs->isAncestorOf(focused));
}

} // namespace

class ZzSplitWorkspaceTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void splitsFlattensAndKeepsOneLeaf()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        QCOMPARE(workspace.groupIds().size(), 1);
        const auto root = workspace.groupIds().constFirst();
        workspace.tabWidget(root)->addTab(
            new QWidget, QStringLiteral("Pinned"));
        const auto right = workspace.splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(right.has_value());
        const auto farRight = workspace.splitGroup(
            right.value(), Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(farRight.has_value());
        QCOMPARE(workspace.groupIds().size(), 3);
        QCOMPARE(workspace.findChildren<QSplitter *>().size(), 1);
        QVERIFY(workspace.removeEmptyGroup(right.value()));
        QCOMPARE(workspace.groupIds().size(), 2);
        QCOMPARE(workspace.findChildren<QSplitter *>().size(), 1);
        QVERIFY(!workspace.removeEmptyGroup(root));
    }

    void rejectsDuplicateRequestedIdWithoutSignals()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        const ZzFluentUI::ZzTabGroupId requested(
            QStringLiteral("requested-group"));
        QSignalSpy addedSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAdded);
        QSignalSpy layoutSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::layoutChanged);

        const auto added = workspace.splitGroup(
            root,
            Qt::Vertical,
            ZzFluentUI::ZzSplitPlacement::Before,
            requested);
        QCOMPARE(added, std::optional(requested));
        QCOMPARE(addedSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 1);

        const auto duplicate = workspace.splitGroup(
            root,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            requested);
        QVERIFY(!duplicate.has_value());
        QCOMPARE(workspace.groupIds().size(), 2);
        QCOMPARE(addedSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 1);
    }

    void rejectsInvalidSplitPlacementWithoutMutation()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        QSignalSpy addedSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAdded);
        QSignalSpy layoutSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::layoutChanged);

        const auto added = workspace.splitGroup(
            root,
            Qt::Horizontal,
            static_cast<ZzFluentUI::ZzSplitPlacement>(255));

        QVERIFY(!added.has_value());
        QCOMPARE(workspace.groupIds().size(), 1);
        QCOMPARE(addedSpy.size(), 0);
        QCOMPARE(layoutSpy.size(), 0);
    }

    void generatesCanonicalGroupIdAndKeepsOneTabWidgetPerLeaf()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        QVERIFY(root.isValid());
        QVERIFY(!root.value().startsWith(u'{'));
        QVERIFY(!root.value().endsWith(u'}'));
        QCOMPARE(root.value().size(), 36);

        const auto second = workspace.splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(second.has_value());
        QCOMPARE(workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(), 2);

        for (const auto &id : workspace.groupIds()) {
            auto *tabs = workspace.tabWidget(id);
            QVERIFY(tabs != nullptr);
            QCOMPARE(workspace.groupId(tabs), id);
            QAccessibleInterface *interface =
                QAccessible::queryAccessibleInterface(tabs->fluentTabBar());
            QVERIFY(interface != nullptr);
            QCOMPARE(interface->role(), QAccessible::PageTabList);
        }
    }

    void enforcesSixtyFourGroupLimit()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        for (int groupCount = 2; groupCount <= 64; ++groupCount) {
            QVERIFY(workspace.splitGroup(
                root,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After).has_value());
        }
        QCOMPARE(workspace.groupIds().size(), 64);
        QVERIFY(!workspace.splitGroup(
            root,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After).has_value());
        QCOMPARE(workspace.groupIds().size(), 64);
    }

    void enforcesSixteenLevelDepthLimit()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        auto source = workspace.groupIds().constFirst();
        for (int split = 1; split < 16; ++split) {
            const auto added = workspace.splitGroup(
                source,
                split % 2 == 0 ? Qt::Horizontal : Qt::Vertical,
                ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY2(added.has_value(), "the first fifteen nested splits must fit");
            source = added.value();
        }

        QVERIFY(!workspace.splitGroup(
            source,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After).has_value());
        QCOMPARE(workspace.groupIds().size(), 16);
    }

    void removesOnlyEmptyNonFinalGroupsAndEmitsAfterCommit()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        const auto empty = workspace.splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(empty.has_value());
        workspace.tabWidget(empty.value())->addTab(
            new QWidget, QStringLiteral("Occupied"));

        QSignalSpy removingSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAboutToBeRemoved);
        QSignalSpy layoutSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::layoutChanged);
        QVERIFY(!workspace.removeEmptyGroup(empty.value()));
        QCOMPARE(removingSpy.size(), 0);
        QCOMPARE(layoutSpy.size(), 0);

        delete workspace.tabWidget(empty.value())->widget(0);
        QVERIFY(workspace.removeEmptyGroup(empty.value()));
        QCOMPARE(removingSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 1);
        QVERIFY(workspace.tabWidget(empty.value()) == nullptr);
        QVERIFY(!workspace.removeEmptyGroup(root));
        QCOMPARE(removingSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 1);
    }

    void keepsActiveGroupIdempotentAndTracksFocus()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        const auto right = workspace.splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(right.has_value());
        QSignalSpy activeSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::activeGroupChanged);

        QCOMPARE(workspace.activeGroupId(), root);
        QVERIFY(workspace.setActiveGroup(root));
        QCOMPARE(activeSpy.size(), 0);
        QVERIFY(workspace.setActiveGroup(right.value()));
        QCOMPARE(workspace.activeGroupId(), right.value());
        QCOMPARE(activeSpy.size(), 1);
        QVERIFY(workspace.setActiveGroup(right.value()));
        QCOMPARE(activeSpy.size(), 1);
        QVERIFY(!workspace.setActiveGroup(
            ZzFluentUI::ZzTabGroupId(QStringLiteral("missing"))));
        QCOMPARE(activeSpy.size(), 1);

        workspace.resize(800, 400);
        workspace.show();
        workspace.tabWidget(root)->setFocus(Qt::OtherFocusReason);
        QTRY_COMPARE(workspace.activeGroupId(), root);
        QCOMPARE(activeSpy.size(), 2);
    }

    void focusesPhysicalAdjacentGroupInLtrAndRtl()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto left = workspace.groupIds().constFirst();
        const auto right = workspace.splitGroup(
            left, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(right.has_value());
        workspace.resize(800, 400);
        workspace.show();
        QCoreApplication::processEvents();

        QVERIFY(workspace.setActiveGroup(left));
        QVERIFY(workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), right.value());
        QVERIFY(zzFocusBelongsTo(
            QApplication::focusWidget(), workspace.tabWidget(right.value())));
        QWidget *const boundaryFocus = QApplication::focusWidget();
        QVERIFY(!workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), right.value());
        QCOMPARE(QApplication::focusWidget(), boundaryFocus);

        workspace.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        const auto ids = workspace.groupIds();
        QVERIFY(ids.size() == 2);
        ZzFluentUI::ZzTabGroupId physicalLeft;
        ZzFluentUI::ZzTabGroupId physicalRight;
        for (const auto &id : ids) {
            const int centerX = workspace.tabWidget(id)
                                    ->mapToGlobal(
                                        workspace.tabWidget(id)->rect().center())
                                    .x();
            if (!physicalLeft.isValid()
                || centerX < workspace.tabWidget(physicalLeft)
                                   ->mapToGlobal(
                                       workspace.tabWidget(physicalLeft)
                                           ->rect().center())
                                   .x()) {
                physicalRight = physicalLeft;
                physicalLeft = id;
            } else {
                physicalRight = id;
            }
        }
        QVERIFY(physicalLeft.isValid());
        QVERIFY(physicalRight.isValid());
        QVERIFY(workspace.setActiveGroup(physicalLeft));
        QVERIFY(workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), physicalRight);
        QVERIFY(zzFocusBelongsTo(
            QApplication::focusWidget(), workspace.tabWidget(physicalRight)));
    }

    void survivesOneThousandSplitRemoveCyclesWithoutObjectGrowth()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        const qsizetype timerCount = workspace.findChildren<QTimer *>().size();
        const qsizetype animationCount =
            workspace.findChildren<QAbstractAnimation *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            const auto added = workspace.splitGroup(
                root,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY(added.has_value());
            QVERIFY(workspace.removeEmptyGroup(added.value()));
        }

        QCOMPARE(workspace.groupIds().size(), 1);
        QCOMPARE(workspace.findChildren<QSplitter *>().size(), 0);
        QCOMPARE(workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(), 1);
        QCOMPARE(workspace.findChildren<QTimer *>().size(), timerCount);
        QCOMPARE(
            workspace.findChildren<QAbstractAnimation *>().size(),
            animationCount);
    }

    void signalHandlersMayDeleteWorkspace()
    {
        auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
        QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
        const auto root = rawWorkspace->groupIds().constFirst();
        connect(
            rawWorkspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAdded,
            rawWorkspace,
            [rawWorkspace] { delete rawWorkspace; });

        const auto added = rawWorkspace->splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(added.has_value());
        QVERIFY(workspace.isNull());
    }

    void splitterDestructionMayDeleteWorkspace()
    {
        auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
        QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
        const auto root = rawWorkspace->groupIds().constFirst();
        QVERIFY(rawWorkspace->splitGroup(
            root,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After).has_value());
        auto *splitter = rawWorkspace->findChild<QSplitter *>();
        QVERIFY(splitter != nullptr);
        connect(
            splitter,
            &QObject::destroyed,
            rawWorkspace,
            [rawWorkspace] { delete rawWorkspace; });

        const auto added = rawWorkspace->splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(added.has_value());
        QVERIFY(workspace.isNull());
    }

    void removalSignalHandlerMayDeleteWorkspace()
    {
        auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
        QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
        const auto root = rawWorkspace->groupIds().constFirst();
        const auto added = rawWorkspace->splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(added.has_value());
        connect(
            rawWorkspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAboutToBeRemoved,
            rawWorkspace,
            [rawWorkspace] { delete rawWorkspace; });

        QVERIFY(rawWorkspace->removeEmptyGroup(added.value()));
        QVERIFY(workspace.isNull());
    }

};

QTEST_MAIN(ZzSplitWorkspaceTest)

#include "ZzSplitWorkspaceTest.moc"
