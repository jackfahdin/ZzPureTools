#pragma once
#include <ZzFluentUI/ZzRadialGauge.h>
#include <QHash>
#include <QVariantAnimation>

namespace ZzFluentUI {

/** @brief ZzRadialGaugeRange 的私有属性与呈现状态。 */
class ZzRadialGaugeRangePrivate final {
  public:
    int fromValue = 0;
    int toValue = 0;
    QColor color;
};

/** @brief ZzRadialGauge 的私有属性与呈现状态。 */
class ZzRadialGaugePrivate final {
  public:
    bool animationWrite = false;
    int animationValue = 0;
    bool interactive = true;
    int valueAnimationDuration = 500;
    ZzRadialGauge::ZzGaugeScaleMode scaleMode = ZzRadialGauge::ProgressScale;
    qreal minimumAngle = -135.0;
    qreal maximumAngle = 135.0;
    int majorTickCount = 11;
    int minorTickCount = 4;
    qreal scaleWidth = 8.0;
    qreal scalePadding = 12.0;
    Qt::PenCapStyle trackCapStyle = Qt::RoundCap;
    Qt::PenCapStyle ringCapStyle = Qt::RoundCap;
    bool progressGradientEnabled = false;
    bool sweepAreaVisible = false;
    qreal sweepAreaOpacity = 0.16;
    QColor progressGradientStartColor;
    QColor progressGradientEndColor;
    qreal needleWidth = 10.0;
    ZzRadialGauge::ZzNeedleStyle needleStyle = ZzRadialGauge::LineNeedle;
    qreal needleLength = 0.62;
    qreal tickLength = 7.0;
    qreal tickWidth = 1.5;
    qreal majorTickLength = 10.0;
    qreal majorTickWidth = 2.0;
    qreal tickPadding = 8.0;
    bool labelsVisible = false;
    qreal labelPadding = 28.0;
    int labelFontPixelSize = 11;
    bool hubVisible = false;
    qreal hubRadius = 11.0;
    bool valueVisible = true;
    ZzRadialGauge::ZzGaugeValuePosition valuePosition = ZzRadialGauge::BottomValue;
    QString title = QString();
    QString unit = QString();
    int valueFontPixelSize = 0;
    QColor needleColor;
    QColor tickColor;
    QColor labelColor;
    QColor valueColor;
    QList<ZzRadialGaugeRange *> ranges;
    QVariantAnimation *valueAnimation = nullptr;
    Qt::FocusPolicy interactiveFocusPolicy = Qt::StrongFocus;
};

} // namespace ZzFluentUI
