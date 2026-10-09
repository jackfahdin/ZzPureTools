#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QHoverEvent>
#include <QtGui/QFocusEvent>
#include <QtGui/QWheelEvent>
#include <limits>
#include <ZzFluentUI/ZzRangeSlider.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

class ZzRangeSliderTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void changingRangeDuringCoincidentHandoffCancelsDrag()
    {
        using Slider = ZzFluentUI::ZzRangeSlider;
        Slider slider;
        slider.resize(200, 32);
        slider.setValues(50, 50);
        slider.show();
        QSignalSpy pressed(&slider, &Slider::sliderPressed);
        QSignalSpy released(&slider, &Slider::sliderReleased);
        QObject::connect(&slider, &Slider::sliderReleased, &slider,
            [&] { slider.setRange(0, 200); });
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(100, 16));
        QTest::mouseMove(&slider, QPoint(145, 16));
        QTest::mouseRelease(&slider, Qt::LeftButton, {}, QPoint(145, 16));
        QCOMPARE(slider.lowerValue(), 50);
        QCOMPARE(slider.upperValue(), 50);
        QCOMPARE(pressed.size(), 1);
        QCOMPARE(released.size(), 1);
    }

    void highContrastGrooveRemainsVisible()
    {
        ZzFluentUI::ZzThemeController theme;
        theme.setMode(ZzFluentUI::ZzThemeMode::HighContrast);
        ZzFluentUI::ZzFluentStyle style(&theme);
        ZzFluentUI::ZzRangeSlider slider;
        slider.setStyle(&style);
        slider.setPalette(style.standardPalette());
        slider.setAutoFillBackground(true);
        slider.resize(200, 32);
        slider.setValues(20, 80);
        slider.show();
        const auto image = slider.grab().toImage();
        QVERIFY(image.pixelColor(5, 16) != image.pixelColor(5, 0));
        slider.setDisabled(true);
        const auto disabled = slider.grab().toImage();
        QVERIFY(disabled.pixelColor(5, 16) != disabled.pixelColor(5, 0));
    }

    void snapAlwaysTracksAndStopsAtPeer()
    {
        using Slider = ZzFluentUI::ZzRangeSlider;
        Slider slider;
        slider.resize(200, 32);
        slider.setRange(-50, 50);
        slider.setValues(-30, 30);
        slider.setSingleStep(10);
        slider.setSnapMode(Slider::ZzSnapMode::SnapAlways);
        slider.show();
        QSignalSpy changed(&slider, &Slider::valuesChanged);
        // 手柄内偏右 4 px 抓取，不应在按下时跳动。
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(50, 16));
        QCOMPARE(slider.lowerValue(), -30);
        QTest::mouseMove(&slider, QPoint(77, 16));
        QCOMPARE(slider.lowerValue(), -10);
        QVERIFY(!changed.isEmpty());
        QTest::mouseMove(&slider, QPoint(194, 16));
        QCOMPARE(slider.lowerValue(), 30);
        QCOMPARE(slider.upperValue(), 30);
        QTest::mouseRelease(&slider, Qt::LeftButton, {}, QPoint(194, 16));
    }

    void wheelAndKeyboardKeepEndpointsBounded()
    {
        ZzFluentUI::ZzRangeSlider slider;
        slider.setValues(20, 80);
        slider.setSingleStep(5);
        slider.setPageStep(10);
        slider.show();
        QTest::keyClick(&slider, Qt::Key_Up);
        QWheelEvent wheel(QPointF(50, 16), slider.mapToGlobal(QPoint(50, 16)),
            {}, QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&slider, &wheel);
        QCOMPARE(slider.upperValue(), 75);
        QTest::keyClick(&slider, Qt::Key_PageDown);
        QCOMPARE(slider.upperValue(), 65);
        QTest::keyClick(&slider, Qt::Key_Home);
        QCOMPARE(slider.upperValue(), 20);
        QTest::keyClick(&slider, Qt::Key_End);
        QCOMPARE(slider.upperValue(), 100);
        slider.setRange(7, 7);
        QTest::keyClick(&slider, Qt::Key_PageUp);
        QCoreApplication::sendEvent(&slider, &wheel);
        QCOMPARE(slider.lowerValue(), 7);
        QCOMPARE(slider.upperValue(), 7);
        QVERIFY(!slider.grab().isNull());
    }

    void interruptionCancelsPreview_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("disable") << 0;
        QTest::newRow("focus-out") << 1;
        QTest::newRow("orientation") << 2;
        QTest::newRow("range") << 3;
    }
    void interruptionCancelsPreview()
    {
        QFETCH(int, kind);
        ZzFluentUI::ZzRangeSlider slider;
        slider.resize(200, 32);
        slider.setValues(20, 80);
        slider.setTracking(false);
        slider.setValueTipEnabled(true);
        slider.show();
        QSignalSpy released(&slider, &ZzFluentUI::ZzRangeSlider::sliderReleased);
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(46, 16));
        QTest::mouseMove(&slider, QPoint(82, 16));
        QCOMPARE(slider.lowerPosition(), 40);
        if (kind == 0) slider.setDisabled(true);
        else if (kind == 1) {
            QFocusEvent event(QEvent::FocusOut);
            QCoreApplication::sendEvent(&slider, &event);
        } else if (kind == 2) slider.setOrientation(Qt::Vertical);
        else slider.setRange(10, 90);
        QCOMPARE(slider.lowerValue(), 20);
        QCOMPARE(slider.lowerPosition(), 20);
        QCOMPARE(released.size(), 1);
        auto *tip = slider.findChild<QWidget *>(QStringLiteral("zzRangeSliderValueTip"));
        QVERIFY(tip && !tip->isVisible());
    }

    void reducedMotionAndDisableStopHandleAnimation()
    {
        ZzFluentUI::ZzThemeController theme;
        theme.setReducedMotion(false);
        ZzFluentUI::ZzFluentStyle style(&theme);
        ZzFluentUI::ZzRangeSlider slider;
        slider.setStyle(&style);
        slider.resize(200, 32);
        slider.setValues(20, 80);
        slider.show();
        QCoreApplication::processEvents();
        const auto running = [&] {
            int count = 0;
            for (auto *animation : slider.findChildren<QVariantAnimation *>())
                count += animation->state() == QAbstractAnimation::Running;
            return count;
        };
        QHoverEvent enter(QEvent::HoverMove, QPointF(46, 16), QPointF(46, 16), QPointF(0, 0));
        QCoreApplication::sendEvent(&slider, &enter);
        QVERIFY(running() > 0);
        theme.setReducedMotion(true);
        QCOMPARE(running(), 0);
        theme.setReducedMotion(false);
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(46, 16));
        QVERIFY(running() > 0);
        slider.setDisabled(true);
        QCOMPARE(running(), 0);
        slider.setEnabled(true);
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(46, 16));
        slider.hide();
        QCOMPARE(running(), 0);
        QVERIFY(slider.findChildren<QVariantAnimation *>().size() <= 2);
    }

    void setValuesUpdatesBothEndpointsAtomically()
    {
        ZzFluentUI::ZzRangeSlider slider;
        QSignalSpy lower(&slider, &ZzFluentUI::ZzRangeSlider::lowerValueChanged);
        QSignalSpy upper(&slider, &ZzFluentUI::ZzRangeSlider::upperValueChanged);
        QSignalSpy both(&slider, &ZzFluentUI::ZzRangeSlider::valuesChanged);
        slider.setValues(75, 25);
        QCOMPARE(slider.lowerValue(), 25);
        QCOMPARE(slider.upperValue(), 75);
        QCOMPARE(lower.size(), 1);
        QCOMPARE(upper.size(), 1);
        QCOMPARE(both.size(), 1);
    }

    void rangeClampsBothEndpointsWithoutDuplicateSignals()
    {
        ZzFluentUI::ZzRangeSlider slider;
        slider.setValues(20, 80);
        QSignalSpy range(&slider, &ZzFluentUI::ZzRangeSlider::rangeChanged);
        QSignalSpy lower(&slider, &ZzFluentUI::ZzRangeSlider::lowerValueChanged);
        QSignalSpy upper(&slider, &ZzFluentUI::ZzRangeSlider::upperValueChanged);
        QSignalSpy both(&slider, &ZzFluentUI::ZzRangeSlider::valuesChanged);
        slider.setRange(70, 30);
        QCOMPARE(slider.minimum(), 30);
        QCOMPARE(slider.maximum(), 70);
        QCOMPARE(slider.lowerValue(), 30);
        QCOMPARE(slider.upperValue(), 70);
        QCOMPARE(range.size(), 1);
        QCOMPARE(lower.size(), 1);
        QCOMPARE(upper.size(), 1);
        QCOMPARE(both.size(), 1);
    }

    void rangeChangedObserversSeeValidEndpoints()
    {
        ZzFluentUI::ZzRangeSlider slider;
        slider.setValues(20, 80);
        bool validInCallback = false;
        QObject::connect(&slider, &ZzFluentUI::ZzRangeSlider::rangeChanged,
            &slider, [&] {
                validInCallback = slider.minimum() <= slider.lowerValue()
                    && slider.lowerValue() <= slider.upperValue()
                    && slider.upperValue() <= slider.maximum();
            });
        slider.setRange(30, 70);
        QVERIFY(validInCallback);
    }

    void reentrantLowerChangeStillNotifiesUpper()
    {
        ZzFluentUI::ZzRangeSlider slider;
        QSignalSpy upper(&slider, &ZzFluentUI::ZzRangeSlider::upperValueChanged);
        QObject::connect(&slider, &ZzFluentUI::ZzRangeSlider::lowerValueChanged,
            &slider, [&](int value) {
                if (value == 20) slider.setLowerValue(30);
            });
        slider.setValues(20, 80);
        QCOMPARE(slider.lowerValue(), 30);
        QCOMPARE(slider.upperValue(), 80);
        QCOMPARE(upper.size(), 1);
        QCOMPARE(upper.first().first().toInt(), 80);
    }

    void valueCallbackMayDeleteSlider()
    {
        auto *slider = new ZzFluentUI::ZzRangeSlider;
        QPointer<ZzFluentUI::ZzRangeSlider> guard = slider;
        QObject::connect(slider, &ZzFluentUI::ZzRangeSlider::lowerValueChanged,
            slider, [slider] { delete slider; });
        slider->setValues(20, 80);
        QVERIFY(guard.isNull());
    }

    void dragWithoutTrackingUpdatesPreviewBeforeCommit()
    {
        ZzFluentUI::ZzRangeSlider slider;
        slider.resize(200, 32);
        slider.setValues(20, 80);
        slider.setTracking(false);
        slider.show();
        QSignalSpy moved(&slider, &ZzFluentUI::ZzRangeSlider::sliderMoved);
        QSignalSpy changed(&slider, &ZzFluentUI::ZzRangeSlider::valuesChanged);
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(46, 16));
        QTest::mouseMove(&slider, QPoint(82, 16));
        QCOMPARE(slider.lowerValue(), 20);
        QCOMPARE(slider.lowerPosition(), 40);
        QCOMPARE(slider.upperPosition(), 80);
        QVERIFY(!moved.isEmpty());
        QCOMPARE(changed.size(), 0);
        QTest::mouseRelease(&slider, Qt::LeftButton, {}, QPoint(82, 16));
        QCOMPARE(slider.lowerValue(), 40);
        QCOMPARE(changed.size(), 1);
    }

    void snapOnReleaseLeavesPreviewUnsapped()
    {
        using Slider = ZzFluentUI::ZzRangeSlider;
        Slider slider;
        slider.resize(200, 32);
        slider.setValues(20, 80);
        slider.setSingleStep(10);
        slider.setSnapMode(Slider::ZzSnapMode::SnapOnRelease);
        slider.setTracking(false);
        slider.show();
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(46, 16));
        QTest::mouseMove(&slider, QPoint(73, 16));
        QCOMPARE(slider.lowerPosition(), 35);
        QCOMPARE(slider.lowerValue(), 20);
        QTest::mouseRelease(&slider, Qt::LeftButton, {}, QPoint(73, 16));
        QCOMPARE(slider.lowerValue(), 40);
    }

    void snapCallbackCancelDoesNotReleaseTwice()
    {
        using Slider = ZzFluentUI::ZzRangeSlider;
        Slider slider;
        slider.resize(200, 32);
        slider.setValues(20, 80);
        slider.setSingleStep(10);
        slider.setSnapMode(Slider::ZzSnapMode::SnapOnRelease);
        slider.setTracking(false);
        slider.show();
        QSignalSpy released(&slider, &Slider::sliderReleased);
        QObject::connect(&slider, &Slider::sliderMoved,
            &slider, [&](int lower, int) {
                if (lower == 40) slider.hide();
            });
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(46, 16));
        QTest::mouseMove(&slider, QPoint(73, 16));
        QTest::mouseRelease(&slider, Qt::LeftButton, {}, QPoint(73, 16));
        QCOMPARE(released.size(), 1);
        QCOMPARE(slider.lowerValue(), 20);
        QCOMPARE(slider.lowerPosition(), 20);
    }

    void overlappingHandlesCanSeparateInBothDirections()
    {
        ZzFluentUI::ZzRangeSlider slider;
        slider.resize(200, 32);
        slider.setValues(50, 50);
        slider.show();
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(105, 16));
        QTest::mouseMove(&slider, QPoint(145, 16));
        QTest::mouseRelease(&slider, Qt::LeftButton, {}, QPoint(145, 16));
        QCOMPARE(slider.lowerValue(), 50);
        QVERIFY(slider.upperValue() > 50);
        slider.setValues(50, 50);
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(95, 16));
        QTest::mouseMove(&slider, QPoint(55, 16));
        QTest::mouseRelease(&slider, Qt::LeftButton, {}, QPoint(55, 16));
        QVERIFY(slider.lowerValue() < 50);
        QCOMPARE(slider.upperValue(), 50);
    }

    void exactCoincidentPressSelectsDirectionWhenDragStarts()
    {
        ZzFluentUI::ZzRangeSlider slider;
        slider.resize(200, 32);
        slider.setValues(50, 50);
        slider.show();
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(100, 16));
        QTest::mouseMove(&slider, QPoint(145, 16));
        QTest::mouseRelease(&slider, Qt::LeftButton, {}, QPoint(145, 16));
        QCOMPARE(slider.lowerValue(), 50);
        QVERIFY(slider.upperValue() > 50);
        slider.setValues(50, 50);
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(100, 16));
        QTest::mouseMove(&slider, QPoint(55, 16));
        QTest::mouseRelease(&slider, Qt::LeftButton, {}, QPoint(55, 16));
        QVERIFY(slider.lowerValue() < 50);
        QCOMPARE(slider.upperValue(), 50);
    }

    void rtlVerticalAndKeyboardRespectValueDirection()
    {
        ZzFluentUI::ZzRangeSlider slider;
        slider.resize(200, 32);
        slider.setValues(20, 80);
        slider.setLayoutDirection(Qt::RightToLeft);
        slider.show();
        QTest::mouseClick(&slider, Qt::LeftButton, {}, QPoint(172, 16));
        QCOMPARE(slider.lowerValue(), 10);
        QTest::keyClick(&slider, Qt::Key_Left);
        QCOMPARE(slider.lowerValue(), 11);
        QTest::keyClick(&slider, Qt::Key_Up);
        QCOMPARE(slider.activeHandle(), ZzFluentUI::ZzRangeSlider::ZzSliderHandle::UpperHandle);
        slider.setOrientation(Qt::Vertical);
        slider.resize(32, 200);
        QTest::mouseClick(&slider, Qt::LeftButton, {}, QPoint(16, 28));
        QCOMPARE(slider.upperValue(), 90);
        QTest::keyClick(&slider, Qt::Key_Down);
        QCOMPARE(slider.upperValue(), 89);
    }

    void fullIntRangeAvoidsArithmeticOverflow()
    {
        ZzFluentUI::ZzRangeSlider slider;
        slider.setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        slider.setValues(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        slider.setSingleStep(std::numeric_limits<int>::max());
        slider.resize(200, 32);
        slider.show();
        QTest::mouseClick(&slider, Qt::LeftButton, {}, QPoint(100, 16));
        QVERIFY(slider.lowerValue() <= slider.upperValue());
        QTest::keyClick(&slider, Qt::Key_PageUp);
        QVERIFY(slider.lowerValue() <= slider.upperValue());
        slider.grab();
    }

    void hidingCancelsUncommittedDrag()
    {
        ZzFluentUI::ZzRangeSlider slider;
        slider.resize(200, 32);
        slider.setValues(20, 80);
        slider.setTracking(false);
        slider.setValueTipEnabled(true);
        slider.show();
        QSignalSpy released(&slider, &ZzFluentUI::ZzRangeSlider::sliderReleased);
        QTest::mousePress(&slider, Qt::LeftButton, {}, QPoint(46, 16));
        QTest::mouseMove(&slider, QPoint(82, 16));
        slider.hide();
        QCOMPARE(slider.lowerValue(), 20);
        QCOMPARE(slider.lowerPosition(), 20);
        QCOMPARE(released.size(), 1);
        auto *tip = slider.findChild<QWidget *>(QStringLiteral("zzRangeSliderValueTip"));
        QVERIFY(tip == nullptr || !tip->isVisible());
    }
};

QTEST_MAIN(ZzRangeSliderTest)
#include "ZzRangeSliderTest.moc"
