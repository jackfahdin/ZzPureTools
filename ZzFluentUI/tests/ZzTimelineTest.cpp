#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzTimeline.h>

#include <QAbstractItemModel>
#include <QPointer>
#include <QProxyStyle>
#include <QSignalSpy>
#include <QStyleFactory>
#include <QVariantAnimation>
#include <QtTest>
#include <ZzTestEventLoop.h>

using namespace ZzFluentUI;

class ZzTimelineTest final : public QObject {
    Q_OBJECT
private slots:
    void insertionOrderAndDisplayOrder();
    void eventChangesUpdateModelAndSignals();
    void transferAndTakePreserveOwnership();
    void externalDeletionAndParentChangeDetach();
    void callbackReparentDoesNotDeleteEvent();
    void defaultsAndInputBounds();
    void mouseClickReportsActualEvent();
    void orientationChangesItemGeometry();
    void nodeAndRailProducePixels();
    void removalNotificationMayDeleteEvent();
    void removalNotificationMayTransferEvent();
    void propertyCallbackMayDeleteEvent();
    void externalDeleteNotificationMayDeleteTimeline();
    void insertionNotificationMayAddAnotherEvent();
    void reducedMotionStopsPulse();
    void insertionNotificationMayDeleteTimeline();
    void keyboardActivationReportsActualEvent();
    void statusNodesUseReferenceColors();
    void clearResetCallbackMayTransferEvent();
    void takeCallbackReownershipReturnsNull();
    void externalReparentDefersModelNotification();
    void externalDeleteDefersModelNotification();
    void insertionCallbackTransfersInFlightEvent();
    void queuedAppendCanBeRemoved();
    void queuedAppendCanBeTransferred();
    void nestedReverseUsesLatestRequest();
    void themeReducedMotionStopsRunningPulse();
    void constructorRejectsInvalidStatus();
    void detachedEventStopsOldTimelineNotifications();
    void readdDetachedEventBeforeQueuedRemoval();
    void cancelledAppendKeepsNewAppendOrder();
};

void ZzTimelineTest::insertionOrderAndDisplayOrder()
{
    ZzTimeline timeline;
    auto* first = timeline.addEvent({}, QStringLiteral("First"));
    auto* second = timeline.addEvent({}, QStringLiteral("Second"));
    timeline.setReverse(true);
    QCOMPARE(timeline.events().size(), 2);
    QCOMPARE(timeline.events().first(), first);
    QCOMPARE(timeline.eventAt(0), second);
    QCOMPARE(timeline.model()->index(0, 0).data().toString(), QStringLiteral("Second"));
}

void ZzTimelineTest::eventChangesUpdateModelAndSignals()
{
    ZzTimeline timeline;
    auto* event = timeline.addEvent(QDateTime::fromString(QStringLiteral("2026-10-08T12:34:00"), Qt::ISODate),
        QStringLiteral("Before"), QStringLiteral("Detail"));
    QSignalSpy titleSpy(event, &ZzTimelineEvent::titleChanged);
    QSignalSpy itemSpy(event, &ZzTimelineEvent::itemChanged);
    QSignalSpy dataSpy(timeline.model(), &QAbstractItemModel::dataChanged);
    event->setTitle(QStringLiteral("After"));
    QCOMPARE(titleSpy.size(), 1);
    QCOMPARE(itemSpy.size(), 1);
    QCOMPARE(dataSpy.size(), 1);
    QCOMPARE(timeline.model()->index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("After"));
    QCOMPARE(timeline.model()->index(0, 0).data(Qt::ToolTipRole).toString(), QStringLiteral("Detail"));
    QCOMPARE(timeline.model()->index(0, 0).data(Qt::AccessibleTextRole).toString(),
        QStringLiteral("After, Detail"));
    event->setTitle(QStringLiteral("After"));
    QCOMPARE(itemSpy.size(), 1);
}

