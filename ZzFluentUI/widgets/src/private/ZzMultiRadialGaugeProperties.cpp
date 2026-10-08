#include <ZzFluentUI/ZzMultiRadialGauge.h>

#include "ZzMultiRadialGaugePrivate.h"

#include <QPointer>
#include <QtMath>

#include <utility>

namespace ZzFluentUI {
namespace {
constexpr qreal FullCircle = 360.0;
constexpr int MaximumTickCount = 720;
constexpr int MaximumMajorTickCount = 180;
} // namespace

QString ZzMultiRadialGaugeItem::label() const
{
    return d_ptr->label;
}

void ZzMultiRadialGaugeItem::setLabel(QString value)
{
    if (d_ptr->label == value)
        return;
    d_ptr->label = value;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit labelChanged(value);
    if (!guard)
        return;
    emit itemChanged();
}

qreal ZzMultiRadialGaugeItem::value() const
{
    return d_ptr->value;
}

void ZzMultiRadialGaugeItem::setValue(qreal value)
{
    if (!qIsFinite(value) || qFuzzyCompare(d_ptr->value + 1.0, value + 1.0)) {
        return;
    }

    d_ptr->value = value;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit valueChanged(value);
    if (!guard)
        return;
    emit itemChanged();
}

QColor ZzMultiRadialGaugeItem::color() const
{
    return d_ptr->color;
}

void ZzMultiRadialGaugeItem::setColor(QColor value)
{
    if (d_ptr->color == value)
        return;
    d_ptr->color = value;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit colorChanged(value);
    if (!guard)
        return;
    emit itemChanged();
}

bool ZzMultiRadialGaugeItem::isVisible() const
{
    return d_ptr->visible;
}

void ZzMultiRadialGaugeItem::setVisible(bool value)
{
    if (d_ptr->visible == value)
        return;
    d_ptr->visible = value;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit visibleChanged(value);
    if (!guard)
        return;
    emit itemChanged();
}

QPointF ZzMultiRadialGaugeItem::titleOffset() const
{
    return d_ptr->titleOffset;
}

void ZzMultiRadialGaugeItem::setTitleOffset(QPointF offset)
{
    if (!qIsFinite(offset.x()) || !qIsFinite(offset.y()) || d_ptr->titleOffset == offset) {
        return;
    }

    d_ptr->titleOffset = offset;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit titleOffsetChanged(offset);
    if (!guard)
        return;
    emit itemChanged();
}

QPointF ZzMultiRadialGaugeItem::detailOffset() const
{
    return d_ptr->detailOffset;
}

void ZzMultiRadialGaugeItem::setDetailOffset(QPointF offset)
{
    if (!qIsFinite(offset.x()) || !qIsFinite(offset.y()) || d_ptr->detailOffset == offset) {
        return;
    }

    d_ptr->detailOffset = offset;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit detailOffsetChanged(offset);
    if (!guard)
        return;
    emit itemChanged();
}

qreal ZzMultiRadialGauge::minimum() const
{
    return d_ptr->minimum;
}

void ZzMultiRadialGauge::setMinimum(qreal minimum)
{
    if (!qIsFinite(minimum)) {
        return;
    }
    setRange(minimum, qMax(minimum, d_ptr->maximum));
}

qreal ZzMultiRadialGauge::maximum() const
{
    return d_ptr->maximum;
}

void ZzMultiRadialGauge::setMaximum(qreal maximum)
{
    if (!qIsFinite(maximum)) {
        return;
    }
    setRange(qMin(d_ptr->minimum, maximum), maximum);
}

qreal ZzMultiRadialGauge::minimumAngle() const
{
    return d_ptr->minimumAngle;
}

void ZzMultiRadialGauge::setMinimumAngle(qreal angle)
{
    if (!qIsFinite(angle))
        return;
    angle = qBound(-FullCircle, angle, FullCircle);
    if (qFuzzyCompare(d_ptr->minimumAngle + 1.0, angle + 1.0)) {
        return;
    }
    d_ptr->minimumAngle = angle;
    update();
    emit minimumAngleChanged(angle);
}

qreal ZzMultiRadialGauge::maximumAngle() const
{
    return d_ptr->maximumAngle;
}

void ZzMultiRadialGauge::setMaximumAngle(qreal angle)
{
    if (!qIsFinite(angle))
        return;
    angle = qBound(-FullCircle, angle, FullCircle);
    if (qFuzzyCompare(d_ptr->maximumAngle + 1.0, angle + 1.0)) {
        return;
    }
    d_ptr->maximumAngle = angle;
    update();
    emit maximumAngleChanged(angle);
}

int ZzMultiRadialGauge::majorTickCount() const
{
    return d_ptr->majorTickCount;
}

void ZzMultiRadialGauge::setMajorTickCount(int count)
{
    count = qBound(2, count, MaximumMajorTickCount);
    if (d_ptr->majorTickCount == count) {
        return;
    }
    d_ptr->majorTickCount = count;
    update();
    emit majorTickCountChanged(count);
}

int ZzMultiRadialGauge::minorTickCount() const
{
    return d_ptr->minorTickCount;
}

void ZzMultiRadialGauge::setMinorTickCount(int count)
{
    count = qBound(0, count, MaximumTickCount);
    if (d_ptr->minorTickCount == count) {
        return;
    }
    d_ptr->minorTickCount = count;
    update();
    emit minorTickCountChanged(count);
}

bool ZzMultiRadialGauge::isTrackVisible() const
{
    return d_ptr->trackVisible;
}

void ZzMultiRadialGauge::setTrackVisible(bool value)
{
    if (d_ptr->trackVisible == value)
        return;
    d_ptr->trackVisible = value;
    update();
    emit trackVisibleChanged(value);
}

qreal ZzMultiRadialGauge::trackWidth() const
{
    return d_ptr->trackWidth;
}

void ZzMultiRadialGauge::setTrackWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.5, value, 100.0);
    if (d_ptr->trackWidth == value)
        return;
    d_ptr->trackWidth = value;
    updateGeometry();
    update();
    emit trackWidthChanged(value);
}

