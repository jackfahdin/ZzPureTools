#include "ZzExampleRadialGaugeHelpers.h"
#include <QScopedValueRollback>

namespace ZzExample {
namespace {
/** @brief 预设中由主题派生的配色随主题更新，固定的示例色保持不变。 */
class ZzGaugePresetTheme final : public QObject
{
public:
    ZzGaugePresetTheme(ZzRadialGauge *gauge, std::function<void()> apply)
        : QObject(gauge)
        , apply_(std::move(apply))
    {
        gauge->installEventFilter(this);
        refresh();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
            refresh();
        return QObject::eventFilter(watched, event);
    }

private:
    void refresh()
    {
        if (updating_)
            return;
        const QScopedValueRollback<bool> guard(updating_, true);
        apply_();
    }
    std::function<void()> apply_;
    bool updating_ = false;
};
}

void configureClassicGauge(ZzRadialGauge *gauge)
{
    gauge->setRange(0, 100);
    gauge->setValue(50);
    gauge->setMinimumAngle(-140.0);
    gauge->setMaximumAngle(140.0);
    gauge->setScaleMode(ZzRadialGauge::TrackScale);
    gauge->setScaleWidth(7.0);
    gauge->setMajorTickCount(11);
    gauge->setMinorTickCount(4);
    gauge->setTickLength(4.0);
    gauge->setTickWidth(1.0);
    gauge->setMajorTickLength(8.0);
    gauge->setMajorTickWidth(1.6);
    gauge->setTickPadding(9.0);
    gauge->setLabelsVisible(true);
    gauge->setLabelPadding(22.0);
    gauge->setLabelFontPixelSize(10);
    gauge->setNeedleStyle(ZzRadialGauge::TriangleNeedle);
    gauge->setNeedleWidth(11.0);
    gauge->setNeedleLength(0.62);
    gauge->setNeedleColor(QColor(QStringLiteral("#EC1460")));
    gauge->setHubVisible(false);
    gauge->setTitle(QStringLiteral("SCORE"));
    gauge->setValuePosition(ZzRadialGauge::CenterValue);
    gauge->setValueFontPixelSize(23);
    gauge->setInteractive(true);

    new ZzGaugePresetTheme(gauge, [gauge] {
        QColor track = gauge->palette().color(QPalette::Text);
        track.setAlpha(220);
        setGaugeTrackColor(gauge, track);
    });
}

void configureProgressGauge(ZzRadialGauge *gauge)
{
    const QColor purple(QStringLiteral("#8067FF"));
    gauge->setRange(0, 100);
    gauge->setValue(70);
    gauge->setMinimumAngle(-135.0);
    gauge->setMaximumAngle(135.0);
    gauge->setScaleMode(ZzRadialGauge::ProgressScale);
    gauge->setScaleWidth(7.0);
    gauge->setMajorTickCount(11);
    gauge->setMinorTickCount(4);
    gauge->setTickLength(3.0);
    gauge->setTickWidth(1.0);
    gauge->setMajorTickLength(6.0);
    gauge->setMajorTickWidth(1.5);
    gauge->setTickPadding(12.0);
    gauge->setLabelsVisible(true);
    gauge->setLabelPadding(23.0);
    gauge->setLabelFontPixelSize(10);
    gauge->setNeedleStyle(ZzRadialGauge::TriangleNeedle);
    gauge->setNeedleWidth(11.0);
    gauge->setNeedleLength(0.66);
    gauge->setNeedleColor(purple);
    gauge->setHubVisible(true);
    gauge->setHubRadius(9.0);
    gauge->setValuePosition(ZzRadialGauge::BottomValue);
    gauge->setValueFontPixelSize(40);
    gauge->setValueColor(purple);
    gauge->setInteractive(true);
    gauge->setProgressGradientEnabled(true);
    gauge->setSweepAreaVisible(true);
    gauge->setProgressGradientStartColor(QColor(QStringLiteral("#38D8FF")));
    gauge->setProgressGradientEndColor(purple);

    setGaugeAccentColor(gauge, purple);
    new ZzGaugePresetTheme(gauge, [gauge] {
        QColor track = gauge->palette().color(QPalette::Text);
        track.setAlpha(210);
        setGaugeTrackColor(gauge, track);
    });
}

void configureSpeedometerGauge(ZzRadialGauge *gauge)
{
    const QColor cyan(QStringLiteral("#21BCE2"));
    const QColor amber(QStringLiteral("#FFB900"));
    const QColor coral(QStringLiteral("#FF6475"));
    gauge->setRange(0, 100);
    gauge->setValue(70);
    gauge->setMinimumAngle(-135.0);
    gauge->setMaximumAngle(135.0);
    gauge->setScaleMode(ZzRadialGauge::RangeScale);
    gauge->setScaleWidth(7.0);
    gauge->clearRanges();
    gauge->addRange(0, 60, cyan);
    gauge->addRange(60, 80, amber);
    gauge->addRange(80, 100, coral);
    gauge->setMajorTickCount(11);
    gauge->setMinorTickCount(4);
    gauge->setTickLength(4.0);
    gauge->setTickWidth(1.0);
    gauge->setMajorTickLength(8.0);
    gauge->setMajorTickWidth(1.6);
    gauge->setTickPadding(10.0);
    new ZzGaugePresetTheme(gauge, [gauge] {
        QColor tick = gauge->palette().color(QPalette::Text);
        tick.setAlpha(220);
        gauge->setTickColor(tick);
    });
    gauge->setLabelsVisible(true);
    gauge->setLabelPadding(27.0);
    gauge->setLabelFontPixelSize(10);
    gauge->setNeedleStyle(ZzRadialGauge::TriangleNeedle);
    gauge->setNeedleWidth(12.0);
    gauge->setNeedleLength(0.58);
    gauge->setNeedleColor(cyan);
    gauge->setHubVisible(false);
    gauge->setUnit(QStringLiteral("km/h"));
    gauge->setValuePosition(ZzRadialGauge::BottomValue);
    gauge->setValueFontPixelSize(24);
    gauge->setValueColor(cyan);
    gauge->setInteractive(true);
}

void configureEChartsBaseGauge(ZzRadialGauge *gauge)
{
    gauge->setRange(0, 100);
    gauge->setValue(50);
    gauge->setMinimumAngle(-140.0);
    gauge->setMaximumAngle(140.0);
    gauge->setScaleMode(ZzRadialGauge::TrackScale);
    gauge->setScaleWidth(5.0);
    gauge->setMajorTickCount(11);
    gauge->setMinorTickCount(4);
    gauge->setTickLength(4.0);
    gauge->setTickWidth(1.0);
    gauge->setMajorTickLength(7.0);
    gauge->setMajorTickWidth(1.4);
    gauge->setTickPadding(8.0);
    gauge->setLabelsVisible(true);
    gauge->setLabelPadding(20.0);
    gauge->setLabelFontPixelSize(9);
    gauge->setNeedleStyle(ZzRadialGauge::TriangleNeedle);
    gauge->setNeedleWidth(8.0);
    gauge->setNeedleLength(0.58);
    gauge->setHubVisible(false);
    gauge->setValuePosition(ZzRadialGauge::CenterValue);
    gauge->setValueFontPixelSize(20);
    gauge->setValueAnimationDuration(240);
    gauge->setInteractive(true);
}

void configureEChartsBasicGauge(ZzRadialGauge *gauge)
{
    const QColor blue(QStringLiteral("#5470C6"));
    configureEChartsBaseGauge(gauge);
    gauge->setTitle(QStringLiteral("SCORE"));
    gauge->setNeedleColor(blue);
    setGaugeAccentColor(gauge, blue);
    hideGaugeTrack(gauge);
}

void configureEChartsSimpleGauge(ZzRadialGauge *gauge)
{
    const QColor blue(QStringLiteral("#5470C6"));
    configureEChartsBaseGauge(gauge);
    gauge->setValue(50);
    gauge->setScaleMode(ZzRadialGauge::ProgressScale);
    gauge->setScaleWidth(6.0);
    gauge->setTitle(QStringLiteral("SCORE"));
    gauge->setNeedleStyle(ZzRadialGauge::TriangleNeedle);
    gauge->setNeedleWidth(8.0);
    gauge->setNeedleColor(blue);
    setGaugeAccentColor(gauge, blue);
    hideGaugeTrack(gauge);
}

void configureEChartsSpeedGauge(ZzRadialGauge *gauge)
{
    const QColor blue(QStringLiteral("#5470C6"));
    configureEChartsBaseGauge(gauge);
    gauge->setValue(70);
    gauge->setScaleMode(ZzRadialGauge::ProgressScale);
    gauge->setScaleWidth(10.0);
    gauge->setNeedleStyle(ZzRadialGauge::LineNeedle);
    gauge->setNeedleWidth(3.0);
    gauge->setNeedleColor(blue);
    gauge->setHubVisible(true);
    gauge->setHubRadius(6.0);
    gauge->setValuePosition(ZzRadialGauge::BottomValue);
    gauge->setValueFontPixelSize(32);
    setGaugeAccentColor(gauge, blue);
    hideGaugeTrack(gauge);
}

void configureEChartsProgressGauge(ZzRadialGauge *gauge)
{
    const QColor cyan(QStringLiteral("#45D1F5"));
    configureEChartsBaseGauge(gauge);
    gauge->setRange(0, 240);
    gauge->setValue(100);
    gauge->setMinimumAngle(-110.0);
    gauge->setMaximumAngle(110.0);
    gauge->setScaleMode(ZzRadialGauge::ProgressScale);
    gauge->setScaleWidth(9.0);
    gauge->setMajorTickCount(13);
    gauge->setNeedleStyle(ZzRadialGauge::LineNeedle);
    gauge->setNeedleWidth(5.0);
    gauge->setNeedleLength(0.64);
    gauge->setNeedleColor(cyan);
    gauge->setHubVisible(false);
    gauge->setUnit(QStringLiteral("km/h"));
    gauge->setValuePosition(ZzRadialGauge::BottomValue);
    gauge->setValueFontPixelSize(18);
    setGaugeAccentColor(gauge, cyan);
    hideGaugeTrack(gauge);
}

void configureEChartsStageGauge(ZzRadialGauge *gauge)
{
    const QColor cyan(QStringLiteral("#42D0D0"));
    configureEChartsBaseGauge(gauge);
    gauge->setValue(70);
    gauge->setScaleMode(ZzRadialGauge::RangeScale);
    gauge->setScaleWidth(13.0);
    gauge->clearRanges();
    gauge->addRange(0, 70, cyan);
    gauge->addRange(70, 80, QColor(QStringLiteral("#3AA7D8")));
    gauge->addRange(80, 100, QColor(QStringLiteral("#FF6B75")));
    gauge->setNeedleStyle(ZzRadialGauge::LineNeedle);
    gauge->setNeedleWidth(4.0);
    gauge->setNeedleColor(cyan);
    gauge->setUnit(QStringLiteral("km/h"));
    gauge->setValuePosition(ZzRadialGauge::CenterValue);
    gauge->setValueFontPixelSize(17);
    gauge->setValueColor(cyan);
    hideGaugeTrack(gauge);
}

void configureEChartsGradeGauge(ZzRadialGauge *gauge)
{
    const QColor cyan(QStringLiteral("#45D2E7"));
    configureEChartsBaseGauge(gauge);
    gauge->setValue(70);
    gauge->setMinimumAngle(-90.0);
    gauge->setMaximumAngle(90.0);
    gauge->setScaleMode(ZzRadialGauge::RangeScale);
    gauge->setScaleWidth(4.0);
    gauge->clearRanges();
    gauge->addRange(0, 25, QColor(QStringLiteral("#FF6475")));
    gauge->addRange(25, 50, QColor(QStringLiteral("#F3C74F")));
    gauge->addRange(50, 75, cyan);
    gauge->addRange(75, 100, QColor(QStringLiteral("#61DDAA")));
    gauge->setMajorTickCount(9);
    gauge->setMinorTickCount(3);
    gauge->setLabelsVisible(false);
    gauge->setNeedleStyle(ZzRadialGauge::TriangleNeedle);
    gauge->setNeedleWidth(10.0);
    gauge->setNeedleLength(0.6);
    gauge->setNeedleColor(cyan);
    gauge->setTitle(QStringLiteral("Grade Rating"));
    gauge->setValuePosition(ZzRadialGauge::CenterValue);
    gauge->setValueFontPixelSize(18);
    hideGaugeTrack(gauge);
}

void configureEChartsTemperatureGauge(ZzRadialGauge *gauge)
{
    const QColor coral(QStringLiteral("#FF9678"));
    configureEChartsBaseGauge(gauge);
    gauge->setRange(0, 60);
    gauge->setValue(20);
    gauge->setMinimumAngle(-110.0);
    gauge->setMaximumAngle(110.0);
    gauge->setScaleMode(ZzRadialGauge::ProgressScale);
    gauge->setScaleWidth(12.0);
    gauge->setMajorTickCount(13);
    gauge->setNeedleStyle(ZzRadialGauge::NoNeedle);
    gauge->setUnit(QStringLiteral("°C"));
    gauge->setValuePosition(ZzRadialGauge::CenterValue);
    gauge->setValueFontPixelSize(28);
    gauge->setValueColor(coral);
    setGaugeAccentColor(gauge, coral);
    hideGaugeTrack(gauge);
}

void configureEChartsMultiTitleGauge(ZzMultiRadialGauge *gauge)
{
    gauge->setRange(0.0, 100.0);
    gauge->setMinimumAngle(-140.0);
    gauge->setMaximumAngle(140.0);
    gauge->setMajorTickCount(11);
    gauge->setMinorTickCount(4);
    gauge->setTrackWidth(7.0);
    gauge->setProgressWidth(7.0);
    gauge->setTrackCapStyle(Qt::RoundCap);
    gauge->setProgressCapStyle(Qt::RoundCap);
    gauge->setProgressOverlap(true);
    gauge->setScalePadding(13.0);
    gauge->setTickLength(3.0);
    gauge->setMajorTickLength(6.0);
    gauge->setTickPadding(8.0);
    gauge->setLabelPadding(17.0);
    gauge->setLabelFontPixelSize(8);
    gauge->setNeedleStyle(ZzMultiRadialGauge::LineNeedle);
    gauge->setNeedleWidth(4.0);
    gauge->setNeedleLength(0.68);
    gauge->setNeedleOffset(QPointF(0.0, 0.08));
    gauge->setHubVisible(true);
    gauge->setHubRadius(6.0);
    gauge->setHubColor(QColor(QStringLiteral("#FAC858")));
    gauge->setTitleFontPixelSize(9);
    gauge->setDetailFontPixelSize(9);
    gauge->setDetailBadgeVisible(true);
    gauge->setDetailBadgePadding(6.0);
    gauge->setValueSuffix(QStringLiteral("%"));
    gauge->setValueAnimationDuration(240);
}

void configureEChartsBarometerGauge(ZzRadialGauge *gauge)
{
    const QColor red(QStringLiteral("#E63746"));
    configureEChartsBaseGauge(gauge);
    gauge->setValue(40);
    gauge->setMinimumAngle(-130.0);
    gauge->setMaximumAngle(130.0);
    gauge->setScaleWidth(2.0);
    gauge->setTickColor(red);
    gauge->setLabelColor(red);
    gauge->setNeedleStyle(ZzRadialGauge::LineNeedle);
    gauge->setNeedleWidth(2.0);
    gauge->setNeedleLength(0.72);
    new ZzGaugePresetTheme(gauge, [gauge] { gauge->setNeedleColor(gauge->palette().color(QPalette::Text)); });
    gauge->setHubVisible(true);
    gauge->setHubRadius(3.0);
    gauge->setTitle(QStringLiteral("PLP"));
    gauge->setValuePosition(ZzRadialGauge::CenterValue);
    gauge->setValueFontPixelSize(16);
    setGaugeTrackColor(gauge, red);
}

} // namespace ZzExample