void ZzTimelineTest::transferAndTakePreserveOwnership()
{
    ZzTimeline source;
    ZzTimeline destination;
    auto* event = source.addEvent({}, QStringLiteral("Move"));
    destination.addEvent(event);
    QCOMPARE(source.events().size(), 0);
    QCOMPARE(destination.events().size(), 1);
    QCOMPARE(event->parent(), &destination);
    QCOMPARE(destination.takeEvent(event), event);
    QCOMPARE(event->parent(), nullptr);
    QCOMPARE(destination.model()->rowCount(), 0);
    delete event;
}

void ZzTimelineTest::externalDeletionAndParentChangeDetach()
{
    ZzTimeline timeline;
    auto* event = timeline.addEvent({}, QStringLiteral("Delete"));
    delete event;
    QCOMPARE(timeline.events().size(), 0);
    QCOMPARE(timeline.eventAt(0), nullptr);
    ZZ_COMPARE_EVENTUALLY(timeline.model()->rowCount(), 0);
    QObject other;
    event = timeline.addEvent({}, QStringLiteral("Reparent"));
    event->setParent(&other);
    QCOMPARE(timeline.events().size(), 0);
    QCOMPARE(timeline.eventAt(0), nullptr);
    ZZ_COMPARE_EVENTUALLY(timeline.model()->rowCount(), 0);
}

void ZzTimelineTest::callbackReparentDoesNotDeleteEvent()
{
    ZzTimeline source;
    ZzTimeline destination;
    auto* event = source.addEvent({}, QStringLiteral("Keep"));
    QPointer<ZzTimelineEvent> alive(event);
    QObject::connect(&source, &ZzTimeline::eventsChanged, &source, [&] {
        if (!source.events().contains(event) && !destination.events().contains(event))
            destination.addEvent(event);
    });
    source.removeEvent(event);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(alive);
    QCOMPARE(destination.eventAt(0), event);
    QCOMPARE(event->parent(), &destination);
}

void ZzTimelineTest::defaultsAndInputBounds()
{
    ZzTimeline timeline;
    QCOMPARE(timeline.orientation(), Qt::Vertical);
    QCOMPARE(timeline.layoutMode(), ZzTimeline::ContentOnRight);
    QCOMPARE(timeline.timestampWidth(), 116);
    QCOMPARE(timeline.nodeSize(), 14);
    QCOMPARE(timeline.lineWidth(), 2.0);
    QCOMPARE(timeline.itemSpacing(), 18);
    QCOMPARE(timeline.horizontalItemWidth(), 240);
    QCOMPARE(timeline.contentPadding(), 12);
    QCOMPARE(timeline.animationDuration(), 1400);
    QVERIFY(timeline.isTimestampVisible());
    QVERIFY(timeline.isDescriptionVisible());
    QCOMPARE(timeline.timestampFormat(), QStringLiteral("yyyy-MM-dd HH:mm"));
    QVERIFY(timeline.isAnimationEnabled());
    QVERIFY(!timeline.lineColor().isValid());
    timeline.setNodeSize(-5);
    timeline.setTimestampWidth(1000);
    timeline.setLineWidth(qQNaN());
    timeline.setAnimationDuration(10);
    QCOMPARE(timeline.nodeSize(), 6);
    QCOMPARE(timeline.timestampWidth(), 400);
    QCOMPARE(timeline.lineWidth(), 2.0);
    QCOMPARE(timeline.animationDuration(), 200);
    ZzTimelineEvent event;
    QCOMPARE(event.status(), ZzTimelineEvent::Normal);
    QCOMPARE(event.placement(), ZzTimelineEvent::Automatic);
    QVERIFY(!event.color().isValid());
    event.setIcon(ZzSegoeIcon::History);
    QCOMPARE(event.icon(), QString(QChar(char16_t(0xe81c))));
}

