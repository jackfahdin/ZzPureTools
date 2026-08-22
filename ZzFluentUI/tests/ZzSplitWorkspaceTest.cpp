#include <QtCore/QAbstractAnimation>
#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QDrag>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QMouseEvent>
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

QList<ZzFluentUI::ZzTabGroupId> zzCreateFourHorizontalGroups(
    ZzFluentUI::ZzSplitWorkspace &workspace)
{
    const auto root = workspace.groupIds().constFirst();
    for (int addedCount = 0; addedCount < 3; ++addedCount) {
        if (!workspace.splitGroup(
                root,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After).has_value()) {
            return {};
        }
    }
    return workspace.groupIds();
}

void zzSetGroupCenter(
    ZzFluentUI::ZzSplitWorkspace &workspace,
    const ZzFluentUI::ZzTabGroupId &id,
    const QPoint &center)
{
    workspace.tabWidget(id)->setGeometry(
        center.x() - 20, center.y() - 20, 40, 40);
}

QList<QList<int>> zzSplitterSizes(const ZzFluentUI::ZzSplitWorkspace &workspace)
{
    QList<QList<int>> result;
    const auto splitters = workspace.findChildren<QSplitter *>();
    result.reserve(splitters.size());
    for (const QSplitter *splitter : splitters) {
        result.push_back(splitter->sizes());
    }
    return result;
}

} // namespace

class ZzSplitWorkspaceTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void transfersTabsAndRollsBackEdgeDrops()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        const auto targetId = workspace.splitGroup(
            sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(targetId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId)->addTab(page, QStringLiteral("Page"));

        QVERIFY(workspace.transferTab(sourceId, 0, targetId.value(), 0));
        QCOMPARE(workspace.tabWidget(targetId.value())->widget(0), page);

        const QList<ZzFluentUI::ZzTabGroupId> before = workspace.groupIds();
        const QList<QList<int>> beforeSizes = zzSplitterSizes(workspace);
        QWidget *const beforeParent = page->parentWidget();
        const int beforeIndex = workspace.tabWidget(targetId.value())->indexOf(page);
        QVERIFY(!workspace.moveTabToDropZone(
            targetId.value(),
            99,
            targetId.value(),
            ZzFluentUI::ZzWorkspaceDropZone::Left));
        QCOMPARE(workspace.groupIds(), before);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(page->parentWidget(), beforeParent);
        QCOMPARE(workspace.tabWidget(targetId.value())->indexOf(page), beforeIndex);
    }

    void movesTabsToAllFiveDropZones()
    {
        const QList<ZzFluentUI::ZzWorkspaceDropZone> edgeZones {
            ZzFluentUI::ZzWorkspaceDropZone::Left,
            ZzFluentUI::ZzWorkspaceDropZone::Top,
            ZzFluentUI::ZzWorkspaceDropZone::Right,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom,
        };
        for (const auto zone : edgeZones) {
            ZzFluentUI::ZzSplitWorkspace workspace;
            const auto targetId = workspace.groupIds().constFirst();
            const auto sourceId = workspace.splitGroup(
                targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY(sourceId.has_value());
            auto *page = new QWidget;
            workspace.tabWidget(sourceId.value())->addTab(
                page, QStringLiteral("Edge"));
            QSignalSpy committedSpy(
                &workspace,
                &ZzFluentUI::ZzSplitWorkspace::tabDropCommitted);
            QSignalSpy layoutSpy(
                &workspace,
                &ZzFluentUI::ZzSplitWorkspace::layoutChanged);
            layoutSpy.clear();

            QVERIFY(workspace.moveTabToDropZone(
                sourceId.value(), 0, targetId, zone));
            QCOMPARE(workspace.groupIds().size(), 2);
            QCOMPARE(committedSpy.size(), 1);
            QCOMPARE(layoutSpy.size(), 1);
            const auto destinationId = workspace.activeGroupId();
            QVERIFY(destinationId != targetId);
            QCOMPARE(workspace.tabWidget(destinationId)->widget(0), page);
            QVERIFY(workspace.tabWidget(sourceId.value()) == nullptr);

            const auto *splitter = workspace.findChild<QSplitter *>();
            QVERIFY(splitter != nullptr);
            const Qt::Orientation expectedOrientation =
                zone == ZzFluentUI::ZzWorkspaceDropZone::Left
                    || zone == ZzFluentUI::ZzWorkspaceDropZone::Right
                ? Qt::Horizontal
                : Qt::Vertical;
            QCOMPARE(splitter->orientation(), expectedOrientation);
        }

        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        const auto targetId = workspace.splitGroup(
            sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(targetId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId)->addTab(page, QStringLiteral("Center"));
        QSignalSpy committedSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::tabDropCommitted);
        QSignalSpy layoutSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::layoutChanged);
        layoutSpy.clear();
        QVERIFY(workspace.moveTabToDropZone(
            sourceId,
            0,
            targetId.value(),
            ZzFluentUI::ZzWorkspaceDropZone::Center));
        QCOMPARE(workspace.tabWidget(targetId.value())->widget(0), page);
        QCOMPARE(committedSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 0);
    }

    void mirrorsPhysicalLeftAndRightDropZonesInRtl()
    {
        for (const auto zone : {
                 ZzFluentUI::ZzWorkspaceDropZone::Left,
                 ZzFluentUI::ZzWorkspaceDropZone::Right}) {
            ZzFluentUI::ZzSplitWorkspace workspace;
            workspace.setLayoutDirection(Qt::RightToLeft);
            const auto targetId = workspace.groupIds().constFirst();
            auto *page = new QWidget;
            workspace.tabWidget(targetId)->addTab(page, QStringLiteral("RTL"));
            workspace.tabWidget(targetId)->addTab(
                new QWidget, QStringLiteral("Retained"));

            QVERIFY(workspace.moveTabToDropZone(
                targetId, 0, targetId, zone));
            const auto ids = workspace.groupIds();
            QCOMPARE(ids.size(), 2);
            const auto destinationId = workspace.activeGroupId();
            const qsizetype destinationIndex = ids.indexOf(destinationId);
            QCOMPARE(
                destinationIndex,
                zone == ZzFluentUI::ZzWorkspaceDropZone::Left ? 1 : 0);
            QCOMPARE(workspace.tabWidget(destinationId)->widget(0), page);
        }
    }

    void transferSignalsMayInvalidateParticipants()
    {
        enum class Action { DeleteSource, DeleteTarget, DeletePage, DeleteWorkspace };
        for (const auto action : {
                 Action::DeleteSource,
                 Action::DeleteTarget,
                 Action::DeletePage,
                 Action::DeleteWorkspace}) {
            auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
            QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
            const auto sourceId = rawWorkspace->groupIds().constFirst();
            const auto targetId = rawWorkspace->splitGroup(
                sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY(targetId.has_value());
            QPointer<ZzFluentUI::ZzTabWidget> source =
                rawWorkspace->tabWidget(sourceId);
            QPointer<ZzFluentUI::ZzTabWidget> target =
                rawWorkspace->tabWidget(targetId.value());
            QPointer<QWidget> page = new QWidget;
            source->addTab(page, QStringLiteral("Guarded"));
            connect(
                target,
                &ZzFluentUI::ZzTabWidget::tabTransferred,
                target,
                [&, action](ZzFluentUI::ZzTabWidget *, int, int, QWidget *) {
                    if (action == Action::DeleteSource) {
                        delete source.data();
                    } else if (action == Action::DeleteTarget) {
                        delete target.data();
                    } else if (action == Action::DeletePage) {
                        delete page.data();
                    } else {
                        delete workspace.data();
                    }
                });

            const bool transferred = rawWorkspace->transferTab(
                sourceId, 0, targetId.value(), 0);
            if (action == Action::DeleteWorkspace) {
                QVERIFY(workspace.isNull());
                QVERIFY(transferred);
            } else {
                QVERIFY(!workspace.isNull());
                QVERIFY(!transferred);
                delete workspace.data();
            }
        }
    }

    void doesNotTakeBackPageClaimedByThirdParty()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        ZzFluentUI::ZzTabWidget thirdParty;
        const auto sourceId = workspace.groupIds().constFirst();
        const auto targetId = workspace.splitGroup(
            sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(targetId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId)->addTab(page, QStringLiteral("Claimed"));
        connect(
            workspace.tabWidget(targetId.value()),
            &ZzFluentUI::ZzTabWidget::tabTransferred,
            &workspace,
            [&](ZzFluentUI::ZzTabWidget *, int, int targetIndex, QWidget *) {
                workspace.tabWidget(targetId.value())->transferTabTo(
                    &thirdParty, targetIndex);
            });

        QVERIFY(!workspace.transferTab(sourceId, 0, targetId.value(), 0));
        QCOMPARE(thirdParty.indexOf(page), 0);
        QCOMPARE(workspace.tabWidget(sourceId)->indexOf(page), -1);
        QCOMPARE(workspace.tabWidget(targetId.value())->indexOf(page), -1);
    }

    void edgeFailureRestoresTreeButLeavesThirdPartyPageAlone()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        ZzFluentUI::ZzTabWidget thirdParty;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId.value())->addTab(
            page, QStringLiteral("Claimed edge"));
        const auto beforeIds = workspace.groupIds();
        const auto beforeSizes = zzSplitterSizes(workspace);
        bool connectedTemporary = false;
        connect(
            workspace.tabWidget(sourceId.value()),
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (connectedTemporary) {
                    return;
                }
                for (const auto &id : workspace.groupIds()) {
                    if (beforeIds.contains(id)) {
                        continue;
                    }
                    connectedTemporary = true;
                    auto *temporary = workspace.tabWidget(id);
                    connect(
                        temporary,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&, temporary](
                            ZzFluentUI::ZzTabWidget *,
                            int,
                            int targetIndex,
                            QWidget *) {
                            temporary->transferTabTo(
                                &thirdParty, targetIndex);
                        });
                    return;
                }
            });

        QVERIFY(!workspace.moveTabToDropZone(
            sourceId.value(),
            0,
            targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom));
        QVERIFY(connectedTemporary);
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(thirdParty.indexOf(page), 0);
        QCOMPARE(workspace.tabWidget(sourceId.value())->indexOf(page), -1);
    }

    void edgeFailureRestoresPageWhenTemporaryTargetIsDestroyed()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId.value())->addTab(
            page, QStringLiteral("Rollback"));
        const auto beforeIds = workspace.groupIds();
        const auto beforeSizes = zzSplitterSizes(workspace);
        QWidget *const beforeParent = page->parentWidget();
        const int beforeIndex =
            workspace.tabWidget(sourceId.value())->indexOf(page);
        bool destroyedTemporary = false;
        connect(
            workspace.tabWidget(sourceId.value()),
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (destroyedTemporary) {
                    return;
                }
                for (const auto &id : workspace.groupIds()) {
                    if (!beforeIds.contains(id)) {
                        destroyedTemporary = true;
                        delete workspace.tabWidget(id);
                        return;
                    }
                }
            });

        QVERIFY(!workspace.moveTabToDropZone(
            sourceId.value(),
            0,
            targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Top));
        QVERIFY(destroyedTemporary);
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(page->parentWidget(), beforeParent);
        QCOMPARE(
            workspace.tabWidget(sourceId.value())->indexOf(page),
            beforeIndex);
    }

    void rejectsForgedDragTokensAndKeepsOverlayBudgetBounded()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        const auto targetId = workspace.splitGroup(
            sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(targetId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId)->addTab(page, QStringLiteral("Drag"));
        workspace.resize(800, 400);
        workspace.show();
        ZzFluentUI::ZzSplitWorkspace otherWorkspace;
        otherWorkspace.resize(400, 300);
        workspace.raise();
        workspace.activateWindow();
        QCoreApplication::processEvents();

        QMimeData forged;
        forged.setData(
            QByteArrayLiteral("application/x-zz-split-workspace-tab-v1"),
            QByteArrayLiteral("forged"));
        QDragEnterEvent forgedEnter(
            QPoint(10, 10), Qt::MoveAction, &forged,
            Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&workspace, &forgedEnter);
        QVERIFY(!forgedEnter.isAccepted());

        if (QApplication::platformName() == QStringLiteral("offscreen")) {
            for (qsizetype count = workspace.groupIds().size();
                 count < 12;
                 ++count) {
                QVERIFY(workspace.splitGroup(
                    targetId.value(),
                    Qt::Horizontal,
                    ZzFluentUI::ZzSplitPlacement::After).has_value());
            }
            QCOMPARE(
                workspace.findChildren<QWidget *>(
                    QStringLiteral("zzSplitWorkspaceDropOverlay")).size(),
                0);
            return;
        }

        bool sawRealDrag = false;
        bool acceptedEnter = false;
        bool acceptedMove = false;
        bool acceptedDrop = false;
        bool rejectedByOtherWorkspace = false;
        qsizetype overlayCount = -1;
        QTimer::singleShot(0, &workspace, [&] {
            auto *drag = workspace.findChild<QDrag *>();
            if (drag == nullptr) {
                QDrag::cancel();
                return;
            }
            sawRealDrag = true;
            const QPoint targetCenter = workspace.tabWidget(targetId.value())
                                            ->mapTo(
                                                &workspace,
                                                workspace.tabWidget(
                                                    targetId.value())
                                                    ->rect()
                                                    .center());
            QDragEnterEvent enter(
                targetCenter,
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&workspace, &enter);
            acceptedEnter = enter.isAccepted();

            QDragEnterEvent foreignEnter(
                otherWorkspace.rect().center(),
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&otherWorkspace, &foreignEnter);
            rejectedByOtherWorkspace = !foreignEnter.isAccepted();

            QDragMoveEvent move(
                targetCenter,
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&workspace, &move);
            acceptedMove = move.isAccepted();
            overlayCount = workspace.findChildren<QWidget *>(
                QStringLiteral("zzSplitWorkspaceDropOverlay")).size();

            QDropEvent drop(
                QPointF(targetCenter),
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&workspace, &drop);
            acceptedDrop = drop.isAccepted();
            QDrag::cancel();
        });
        auto *sourceBar = workspace.tabWidget(sourceId)->fluentTabBar();
        const QPoint pressPosition = sourceBar->tabRect(0).center();
        const QPoint movePosition = pressPosition
            + QPoint(QApplication::startDragDistance() + 2, 0);
        QTest::mousePress(
            sourceBar,
            Qt::LeftButton,
            Qt::NoModifier,
            pressPosition);
        QMouseEvent move(
            QEvent::MouseMove,
            QPointF(movePosition),
            QPointF(sourceBar->mapToGlobal(movePosition)),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QCoreApplication::sendEvent(sourceBar, &move);
        QTest::mouseRelease(
            sourceBar,
            Qt::LeftButton,
            Qt::NoModifier,
            movePosition);

        QVERIFY(sawRealDrag);
        QVERIFY(acceptedEnter);
        QVERIFY(rejectedByOtherWorkspace);
        QVERIFY(acceptedMove);
        QVERIFY(acceptedDrop);
        QCOMPARE(overlayCount, 1);
        QCOMPARE(workspace.tabWidget(targetId.value())->indexOf(page), 0);
        QCOMPARE(
            workspace.findChildren<QWidget *>(
                QStringLiteral("zzSplitWorkspaceDropOverlay")).size(),
            1);

        for (qsizetype count = workspace.groupIds().size();
             count < 12;
             ++count) {
            QVERIFY(workspace.splitGroup(
                targetId.value(),
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After).has_value());
        }
        QCOMPARE(
            workspace.findChildren<QWidget *>(
                QStringLiteral("zzSplitWorkspaceDropOverlay")).size(),
            1);
    }

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

    void adjacentFocusPrioritizesPrimaryAxisDistance()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto ids = zzCreateFourHorizontalGroups(workspace);
        QCOMPARE(ids.size(), 4);
        const auto active = ids.at(0);
        const auto fartherPrimary = ids.at(1);
        const auto nearestPrimary = ids.at(2);
        const auto bestSecondaryOnly = ids.at(3);
        QVERIFY(workspace.setActiveGroup(active));

        zzSetGroupCenter(workspace, active, QPoint(100, 400));
        zzSetGroupCenter(workspace, fartherPrimary, QPoint(400, 400));
        zzSetGroupCenter(workspace, nearestPrimary, QPoint(300, 750));
        zzSetGroupCenter(workspace, bestSecondaryOnly, QPoint(350, 400));

        QVERIFY(workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), nearestPrimary);
    }

    void adjacentFocusUsesSecondaryDistanceWhenPrimaryTies()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto ids = zzCreateFourHorizontalGroups(workspace);
        QCOMPARE(ids.size(), 4);
        const auto active = ids.at(0);
        const auto fartherSecondary = ids.at(1);
        const auto nearestSecondary = ids.at(2);
        const auto fartherPrimary = ids.at(3);
        QVERIFY(workspace.setActiveGroup(active));

        zzSetGroupCenter(workspace, active, QPoint(100, 400));
        zzSetGroupCenter(workspace, fartherSecondary, QPoint(300, 100));
        zzSetGroupCenter(workspace, nearestSecondary, QPoint(300, 350));
        zzSetGroupCenter(workspace, fartherPrimary, QPoint(350, 400));

        QVERIFY(workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), nearestSecondary);
    }

    void adjacentFocusUsesStableOrderWhenDistancesTie()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto ids = zzCreateFourHorizontalGroups(workspace);
        QCOMPARE(ids.size(), 4);
        const auto active = ids.at(0);
        const auto firstStableCandidate = ids.at(1);
        const auto secondStableCandidate = ids.at(2);
        const auto fartherPrimary = ids.at(3);
        QVERIFY(workspace.setActiveGroup(active));

        zzSetGroupCenter(workspace, active, QPoint(100, 400));
        zzSetGroupCenter(workspace, firstStableCandidate, QPoint(300, 350));
        zzSetGroupCenter(workspace, secondStableCandidate, QPoint(300, 350));
        zzSetGroupCenter(workspace, fartherPrimary, QPoint(350, 400));

        QVERIFY(workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), firstStableCandidate);
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
