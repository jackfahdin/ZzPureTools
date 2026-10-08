#include <ZzFluentUI/ZzMultiProgressRing.h>

#include "ZzMultiProgressRingPrivate.h"

#include <QPointer>
#include <QtMath>

#include <utility>

namespace ZzFluentUI {
namespace {
constexpr qreal FullCircle = 360.0;
}

QString ZzMultiProgressRingItem::label() const
{
    return d_ptr->label;
}

void ZzMultiProgressRingItem::setLabel(QString label)
{
    if (d_ptr->label == label) {
        return;
    }

    d_ptr->label = std::move(label);
    QPointer<ZzMultiProgressRingItem> guard(this);
    emit labelChanged(d_ptr->label);
    if (!guard)
        return;
    emit itemChanged();
}

qreal ZzMultiProgressRingItem::value() const
{
    return d_ptr->value;
}

void ZzMultiProgressRingItem::setValue(qreal value)
{
    if (!qIsFinite(value) || qFuzzyCompare(d_ptr->value + 1.0, value + 1.0)) {
        return;
    }

    d_ptr->value = value;
    QPointer<ZzMultiProgressRingItem> guard(this);
    emit valueChanged(value);
    if (!guard)
        return;
    emit itemChanged();
}

QColor ZzMultiProgressRingItem::color() const
{
    return d_ptr->color;
}

void ZzMultiProgressRingItem::setColor(QColor color)
{
    if (d_ptr->color == color) {
        return;
    }

    d_ptr->color = color;
    QPointer<ZzMultiProgressRingItem> guard(this);
    emit colorChanged(color);
    if (!guard)
        return;
    emit itemChanged();
}

qreal ZzMultiProgressRing::minimum() const
{
    return d_ptr->minimum;
}

void ZzMultiProgressRing::setMinimum(qreal minimum)
{
    if (!qIsFinite(minimum) || qFuzzyCompare(d_ptr->minimum + 1.0, minimum + 1.0)) {
        return;
    }

    d_ptr->minimum = minimum;
    update();
    emit minimumChanged(minimum);
}

qreal ZzMultiProgressRing::maximum() const
{
    return d_ptr->maximum;
}

void ZzMultiProgressRing::setMaximum(qreal maximum)
{
    if (!qIsFinite(maximum) || qFuzzyCompare(d_ptr->maximum + 1.0, maximum + 1.0)) {
        return;
    }

    d_ptr->maximum = maximum;
    update();
    emit maximumChanged(maximum);
}

qreal ZzMultiProgressRing::startAngle() const
{
    return d_ptr->startAngle;
}

void ZzMultiProgressRing::setStartAngle(qreal angle)
{
    if (!qIsFinite(angle)) {
        return;
    }

    angle = std::fmod(angle, FullCircle);
    if (qFuzzyCompare(d_ptr->startAngle + 1.0, angle + 1.0)) {
        return;
    }

    d_ptr->startAngle = angle;
    update();
    emit startAngleChanged(angle);
}

qreal ZzMultiProgressRing::sweepAngle() const
{
    return d_ptr->sweepAngle;
}

void ZzMultiProgressRing::setSweepAngle(qreal angle)
{
    if (!qIsFinite(angle)) {
        return;
    }

    angle = qBound(0.0, angle, FullCircle);
    if (qFuzzyCompare(d_ptr->sweepAngle + 1.0, angle + 1.0)) {
        return;
    }

    d_ptr->sweepAngle = angle;
    update();
    emit sweepAngleChanged(angle);
}

qreal ZzMultiProgressRing::ringWidth() const
{
    return d_ptr->ringWidth;
}

void ZzMultiProgressRing::setRingWidth(qreal width)
{
    if (!qIsFinite(width)) {
        return;
    }

    width = qBound(0.5, width, 100.0);
    if (qFuzzyCompare(d_ptr->ringWidth, width)) {
        return;
    }

    d_ptr->ringWidth = width;
    updateGeometry();
    update();
    emit ringWidthChanged(width);
}

qreal ZzMultiProgressRing::ringSpacing() const
{
    return d_ptr->ringSpacing;
}

void ZzMultiProgressRing::setRingSpacing(qreal spacing)
{
    if (!qIsFinite(spacing)) {
        return;
    }

    spacing = qBound(0.0, spacing, 100.0);
    if (qFuzzyCompare(d_ptr->ringSpacing + 1.0, spacing + 1.0)) {
        return;
    }

    d_ptr->ringSpacing = spacing;
    updateGeometry();
    update();
    emit ringSpacingChanged(spacing);
}

qreal ZzMultiProgressRing::ringPadding() const
{
    return d_ptr->ringPadding;
}

void ZzMultiProgressRing::setRingPadding(qreal padding)
{
    if (!qIsFinite(padding)) {
        return;
    }

    padding = qBound(0.0, padding, 200.0);
    if (qFuzzyCompare(d_ptr->ringPadding + 1.0, padding + 1.0)) {
        return;
    }

    d_ptr->ringPadding = padding;
    updateGeometry();
    update();
    emit ringPaddingChanged(padding);
}

Qt::PenCapStyle ZzMultiProgressRing::capStyle() const
{
    return d_ptr->capStyle;
}

void ZzMultiProgressRing::setCapStyle(Qt::PenCapStyle style)
{
    if (style != Qt::FlatCap && style != Qt::SquareCap && style != Qt::RoundCap) {
        return;
    }
    if (d_ptr->capStyle == style) {
        return;
    }

    d_ptr->capStyle = style;
    update();
    emit capStyleChanged(style);
}

bool ZzMultiProgressRing::isTrackVisible() const
{
    return d_ptr->trackVisible;
}

void ZzMultiProgressRing::setTrackVisible(bool value)
{
    if (d_ptr->trackVisible == value)
        return;
    d_ptr->trackVisible = value;
    update();
    emit trackVisibleChanged(value);
}

QColor ZzMultiProgressRing::trackColor() const
{
    return d_ptr->trackColor;
}

void ZzMultiProgressRing::setTrackColor(QColor value)
{
    if (d_ptr->trackColor == value)
        return;
    d_ptr->trackColor = value;
    update();
    emit trackColorChanged(value);
}

bool ZzMultiProgressRing::areDetailsVisible() const
{
    return d_ptr->detailsVisible;
}

void ZzMultiProgressRing::setDetailsVisible(bool value)
{
    if (d_ptr->detailsVisible == value)
        return;
    d_ptr->detailsVisible = value;
    update();
    emit detailsVisibleChanged(value);
}

bool ZzMultiProgressRing::isValueBadgeVisible() const
{
    return d_ptr->valueBadgeVisible;
}

void ZzMultiProgressRing::setValueBadgeVisible(bool value)
{
    if (d_ptr->valueBadgeVisible == value)
        return;
    d_ptr->valueBadgeVisible = value;
    update();
    emit valueBadgeVisibleChanged(value);
}

QColor ZzMultiProgressRing::labelColor() const
{
    return d_ptr->labelColor;
}

void ZzMultiProgressRing::setLabelColor(QColor value)
{
    if (d_ptr->labelColor == value)
        return;
    d_ptr->labelColor = value;
    update();
    emit labelColorChanged(value);
}

QString ZzMultiProgressRing::valueSuffix() const
{
    return d_ptr->valueSuffix;
}

void ZzMultiProgressRing::setValueSuffix(QString value)
{
    if (d_ptr->valueSuffix == value)
        return;
    d_ptr->valueSuffix = value;
    update();
    emit valueSuffixChanged(value);
}

int ZzMultiProgressRing::valueDecimals() const
{
    return d_ptr->valueDecimals;
}

void ZzMultiProgressRing::setValueDecimals(int decimals)
{
    decimals = qBound(0, decimals, 6);
    if (d_ptr->valueDecimals == decimals) {
        return;
    }

    d_ptr->valueDecimals = decimals;
    update();
    emit valueDecimalsChanged(decimals);
}

int ZzMultiProgressRing::labelFontPixelSize() const
{
    return d_ptr->labelFontPixelSize;
}

void ZzMultiProgressRing::setLabelFontPixelSize(int size)
{
    size = qBound(0, size, 200);
    if (d_ptr->labelFontPixelSize == size) {
        return;
    }

    d_ptr->labelFontPixelSize = size;
    update();
    emit labelFontPixelSizeChanged(size);
}

int ZzMultiProgressRing::valueFontPixelSize() const
{
    return d_ptr->valueFontPixelSize;
}

void ZzMultiProgressRing::setValueFontPixelSize(int size)
{
    size = qBound(0, size, 200);
    if (d_ptr->valueFontPixelSize == size) {
        return;
    }

    d_ptr->valueFontPixelSize = size;
    update();
    emit valueFontPixelSizeChanged(size);
}

int ZzMultiProgressRing::valueAnimationDuration() const
{
    return d_ptr->valueAnimationDuration;
}

void ZzMultiProgressRing::setValueAnimationDuration(int duration)
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