void ZzTimelineTest::mouseClickReportsActualEvent()
{
    ZzTimeline timeline;
    timeline.resize(500, 250);
    auto* first = timeline.addEvent({}, QStringLiteral("First"));
    auto* second = timeline.addEvent({}, QStringLiteral("Second"));
    timeline.setReverse(true);
    timeline.show();
    ZZ_VERIFY_EVENTUALLY(timeline.visualRect(timeline.model()->index(0, 0)).isValid());
    QSignalSpy clickSpy(&timeline, &ZzTimeline::eventClicked);
    QTest::mouseClick(
        timeline.viewport(), Qt::LeftButton, {}, timeline.visualRect(timeline.model()->index(0, 0)).center());
    QCOMPARE(clickSpy.size(), 1);
    QCOMPARE(qvariant_cast<ZzTimelineEvent*>(clickSpy.at(0).at(0)), second);
    Q_UNUSED(first);
}

void ZzTimelineTest::orientationChangesItemGeometry()
{
    ZzTimeline timeline;
    timeline.resize(550, 320);
    timeline.addEvent({}, QStringLiteral("First"));
    timeline.addEvent({}, QStringLiteral("Second"));
    timeline.show();
    ZZ_VERIFY_EVENTUALLY(timeline.visualRect(timeline.model()->index(0, 0)).isValid());
    const QRect vertical = timeline.visualRect(timeline.model()->index(0, 0));
    timeline.setOrientation(Qt::Horizontal);
    ZZ_VERIFY_EVENTUALLY(timeline.visualRect(timeline.model()->index(0, 0)).width() < vertical.width());
    QCOMPARE(timeline.flow(), QListView::LeftToRight);
    QCOMPARE(timeline.visualRect(timeline.model()->index(0, 0)).width(), 240);
}

void ZzTimelineTest::nodeAndRailProducePixels()
{
    ZzTimeline timeline;
    timeline.resize(500, 240);
    timeline.setLineColor(QColor(QStringLiteral("#ff0000")));
    timeline.addEvent({}, QStringLiteral("First"));
    timeline.addEvent({}, QStringLiteral("Second"));
    timeline.show();
    ZZ_VERIFY_EVENTUALLY(timeline.visualRect(timeline.model()->index(0, 0)).isValid());
    const QImage image = timeline.viewport()->grab().toImage().convertToFormat(QImage::Format_RGB32);
    int redPixels = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.red() > 220 && pixel.green() < 80 && pixel.blue() < 80)
                ++redPixels;
        }
    QVERIFY(redPixels > 10);
}

void ZzTimelineTest::removalNotificationMayDeleteEvent()
{
    ZzTimeline timeline;
    auto* event = timeline.addEvent({}, QStringLiteral("Delete in callback"));
    QPointer<ZzTimelineEvent> alive(event);
    QObject::connect(
        timeline.model(), &QAbstractItemModel::rowsAboutToBeRemoved, &timeline, [event] { delete event; });
    timeline.removeEvent(event);
    QVERIFY(!alive);
    QCOMPARE(timeline.model()->rowCount(), 0);
    QCOMPARE(timeline.events().size(), 0);
}

void ZzTimelineTest::removalNotificationMayTransferEvent()
{
    ZzTimeline source;
    ZzTimeline destination;
    auto* event = source.addEvent({}, QStringLiteral("Transfer in callback"));
    QObject::connect(source.model(), &QAbstractItemModel::rowsAboutToBeRemoved, &source,
        [&] { destination.addEvent(event); });
    source.removeEvent(event);
    QCOMPARE(source.model()->rowCount(), 0);
    QCOMPARE(destination.eventAt(0), event);
    QCOMPARE(event->parent(), &destination);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(destination.eventAt(0), event);
}

