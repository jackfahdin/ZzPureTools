#include <QtTest/QTest>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtTest/QSignalSpy>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QProxyStyle>
#include <limits>

#include <ZzFluentUI/ZzRadialGauge.h>
#include <ZzFluentUI/ZzMultiRadialGauge.h>
#include <ZzFluentUI/ZzMultiProgressRing.h>
using namespace ZzFluentUI;

class ZzRadialGaugeTest final : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void interactionAndTracking();
    void animationInterruption();
    void itemLifetime();
    void finiteInputsAndSmallPaint();
    void multiAnimationLifecycle();
    void angleMappingAndExtremeRange();
    void itemOwnershipTransfer();
    void rangeNotificationReentry();
    void rangeOwnershipTransfer();
    void multiGaugeOwnershipReentry();
    void multiRingOwnershipReentry();
    void consecutiveAnimationsDoNotPublishConfigurationValues();
    void transferredItemsCanBeDeletedFromSourceNotification();
    void transferredRangeCanBeDeletedFromSourceNotification();
    void animationTargetNotificationCanDeleteGauge();
    void boundedAnglesNotifyOnlyEffectiveChanges();
};

class MotionStyle final : public QProxyStyle {
  public:
    bool motion = true;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
                  QStyleHintReturn *data = nullptr) const override
    {
        return hint == SH_Widget_Animate ? int(motion) : QProxyStyle::styleHint(hint, option, widget, data);
    }
};

void ZzRadialGaugeTest::interactionAndTracking()
{
    ZzRadialGauge gauge;
    gauge.resize(240, 240);
    gauge.setRange(0, 100);
    gauge.setValueAnimationDuration(0);
    gauge.show();
    QTest::mouseClick(&gauge, Qt::LeftButton, {}, QPoint(210, 210));
    QCOMPARE(gauge.value(), 100);
    QTest::mouseClick(&gauge, Qt::LeftButton, {}, QPoint(30, 210));
    QCOMPARE(gauge.value(), 0);
    gauge.setTracking(false);
    QTest::mousePress(&gauge, Qt::LeftButton, {}, QPoint(120, 20));
    QCOMPARE(gauge.value(), 0);
    QCOMPARE(gauge.sliderPosition(), 50);
    QTest::mouseRelease(&gauge, Qt::LeftButton, {}, QPoint(120, 20));
    QCOMPARE(gauge.value(), 50);
    QTest::keyClick(&gauge, Qt::Key_Right);
    QCOMPARE(gauge.value(), 51);
    QWheelEvent wheel(QPointF(120, 120), QPointF(120, 120), {}, QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(&gauge, &wheel);
    QVERIFY(gauge.value() > 51);
    gauge.setInteractive(false);
    const int before = gauge.value();
    QTest::mouseClick(&gauge, Qt::LeftButton, {}, QPoint(30, 210));
    QTest::keyClick(&gauge, Qt::Key_Left);
    QApplication::sendEvent(&gauge, &wheel);
    QCOMPARE(gauge.value(), before);
    gauge.setValue(17);
    QCOMPARE(gauge.value(), 17);
}

void ZzRadialGaugeTest::animationInterruption()
{
    MotionStyle style;
    ZzRadialGauge gauge;
    gauge.setStyle(&style);
    gauge.setRange(0, 100);
    gauge.show();
    gauge.setValue(90);
    QVERIFY(gauge.isValueAnimating());
    static_cast<QDial *>(&gauge)->setValue(23);
    QVERIFY(!gauge.isValueAnimating());
    QTest::qWait(550);
    QCOMPARE(gauge.value(), 23);
    gauge.setValue(80);
    gauge.hide();
    QVERIFY(!gauge.isValueAnimating());
    QCOMPARE(gauge.value(), 80);
    gauge.show();
    gauge.setValue(30);
    gauge.setEnabled(false);
    QVERIFY(!gauge.isValueAnimating());
    QCOMPARE(gauge.value(), 30);
    gauge.setEnabled(true);
    gauge.setValue(60);
    style.motion = false;
    QEvent changed(QEvent::StyleChange);
    QApplication::sendEvent(&gauge, &changed);
    QVERIFY(!gauge.isValueAnimating());
    QCOMPARE(gauge.value(), 60);
}

void ZzRadialGaugeTest::itemLifetime()
{
    ZzRadialGauge gauge;
    auto *range = gauge.addRange(0, 50, Qt::red);
    delete range;
    QVERIFY(gauge.ranges().isEmpty());
    gauge.addRange(0, 100, Qt::green);
    gauge.clearRanges();
    QVERIFY(gauge.ranges().isEmpty());
    ZzMultiRadialGauge multi;
    auto *item = multi.addItem(QStringLiteral("CPU"), 20, Qt::red);
    multi.addItem(item);
    QCOMPARE(multi.items().size(), 1);
    delete item;
    QVERIFY(multi.items().isEmpty());
    item = multi.addItem(QStringLiteral("CPU"), 20, Qt::red);
    connect(item, &ZzMultiRadialGaugeItem::valueChanged, &multi, [item] { delete item; });
    item->setValue(40);
    QVERIFY(multi.items().isEmpty());
    ZzMultiProgressRing rings;
    auto *ring = rings.addItem(QStringLiteral("RAM"), 42, Qt::blue);
    rings.addItem(ring);
    QCOMPARE(rings.items().size(), 1);
    QPointer<ZzMultiProgressRingItem> guard(ring);
    rings.clearItems();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!guard);
    QVERIFY(rings.items().isEmpty());
}

