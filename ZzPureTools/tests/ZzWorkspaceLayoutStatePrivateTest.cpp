#include "../widgets/src/private/ZzWorkspaceLayoutStatePrivate.h"

#include <QtTest/QTest>

#include <ZzFluentUI/ZzActivityArea.h>

namespace {

ZzPureTools::ZzWorkspaceLayoutStatePrivate::ZzWorkspaceSnapshot
zzTwoSideSnapshot()
{
    using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

    ZzLayoutState::ZzWorkspaceSnapshot snapshot;
    snapshot.leftSide.order = {QStringLiteral("explorer")};
    snapshot.leftSide.visible = {QStringLiteral("explorer")};
    snapshot.leftSide.current = QStringLiteral("explorer");
    snapshot.rightSide.order = {QStringLiteral("terminal")};
    snapshot.rightSide.visible = {QStringLiteral("terminal")};
    snapshot.rightSide.current = QStringLiteral("terminal");
    snapshot.activity.rightPrimary = {QStringLiteral("terminal")};
    snapshot.activity.leftCurrent = QStringLiteral("explorer");
    snapshot.activity.rightCurrent = QStringLiteral("terminal");
    return snapshot;
}

} // namespace

class ZzWorkspaceLayoutStatePrivateTest final : public QObject
{
    Q_OBJECT

private slots:
    void sideFallbackUsesPaneCurrentInsteadOfActivityCurrent()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        ZzLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.leftSide.order = {QStringLiteral("one"), QStringLiteral("two")};
        snapshot.leftSide.visible = {QStringLiteral("one"), QStringLiteral("two")};
        snapshot.leftSide.current = QStringLiteral("two");
        snapshot.activity.leftCurrent = QStringLiteral("one");

        ZzLayoutState::ZzLayoutRequest request;
        request.leftCurrent = QStringLiteral("unknown");

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, request);
        QVERIFY(target.has_value());
        QCOMPARE(target->leftSide.current, QStringLiteral("two"));
    }

    void moveTargetDoesNotChangeWhenObservedStateChanges()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        const auto snapshot = zzTwoSideSnapshot();
        const auto target = ZzLayoutState::buildActivityMoveTarget(
            snapshot, QStringLiteral("terminal"),
            ZzFluentUI::ZzActivityArea::RightPrimary, 0);
        QVERIFY(target.has_value());

        auto observed = snapshot;
        observed.rightSide.visible.clear();
        QCOMPARE(target->rightSide.visible, QStringList({QStringLiteral("terminal")}));
    }

    void emptySideForcesCollapse()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        ZzLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.leftSide.collapsed = false;
        snapshot.leftSide.current = QStringLiteral("stale");

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, {});
        QVERIFY(target.has_value());
        QVERIFY(target->leftSide.collapsed);
        QVERIFY(target->leftSide.current.isEmpty());
    }

    void unknownPanelKeepsSnapshotSideState()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        const auto snapshot = zzTwoSideSnapshot();
        ZzLayoutState::ZzLayoutRequest request;
        request.rightCurrent = QStringLiteral("unknown");

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, request);
        QVERIFY(target.has_value());
        QCOMPARE(target->rightSide.current, QStringLiteral("terminal"));
        QCOMPARE(target->rightSide.visible, QStringList({QStringLiteral("terminal")}));
    }

    void splitVisibleAndSizesAreAligned()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        ZzLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.split.visible = {QStringLiteral("left"), QStringLiteral("right")};
        snapshot.split.sizes = {480};

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, {});
        QVERIFY(target.has_value());
        QCOMPARE(target->split.sizes, QList<int>({480, 1}));
    }

    void activityCurrentIsDerivedFromSideTarget()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        const auto snapshot = zzTwoSideSnapshot();
        ZzLayoutState::ZzLayoutRequest request;
        request.leftCurrent = QStringLiteral("explorer");
        request.rightCurrent = QStringLiteral("terminal");

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, request);
        QVERIFY(target.has_value());
        QCOMPARE(target->activity.leftCurrent, target->leftSide.current);
        QCOMPARE(target->activity.rightCurrent, target->rightSide.current);
    }

    void invalidMoveReturnsNullopt()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        const auto snapshot = zzTwoSideSnapshot();
        QVERIFY(!ZzLayoutState::buildActivityMoveTarget(
            snapshot, QStringLiteral("unknown"),
            ZzFluentUI::ZzActivityArea::LeftPrimary, 0));
        QVERIFY(!ZzLayoutState::buildActivityMoveTarget(
            snapshot, QStringLiteral("terminal"),
            ZzFluentUI::ZzActivityArea::LeftPrimary, -1));
    }
};

QTEST_MAIN(ZzWorkspaceLayoutStatePrivateTest)

#include "ZzWorkspaceLayoutStatePrivateTest.moc"