void ZzTimelineTest::propertyCallbackMayDeleteEvent()
{
    ZzTimeline timeline;
    auto* event = timeline.addEvent({}, QStringLiteral("Before"));
    QPointer<ZzTimelineEvent> alive(event);
    QSignalSpy itemSpy(event, &ZzTimelineEvent::itemChanged);
    QObject::connect(event, &ZzTimelineEvent::titleChanged, &timeline, [event] { delete event; });
    event->setTitle(QStringLiteral("After"));
    QVERIFY(!alive);
    QCOMPARE(itemSpy.size(), 0);
    QVERIFY(timeline.events().isEmpty());
    QCOMPARE(timeline.eventAt(0), nullptr);
    ZZ_COMPARE_EVENTUALLY(timeline.model()->rowCount(), 0);
}

void ZzTimelineTest::externalDeleteNotificationMayDeleteTimeline()
{
    auto* timeline = new ZzTimeline;
    auto* event = timeline->addEvent({}, QStringLiteral("Before"));
    QPointer<ZzTimeline> alive(timeline);
    QObject::connect(timeline, &ZzTimeline::eventsChanged, timeline, [timeline] { delete timeline; });
    delete event;
    QVERIFY(alive);
    QCoreApplication::processEvents();
    QVERIFY(!alive);
}

void ZzTimelineTest::insertionNotificationMayAddAnotherEvent()
{
    ZzTimeline timeline;
    ZzTimelineEvent second({}, QStringLiteral("Second"));
    bool didAdd = false;
    QObject::connect(timeline.model(), &QAbstractItemModel::rowsAboutToBeInserted, &timeline, [&] {
        if (!didAdd) {
            didAdd = true;
            timeline.addEvent(&second);
        }
    });
    auto* first = timeline.addEvent({}, QStringLiteral("First"));
    QCOMPARE(timeline.model()->rowCount(), 2);
    QCOMPARE(timeline.eventAt(0), first);
    QCOMPARE(timeline.eventAt(1), &second);
    QCOMPARE(timeline.model()->index(1, 0).data().toString(), QStringLiteral("Second"));
    QCOMPARE(timeline.takeEvent(&second), &second);
}

class TimelineMotionStyle final : public QProxyStyle {
public:
    bool animate = false;
    int styleHint(StyleHint hint, const QStyleOption* option = nullptr, const QWidget* widget = nullptr,
        QStyleHintReturn* data = nullptr) const override
    {
        return hint == SH_Widget_Animate ? int(animate) : QProxyStyle::styleHint(hint, option, widget, data);
    }
};

void ZzTimelineTest::reducedMotionStopsPulse()
{
    TimelineMotionStyle style;
    ZzTimeline timeline;
    timeline.setStyle(&style);
    timeline.addEvent({}, QStringLiteral("Current"), {}, ZzTimelineEvent::Current);
    timeline.show();
    auto* animation = timeline.findChild<QVariantAnimation*>();
    QVERIFY(animation);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    style.animate = true;
    timeline.setAnimationEnabled(false);
    timeline.setAnimationEnabled(true);
    QCOMPARE(animation->state(), QAbstractAnimation::Running);
    style.animate = false;
    timeline.setAnimationEnabled(false);
    timeline.setAnimationEnabled(true);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
}

void ZzTimelineTest::insertionNotificationMayDeleteTimeline()
{
    auto* timeline = new ZzTimeline;
    QPointer<ZzTimeline> alive(timeline);
    QObject::connect(timeline->model(), &QAbstractItemModel::rowsAboutToBeInserted, timeline,
        [timeline] { delete timeline; });
    QCOMPARE(timeline->addEvent({}, QStringLiteral("Delete owner")), nullptr);
    QVERIFY(!alive);
}

