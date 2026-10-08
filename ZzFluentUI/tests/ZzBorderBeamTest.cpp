#include <QAccessible>
#include <QLabel>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <ZzFluentUI/ZzBorderBeam.h>
#include <ZzFluentUI/ZzBorderBeamButton.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <limits>

using namespace ZzFluentUI;

class ZzBorderBeamTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void lifecycleAndPhase()
    {
        ZzBorderBeam beam;
        beam.setAnimationDuration(1000);
        beam.setInitialProgress(0.3);
        QVERIFY(!beam.isRunning());
        beam.show();
        QTRY_VERIFY(beam.isRunning());
        QTRY_VERIFY(beam.progress() > 0.32);
        beam.setAnimationEnabled(false);
        const qreal paused = beam.progress();
        QTest::qWait(40);
        QCOMPARE(beam.progress(), paused);
        beam.setDirection(ZzBorderBeam::CounterClockwise);
        beam.setAnimationDuration(2000);
        QCOMPARE(beam.progress(), paused);
        beam.setAnimationEnabled(true);
        QTRY_VERIFY(beam.progress() < paused);
        beam.hide();
        QVERIFY(!beam.isRunning());
        const qreal hidden = beam.progress();
        QTest::qWait(40);
        QCOMPARE(beam.progress(), hidden);
        beam.show();
        QVERIFY(beam.isRunning());
        beam.setDisabled(true);
        QVERIFY(!beam.isRunning());
        beam.setEnabled(true);
        QVERIFY(beam.isRunning());
        beam.setBeamLength(0);
        QVERIFY(!beam.isRunning());
        beam.restartAnimation();
        QCOMPARE(beam.progress(), 0.3);
    }

    void validatesInputsAndNotifiesOnce()
    {
        ZzBorderBeam beam;
        QSignalSpy length(&beam, &ZzBorderBeam::beamLengthChanged);
        beam.setBeamLength(std::numeric_limits<qreal>::quiet_NaN());
        QCOMPARE(beam.beamLength(), 60.0);
        beam.setBeamLength(-10);
        beam.setBeamLength(-20);
        QCOMPARE(beam.beamLength(), 0.0);
        QCOMPARE(length.count(), 1);
        beam.setBeamWidth(1e20);
        QCOMPARE(beam.beamWidth(), 32.0);
        beam.setCornerRadius(-5);
        QCOMPARE(beam.cornerRadius(), 0.0);
        beam.setBeamCount(1000000);
        QCOMPARE(beam.beamCount(), 8);
        beam.setAnimationDuration(0);
        QCOMPARE(beam.animationDuration(), 100);
        beam.setInitialProgress(2);
        QCOMPARE(beam.initialProgress(), 1.0);
        QVERIFY(beam.progress() >= 0 && beam.progress() < 1);
        beam.setDirection(static_cast<ZzBorderBeam::Direction>(99));
        QCOMPARE(beam.direction(), ZzBorderBeam::Clockwise);
        beam.setAnimationEnabled(false);
        beam.resize(1, 1);
        QVERIFY(!beam.grab().isNull());
    }

    void reducedMotionStopsLiveAnimation()
    {
        ZzThemeController theme;
        theme.setReducedMotion(false);
        ZzFluentStyle style(&theme);
        ZzBorderBeam beam;
        beam.setStyle(&style);
        beam.show();
        QTRY_VERIFY(beam.isRunning());
        theme.setReducedMotion(true);
        QTRY_VERIFY(!beam.isRunning());
        QVERIFY(beam.isAnimationEnabled());
        theme.setReducedMotion(false);
        QTRY_VERIFY(beam.isRunning());
    }

    void containerAndButtonKeepQtSemantics()
    {
        ZzBorderBeam beam;
        auto *layout = new QVBoxLayout(&beam);
        layout->addWidget(new QLabel(QStringLiteral("Native content")));
        beam.show();
        QVERIFY(beam.findChild<QLabel *>()->isVisible());
        ZzBorderBeamButton button(QStringLiteral("Beam"));
        button.setCheckable(true);
        button.show();
        QSignalSpy clicks(&button, &QPushButton::clicked);
        QTest::mouseClick(&button, Qt::LeftButton);
        QCOMPARE(clicks.count(), 1);
        QVERIFY(button.isChecked());
        QTest::keyClick(&button, Qt::Key_Space);
        QCOMPARE(clicks.count(), 2);
        QVERIFY(!button.isChecked());
        QCOMPARE(QAccessible::queryAccessibleInterface(&button)->role(), QAccessible::CheckBox);
        button.setDisabled(true);
        QVERIFY(!button.isRunning());
        QTest::mouseClick(&button, Qt::LeftButton);
        QCOMPARE(clicks.count(), 2);
    }

    void forcedButtonThemeKeepsTextReadable()
    {
        for (const bool dark : { false, true }) {
            QWidget parent;
            auto palette = parent.palette();
            palette.setColor(QPalette::ButtonText, dark ? Qt::black : Qt::white);
            parent.setPalette(palette);
            ZzBorderBeamButton button(QStringLiteral("Readable"), &parent);
            button.setBeamLength(0);
            button.setFocusPolicy(Qt::NoFocus);
            button.setFixedSize(180, 40);
            button.setThemeMode(dark ? ZzBorderBeam::DarkTheme : ZzBorderBeam::LightTheme);
            parent.show();
            const auto image = button.grab().toImage();
            int contrastingPixels = 0;
            for (int y = 8; y < image.height() - 8; ++y) {
                for (int x = 20; x < image.width() - 20; ++x) {
                    const int gray = qGray(image.pixel(x, y));
                    if (dark ? gray > 210 : gray < 60)
                        ++contrastingPixels;
                }
            }
            QVERIFY2(contrastingPixels > 20, "Forced button theme must retain readable label contrast");
            palette.setColor(QPalette::ButtonText, Qt::red);
            button.setPalette(palette);
            const auto custom = button.grab().toImage();
            int redPixels = 0;
            for (int y = 8; y < custom.height() - 8; ++y) {
                for (int x = 20; x < custom.width() - 20; ++x) {
                    const auto color = custom.pixelColor(x, y);
                    if (color.red() > 150 && color.green() < 90 && color.blue() < 90)
                        ++redPixels;
                }
            }
            QVERIFY2(redPixels > 20, "Explicit ButtonText palette must remain authoritative");
        }
    }
};

QTEST_MAIN(ZzBorderBeamTest)
#include "ZzBorderBeamTest.moc"
