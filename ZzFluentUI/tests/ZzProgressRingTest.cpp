#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPair>
#include <QtCore/QTimer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QProxyStyle>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzThemeController.h>

namespace {

/** @brief 为动画生命周期测试提供确定启用动效的基础样式。 */
class ZzProgressAnimationStyle final : public QProxyStyle
{
public:
    /** @brief 对 Widget 动效返回启用，其余提示委托平台样式。 */
    [[nodiscard]] int styleHint(
        StyleHint hint,
        const QStyleOption *option = nullptr,
        const QWidget *widget = nullptr,
        QStyleHintReturn *returnData = nullptr) const override
    {
        if (hint == SH_Widget_Animate) {
            return 1;
        }
        return QProxyStyle::styleHint(
            hint,
            option,
            widget,
            returnData);
    }
};

/** @brief 使用高区分度 palette 渲染单个环形进度。 */
QImage zzRenderRing(
    ZzFluentUI::ZzProgressRing *ring,
    int minimum,
    int maximum,
    int value,
    bool inverted = false)
{
    Q_ASSERT(ring != nullptr);
    QPalette palette = ring->palette();
    palette.setColor(QPalette::Active, QPalette::Mid, Qt::black);
    palette.setColor(QPalette::Inactive, QPalette::Mid, Qt::black);
    palette.setColor(QPalette::Disabled, QPalette::Mid, Qt::darkGray);
    palette.setColor(QPalette::Active, QPalette::Highlight, Qt::red);
    palette.setColor(QPalette::Inactive, QPalette::Highlight, Qt::red);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, Qt::gray);
    palette.setColor(QPalette::Active, QPalette::Text, Qt::green);
    palette.setColor(QPalette::Inactive, QPalette::Text, Qt::green);
    ring->setPalette(palette);
    ring->setRange(minimum, maximum);
    ring->setValue(value);
    ring->setInvertedAppearance(inverted);
    ring->resize(80, 80);

    QImage image(ring->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    ring->render(&painter);
    painter.end();
    return image;
}

/** @brief 统计由高区分度 palette 绘制的红色进度像素。 */
int zzRedPixelCount(const QImage &image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.red() > color.green() + 48
                && color.red() > color.blue() + 48
                && color.alpha() > 64) {
                ++count;
            }
        }
    }
    return count;
}

/** @brief 分别统计图像左右半区的红色进度像素。 */
QPair<int, int> zzRedHalfCounts(const QImage &image)
{
    int left = 0;
    int right = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.red() <= color.green() + 48
                || color.red() <= color.blue() + 48
                || color.alpha() <= 64) {
                continue;
            }
            if (x < image.width() / 2) {
                ++left;
            } else {
                ++right;
            }
        }
    }
    return {left, right};
}

} // namespace

