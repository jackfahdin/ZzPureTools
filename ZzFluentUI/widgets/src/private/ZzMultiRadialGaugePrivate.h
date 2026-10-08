#pragma once
#include <ZzFluentUI/ZzMultiRadialGauge.h>
#include <QHash>
#include <QVariantAnimation>

namespace ZzFluentUI {

/** @brief ZzMultiRadialGaugeItem 的私有属性与呈现状态。 */
class ZzMultiRadialGaugeItemPrivate final {
  public:
    /** @brief QObject 修改父子关系期间拒绝嵌套接管。 */
    bool adoptionInProgress = false;
    QString label = QString();
    qreal value = 0.0;
    QColor color = QColor();
    bool visible = true;
    QPointF titleOffset = QPointF(0.0, 0.2);
    QPointF detailOffset = QPointF(0.0, 0.4);
};

/** @brief ZzMultiRadialGauge 的私有属性与呈现状态。 */
class ZzMultiRadialGaugePrivate final {
  public:
    qreal minimum = 0.0;
    qreal maximum = 100.0;
    qreal minimumAngle = -135.0;
    qreal maximumAngle = 135.0;
    int majorTickCount = 11;
    int minorTickCount = 4;
    bool trackVisible = true;
    qreal trackWidth = 8.0;
    QColor trackColor = QColor();
    Qt::PenCapStyle trackCapStyle = Qt::RoundCap;
    bool progressVisible = true;
    bool progressOverlap = true;
    qreal progressWidth = 8.0;
    qreal progressSpacing = 2.0;
    Qt::PenCapStyle progressCapStyle = Qt::RoundCap;
    qreal scalePadding = 12.0;
    qreal tickLength = 4.0;
    qreal tickWidth = 1.0;
    qreal majorTickLength = 8.0;
    qreal majorTickWidth = 1.8;
    qreal tickPadding = 8.0;
    QColor tickColor = QColor();
    bool labelsVisible = true;
    qreal labelPadding = 13.0;
    int labelFontPixelSize = 10;
    QColor labelColor = QColor();
    ZzMultiRadialGauge::NeedleStyle needleStyle = ZzMultiRadialGauge::LineNeedle;
    qreal needleWidth = 5.0;
    qreal needleLength = 0.72;
    QPointF needleOffset = QPointF(0.0, 0.08);
    bool hubVisible = true;
    qreal hubRadius = 7.0;
    QColor hubColor = QColor();
    bool titleVisible = true;
    bool detailVisible = true;
    bool detailBadgeVisible = true;
    int titleFontPixelSize = 12;
    int detailFontPixelSize = 12;
    QColor titleColor = QColor();
    QColor detailTextColor = QColor();
    qreal detailBadgePadding = 8.0;
    QString valueSuffix = QStringLiteral("%");
    int valueDecimals = 0;
    int valueAnimationDuration = 500;
    QList<ZzMultiRadialGaugeItem *> items;
    QHash<const ZzMultiRadialGaugeItem *, qreal> displayedValues;
    QHash<const ZzMultiRadialGaugeItem *, qreal> animationStartValues;
    QVariantAnimation *valueAnimation = nullptr;
};

} // namespace ZzFluentUI
