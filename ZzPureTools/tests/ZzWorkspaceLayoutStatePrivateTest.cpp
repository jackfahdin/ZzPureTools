#include "../widgets/src/private/ZzWorkspaceLayoutStatePrivate.h"

#include <algorithm>
#include <optional>

#include <QtCore/QElapsedTimer>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzActivityArea.h>

namespace {

using ZzPlannerLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

struct ZzPlannerFixture final
{
    ZzPlannerLayoutState::ZzWorkspaceSnapshot snapshot;
    ZzPlannerLayoutState::ZzLayoutRequest request;
    QStringList expectedOrder;
    QList<int> expectedSizes;
};

struct ZzPlannerMeasurement final
{
    qint64 medianNanoseconds = 0;
    quint64 checksum = 0;
};

[[nodiscard]] ZzPlannerLayoutState::ZzPanelIdentity zzSideIdentity(
    const QString &id)
{
    ZzPlannerLayoutState::ZzPanelIdentity identity;
    identity.id = id;
    identity.kind = ZzPlannerLayoutState::ZzPanelKind::Side;
    return identity;
}

[[nodiscard]] ZzPlannerFixture zzAlternatingPlannerFixture(int count)
{
    ZzPlannerFixture fixture;
    fixture.snapshot.leftSide.order.reserve(count);
    fixture.snapshot.leftSide.visible.reserve(count);
    fixture.snapshot.leftSide.sizes.reserve(count);
    fixture.snapshot.activity.leftPrimary.reserve(count);
    fixture.snapshot.identities.reserve(count);
    fixture.expectedOrder.reserve(count);
    fixture.expectedSizes.reserve(count);

    for (int index = 0; index < count; ++index) {
        const QString id = QStringLiteral("side-%1")
            .arg(index, 4, 10, QLatin1Char('0'));
        fixture.snapshot.identities.append(zzSideIdentity(id));
        fixture.snapshot.leftSide.order.append(id);
        fixture.snapshot.leftSide.visible.append(id);
        fixture.snapshot.leftSide.sizes.append(100 + index);
        fixture.snapshot.activity.leftPrimary.append(id);
        fixture.expectedOrder.append(id);
        fixture.expectedSizes.append(index % 2 == 0
                ? 2000 + index : 100 + index);
    }

    fixture.snapshot.leftSide.current = fixture.snapshot.leftSide.order.value(0);
    fixture.request.projection = ZzPlannerLayoutState::ZzWorkspaceProjection{};
    auto &projection = *fixture.request.projection;
    for (int index = 0; index < count; index += 2) {
        const QString &id = fixture.snapshot.leftSide.order.at(index);
        projection.leftSide.order.append(id);
        projection.leftSide.visible.append(id);
        projection.leftSide.sizes.append(2000 + index);
        projection.activity.leftPrimary.append(id);
    }
    fixture.request.leftCurrent = fixture.snapshot.leftSide.order.value(0);
    return fixture;
}

[[nodiscard]] std::optional<ZzPlannerMeasurement> zzMeasurePlanner(
    const ZzPlannerFixture &fixture,
    int repetitions)
{
    QElapsedTimer timer;
    timer.start();
    quint64 checksum = 0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto target = ZzPlannerLayoutState::buildRestoreTarget(
            fixture.snapshot, fixture.request);
        if (!target.has_value()
            || target->leftSide.order.size() != fixture.expectedOrder.size()
            || target->leftSide.sizes.size() != fixture.expectedSizes.size()) {
            return std::nullopt;
        }
        checksum += static_cast<quint64>(target->leftSide.sizes.front());
        checksum += static_cast<quint64>(target->leftSide.sizes.back());
    }
    return ZzPlannerMeasurement{timer.nsecsElapsed(), checksum};
}