QColor ZzMultiRadialGauge::trackColor() const
{
    return d_ptr->trackColor;
}

void ZzMultiRadialGauge::setTrackColor(QColor value)
{
    if (d_ptr->trackColor == value)
        return;
    d_ptr->trackColor = value;
    update();
    emit trackColorChanged(value);
}

Qt::PenCapStyle ZzMultiRadialGauge::trackCapStyle() const
{
    return d_ptr->trackCapStyle;
}

void ZzMultiRadialGauge::setTrackCapStyle(Qt::PenCapStyle style)
{
    if (style != Qt::FlatCap && style != Qt::SquareCap && style != Qt::RoundCap) {
        return;
    }
    if (d_ptr->trackCapStyle == style) {
        return;
    }
    d_ptr->trackCapStyle = style;
    update();
    emit trackCapStyleChanged(style);
}

bool ZzMultiRadialGauge::isProgressVisible() const
{
    return d_ptr->progressVisible;
}

void ZzMultiRadialGauge::setProgressVisible(bool value)
{
    if (d_ptr->progressVisible == value)
        return;
    d_ptr->progressVisible = value;
    update();
    emit progressVisibleChanged(value);
}

bool ZzMultiRadialGauge::isProgressOverlap() const
{
    return d_ptr->progressOverlap;
}

void ZzMultiRadialGauge::setProgressOverlap(bool value)
{
    if (d_ptr->progressOverlap == value)
        return;
    d_ptr->progressOverlap = value;
    update();
    emit progressOverlapChanged(value);
}

qreal ZzMultiRadialGauge::progressWidth() const
{
    return d_ptr->progressWidth;
}