void ZzRadialGaugeTest::finiteInputsAndSmallPaint()
{
    const qreal nan = std::numeric_limits<qreal>::quiet_NaN();
    const qreal inf = std::numeric_limits<qreal>::infinity();
    ZzRadialGauge gauge;
    gauge.setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    gauge.setScaleWidth(nan);
    gauge.setMinimumAngle(inf);
    QCOMPARE(gauge.scaleWidth(), 8.0);
    gauge.setMajorTickCount(std::numeric_limits<int>::max());
    gauge.setMinorTickCount(std::numeric_limits<int>::max());
    gauge.resize(8, 8);
    QVERIFY(!gauge.grab().isNull());
    ZzMultiRadialGauge multi;
    auto *item = multi.addItem(QString(), 20);
    item->setValue(nan);
    QCOMPARE(item->value(), 20.0);
    multi.setRange(-std::numeric_limits<qreal>::max(), std::numeric_limits<qreal>::max());
    multi.resize(8, 8);
    QVERIFY(!multi.grab().isNull());
    ZzMultiProgressRing ring;
    ring.addItem(QString(), inf);
    QCOMPARE(ring.items().first()->value(), 0.0);
    ring.resize(8, 8);
    QVERIFY(!ring.grab().isNull());
}

void ZzRadialGaugeTest::multiAnimationLifecycle()
{
    MotionStyle style;
    ZzMultiRadialGauge gauge;
    ZzMultiProgressRing ring;
    gauge.setStyle(&style);
    ring.setStyle(&style);
    gauge.show();
    ring.show();
    auto *item = gauge.addItem(QStringLiteral("CPU"), 0);
    auto *ringItem = ring.addItem(QStringLiteral("RAM"), 0);
    item->setValue(100);
    ringItem->setValue(100);
    auto *gaugeAnimation = gauge.findChild<QVariantAnimation *>();
    auto *ringAnimation = ring.findChild<QVariantAnimation *>();
    QVERIFY(gaugeAnimation);
    QVERIFY(ringAnimation);
    QCOMPARE(gaugeAnimation->state(), QAbstractAnimation::Running);
    QCOMPARE(ringAnimation->state(), QAbstractAnimation::Running);
    gauge.hide();
    ring.setEnabled(false);
    QCOMPARE(gaugeAnimation->state(), QAbstractAnimation::Stopped);
    QCOMPARE(ringAnimation->state(), QAbstractAnimation::Stopped);
    item->setValue(42);
    ringItem->setValue(42);
    QCOMPARE(gaugeAnimation->state(), QAbstractAnimation::Stopped);
    QCOMPARE(ringAnimation->state(), QAbstractAnimation::Stopped);
    gauge.show();
    ring.setEnabled(true);
    item->setValue(80);
    ringItem->setValue(80);
    style.motion = false;
    QEvent event(QEvent::StyleChange);
    QApplication::sendEvent(&gauge, &event);
    QApplication::sendEvent(&ring, &event);
    QCOMPARE(gaugeAnimation->state(), QAbstractAnimation::Stopped);
    QCOMPARE(ringAnimation->state(), QAbstractAnimation::Stopped);
    style.motion = true;
    item->setValue(10);
    ringItem->setValue(10);
    delete item;
    delete ringItem;
    QCOMPARE(gaugeAnimation->state(), QAbstractAnimation::Stopped);
    QCOMPARE(ringAnimation->state(), QAbstractAnimation::Stopped);
}