void ZzTimelineTest::keyboardActivationReportsActualEvent()
{
    ZzTimeline timeline;
    timeline.resize(500, 250);
    timeline.addEvent({}, QStringLiteral("First"));
    auto* second = timeline.addEvent({}, QStringLiteral("Second"));
    timeline.setCurrentIndex(timeline.model()->index(1, 0));
    timeline.show();
    timeline.setFocus();
    QSignalSpy activatedSpy(&timeline, &ZzTimeline::eventActivated);
#ifdef Q_OS_MACOS
    // macOS 上 QAbstractItemView 的键盘激活约定是 ⌘O，Return 只尝试进入编辑
    QTest::keyClick(&timeline, Qt::Key_O, Qt::ControlModifier);
#else
    QTest::keyClick(&timeline, Qt::Key_Return);
#endif
    QCOMPARE(activatedSpy.size(), 1);
    QCOMPARE(qvariant_cast<ZzTimelineEvent*>(activatedSpy.at(0).at(0)), second);
}

void ZzTimelineTest::statusNodesUseReferenceColors()
{
    ZzTimeline timeline;
    timeline.resize(500, 340);
    timeline.setAnimationEnabled(false);
    timeline.addEvent({}, QStringLiteral("Completed"), {}, ZzTimelineEvent::Completed);
    timeline.addEvent({}, QStringLiteral("Warning"), {}, ZzTimelineEvent::Warning);
    timeline.addEvent({}, QStringLiteral("Error"), {}, ZzTimelineEvent::Error);
    timeline.show();
    ZZ_VERIFY_EVENTUALLY(timeline.visualRect(timeline.model()->index(2, 0)).isValid());
    const QImage image = timeline.viewport()->grab().toImage().convertToFormat(QImage::Format_RGB32);
    const QList<QColor> expected = { QColor(QStringLiteral("#107C10")), QColor(QStringLiteral("#F2A900")),
        QColor(QStringLiteral("#D13438")) };
    for (const QColor target : expected) {
        int matches = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const QColor pixel = image.pixelColor(x, y);
                if (qAbs(pixel.red() - target.red()) < 8 && qAbs(pixel.green() - target.green()) < 8
                    && qAbs(pixel.blue() - target.blue()) < 8)
                    ++matches;
            }
        QVERIFY2(matches > 5, qPrintable(target.name()));
    }
}

void ZzTimelineTest::clearResetCallbackMayTransferEvent()
{
    ZzTimeline source;
    ZzTimeline destination;
    auto* event = source.addEvent({}, QStringLiteral("Keep"));
    QPointer<ZzTimelineEvent> alive(event);
    QObject::connect(source.model(), &QAbstractItemModel::modelAboutToBeReset, &source,
        [&] { destination.addEvent(event); });
    source.clearEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(alive);
    QCOMPARE(source.model()->rowCount(), 0);
    QCOMPARE(destination.eventAt(0), event);
}

void ZzTimelineTest::takeCallbackReownershipReturnsNull()
{
    ZzTimeline source;
    ZzTimeline destination;
    auto* event = source.addEvent({}, QStringLiteral("Retaken"));
    QObject::connect(&source, &ZzTimeline::eventsChanged, &source, [&] {
        if (!source.events().contains(event) && !destination.events().contains(event))
            destination.addEvent(event);
    });
    QCOMPARE(source.takeEvent(event), nullptr);
    QCOMPARE(destination.eventAt(0), event);
    QCOMPARE(event->parent(), &destination);
}

void ZzTimelineTest::externalReparentDefersModelNotification()
{
    ZzTimeline timeline;
    QObject other;
    auto* event = timeline.addEvent({}, QStringLiteral("Moving"));
    QPointer<ZzTimelineEvent> alive(event);
    bool insideParentChange = true;
    bool notifiedInside = false;
    QObject::connect(timeline.model(), &QAbstractItemModel::rowsAboutToBeRemoved, &timeline, [&] {
        notifiedInside = insideParentChange;
        if (!insideParentChange)
            delete event;
    });
    event->setParent(&other);
    insideParentChange = false;
    QVERIFY(!notifiedInside);
    QVERIFY(timeline.events().isEmpty());
    QCOMPARE(timeline.eventAt(0), nullptr);
    ZZ_COMPARE_EVENTUALLY(timeline.model()->rowCount(), 0);
    QVERIFY(!alive);
}