void ZzMultiRadialGauge::setProgressWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.5, value, 100.0);
    if (d_ptr->progressWidth == value)
        return;
    d_ptr->progressWidth = value;
    updateGeometry();
    update();
    emit progressWidthChanged(value);
}

qreal ZzMultiRadialGauge::progressSpacing() const
{
    return d_ptr->progressSpacing;
}

void ZzMultiRadialGauge::setProgressSpacing(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->progressSpacing == value)
        return;
    d_ptr->progressSpacing = value;
    updateGeometry();
    update();
    emit progressSpacingChanged(value);
}

Qt::PenCapStyle ZzMultiRadialGauge::progressCapStyle() const
{
    return d_ptr->progressCapStyle;
}

void ZzMultiRadialGauge::setProgressCapStyle(Qt::PenCapStyle style)
{
    if (style != Qt::FlatCap && style != Qt::SquareCap && style != Qt::RoundCap) {
        return;
    }
    if (d_ptr->progressCapStyle == style) {
        return;
    }
    d_ptr->progressCapStyle = style;
    update();
    emit progressCapStyleChanged(style);
}

qreal ZzMultiRadialGauge::scalePadding() const
{
    return d_ptr->scalePadding;
}

void ZzMultiRadialGauge::setScalePadding(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 200.0);
    if (d_ptr->scalePadding == value)
        return;
    d_ptr->scalePadding = value;
    updateGeometry();
    update();
    emit scalePaddingChanged(value);
}

qreal ZzMultiRadialGauge::tickLength() const
{
    return d_ptr->tickLength;
}

void ZzMultiRadialGauge::setTickLength(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->tickLength == value)
        return;
    d_ptr->tickLength = value;
    updateGeometry();
    update();
    emit tickLengthChanged(value);
}

qreal ZzMultiRadialGauge::tickWidth() const
{
    return d_ptr->tickWidth;
}

void ZzMultiRadialGauge::setTickWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.1, value, 50.0);
    if (d_ptr->tickWidth == value)
        return;
    d_ptr->tickWidth = value;
    updateGeometry();
    update();
    emit tickWidthChanged(value);
}

qreal ZzMultiRadialGauge::majorTickLength() const
{
    return d_ptr->majorTickLength;
}

void ZzMultiRadialGauge::setMajorTickLength(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->majorTickLength == value)
        return;
    d_ptr->majorTickLength = value;
    updateGeometry();
    update();
    emit majorTickLengthChanged(value);
}

qreal ZzMultiRadialGauge::majorTickWidth() const
{
    return d_ptr->majorTickWidth;
}

void ZzMultiRadialGauge::setMajorTickWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.1, value, 50.0);
    if (d_ptr->majorTickWidth == value)
        return;
    d_ptr->majorTickWidth = value;
    updateGeometry();
    update();
    emit majorTickWidthChanged(value);
}

qreal ZzMultiRadialGauge::tickPadding() const
{
    return d_ptr->tickPadding;
}

void ZzMultiRadialGauge::setTickPadding(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->tickPadding == value)
        return;
    d_ptr->tickPadding = value;
    updateGeometry();
    update();
    emit tickPaddingChanged(value);
}

QColor ZzMultiRadialGauge::tickColor() const
{
    return d_ptr->tickColor;
}

void ZzMultiRadialGauge::setTickColor(QColor value)
{
    if (d_ptr->tickColor == value)
        return;
    d_ptr->tickColor = value;
    update();
    emit tickColorChanged(value);
}

bool ZzMultiRadialGauge::areLabelsVisible() const
{
    return d_ptr->labelsVisible;
}

void ZzMultiRadialGauge::setLabelsVisible(bool value)
{
    if (d_ptr->labelsVisible == value)
        return;
    d_ptr->labelsVisible = value;
    update();
    emit labelsVisibleChanged(value);
}

qreal ZzMultiRadialGauge::labelPadding() const
{
    return d_ptr->labelPadding;
}