void ZzRadialGaugeTest::angleMappingAndExtremeRange()
{
    ZzRadialGauge gauge;
    gauge.resize(240, 240);
    gauge.setValueAnimationDuration(0);
    gauge.setRange(0, 100);
    gauge.show();
    QTest::mouseClick(&gauge, Qt::LeftButton, {}, QPoint(100, 220));
    QCOMPARE(gauge.value(), 0);
    QTest::mouseClick(&gauge, Qt::LeftButton, {}, QPoint(140, 220));
    QCOMPARE(gauge.value(), 100);
    gauge.setMinimumAngle(135);
    gauge.setMaximumAngle(-135);
    QTest::mouseClick(&gauge, Qt::LeftButton, {}, QPoint(120, 220));
    QCOMPARE(gauge.value(), 50);
    gauge.setMinimumAngle(0);
    gauge.setMaximumAngle(360);
    QTest::mouseClick(&gauge, Qt::LeftButton, {}, QPoint(20, 120));
    QCOMPARE(gauge.value(), 75);
    gauge.setValue(100);
    QTest::mouseClick(&gauge, Qt::LeftButton, {}, QPoint(120, 20));
    QCOMPARE(gauge.value(), 100);
    gauge.setMinimumAngle(-135);
    gauge.setMaximumAngle(135);
    gauge.setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    QTest::mouseClick(&gauge, Qt::LeftButton, {}, QPoint(120, 20));
    QCOMPARE(gauge.value(), 0);
    gauge.setRange(7, 7);
    QTest::mouseClick(&gauge, Qt::LeftButton, {}, QPoint(20, 120));
    QCOMPARE(gauge.value(), 7);
}

void ZzRadialGaugeTest::itemOwnershipTransfer()
{
    ZzMultiRadialGauge source;
    ZzMultiRadialGauge target;
    auto *item = source.addItem(QStringLiteral("CPU"), 5);
    target.addItem(item);
    QVERIFY(source.items().isEmpty());
    QCOMPARE(target.items().size(), 1);
    source.clearItems();
    QCOMPARE(item->parent(), &target);
    target.removeItem(item);
    target.addItem(item);
    QVERIFY(target.items().isEmpty());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void ZzRadialGaugeTest::rangeNotificationReentry()
{
    ZzMultiRadialGauge gauge;
    QSignalSpy spy(&gauge, &ZzMultiRadialGauge::maximumChanged);
    connect(&gauge, &ZzMultiRadialGauge::minimumChanged, &gauge, [&gauge](qreal minimum) {
        if (minimum == 10)
            gauge.setRange(20, 30);
    });
    gauge.setRange(10, 90);
    QCOMPARE(gauge.maximum(), 30.0);
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.last().first().toReal(), 30.0);
}

void ZzRadialGaugeTest::rangeOwnershipTransfer()
{
    ZzRadialGauge gauge;
    QObject owner;
    auto *range = gauge.addRange(0, 50, Qt::red);
    range->setParent(&owner);
    QVERIFY(gauge.ranges().isEmpty());
    gauge.clearRanges();
    QCOMPARE(range->parent(), &owner);
}

template <typename Gauge> void verifyOwnershipReentry()
{
    auto source = std::make_unique<Gauge>();
    auto target = std::make_unique<Gauge>();
    auto *item = source->addItem(QStringLiteral("CPU"), 5);
    QPointer itemGuard(item);
    QSignalSpy targetChanges(target.get(), &Gauge::itemsChanged);
    int sourceNotifications = 0;
    QObject::connect(source.get(), &Gauge::itemsChanged, source.get(), [&] {
        ++sourceNotifications;
        target->addItem(item);
    });
    target->addItem(item);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);

    const qsizetype targetCount = target->items().size();
    if (targetCount != 1) {
        // 已损坏的 QObject child 表可能重复持有裸指针；仅在 RED 时避免析构遮蔽断言。
        (void)source.release();
        (void)target.release();
        item->setParent(nullptr);
    }
    QCOMPARE(targetCount, 1);
    QCOMPARE(sourceNotifications, 1);
    QVERIFY(source->items().isEmpty());
    QCOMPARE(item->parent(), target.get());
    QCOMPARE(target->children().count(item), 1);
    QCOMPARE(targetChanges.size(), 1);

    target->removeItem(item);
    QVERIFY(target->items().isEmpty());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(itemGuard.isNull());
    QVERIFY(target->items().isEmpty());
    QCOMPARE(targetChanges.size(), 2);
    target->resize(120, 120);
    QVERIFY(!target->grab().isNull());
}

void ZzRadialGaugeTest::multiGaugeOwnershipReentry()
{
    verifyOwnershipReentry<ZzMultiRadialGauge>();
}

void ZzRadialGaugeTest::multiRingOwnershipReentry()
{
    verifyOwnershipReentry<ZzMultiProgressRing>();
}

