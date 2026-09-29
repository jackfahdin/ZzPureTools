#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtCore/QPointer>
#include <QtCore/QDir>
#include <QtGui/QMouseEvent>
#include <QtGui/QScreen>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStyleOptionSlider>
#include <limits>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzSliderValueTip.h>
#include <ZzFluentUI/ZzThemeController.h>

namespace {

/** @brief 通过原生初始化接口获取真实滑柄，避免测试复制生产几何公式。 */
class ZzSliderProbe final : public QSlider
{
public:
    using QSlider::QSlider;
    QRect handleRect() const
    {
        QStyleOptionSlider option;
        initStyleOption(&option);
        return style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
    }
};

QLabel *zzTip(QSlider &slider)
{
    return slider.findChild<QLabel *>(QStringLiteral("zzSliderValueTip"));
}

} // namespace

/** @brief 验证滑块提示的真实输入、显示和生命周期。 */
class ZzSliderValueTipTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void showsPercentageOnKeyboardInput_data()
    {
        QTest::addColumn<int>("minimum");
        QTest::addColumn<int>("maximum");
        QTest::addColumn<int>("initial");
        QTest::addColumn<QString>("expected");
        QTest::newRow("offset-range") << 20 << 80 << 49 << QStringLiteral("50%");
        QTest::newRow("negative-range") << -100 << 100 << -1 << QStringLiteral("50%");
        QTest::newRow("full-int-range") << std::numeric_limits<int>::min()
            << std::numeric_limits<int>::max() << -1 << QStringLiteral("50%");
        QTest::newRow("zero-span") << 7 << 7 << 7 << QStringLiteral("0%");
        QTest::newRow("rounding") << 0 << 3 << 0 << QStringLiteral("33%");
    }

    void showsPercentageOnKeyboardInput()
    {
        QFETCH(int, minimum);
        QFETCH(int, maximum);
        QFETCH(int, initial);
        QFETCH(QString, expected);
        QSlider slider(Qt::Horizontal);
        QVERIFY(ZzFluentUI::ZzSliderValueTip::attach(&slider) != nullptr);
        slider.setRange(minimum, maximum);
        slider.setValue(initial);
        slider.resize(240, 40);
        slider.show();
        QCoreApplication::processEvents();
        QTest::keyClick(&slider, Qt::Key_Right);
        auto *tip = slider.findChild<QLabel *>(QStringLiteral("zzSliderValueTip"));
        QVERIFY(tip != nullptr);
        QVERIFY(tip->isVisible());
        QCOMPARE(tip->text(), expected);
    }

    void reusesAttachmentAndOnlyOpensForInput()
    {
        using namespace ZzFluentUI;
        QSlider slider(Qt::Horizontal);
        slider.setRange(-100, 100);
        auto *attachment = ZzSliderValueTip::attach(&slider);
        QCOMPARE(ZzSliderValueTip::attach(&slider), attachment);
        QCOMPARE(slider.findChildren<ZzSliderValueTip *>().size(), 1);
        QVERIFY(ZzSliderValueTip::attach(nullptr) == nullptr);
        slider.show();
        slider.setValue(-42);
        QCoreApplication::processEvents();
        QVERIFY(zzTip(slider) == nullptr);
        slider.setSliderDown(true);
        slider.setSliderPosition(-42);
        QVERIFY(zzTip(slider) == nullptr);
        slider.setSliderDown(false);
        QSignalSpy changed(attachment, &ZzSliderValueTip::modeChanged);
        attachment->setMode(ZzSliderValueTipMode::Value);
        QTest::keyClick(&slider, Qt::Key_Right);
        auto *tip = zzTip(slider);
        QVERIFY(tip != nullptr);
        QCOMPARE(tip->text(), QStringLiteral("-41"));
        attachment->setMode(ZzSliderValueTipMode::Value);
        QCOMPARE(changed.size(), 1);
        attachment->setMode(ZzSliderValueTipMode::Disabled);
        QVERIFY(!tip->isVisible());
        QTest::keyClick(&slider, Qt::Key_Right);
        QVERIFY(!tip->isVisible());
        attachment->setMode(static_cast<ZzSliderValueTipMode>(200));
        QCOMPARE(attachment->mode(), ZzSliderValueTipMode::Disabled);
        attachment->setMode(ZzSliderValueTipMode::Percentage);
        for (int i = 0; i < 5; ++i) {
            QTest::keyClick(&slider, Qt::Key_Right);
            QCOMPARE(zzTip(slider), tip);
        }
        QCOMPARE(slider.findChildren<QLabel *>().size(), 1);
        QPointer<QLabel> ownedTip = tip;
        delete attachment;
        QVERIFY(ownedTip.isNull());
        QTest::keyClick(&slider, Qt::Key_Right);
        QVERIFY(zzTip(slider) == nullptr);
    }

    void followsHandleWithoutTracking_data()
    {
        QTest::addColumn<bool>("vertical");
        QTest::addColumn<bool>("rtl");
        QTest::addColumn<bool>("inverted");
        for (const bool vertical : {false, true}) {
            for (const bool rtl : {false, true}) {
                for (const bool inverted : {false, true}) {
                    QTest::newRow(qPrintable(QStringLiteral("%1-%2-%3").arg(vertical).arg(rtl).arg(inverted)))
                        << vertical << rtl << inverted;
                }
            }
        }
    }

    void followsHandleWithoutTracking()
    {
        QFETCH(bool, vertical);
        QFETCH(bool, rtl);
        QFETCH(bool, inverted);
        ZzSliderProbe slider(vertical ? Qt::Vertical : Qt::Horizontal);
        slider.setRange(0, 100);
        slider.setValue(50);
        slider.setTracking(false);
        slider.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        slider.setInvertedAppearance(inverted);
        QVERIFY(ZzFluentUI::ZzSliderValueTip::attach(&slider) != nullptr);
        slider.resize(vertical ? QSize(40, 300) : QSize(300, 40));
        slider.move(200, 200);
        slider.show();
        QCoreApplication::processEvents();
        const QPoint start = slider.handleRect().center();
        QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, start);
        const QPoint target = start + (vertical ? QPoint(0, 65) : QPoint(65, 0));
        QMouseEvent move(QEvent::MouseMove, target, slider.mapToGlobal(target),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&slider, &move);
        auto *tip = zzTip(slider);
        QVERIFY(tip != nullptr);
        QVERIFY(tip->isVisible());
        QCOMPARE(slider.value(), 50);
        QVERIFY(slider.sliderPosition() != 50);
        QCOMPARE(tip->text(), QString::number(slider.sliderPosition()) + QLatin1Char('%'));
        const QRect handle(slider.mapToGlobal(slider.handleRect().topLeft()), slider.handleRect().size());
        QVERIFY(!tip->geometry().intersects(handle));
        QVERIFY(slider.screen()->availableGeometry().contains(tip->geometry()));
        QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, target);
        QVERIFY(!tip->isVisible());
        QCOMPARE(slider.value(), slider.sliderPosition());
    }

    void hidesAfterWheelAndOnEnvironmentChanges()
    {
        QWidget host;
        host.resize(400, 200);
        QSlider slider(Qt::Horizontal, &host);
        slider.setGeometry(50, 70, 240, 40);
        slider.setValue(50);
        QVERIFY(ZzFluentUI::ZzSliderValueTip::attach(&slider) != nullptr);
        host.show();
        QCoreApplication::processEvents();
        const QPoint center = slider.rect().center();
        QWheelEvent wheel(center, slider.mapToGlobal(center), QPoint(), QPoint(0, 120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&slider, &wheel);
        auto *tip = zzTip(slider);
        QVERIFY(tip != nullptr);
        QVERIFY(tip->isVisible());
        QVERIFY(slider.value() > 50);
        QTest::qWait(1050);
        QVERIFY(!tip->isVisible());
        QTest::keyClick(&slider, Qt::Key_Left);
        const QPoint before = tip->pos();
        host.move(host.pos() + QPoint(20, 15));
        QCoreApplication::processEvents();
        QCOMPARE(tip->pos(), before + QPoint(20, 15));
        slider.setEnabled(false);
        QVERIFY(!tip->isVisible());
        slider.setEnabled(true);
        QTest::keyClick(&slider, Qt::Key_Right);
        QVERIFY(tip->isVisible());
        host.hide();
        QVERIFY(!tip->isVisible());
    }

    void followsThemeAndScreenEdges()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzSliderProbe slider(Qt::Horizontal);
        slider.setStyle(&style);
        slider.setValue(50);
        slider.resize(200, 40);
        slider.move(slider.screen()->availableGeometry().topLeft());
        QVERIFY(ZzFluentUI::ZzSliderValueTip::attach(&slider) != nullptr);
        slider.show();
        QCoreApplication::processEvents();
        QTest::keyClick(&slider, Qt::Key_Right);
        auto *tip = zzTip(slider);
        QVERIFY(tip != nullptr);
        QVERIFY(slider.screen()->availableGeometry().contains(tip->geometry()));
        for (const auto mode : {ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark, ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            QCoreApplication::processEvents();
            QCOMPARE(tip->palette().color(QPalette::ToolTipText), slider.palette().color(QPalette::ToolTipText));
            QCOMPARE(tip->style(), slider.style());
            const QPixmap image = tip->grab();
            QVERIFY(!image.isNull());
            const QString captureDir = qEnvironmentVariable("ZZ_SLIDER_TIP_CAPTURE_DIR");
            if (!captureDir.isEmpty()) {
                QVERIFY(image.save(QDir(captureDir).filePath(
                    QStringLiteral("slider-tip-%1.png").arg(static_cast<int>(mode)))));
            }
        }
        QVERIFY(tip->testAttribute(Qt::WA_ShowWithoutActivating));
        QCOMPARE(tip->focusPolicy(), Qt::NoFocus);
    }
};

QTEST_MAIN(ZzSliderValueTipTest)
#include "ZzSliderValueTipTest.moc"