void ZzMultiRadialGauge::setLabelPadding(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->labelPadding == value)
        return;
    d_ptr->labelPadding = value;
    updateGeometry();
    update();
    emit labelPaddingChanged(value);
}

int ZzMultiRadialGauge::labelFontPixelSize() const
{
    return d_ptr->labelFontPixelSize;
}

void ZzMultiRadialGauge::setLabelFontPixelSize(int size)
{
    size = qBound(1, size, 200);
    if (d_ptr->labelFontPixelSize == size) {
        return;
    }
    d_ptr->labelFontPixelSize = size;
    update();
    emit labelFontPixelSizeChanged(size);
}

QColor ZzMultiRadialGauge::labelColor() const
{
    return d_ptr->labelColor;
}

void ZzMultiRadialGauge::setLabelColor(QColor value)
{
    if (d_ptr->labelColor == value)
        return;
    d_ptr->labelColor = value;
    update();
    emit labelColorChanged(value);
}

ZzMultiRadialGauge::NeedleStyle ZzMultiRadialGauge::needleStyle() const
{
    return d_ptr->needleStyle;
}

void ZzMultiRadialGauge::setNeedleStyle(NeedleStyle style)
{
    if (style < NoNeedle || style > TriangleNeedle || d_ptr->needleStyle == style) {
        return;
    }
    d_ptr->needleStyle = style;
    update();
    emit needleStyleChanged(style);
}

qreal ZzMultiRadialGauge::needleWidth() const
{
    return d_ptr->needleWidth;
}

void ZzMultiRadialGauge::setNeedleWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.5, value, 100.0);
    if (d_ptr->needleWidth == value)
        return;
    d_ptr->needleWidth = value;
    updateGeometry();
    update();
    emit needleWidthChanged(value);
}

qreal ZzMultiRadialGauge::needleLength() const
{
    return d_ptr->needleLength;
}

void ZzMultiRadialGauge::setNeedleLength(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.05, value, 1.2);
    if (d_ptr->needleLength == value)
        return;
    d_ptr->needleLength = value;
    updateGeometry();
    update();
    emit needleLengthChanged(value);
}

QPointF ZzMultiRadialGauge::needleOffset() const
{
    return d_ptr->needleOffset;
}

void ZzMultiRadialGauge::setNeedleOffset(QPointF offset)
{
    if (!qIsFinite(offset.x()) || !qIsFinite(offset.y()) || d_ptr->needleOffset == offset) {
        return;
    }
    d_ptr->needleOffset = offset;
    update();
    emit needleOffsetChanged(offset);
}

bool ZzMultiRadialGauge::isHubVisible() const
{
    return d_ptr->hubVisible;
}

void ZzMultiRadialGauge::setHubVisible(bool value)
{
    if (d_ptr->hubVisible == value)
        return;
    d_ptr->hubVisible = value;
    update();
    emit hubVisibleChanged(value);
}

qreal ZzMultiRadialGauge::hubRadius() const
{
    return d_ptr->hubRadius;
}

void ZzMultiRadialGauge::setHubRadius(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->hubRadius == value)
        return;
    d_ptr->hubRadius = value;
    updateGeometry();
    update();
    emit hubRadiusChanged(value);
}

QColor ZzMultiRadialGauge::hubColor() const
{
    return d_ptr->hubColor;
}

void ZzMultiRadialGauge::setHubColor(QColor value)
{
    if (d_ptr->hubColor == value)
        return;
    d_ptr->hubColor = value;
    update();
    emit hubColorChanged(value);
}

bool ZzMultiRadialGauge::isTitleVisible() const
{
    return d_ptr->titleVisible;
}

void ZzMultiRadialGauge::setTitleVisible(bool value)
{
    if (d_ptr->titleVisible == value)
        return;
    d_ptr->titleVisible = value;
    update();
    emit titleVisibleChanged(value);
}

bool ZzMultiRadialGauge::isDetailVisible() const
{
    return d_ptr->detailVisible;
}