void ZzRadialGaugeTest::consecutiveAnimationsDoNotPublishConfigurationValues()
{
    MotionStyle style;
    ZzRadialGauge gauge;
    gauge.setStyle(&style);
    gauge.setRange(0, 100);
    gauge.setValueAnimationDuration(60);
    gauge.show();
    gauge.setValue(90);
    QTRY_VERIFY(!gauge.isValueAnimating());
    QCOMPARE(gauge.value(), 90);

    QSignalSpy changes(&gauge, &ZzRadialGauge::valueChanged);
    gauge.setValue(20);
    QVERIFY2(changes.isEmpty(), "配置下一次动画不应提前发布目标或回跳起点");
    QCOMPARE(gauge.value(), 90);
    QVERIFY(gauge.isValueAnimating());
    QTRY_VERIFY(!gauge.isValueAnimating());
    QCOMPARE(gauge.value(), 20);
    QVERIFY(!changes.isEmpty());
    int previous = 90;
    for (const auto &arguments : changes) {
        const int value = arguments.first().toInt();
        QVERIFY(value <= previous);
        previous = value;
    }
}

template <typename Gauge> void verifyDeletionFromSourceNotification()
{
    Gauge source;
    Gauge target;
    auto *item = source.addItem(QStringLiteral("CPU"), 5);
    QPointer itemGuard(item);
    bool transferComplete = false;
    bool prematureNotification = false;
    QObject::connect(&source, &Gauge::itemsChanged, &source, [&] {
        if (!transferComplete) {
            prematureNotification = true;
            return;
        }
        delete item;
    });
    target.addItem(item);
    transferComplete = true;
    QVERIFY2(!prematureNotification, "源控件不应在 QObject 父子关系更新途中通知外部槽");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QVERIFY(itemGuard.isNull());
    QVERIFY(source.items().isEmpty());
    QVERIFY(target.items().isEmpty());
}

void ZzRadialGaugeTest::transferredItemsCanBeDeletedFromSourceNotification()
{
    verifyDeletionFromSourceNotification<ZzMultiRadialGauge>();
    verifyDeletionFromSourceNotification<ZzMultiProgressRing>();
}

void ZzRadialGaugeTest::transferredRangeCanBeDeletedFromSourceNotification()
{
    ZzRadialGauge source;
    QObject owner;
    auto *range = source.addRange(0, 50, Qt::red);
    QPointer rangeGuard(range);
    bool transferComplete = false;
    bool prematureNotification = false;
    connect(&source, &ZzRadialGauge::rangesChanged, &source, [&] {
        if (!transferComplete) {
            prematureNotification = true;
            return;
        }
        delete range;
    });
    range->setParent(&owner);
    transferComplete = true;
    QVERIFY2(!prematureNotification, "Range 转移通知不得暴露尚未完成的 QObject 操作");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QVERIFY(rangeGuard.isNull());
    QVERIFY(source.ranges().isEmpty());
}

void ZzRadialGaugeTest::animationTargetNotificationCanDeleteGauge()
{
    MotionStyle style;
    auto gauge = std::make_unique<ZzRadialGauge>();
    gauge->setStyle(&style);
    gauge->setValueAnimationDuration(60);
    gauge->show();
    gauge->setValue(90);
    QTRY_VERIFY(!gauge->isValueAnimating());
    bool configuring = true;
    bool prematureNotification = false;
    connect(gauge.get(), &ZzRadialGauge::valueChanged, this, [&](int value) {
        if (value != 20)
            return;
        if (configuring) {
            prematureNotification = true;
            return;
        }
        gauge.reset();
    });
    gauge->setValue(20);
    configuring = false;
    QVERIFY2(!prematureNotification, "动画配置阶段不得触发可销毁控件的业务通知");
    QTRY_VERIFY(!gauge);
}

void ZzRadialGaugeTest::boundedAnglesNotifyOnlyEffectiveChanges()
{
    ZzMultiRadialGauge gauge;
    QSignalSpy minimumChanges(&gauge, &ZzMultiRadialGauge::minimumAngleChanged);
    QSignalSpy maximumChanges(&gauge, &ZzMultiRadialGauge::maximumAngleChanged);
    gauge.setMinimumAngle(360);
    gauge.setMaximumAngle(-360);
    gauge.setMinimumAngle(1000);
    gauge.setMaximumAngle(-1000);
    QCOMPARE(minimumChanges.size(), 1);
    QCOMPARE(maximumChanges.size(), 1);
}

QTEST_MAIN(ZzRadialGaugeTest)
#include "ZzRadialGaugeTest.moc"