/** @brief 验证环形进度复用 Qt 语义并维持单动画与稳定绘制。 */
class ZzProgressRingTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableDefaults()
    {
        ZzFluentUI::ZzProgressRing ring;

        QCOMPARE(ring.minimum(), 0);
        QCOMPARE(ring.maximum(), 100);
        QCOMPARE(ring.value(), 0);
        QCOMPARE(ring.ringWidth(), 4);
        QCOMPARE(ring.sizeHint(), QSize(48, 48));
        QCOMPARE(ring.minimumSizeHint(), QSize(24, 24));
        QCOMPARE(ring.findChildren<QVariantAnimation *>().size(), 1);
        QVERIFY(ring.findChildren<QTimer *>().isEmpty());
    }

    void emitsRingWidthOnlyForEffectiveChanges()
    {
        ZzFluentUI::ZzProgressRing ring;
        QSignalSpy spy(
            &ring,
            &ZzFluentUI::ZzProgressRing::ringWidthChanged);

        ring.setRingWidth(4);
        QCOMPARE(spy.count(), 0);
        ring.setRingWidth(0);
        QCOMPARE(ring.ringWidth(), 1);
        QCOMPARE(spy.count(), 1);
        ring.setRingWidth(-10);
        QCOMPARE(spy.count(), 1);
        ring.setRingWidth(64);
        QCOMPARE(ring.ringWidth(), 64);
        QCOMPARE(spy.count(), 2);
        ring.setRingWidth(100);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(ring.minimumSizeHint(), QSize(132, 132));
        QCOMPARE(ring.sizeHint(), QSize(132, 132));
    }

    void preservesNativeRangeValueAndSignalSemantics()
    {
        ZzFluentUI::ZzProgressRing ring;
        QSignalSpy valueSpy(&ring, &QProgressBar::valueChanged);

        ring.setRange(20, 120);
        QCOMPARE(ring.minimum(), 20);
        QCOMPARE(ring.maximum(), 120);
        valueSpy.clear();
        ring.setValue(70);
        QCOMPARE(valueSpy.count(), 1);
        QCOMPARE(ring.value(), 70);
        ring.setValue(70);
        QCOMPARE(valueSpy.count(), 1);
        ring.setMinimum(80);
        QCOMPARE(ring.minimum(), 80);
        QCOMPARE(ring.maximum(), 120);
        QVERIFY(ring.value() < ring.minimum());
        ring.setMaximum(60);
        QCOMPARE(ring.minimum(), 60);
        QCOMPARE(ring.maximum(), 60);
    }

    void rendersDeterminateIndeterminateAndEqualRanges()
    {
        ZzFluentUI::ZzProgressRing ring;
        ring.setTextVisible(false);

        const int emptyCount = zzRedPixelCount(
            zzRenderRing(&ring, 20, 120, 20));
        const int halfCount = zzRedPixelCount(
            zzRenderRing(&ring, 20, 120, 70));
        const int fullCount = zzRedPixelCount(
            zzRenderRing(&ring, 20, 120, 120));
        const int busyCount = zzRedPixelCount(
            zzRenderRing(&ring, 0, 0, 0));
        const QImage equalImage = zzRenderRing(&ring, 5, 5, 5);

        QCOMPARE(emptyCount, 0);
        QVERIFY(halfCount > emptyCount);
        QVERIFY(fullCount > halfCount);
        QVERIFY(busyCount > emptyCount);
        QVERIFY(busyCount < fullCount);
        QVERIFY(!equalImage.isNull());

        ring.setTextVisible(true);
        ring.setFormat(QString(400, QLatin1Char('W')));
        QVERIFY(!zzRenderRing(&ring, 0, 100, 72).isNull());
    }

    void invertedAppearanceReversesArcDirection()
    {
        ZzFluentUI::ZzProgressRing ring;
        ring.setTextVisible(false);
        const QPair<int, int> normal = zzRedHalfCounts(
            zzRenderRing(&ring, 0, 100, 25, false));
        const QPair<int, int> inverted = zzRedHalfCounts(
            zzRenderRing(&ring, 0, 100, 25, true));

        QVERIFY(normal.second > normal.first);
        QVERIFY(inverted.first > inverted.second);
    }

    void startsAndStopsOneAnimationFromLifecycle()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(
            &controller,
            new ZzProgressAnimationStyle);
        ZzFluentUI::ZzProgressRing ring;
        ring.setStyle(&style);
        ring.setRange(0, 0);
        auto *animation = ring.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);

        ring.show();
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));
        ring.hide();
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        ring.show();
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));
        ring.setEnabled(false);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        ring.setEnabled(true);
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));
        ring.setRange(0, 100);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        ring.setRange(0, 0);
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));
        controller.setReducedMotion(true);
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Stopped;
        }));
    }

    void repeatedStateChangesDoNotGrowObjects()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(
            &controller,
            new ZzProgressAnimationStyle);
        ZzFluentUI::ZzProgressRing ring;
        ring.setStyle(&style);
        ring.show();
        const qsizetype descendants = ring.findChildren<QObject *>().size();
        const qsizetype animations =
            ring.findChildren<QAbstractAnimation *>().size();
        const qsizetype timers = ring.findChildren<QTimer *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            if ((iteration % 2) == 0) {
                ring.setRange(0, 0);
            } else {
                ring.setRange(20, 120);
                ring.setValue(20 + (iteration % 101));
            }
        }

        QCOMPARE(ring.findChildren<QObject *>().size(), descendants);
        QCOMPARE(
            ring.findChildren<QAbstractAnimation *>().size(),
            animations);
        QCOMPARE(ring.findChildren<QTimer *>().size(), timers);
        QCOMPARE(animations, 1);
        QCOMPARE(timers, 0);
        ring.hide();
        QCOMPARE(
            ring.findChild<QVariantAnimation *>()->state(),
            QAbstractAnimation::Stopped);
    }

    void exposesProgressBarAccessibility()
    {
        ZzFluentUI::ZzProgressRing ring;
        ring.setAccessibleName(QStringLiteral("Build progress"));
        ring.setRange(10, 90);
        ring.setValue(40);
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&ring);

        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::ProgressBar);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Build progress"));
        QAccessibleValueInterface *valueInterface =
            interface->valueInterface();
        QVERIFY(valueInterface != nullptr);
        QCOMPARE(valueInterface->minimumValue().toInt(), 10);
        QCOMPARE(valueInterface->maximumValue().toInt(), 90);
        QCOMPARE(valueInterface->currentValue().toInt(), 40);
    }

    void destroysRunningAnimationWithoutDeferredCallbacks()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(
            &controller,
            new ZzProgressAnimationStyle);
        auto *ring = new ZzFluentUI::ZzProgressRing;
        ring->setStyle(&style);
        ring->setRange(0, 0);
        ring->show();
        auto *animation = ring->findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));

        ring->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

QTEST_MAIN(ZzProgressRingTest)

#include "ZzProgressRingTest.moc"
