#pragma once
#include <ZzFluentUI/ZzMultiProgressRing.h>
#include <QHash>
#include <QVariantAnimation>

namespace ZzFluentUI {

/** @brief ZzMultiProgressRingItem 的私有属性与呈现状态。 */
class ZzMultiProgressRingItemPrivate final {
  public:
    /** @brief QObject 修改父子关系期间拒绝嵌套接管。 */
    bool adoptionInProgress = false;
    QString label = QString();
    qreal value = 0.0;
    QColor color = QColor();
};

/** @brief ZzMultiProgressRing 的私有属性与呈现状态。 */
class ZzMultiProgressRingPrivate final {
  public:
    qreal minimum = 0.0;
    qreal maximum = 100.0;
    qreal startAngle = 0.0;
    qreal sweepAngle = 360.0;
    qreal ringWidth = 8.0;
    qreal ringSpacing = 4.0;
    qreal ringPadding = 12.0;
    Qt::PenCapStyle capStyle = Qt::RoundCap;
    bool trackVisible = false;
    QColor trackColor = QColor();
    bool detailsVisible = true;
    bool valueBadgeVisible = true;
    QColor labelColor = QColor();
    QString valueSuffix = QStringLiteral("%");
    int valueDecimals = 0;
    int labelFontPixelSize = 0;
    int valueFontPixelSize = 0;
    int valueAnimationDuration = 500;
    QList<ZzMultiProgressRingItem *> items;
    QHash<const ZzMultiProgressRingItem *, qreal> displayedValues;
    QHash<const ZzMultiProgressRingItem *, qreal> animationStartValues;
    QVariantAnimation *valueAnimation = nullptr;
};

} // namespace ZzFluentUI