void ZzTimelineTest::externalDeleteDefersModelNotification()
{
    auto* timeline = new ZzTimeline;
    auto* event = timeline->addEvent({}, QStringLiteral("Deleting"));
    QPointer<ZzTimeline> alive(timeline);
    bool insideEventDelete = true;
    bool notifiedInside = false;
    QObject::connect(timeline->model(), &QAbstractItemModel::rowsAboutToBeRemoved, timeline, [&] {
        notifiedInside = insideEventDelete;
        if (!insideEventDelete)
            delete timeline;
    });
    delete event;
    insideEventDelete = false;
    QVERIFY(!notifiedInside);
    ZZ_VERIFY_EVENTUALLY(!alive);
}

void ZzTimelineTest::insertionCallbackTransfersInFlightEvent()
{
    ZzTimeline source;
    ZzTimeline destination;
    auto* event = new ZzTimelineEvent({}, QStringLiteral("Transfer"));
    bool didTransfer = false;
    QObject::connect(source.model(), &QAbstractItemModel::rowsAboutToBeInserted, &source, [&] {
        if (!didTransfer) {
            didTransfer = true;
            destination.addEvent(event);
        }
    });
    source.addEvent(event);
    QVERIFY(source.events().isEmpty());
    QCOMPARE(source.model()->rowCount(), 0);
    QCOMPARE(destination.eventAt(0), event);
    QCOMPARE(event->parent(), &destination);
}

void ZzTimelineTest::queuedAppendCanBeRemoved()
{
    ZzTimeline timeline;
    auto* second = new ZzTimelineEvent({}, QStringLiteral("Queued"));
    QPointer<ZzTimelineEvent> alive(second);
    bool didQueue = false;
    QObject::connect(timeline.model(), &QAbstractItemModel::rowsAboutToBeInserted, &timeline, [&] {
        if (!didQueue) {
            didQueue = true;
            timeline.addEvent(second);
            timeline.removeEvent(second);
        }
    });
    auto* first = timeline.addEvent({}, QStringLiteral("First"));
    QCOMPARE(timeline.events(), QList<ZzTimelineEvent*>({ first }));
    QCOMPARE(timeline.model()->rowCount(), 1);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!alive);
}

void ZzTimelineTest::queuedAppendCanBeTransferred()
{
    ZzTimeline source;
    ZzTimeline destination;
    auto* second = new ZzTimelineEvent({}, QStringLiteral("Queued"));
    bool didQueue = false;
    QObject::connect(source.model(), &QAbstractItemModel::rowsAboutToBeInserted, &source, [&] {
        if (!didQueue) {
            didQueue = true;
            source.addEvent(second);
            destination.addEvent(second);
        }
    });
    auto* first = source.addEvent({}, QStringLiteral("First"));
    QCOMPARE(source.events(), QList<ZzTimelineEvent*>({ first }));
    QCOMPARE(destination.eventAt(0), second);
    QCOMPARE(second->parent(), &destination);
}

void ZzTimelineTest::nestedReverseUsesLatestRequest()
{
    ZzTimeline timeline;
    bool didSet = false;
    QObject::connect(timeline.model(), &QAbstractItemModel::rowsAboutToBeInserted, &timeline, [&] {
        if (!didSet) {
            didSet = true;
            timeline.setReverse(true);
            timeline.setReverse(false);
        }
    });
    auto* first = timeline.addEvent({}, QStringLiteral("First"));
    timeline.addEvent({}, QStringLiteral("Second"));
    QVERIFY(!timeline.isReverse());
    QCOMPARE(timeline.eventAt(0), first);
    QCOMPARE(timeline.model()->index(0, 0).data().toString(), QStringLiteral("First"));
}