ZzPlannerLayoutState::ZzWorkspaceSnapshot zzTwoSideSnapshot()
{
    ZzPlannerLayoutState::ZzWorkspaceSnapshot snapshot;
    snapshot.leftSide.order = {QStringLiteral("explorer")};
    snapshot.leftSide.visible = {QStringLiteral("explorer")};
    snapshot.leftSide.sizes = {240};
    snapshot.leftSide.current = QStringLiteral("explorer");
    snapshot.rightSide.order = {QStringLiteral("terminal")};
    snapshot.rightSide.visible = {QStringLiteral("terminal")};
    snapshot.rightSide.sizes = {360};
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
    void alternatingOmissionsKeepStableAnchorsAndSizes()
    {
        ZzPlannerLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.leftSide.order = {QStringLiteral("a"), QStringLiteral("b"),
            QStringLiteral("c"), QStringLiteral("d"), QStringLiteral("e"),
            QStringLiteral("f"), QStringLiteral("g")};
        snapshot.leftSide.visible = snapshot.leftSide.order;
        snapshot.leftSide.sizes = {101, 102, 103, 104, 105, 106, 107};
        snapshot.leftSide.current = QStringLiteral("a");
        snapshot.activity.leftPrimary = snapshot.leftSide.order;
        for (const QString &id : std::as_const(snapshot.leftSide.order)) {
            snapshot.identities.append(zzSideIdentity(id));
        }

        ZzPlannerLayoutState::ZzLayoutRequest request;
        request.projection = ZzPlannerLayoutState::ZzWorkspaceProjection{};
        request.projection->leftSide.order = {QStringLiteral("f"),
            QStringLiteral("b"), QStringLiteral("d")};
        request.projection->leftSide.visible = request.projection->leftSide.order;
        request.projection->leftSide.sizes = {606, 202, 404};
        request.projection->activity.leftPrimary =
            request.projection->leftSide.order;
        request.leftCurrent = QStringLiteral("f");

        const auto target = ZzPlannerLayoutState::buildRestoreTarget(snapshot, request);
        QVERIFY(target.has_value());
        const QStringList expectedOrder = {QStringLiteral("e"),
            QStringLiteral("f"), QStringLiteral("g"), QStringLiteral("a"),
            QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")};
        const QList<int> expectedSizes = {105, 606, 107, 101, 202, 103, 404};
        QCOMPARE(target->leftSide.order, expectedOrder);
        QCOMPARE(target->leftSide.visible, expectedOrder);
        QCOMPARE(target->leftSide.sizes, expectedSizes);
        QCOMPARE(target->leftSide.current, QStringLiteral("f"));
        QCOMPARE(target->activity.leftActive,
            QSet<QString>(expectedOrder.cbegin(), expectedOrder.cend()));
        QCOMPARE(target->leftSide.contents.size(), expectedOrder.size());
        for (qsizetype index = 0; index < expectedOrder.size(); ++index) {
            QCOMPARE(target->leftSide.contents.at(index).panelId,
                expectedOrder.at(index));
        }
        QCOMPARE(target->activity.leftPrimary, expectedOrder);
    }

    void restorePlannerScalesBelowQuadraticGrowth()
    {
        const ZzPlannerFixture smallFixture = zzAlternatingPlannerFixture(512);
        const ZzPlannerFixture largeFixture = zzAlternatingPlannerFixture(4096);
        QVERIFY(ZzPlannerLayoutState::buildRestoreTarget(
                     smallFixture.snapshot, smallFixture.request)
                     .has_value());
        QVERIFY(ZzPlannerLayoutState::buildRestoreTarget(
                     largeFixture.snapshot, largeFixture.request)
                     .has_value());

        const auto measure = [](const ZzPlannerFixture &fixture) {
            QList<qint64> samples;
            quint64 checksum = 0;
            samples.reserve(7);
            for (int sample = 0; sample < 7; ++sample) {
                const auto measurement = zzMeasurePlanner(fixture, 3);
                if (!measurement.has_value()) {
                    return std::optional<ZzPlannerMeasurement>{};
                }
                samples.append(measurement->medianNanoseconds);
                checksum += measurement->checksum;
            }
            std::sort(samples.begin(), samples.end());
            return std::optional<ZzPlannerMeasurement>{ZzPlannerMeasurement{
                samples.at(samples.size() / 2), checksum}};
        };

        const auto small = measure(smallFixture);
        const auto large = measure(largeFixture);
        QVERIFY(small.has_value());
        QVERIFY(large.has_value());
        QVERIFY(small->medianNanoseconds > 0);
        QVERIFY(large->medianNanoseconds > 0);
        QVERIFY(small->checksum > 0);
        QVERIFY(large->checksum > 0);

        for (const ZzPlannerFixture *fixture : {&smallFixture, &largeFixture}) {
            const auto target = ZzPlannerLayoutState::buildRestoreTarget(
                fixture->snapshot, fixture->request);
            QVERIFY(target.has_value());
            QCOMPARE(target->leftSide.order, fixture->expectedOrder);
            QCOMPARE(target->leftSide.visible, fixture->expectedOrder);
            QCOMPARE(target->leftSide.sizes, fixture->expectedSizes);
            QCOMPARE(target->leftSide.contents.size(), fixture->expectedOrder.size());
            for (qsizetype index = 0; index < fixture->expectedOrder.size(); ++index) {
                QCOMPARE(target->leftSide.contents.at(index).panelId,
                    fixture->expectedOrder.at(index));
                QCOMPARE(target->leftSide.contents.at(index).stackIdentity,
                    target->leftSide.stackIdentity);
                QCOMPARE(target->leftSide.contents.at(index).ancestry,
                    QList<ZzPlannerLayoutState::ZzSubsystemIdentity>({
                        target->leftSide.paneIdentity,
                        target->leftSide.stackIdentity}));
            }
            auto expectedActivity = fixture->snapshot.activity;
            expectedActivity.leftPrimary = fixture->expectedOrder;
            expectedActivity.leftCurrent = fixture->request.leftCurrent;
            expectedActivity.leftActive = QSet<QString>(
                fixture->expectedOrder.cbegin(), fixture->expectedOrder.cend());
            QCOMPARE(target->activity, expectedActivity);
        }
        QVERIFY2(large->medianNanoseconds < small->medianNanoseconds * 20,
            qPrintable(QStringLiteral(
                "Planner grew from %1 ns to %2 ns (%3x)")
                    .arg(small->medianNanoseconds)
                    .arg(large->medianNanoseconds)
                    .arg(static_cast<double>(large->medianNanoseconds)
                        / static_cast<double>(small->medianNanoseconds))));
    }

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

    void projectionFallbackUsesSnapshotPaneCurrent()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        ZzLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.leftSide.order = {QStringLiteral("one"), QStringLiteral("two")};
        snapshot.leftSide.visible = {QStringLiteral("one"), QStringLiteral("two")};
        snapshot.leftSide.current = QStringLiteral("two");
        snapshot.activity.leftCurrent = QStringLiteral("activity-current");

        ZzLayoutState::ZzLayoutRequest request;
        request.projection = static_cast<ZzLayoutState::ZzWorkspaceProjection>(
            snapshot);
        request.projection->leftSide.current = QStringLiteral("one");
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

    void sideVisibleAndSizesStayAttachedToPanelIds()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        ZzLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.leftSide.order = {
            QStringLiteral("left"), QStringLiteral("right")};
        snapshot.leftSide.visible = {
            QStringLiteral("unknown"), QStringLiteral("right"),
            QStringLiteral("left")};
        snapshot.leftSide.sizes = {99, 320, 240};

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, {});
        QVERIFY(target.has_value());
        QCOMPARE(target->leftSide.visible,
            QStringList({QStringLiteral("right"), QStringLiteral("left")}));
        QCOMPARE(target->leftSide.sizes, QList<int>({320, 240}));
    }

    void activityMoveKeepsSizeAttachedToPanelId()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        QObject leftPane;
        QObject leftStack;
        QObject rightPane;
        QObject rightStack;
        auto snapshot = zzTwoSideSnapshot();
        snapshot.leftSide.paneIdentity = {&leftPane, &leftPane};
        snapshot.leftSide.stackIdentity = {&leftStack, &leftStack};
        snapshot.leftSide.contents = {{QStringLiteral("explorer"),
            {&leftStack, &leftStack},
            {{&leftPane, &leftPane}, {&leftStack, &leftStack}}}};
        snapshot.rightSide.paneIdentity = {&rightPane, &rightPane};
        snapshot.rightSide.stackIdentity = {&rightStack, &rightStack};
        snapshot.rightSide.contents = {{QStringLiteral("terminal"),
            {&rightStack, &rightStack},
            {{&rightPane, &rightPane}, {&rightStack, &rightStack}}}};
        const auto target = ZzLayoutState::buildActivityMoveTarget(
            snapshot, QStringLiteral("terminal"),
            ZzFluentUI::ZzActivityArea::LeftPrimary, 0);
        QVERIFY(target.has_value());
        QCOMPARE(target->leftSide.visible,
            QStringList({QStringLiteral("terminal"),
                QStringLiteral("explorer")}));
        QCOMPARE(target->leftSide.sizes, QList<int>({360, 240}));
        QVERIFY(target->rightSide.visible.isEmpty());
        QVERIFY(target->rightSide.sizes.isEmpty());
        QCOMPARE(target->leftSide.contents.size(), 2);
        QCOMPARE(target->leftSide.contents.at(0).panelId,
            QStringLiteral("terminal"));
        QCOMPARE(target->leftSide.contents.at(0).stackIdentity.object.data(),
            &leftStack);
        QCOMPARE(target->leftSide.contents.at(0).ancestry.size(), 2);
        QCOMPARE(target->leftSide.contents.at(0).ancestry.at(0).object.data(),
            &leftPane);
        QCOMPARE(target->leftSide.contents.at(0).ancestry.at(1).object.data(),
            &leftStack);
        QVERIFY(target->rightSide.contents.isEmpty());
    }

    void activityCurrentIsDerivedFromSideTarget()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        auto snapshot = zzTwoSideSnapshot();
        snapshot.activity.leftCurrent = QStringLiteral("stale-left");
        snapshot.activity.rightCurrent = QStringLiteral("stale-right");

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, {});
        QVERIFY(target.has_value());
        QCOMPARE(target->activity.leftCurrent, QStringLiteral("explorer"));
        QCOMPARE(target->activity.rightCurrent, QStringLiteral("terminal"));
    }

    void activityActiveSetsAreDerivedFromSideVisibility()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        auto snapshot = zzTwoSideSnapshot();
        snapshot.leftSide.order.append(QStringLiteral("search"));
        snapshot.leftSide.visible.append(QStringLiteral("search"));
        snapshot.leftSide.sizes.append(180);
        snapshot.activity.leftActive = {QStringLiteral("stale-left")};
        snapshot.activity.rightActive = {QStringLiteral("stale-right")};

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, {});
        QVERIFY(target.has_value());
        QCOMPARE(target->activity.leftActive,
            QSet<QString>({QStringLiteral("explorer"),
                QStringLiteral("search")}));
        QCOMPARE(target->activity.rightActive,
            QSet<QString>({QStringLiteral("terminal")}));

        auto changed = *target;
        changed.activity.leftActive.remove(QStringLiteral("search"));
        QVERIFY(!ZzLayoutState::equals(*target, changed));

        changed = *target;
        changed.activity.rightActive.remove(QStringLiteral("terminal"));
        QVERIFY(!ZzLayoutState::equals(*target, changed));
    }

    void splitProjectionPreservesNormalizedTreeAndSavedPages()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        ZzLayoutState::ZzSplitNode editor;
        editor.groupId = QStringLiteral("editor");
        editor.currentIndex = 2;
        ZzLayoutState::ZzSplitNode terminal;
        terminal.groupId = QStringLiteral("terminal");
        ZzLayoutState::ZzSplitNode outline;
        outline.groupId = QStringLiteral("outline");
        ZzLayoutState::ZzSplitNode secondary;
        secondary.leaf = false;
        secondary.orientation = Qt::Vertical;
        secondary.children = {terminal, outline};
        secondary.sizes = {300};

        ZzLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.split.root.leaf = false;
        snapshot.split.root.orientation = Qt::Horizontal;
        snapshot.split.root.children = {editor, secondary};
        snapshot.split.root.sizes = {640};
        snapshot.split.activeGroup = QStringLiteral("terminal");
        snapshot.split.groupOrder = {
            QStringLiteral("terminal"), QStringLiteral("ghost"),
            QStringLiteral("editor"), QStringLiteral("terminal")};
        snapshot.split.savedPages = {
            {QStringLiteral("page:editor"), QStringLiteral("editor"), 0,
                false},
            {QStringLiteral("page:terminal"), QStringLiteral("terminal"), 1,
                true}};
        snapshot.split.canonicalState = QByteArray::fromHex("0201020304");

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, {});
        QVERIFY(target.has_value());
        QVERIFY(!target->split.root.leaf);
        QCOMPARE(target->split.root.orientation, Qt::Horizontal);
        QCOMPARE(target->split.root.sizes, QList<int>({640, 1}));
        QCOMPARE(target->split.root.children.size(), 2);
        QCOMPARE(target->split.root.children.at(0).groupId,
            QStringLiteral("editor"));
        QCOMPARE(target->split.root.children.at(0).currentIndex, 2);
        QVERIFY(!target->split.root.children.at(1).leaf);
        QCOMPARE(target->split.root.children.at(1).orientation, Qt::Vertical);
        QCOMPARE(target->split.root.children.at(1).sizes,
            QList<int>({300, 1}));
        QCOMPARE(target->split.activeGroup, QStringLiteral("terminal"));
        QCOMPARE(target->split.groupOrder,
            QStringList({QStringLiteral("terminal"),
                QStringLiteral("editor"), QStringLiteral("outline")}));
        QCOMPARE(target->split.savedPages.size(), 2);
        QCOMPARE(target->split.savedPages.at(0).key,
            QStringLiteral("page:editor"));
        QCOMPARE(target->split.savedPages.at(0).groupId,
            QStringLiteral("editor"));
        QCOMPARE(target->split.savedPages.at(0).order, 0);
        QVERIFY(!target->split.savedPages.at(0).current);
        QCOMPARE(target->split.savedPages.at(1).key,
            QStringLiteral("page:terminal"));
        QCOMPARE(target->split.savedPages.at(1).groupId,
            QStringLiteral("terminal"));
        QCOMPARE(target->split.savedPages.at(1).order, 1);
        QVERIFY(target->split.savedPages.at(1).current);
        QCOMPARE(target->split.canonicalState,
            QByteArray::fromHex("0201020304"));
    }

    void splitProjectionRepresentsSchemaOneRootCurrentIndex()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        ZzLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.split.root.groupId = QStringLiteral("schema-one-root");
        snapshot.split.root.currentIndex = 3;
        snapshot.split.activeGroup = QStringLiteral("schema-one-root");
        snapshot.split.groupOrder = {QStringLiteral("schema-one-root")};
        snapshot.split.canonicalState = QByteArray::fromHex("01030a0b");

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, {});
        QVERIFY(target.has_value());
        QVERIFY(target->split.root.leaf);
        QCOMPARE(target->split.root.groupId,
            QStringLiteral("schema-one-root"));
        QCOMPARE(target->split.root.currentIndex, 3);
        QCOMPARE(target->split.activeGroup,
            QStringLiteral("schema-one-root"));
        QCOMPARE(target->split.canonicalState,
            QByteArray::fromHex("01030a0b"));
    }

    void plannerPreservesRuntimeIdentityAndSubsystemContracts()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        QObject pane;
        QObject stack;
        QObject model;
        QObject dockOwner;
        QObject dockObject;
        QWidget panelWidget;

        ZzLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.leftSide.paneIdentity = {&pane, &pane};
        snapshot.leftSide.stackIdentity = {&stack, &stack};
        snapshot.leftSide.order = {QStringLiteral("explorer")};
        snapshot.leftSide.visible = {QStringLiteral("explorer")};
        snapshot.leftSide.sizes = {240};
        snapshot.leftSide.current = QStringLiteral("explorer");
        snapshot.leftSide.contents = {{QStringLiteral("explorer"),
            {&stack, &stack}, {{&pane, &pane}, {&stack, &stack}}}};
        snapshot.bottom.paneIdentity = {&pane, &pane};
        snapshot.bottom.stackIdentity = {&stack, &stack};
        snapshot.bottom.order = {QStringLiteral("problems")};
        snapshot.bottom.current = QStringLiteral("problems");
        snapshot.bottom.collapsed = false;
        snapshot.bottom.height = 190;
        snapshot.bottom.contents = {{QStringLiteral("problems"),
            {&stack, &stack}, {{&pane, &pane}, {&stack, &stack}}}};
        snapshot.activity.modelIdentity = {&model, &model};
        snapshot.activity.leftPrimary = {QStringLiteral("explorer")};
        snapshot.activity.leftSecondary = {QStringLiteral("search")};
        snapshot.activity.rightPrimary = {QStringLiteral("source-control")};
        snapshot.activity.rightSecondary = {QStringLiteral("outline")};
        snapshot.title.mode = ZzLayoutState::ZzTitleMode::Custom;
        snapshot.title.hostTitle = QStringLiteral("Host title");
        snapshot.title.titleBarTitle = QStringLiteral("Title bar title");

        ZzLayoutState::ZzPanelIdentity dockPanel;
        dockPanel.id = QStringLiteral("inspector");
        dockPanel.kind = ZzLayoutState::ZzPanelKind::Dock;
        dockPanel.widget = &panelWidget;
        dockPanel.rawWidget = &panelWidget;
        dockPanel.registrationGeneration = 7;
        dockPanel.dock = &dockObject;
        dockPanel.rawDock = reinterpret_cast<ZzFluentUI::ZzDockPanel *>(
            quintptr(0x1234));
        snapshot.identities = {dockPanel};
        snapshot.dock.state = QByteArray::fromHex("d0c0");
        snapshot.dock.docks = {{dockPanel, Qt::RightDockWidgetArea, true, true,
            {&dockOwner, &dockOwner}}};

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, {});
        QVERIFY(target.has_value());
        QCOMPARE(target->identities.size(), 1);
        QCOMPARE(target->identities.constFirst().widget.data(), &panelWidget);
        QCOMPARE(target->identities.constFirst().rawWidget, &panelWidget);
        QCOMPARE(target->identities.constFirst().registrationGeneration,
            quint64(7));
        QCOMPARE(target->identities.constFirst().dock.data(), &dockObject);
        QCOMPARE(target->identities.constFirst().rawDock,
            reinterpret_cast<ZzFluentUI::ZzDockPanel *>(quintptr(0x1234)));
        QCOMPARE(target->leftSide.paneIdentity.object.data(), &pane);
        QCOMPARE(target->leftSide.stackIdentity.object.data(), &stack);
        QCOMPARE(target->leftSide.contents.size(), 1);
        QCOMPARE(target->bottom.order,
            QStringList({QStringLiteral("problems")}));
        QCOMPARE(target->bottom.current, QStringLiteral("problems"));
        QVERIFY(!target->bottom.collapsed);
        QCOMPARE(target->bottom.height, 190);
        QCOMPARE(target->bottom.paneIdentity.object.data(), &pane);
        QCOMPARE(target->bottom.stackIdentity.object.data(), &stack);
        QCOMPARE(target->bottom.contents.size(), 1);
        QCOMPARE(target->activity.modelIdentity.object.data(), &model);
        QCOMPARE(target->activity.leftPrimary,
            QStringList({QStringLiteral("explorer")}));
        QCOMPARE(target->activity.leftSecondary,
            QStringList({QStringLiteral("search")}));
        QCOMPARE(target->activity.rightPrimary,
            QStringList({QStringLiteral("source-control")}));
        QCOMPARE(target->activity.rightSecondary,
            QStringList({QStringLiteral("outline")}));
        QCOMPARE(target->dock.state, QByteArray::fromHex("d0c0"));
        QCOMPARE(target->dock.docks.size(), 1);
        QCOMPARE(target->dock.docks.constFirst().area,
            Qt::RightDockWidgetArea);
        QVERIFY(target->dock.docks.constFirst().floating);
        QVERIFY(target->dock.docks.constFirst().visible);
        QCOMPARE(target->dock.docks.constFirst().actualOwnerIdentity.object.data(),
            &dockOwner);
        QCOMPARE(target->title.hostTitle, QStringLiteral("Host title"));
        QCOMPARE(target->title.titleBarTitle,
            QStringLiteral("Title bar title"));
    }

    void equalsIncludesRuntimeRawIdentityAndRegistrationGeneration()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        QObject pane;
        QObject replacementPane;
        QObject dock;
        QObject replacementDock;
        QWidget widget;
        QWidget replacementWidget;
        ZzLayoutState::ZzWorkspaceProjection baseline;
        baseline.leftSide.paneIdentity = {&pane, &pane};
        ZzLayoutState::ZzPanelIdentity identity;
        identity.id = QStringLiteral("explorer");
        identity.widget = &widget;
        identity.rawWidget = &widget;
        identity.registrationGeneration = 11;
        identity.dock = &dock;
        identity.rawDock = reinterpret_cast<ZzFluentUI::ZzDockPanel *>(
            quintptr(0x1000));
        baseline.identities = {identity};

        auto changed = baseline;
        ++changed.identities[0].registrationGeneration;
        QVERIFY(!ZzLayoutState::equals(baseline, changed));

        changed = baseline;
        changed.identities[0].rawWidget = &replacementWidget;
        QVERIFY(!ZzLayoutState::equals(baseline, changed));

        changed = baseline;
        changed.identities[0].widget = &replacementWidget;
        QVERIFY(!ZzLayoutState::equals(baseline, changed));

        changed = baseline;
        changed.identities[0].dock = &replacementDock;
        QVERIFY(!ZzLayoutState::equals(baseline, changed));

        changed = baseline;
        changed.identities[0].rawDock =
            reinterpret_cast<ZzFluentUI::ZzDockPanel *>(quintptr(0x2000));
        QVERIFY(!ZzLayoutState::equals(baseline, changed));

        changed = baseline;
        changed.leftSide.paneIdentity.rawObject = &replacementPane;
        QVERIFY(!ZzLayoutState::equals(baseline, changed));

        changed = baseline;
        changed.leftSide.paneIdentity.object = &replacementPane;
        QVERIFY(!ZzLayoutState::equals(baseline, changed));
    }

    void serializedProjectionDoesNotEraseSnapshotRuntimeIdentity()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        QObject pane;
        QObject stack;
        QObject model;
        QWidget widget;
        ZzLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.leftSide.paneIdentity = {&pane, &pane};
        snapshot.leftSide.stackIdentity = {&stack, &stack};
        snapshot.activity.modelIdentity = {&model, &model};
        ZzLayoutState::ZzPanelIdentity identity;
        identity.id = QStringLiteral("explorer");
        identity.widget = &widget;
        identity.rawWidget = &widget;
        identity.registrationGeneration = 5;
        snapshot.identities = {identity};

        ZzLayoutState::ZzLayoutRequest request;
        request.projection.emplace();
        request.projection->leftSide.order = {QStringLiteral("explorer")};
        request.projection->leftSide.visible = {QStringLiteral("explorer")};
        request.projection->leftSide.sizes = {260};
        request.projection->leftSide.current = QStringLiteral("explorer");

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, request);
        QVERIFY(target.has_value());
        QCOMPARE(target->leftSide.paneIdentity.object.data(), &pane);
        QCOMPARE(target->leftSide.paneIdentity.rawObject, &pane);
        QCOMPARE(target->leftSide.stackIdentity.object.data(), &stack);
        QCOMPARE(target->activity.modelIdentity.object.data(), &model);
        QCOMPARE(target->identities.size(), 1);
        QCOMPARE(target->identities.constFirst().widget.data(), &widget);
        QCOMPARE(target->identities.constFirst().registrationGeneration,
            quint64(5));
        QCOMPARE(target->leftSide.width, 280);
        QCOMPARE(target->leftSide.sizes, QList<int>({260}));
    }

    void serializedProjectionReconcilesRegisteredSidePanels()
    {
        using ZzLayoutState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

        QObject leftPane;
        QObject leftStack;
        QObject rightPane;
        QObject rightStack;
        ZzLayoutState::ZzPanelIdentity explorer;
        explorer.id = QStringLiteral("explorer");
        explorer.kind = ZzLayoutState::ZzPanelKind::Side;
        ZzLayoutState::ZzPanelIdentity search;
        search.id = QStringLiteral("search");
        search.kind = ZzLayoutState::ZzPanelKind::Side;
        ZzLayoutState::ZzPanelIdentity terminal;
        terminal.id = QStringLiteral("terminal");
        terminal.kind = ZzLayoutState::ZzPanelKind::Side;

        ZzLayoutState::ZzWorkspaceSnapshot snapshot;
        snapshot.identities = {explorer, search, terminal};
        snapshot.leftSide.paneIdentity = {&leftPane, &leftPane};
        snapshot.leftSide.stackIdentity = {&leftStack, &leftStack};
        snapshot.leftSide.order = {
            QStringLiteral("explorer"), QStringLiteral("search")};
        snapshot.leftSide.visible = {
            QStringLiteral("explorer"), QStringLiteral("search")};
        snapshot.leftSide.sizes = {240, 180};
        snapshot.leftSide.current = QStringLiteral("explorer");
        snapshot.leftSide.collapsed = false;
        snapshot.leftSide.width = 300;
        snapshot.leftSide.contents = {
            {QStringLiteral("explorer"), {&leftStack, &leftStack},
                {{&leftPane, &leftPane}, {&leftStack, &leftStack}}},
            {QStringLiteral("search"), {&leftStack, &leftStack},
                {{&leftPane, &leftPane}, {&leftStack, &leftStack}}}};
        snapshot.rightSide.paneIdentity = {&rightPane, &rightPane};
        snapshot.rightSide.stackIdentity = {&rightStack, &rightStack};
        snapshot.rightSide.order = {QStringLiteral("terminal")};
        snapshot.rightSide.visible = {QStringLiteral("terminal")};
        snapshot.rightSide.sizes = {360};
        snapshot.rightSide.current = QStringLiteral("terminal");
        snapshot.rightSide.collapsed = false;
        snapshot.rightSide.width = 380;
        snapshot.rightSide.contents = {
            {QStringLiteral("terminal"), {&rightStack, &rightStack},
                {{&rightPane, &rightPane}, {&rightStack, &rightStack}}}};
        snapshot.activity.leftPrimary = {
            QStringLiteral("explorer"), QStringLiteral("search")};
        snapshot.activity.rightPrimary = {QStringLiteral("terminal")};
        snapshot.activity.leftCurrent = QStringLiteral("explorer");
        snapshot.activity.rightCurrent = QStringLiteral("terminal");
        snapshot.activity.leftActive = {
            QStringLiteral("explorer"), QStringLiteral("search")};
        snapshot.activity.rightActive = {QStringLiteral("terminal")};

        ZzLayoutState::ZzLayoutRequest request;
        request.projection.emplace();
        request.projection->leftSide.order = {
            QStringLiteral("search"), QStringLiteral("ghost")};
        request.projection->leftSide.visible = {
            QStringLiteral("search"), QStringLiteral("ghost")};
        request.projection->leftSide.sizes = {190, 999};
        request.projection->leftSide.current = QStringLiteral("ghost");
        request.projection->leftSide.collapsed = false;
        request.projection->leftSide.width = 320;
        request.projection->leftSide.contents = {
            {QStringLiteral("search"), {}, {}},
            {QStringLiteral("ghost"), {}, {}}};
        request.projection->rightSide.order = {
            QStringLiteral("terminal"), QStringLiteral("ghost")};
        request.projection->rightSide.visible = {
            QStringLiteral("terminal"), QStringLiteral("ghost")};
        request.projection->rightSide.sizes = {420, 998};
        request.projection->rightSide.current = QStringLiteral("ghost");
        request.projection->rightSide.collapsed = false;
        request.projection->rightSide.width = 400;
        request.projection->rightSide.contents = {
            {QStringLiteral("terminal"), {}, {}},
            {QStringLiteral("ghost"), {}, {}}};
        request.projection->activity.leftPrimary = {
            QStringLiteral("search"), QStringLiteral("ghost")};
        request.projection->activity.rightPrimary = {
            QStringLiteral("terminal"), QStringLiteral("ghost")};
        request.projection->activity.leftCurrent = QStringLiteral("ghost");
        request.projection->activity.rightCurrent = QStringLiteral("ghost");
        request.projection->activity.leftActive = {
            QStringLiteral("search"), QStringLiteral("ghost")};
        request.projection->activity.rightActive = {
            QStringLiteral("terminal"), QStringLiteral("ghost")};

        const auto target = ZzLayoutState::buildRestoreTarget(snapshot, request);
        QVERIFY(target.has_value());
        QCOMPARE(target->leftSide.order,
            QStringList({QStringLiteral("explorer"),
                QStringLiteral("search")}));
        QCOMPARE(target->leftSide.visible,
            QStringList({QStringLiteral("explorer"),
                QStringLiteral("search")}));
        QCOMPARE(target->leftSide.sizes, QList<int>({240, 190}));
        QCOMPARE(target->leftSide.current, QStringLiteral("explorer"));
        QCOMPARE(target->leftSide.contents.size(), 2);
        QCOMPARE(target->leftSide.contents.at(0).panelId,
            QStringLiteral("explorer"));
        QCOMPARE(target->leftSide.contents.at(1).panelId,
            QStringLiteral("search"));
        QCOMPARE(target->rightSide.order,
            QStringList({QStringLiteral("terminal")}));
        QCOMPARE(target->rightSide.visible,
            QStringList({QStringLiteral("terminal")}));
        QCOMPARE(target->rightSide.sizes, QList<int>({420}));
        QCOMPARE(target->rightSide.current, QStringLiteral("terminal"));
        QCOMPARE(target->rightSide.contents.size(), 1);
        QCOMPARE(target->rightSide.contents.constFirst().panelId,
            QStringLiteral("terminal"));
        QCOMPARE(target->activity.leftPrimary,
            QStringList({QStringLiteral("explorer"),
                QStringLiteral("search")}));
        QCOMPARE(target->activity.rightPrimary,
            QStringList({QStringLiteral("terminal")}));
        QCOMPARE(target->activity.leftCurrent, QStringLiteral("explorer"));
        QCOMPARE(target->activity.rightCurrent, QStringLiteral("terminal"));
        QCOMPARE(target->activity.leftActive,
            QSet<QString>({QStringLiteral("explorer"),
                QStringLiteral("search")}));
        QCOMPARE(target->activity.rightActive,
            QSet<QString>({QStringLiteral("terminal")}));
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
