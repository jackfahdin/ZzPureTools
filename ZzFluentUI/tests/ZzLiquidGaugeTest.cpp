#include <ZzTestEventLoop.h>
#include <QImage>
#include <QMetaProperty>
#include <QPainter>
#include <QPointer>
#include <QProgressBar>
#include <QProxyStyle>
#include <QSignalSpy>
#include <QTest>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <limits>

#include <ZzFluentUI/ZzLiquidGauge.h>
using Gauge = ZzFluentUI::ZzLiquidGauge;

namespace {
class AnimationStyle final : public QProxyStyle
{
public:
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr, QStyleHintReturn *result = nullptr) const override
    {
        return hint == SH_Widget_Animate ? 1 : QProxyStyle::styleHint(hint, option, widget, result);
    }
};

QImage render(Gauge &gauge)
{
    QImage image(gauge.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    gauge.render(&painter);
    return image;
}

void configurePixels(Gauge &gauge)
{
    gauge.resize(120, 120);
    gauge.setTextVisible(false);
    gauge.setProperty("shape", 1);
    gauge.setProperty("animationEnabled", false);
    gauge.setProperty("outlineWidth", 0.0);
    gauge.setProperty("outlineDistance", 0.0);
    gauge.setProperty("waveColor", QColor(Qt::red));
    gauge.setProperty("backgroundColor", QColor(Qt::blue));
}
} // namespace

class ZzLiquidGaugeTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Missing clamping, accepting NaN, or notifying unchanged effective values breaks this contract.
    void normalizesProperties_data()
    {
        QTest::addColumn<QByteArray>("name");
        QTest::addColumn<double>("initial");
        QTest::addColumn<double>("low");
        QTest::addColumn<double>("high");
        QTest::newRow("amplitude") << QByteArray("waveAmplitude") << 6.0 << 0.0 << 100.0;
        QTest::newRow("count") << QByteArray("waveCount") << 3.0 << 1.0 << 20.0;
        QTest::newRow("duration") << QByteArray("waveAnimationDuration") << 2400.0 << 100.0 << 60000.0;
        QTest::newRow("opacity") << QByteArray("secondaryWaveOpacity") << 0.45 << 0.0 << 1.0;
        QTest::newRow("outline") << QByteArray("outlineWidth") << 2.0 << 0.0 << 100.0;
        QTest::newRow("distance") << QByteArray("outlineDistance") << 3.0 << 0.0 << 100.0;
        QTest::newRow("font") << QByteArray("contentFontPixelSize") << 0.0 << 0.0 << 200.0;
    }

    void normalizesProperties()
    {
        QFETCH(QByteArray, name);
        QFETCH(double, initial);
        QFETCH(double, low);
        QFETCH(double, high);
        Gauge gauge;
        const int index = gauge.metaObject()->indexOfProperty(name.constData());
        QVERIFY2(index >= 0, name.constData());
        const QMetaProperty property = gauge.metaObject()->property(index);
        QSignalSpy spy(&gauge, property.notifySignal());
        QCOMPARE(property.read(&gauge).toDouble(), initial);
        QVERIFY(property.write(&gauge, -1));
        QCOMPARE(property.read(&gauge).toDouble(), low);
        QCOMPARE(spy.count(), initial == low ? 0 : 1);
        const qsizetype afterLow = spy.count();
        QVERIFY(property.write(&gauge, -2));
        QCOMPARE(spy.count(), afterLow);
        QVERIFY(property.write(&gauge, 100000));
        QCOMPARE(property.read(&gauge).toDouble(), high);
        QCOMPARE(spy.count(), afterLow + 1);
        if (property.metaType() == QMetaType::fromType<qreal>()) {
            QVERIFY(property.write(&gauge, std::numeric_limits<qreal>::quiet_NaN()));
            QVERIFY(property.write(&gauge, std::numeric_limits<qreal>::infinity()));
            QCOMPARE(property.read(&gauge).toDouble(), high);
            QCOMPARE(spy.count(), afterLow + 1);
        }
    }

    // The widget must retain the native progress range/reset behavior while painting that same value.
    void paintsEmptyFullResetAndLargeRanges()
    {
        Gauge gauge;
        configurePixels(gauge);
        gauge.setRange(0, 100);
        gauge.setValue(0);
        QCOMPARE(render(gauge).pixelColor(60, 60), QColor(Qt::blue));
        gauge.setValue(100);
        QCOMPARE(render(gauge).pixelColor(60, 60), QColor(Qt::red));
        gauge.reset();
        QVERIFY(gauge.text().isEmpty());
        QCOMPARE(render(gauge).pixelColor(60, 60), QColor(Qt::blue));
        gauge.setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        gauge.setValue(0);
        const QImage half = render(gauge);
        QCOMPARE(half.pixelColor(60, 30), QColor(Qt::blue));
        QCOMPARE(half.pixelColor(60, 90), QColor(Qt::red));
        gauge.setValue(std::numeric_limits<int>::max());
        QCOMPARE(render(gauge).pixelColor(60, 30), QColor(Qt::red));
        gauge.setRange(5, 5);
        gauge.setValue(5);
        QCOMPARE(render(gauge).pixelColor(60, 60), QColor(Qt::blue));
        gauge.setRange(20, 120);
        gauge.setValue(70);
        gauge.setFormat(QStringLiteral("%v / %m (%p%)"));
        QCOMPARE(gauge.text(), QStringLiteral("70 / 100 (50%)"));
    }

    // Failing to clear explicit colors would prevent a later palette/theme update from taking effect.
    void restoresPaletteColors()
    {
        Gauge gauge;
        configurePixels(gauge);
        QPalette palette = gauge.palette();
        palette.setColor(QPalette::All, QPalette::Accent, Qt::green);
        palette.setColor(QPalette::All, QPalette::Base, Qt::yellow);
        gauge.setPalette(palette);
        gauge.setValue(100);
        QCOMPARE(render(gauge).pixelColor(60, 60), QColor(Qt::red));
        QVERIFY(gauge.setProperty("waveColor", QColor()));
        QCOMPARE(render(gauge).pixelColor(60, 60), QColor(Qt::green));
        gauge.setValue(0);
        QVERIFY(gauge.setProperty("backgroundColor", QColor()));
        QCOMPARE(render(gauge).pixelColor(60, 60), QColor(Qt::yellow));
        palette.setColor(QPalette::All, QPalette::Base, Qt::cyan);
        gauge.setPalette(palette);
        QCOMPARE(render(gauge).pixelColor(60, 60), QColor(Qt::cyan));
    }

    // A live timer during any pause condition would move pixels or expose running=true.
    void pausesAndResumesAnimation()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller, new AnimationStyle);
        Gauge gauge;
        configurePixels(gauge);
        gauge.setStyle(&style);
        gauge.setValue(50);
        QVERIFY(gauge.setProperty("animationEnabled", true));
        QVERIFY(!gauge.property("running").toBool());
        gauge.show();
        ZZ_VERIFY_EVENTUALLY(gauge.property("running").toBool());
        const QImage before = render(gauge);
        ZZ_VERIFY_EVENTUALLY(render(gauge) != before);
        gauge.hide();
        QVERIFY(!gauge.property("running").toBool());
        const QImage hidden = render(gauge);
        QTest::qWait(80);
        QCOMPARE(render(gauge), hidden);
        gauge.show();
        ZZ_VERIFY_EVENTUALLY(gauge.property("running").toBool());
        gauge.setEnabled(false);
        QVERIFY(!gauge.property("running").toBool());
        gauge.setEnabled(true);
        QVERIFY(gauge.property("running").toBool());
        gauge.setProperty("waveAmplitude", 0.0);
        QVERIFY(!gauge.property("running").toBool());
        gauge.setProperty("waveAmplitude", 6.0);
        QVERIFY(gauge.property("running").toBool());
        gauge.setProperty("animationEnabled", false);
        QVERIFY(!gauge.property("running").toBool());
        gauge.setProperty("animationEnabled", true);
        QVERIFY(gauge.property("running").toBool());
        controller.setReducedMotion(true);
        ZZ_VERIFY_EVENTUALLY(!gauge.property("running").toBool());
        controller.setReducedMotion(false);
        ZZ_VERIFY_EVENTUALLY(gauge.property("running").toBool());
    }

    // Shape clipping must stay bounded and negative inner geometry must never reach the painter.
    void clipsEveryShapeAndHandlesTinySizes()
    {
        Gauge gauge;
        configurePixels(gauge);
        gauge.setValue(100);
        for (int shape = 0; shape != 4; ++shape) {
            QVERIFY(gauge.setProperty("shape", shape));
            gauge.resize(120, 120);
            const QImage image = render(gauge);
            QCOMPARE(image.pixelColor(60, 60), QColor(Qt::red));
            QVERIFY(image.pixelColor(0, 0) != QColor(Qt::red));
            gauge.resize(1, 1);
            QVERIFY(!render(gauge).isNull());
        }
        gauge.setProperty("shape", 100);
        QCOMPARE(gauge.property("shape").toInt(), 3);
        gauge.resize(12, 12);
        gauge.setProperty("outlineWidth", 100);
        gauge.setProperty("outlineDistance", 100);
        QVERIFY(!render(gauge).isNull());
    }
};

QTEST_MAIN(ZzLiquidGaugeTest)
#include "ZzLiquidGaugeTest.moc"