void ZzTimelineTest::themeReducedMotionStopsRunningPulse()
{
    ZzThemeController controller;
    std::unique_ptr<QStyle> fusion(QStyleFactory::create(QStringLiteral("Fusion")));
    QVERIFY(fusion);
    ZzFluentStyle style(&controller, fusion.release());
    ZzTimeline timeline;
    timeline.setStyle(&style);
    timeline.addEvent({}, QStringLiteral("Current"), {}, ZzTimelineEvent::Current);
    timeline.show();
    auto* animation = timeline.findChild<QVariantAnimation*>();
    QVERIFY(animation);
    QCOMPARE(animation->state(), QAbstractAnimation::Running);
    controller.setReducedMotion(true);
    QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
}

void ZzTimelineTest::constructorRejectsInvalidStatus()
{
    ZzTimelineEvent event({}, QStringLiteral("Invalid"), {}, static_cast<ZzTimelineEvent::ZzTimelineStatus>(7));
    QCOMPARE(event.status(), ZzTimelineEvent::Normal);
}

void ZzTimelineTest::detachedEventStopsOldTimelineNotifications()
{
    ZzTimeline timeline;
    QObject other;
    auto* event = timeline.addEvent({}, QStringLiteral("Before"));
    QSignalSpy changes(&timeline, &ZzTimeline::eventsChanged);
    event->setParent(&other);
    event->setTitle(QStringLiteral("After"));
    QCOMPARE(changes.size(), 0);
    ZZ_COMPARE_EVENTUALLY(timeline.model()->rowCount(), 0);
    QCOMPARE(changes.size(), 1);
    event->setTitle(QStringLiteral("Later"));
    QCOMPARE(changes.size(), 1);
}

void ZzTimelineTest::readdDetachedEventBeforeQueuedRemoval()
{
    ZzTimeline timeline;
    QObject other;
    auto* event = timeline.addEvent({}, QStringLiteral("Return"));
    event->setParent(&other);
    QVERIFY(timeline.events().isEmpty());
    timeline.addEvent(event);
    QCOMPARE(event->parent(), &timeline);
    QCOMPARE(timeline.events(), QList<ZzTimelineEvent*>({ event }));
    QCoreApplication::processEvents();
    QCOMPARE(timeline.model()->rowCount(), 1);
    QCOMPARE(timeline.eventAt(0), event);
    QCOMPARE(timeline.model()->index(0, 0).data().toString(), QStringLiteral("Return"));
    QSignalSpy dataChanged(timeline.model(), &QAbstractItemModel::dataChanged);
    event->setTitle(QStringLiteral("Returned title"));
    QCOMPARE(dataChanged.size(), 1);
    QCOMPARE(timeline.model()->index(0, 0).data().toString(), QStringLiteral("Returned title"));
}

void ZzTimelineTest::cancelledAppendKeepsNewAppendOrder()
{
    ZzTimeline timeline;
    auto* second = new ZzTimelineEvent({}, QStringLiteral("Second"));
    auto* third = new ZzTimelineEvent({}, QStringLiteral("Third"));
    bool didQueue = false;
    QObject::connect(timeline.model(), &QAbstractItemModel::rowsAboutToBeInserted, &timeline, [&] {
        if (didQueue)
            return;
        didQueue = true;
        timeline.addEvent(second);
        QCOMPARE(timeline.takeEvent(second), second);
        timeline.addEvent(third);
        timeline.addEvent(second);
    });
    auto* first = timeline.addEvent({}, QStringLiteral("First"));
    QCOMPARE(timeline.events(), QList<ZzTimelineEvent*>({ first, third, second }));
    QCOMPARE(timeline.model()->index(1, 0).data().toString(), QStringLiteral("Third"));
    QCOMPARE(timeline.model()->index(2, 0).data().toString(), QStringLiteral("Second"));
}

QTEST_MAIN(ZzTimelineTest)
#include "ZzTimelineTest.moc"