void ZzMultiRadialGauge::setDetailVisible(bool value)
{
    if (d_ptr->detailVisible == value)
        return;
    d_ptr->detailVisible = value;
    update();
    emit detailVisibleChanged(value);
}

bool ZzMultiRadialGauge::isDetailBadgeVisible() const
{
    return d_ptr->detailBadgeVisible;
}

void ZzMultiRadialGauge::setDetailBadgeVisible(bool value)
{
    if (d_ptr->detailBadgeVisible == value)
        return;
    d_ptr->detailBadgeVisible = value;
    update();
    emit detailBadgeVisibleChanged(value);
}

int ZzMultiRadialGauge::titleFontPixelSize() const
{
    return d_ptr->titleFontPixelSize;
}

void ZzMultiRadialGauge::setTitleFontPixelSize(int size)
{
    size = qBound(1, size, 200);
    if (d_ptr->titleFontPixelSize == size) {
        return;
    }
    d_ptr->titleFontPixelSize = size;
    update();
    emit titleFontPixelSizeChanged(size);
}

int ZzMultiRadialGauge::detailFontPixelSize() const
{
    return d_ptr->detailFontPixelSize;
}

void ZzMultiRadialGauge::setDetailFontPixelSize(int size)
{
    size = qBound(1, size, 200);
    if (d_ptr->detailFontPixelSize == size) {
        return;
    }
    d_ptr->detailFontPixelSize = size;
    update();
    emit detailFontPixelSizeChanged(size);
}

QColor ZzMultiRadialGauge::titleColor() const
{
    return d_ptr->titleColor;
}

void ZzMultiRadialGauge::setTitleColor(QColor value)
{
    if (d_ptr->titleColor == value)
        return;
    d_ptr->titleColor = value;
    update();
    emit titleColorChanged(value);
}

QColor ZzMultiRadialGauge::detailTextColor() const
{
    return d_ptr->detailTextColor;
}

void ZzMultiRadialGauge::setDetailTextColor(QColor value)
{
    if (d_ptr->detailTextColor == value)
        return;
    d_ptr->detailTextColor = value;
    update();
    emit detailTextColorChanged(value);
}

qreal ZzMultiRadialGauge::detailBadgePadding() const
{
    return d_ptr->detailBadgePadding;
}

void ZzMultiRadialGauge::setDetailBadgePadding(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->detailBadgePadding == value)
        return;
    d_ptr->detailBadgePadding = value;
    updateGeometry();
    update();
    emit detailBadgePaddingChanged(value);
}

QString ZzMultiRadialGauge::valueSuffix() const
{
    return d_ptr->valueSuffix;
}

void ZzMultiRadialGauge::setValueSuffix(QString value)
{
    if (d_ptr->valueSuffix == value)
        return;
    d_ptr->valueSuffix = value;
    update();
    emit valueSuffixChanged(value);
}

int ZzMultiRadialGauge::valueDecimals() const
{
    return d_ptr->valueDecimals;
}

void ZzMultiRadialGauge::setValueDecimals(int decimals)
{
    decimals = qBound(0, decimals, 6);
    if (d_ptr->valueDecimals == decimals) {
        return;
    }
    d_ptr->valueDecimals = decimals;
    update();
    emit valueDecimalsChanged(decimals);
}

int ZzMultiRadialGauge::valueAnimationDuration() const
{
    return d_ptr->valueAnimationDuration;
}

void ZzMultiRadialGauge::setValueAnimationDuration(int duration)
{
    duration = qBound(0, duration, 5000);
    if (d_ptr->valueAnimationDuration == duration) {
        return;
    }
    d_ptr->valueAnimationDuration = duration;
    if (duration == 0) {
        d_ptr->valueAnimation->stop();
        synchronizeDisplayedValues();
    }
    emit valueAnimationDurationChanged(duration);
}

} // namespace ZzFluentUI
