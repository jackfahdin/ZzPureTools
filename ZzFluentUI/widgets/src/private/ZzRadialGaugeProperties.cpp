#include <ZzFluentUI/ZzRadialGauge.h>

#include "ZzRadialGaugePrivate.h"

#include <QPointer>
#include <QtMath>

#include <utility>

namespace ZzFluentUI {
namespace {
constexpr qreal FullCircle = 360.0;
constexpr int MaximumTickCount = 720;
constexpr int MaximumMajorTickCount = 180;
} // namespace

int ZzRadialGaugeRange::fromValue() const
{
    return d_ptr->fromValue;
}

void ZzRadialGaugeRange::setFromValue(int value)
{
    if (d_ptr->fromValue == value) {
        return;
    }

    d_ptr->fromValue = value;
    QPointer<ZzRadialGaugeRange> guard(this);
    emit fromValueChanged(value);
    if (!guard)
        return;
    emit rangeChanged();
}

int ZzRadialGaugeRange::toValue() const
{
    return d_ptr->toValue;
}

void ZzRadialGaugeRange::setToValue(int value)
{
    if (d_ptr->toValue == value) {
        return;
    }

    d_ptr->toValue = value;
    QPointer<ZzRadialGaugeRange> guard(this);
    emit toValueChanged(value);
    if (!guard)
        return;
    emit rangeChanged();
}

QColor ZzRadialGaugeRange::color() const
{
    return d_ptr->color;
}

void ZzRadialGaugeRange::setColor(QColor color)
{
    if (d_ptr->color == color) {
        return;
    }

    d_ptr->color = color;
    QPointer<ZzRadialGaugeRange> guard(this);
    emit colorChanged(color);
    if (!guard)
        return;
    emit rangeChanged();
}

bool ZzRadialGauge::isInteractive() const
{
    return d_ptr->interactive;
}

void ZzRadialGauge::setInteractive(bool interactive)
{
    QPointer<ZzRadialGauge> guard(this);
    if (d_ptr->interactive == interactive) {
        return;
    }

    if (!interactive) {
        if (isSliderDown()) {
            if (!hasTracking()) {
                setValue(sliderPosition());
                if (!guard)
                    return;
            }
            setSliderDown(false);
            if (!guard)
                return;
        }
        d_ptr->interactiveFocusPolicy = focusPolicy();
        setFocusPolicy(Qt::NoFocus);
    } else {
        setFocusPolicy(d_ptr->interactiveFocusPolicy);
    }

    d_ptr->interactive = interactive;
    emit interactiveChanged(interactive);
}

int ZzRadialGauge::valueAnimationDuration() const
{
    return d_ptr->valueAnimationDuration;
}

void ZzRadialGauge::setValueAnimationDuration(int duration)
{
    duration = qBound(0, duration, 5000);
    if (d_ptr->valueAnimationDuration == duration) {
        return;
    }

    d_ptr->valueAnimationDuration = duration;
    if (d_ptr->valueAnimation) {
        QPointer<ZzRadialGauge> guard(this);
        settleValueAnimation();
        if (!guard)
            return;
        d_ptr->valueAnimation->setDuration(duration);
    }
    update();
    emit valueAnimationDurationChanged(duration);
}

ZzRadialGauge::ScaleMode ZzRadialGauge::scaleMode() const
{
    return d_ptr->scaleMode;
}

void ZzRadialGauge::setScaleMode(ScaleMode mode)
{
    if (mode < TrackScale || mode > RangeScale || d_ptr->scaleMode == mode) {
        return;
    }

    d_ptr->scaleMode = mode;
    const Qt::PenCapStyle defaultCapStyle = mode == ProgressScale ? Qt::RoundCap : Qt::FlatCap;
    const bool trackCapStyleDidChange = d_ptr->trackCapStyle != defaultCapStyle;
    const bool ringCapStyleDidChange = d_ptr->ringCapStyle != defaultCapStyle;
    d_ptr->trackCapStyle = defaultCapStyle;
    d_ptr->ringCapStyle = defaultCapStyle;
    update();
    QPointer<ZzRadialGauge> guard(this);
    emit scaleModeChanged(mode);
    if (!guard || d_ptr->scaleMode != mode)
        return;
    if (trackCapStyleDidChange) {
        emit trackCapStyleChanged(d_ptr->trackCapStyle);
    }
    if (!guard || d_ptr->scaleMode != mode)
        return;
    if (ringCapStyleDidChange) {
        emit ringCapStyleChanged(d_ptr->ringCapStyle);
    }
}

qreal ZzRadialGauge::minimumAngle() const
{
    return d_ptr->minimumAngle;
}

void ZzRadialGauge::setMinimumAngle(qreal angle)
{
    if (!qIsFinite(angle)) {
        return;
    }

    angle = qBound(-FullCircle, angle, FullCircle);
    if (qFuzzyCompare(d_ptr->minimumAngle, angle)) {
        return;
    }

    d_ptr->minimumAngle = angle;
    update();
    emit minimumAngleChanged(angle);
}

qreal ZzRadialGauge::maximumAngle() const
{
    return d_ptr->maximumAngle;
}

void ZzRadialGauge::setMaximumAngle(qreal angle)
{
    if (!qIsFinite(angle)) {
        return;
    }

    angle = qBound(-FullCircle, angle, FullCircle);
    if (qFuzzyCompare(d_ptr->maximumAngle, angle)) {
        return;
    }

    d_ptr->maximumAngle = angle;
    update();
    emit maximumAngleChanged(angle);
}

int ZzRadialGauge::majorTickCount() const
{
    return d_ptr->majorTickCount;
}

void ZzRadialGauge::setMajorTickCount(int count)
{
    count = qBound(2, count, MaximumMajorTickCount);
    if (d_ptr->majorTickCount == count) {
        return;
    }

    d_ptr->majorTickCount = count;
    update();
    emit majorTickCountChanged(count);
}

int ZzRadialGauge::minorTickCount() const
{
    return d_ptr->minorTickCount;
}

void ZzRadialGauge::setMinorTickCount(int count)
{
    count = qBound(0, count, MaximumTickCount);
    if (d_ptr->minorTickCount == count) {
        return;
    }

    d_ptr->minorTickCount = count;
    update();
    emit minorTickCountChanged(count);
}

qreal ZzRadialGauge::scaleWidth() const
{
    return d_ptr->scaleWidth;
}

void ZzRadialGauge::setScaleWidth(qreal width)
{
    if (!qIsFinite(width)) {
        return;
    }

    width = qBound(0.5, width, 100.0);
    if (qFuzzyCompare(d_ptr->scaleWidth, width)) {
        return;
    }

    d_ptr->scaleWidth = width;
    update();
    emit scaleWidthChanged(width);
}

qreal ZzRadialGauge::scalePadding() const
{
    return d_ptr->scalePadding;
}

void ZzRadialGauge::setScalePadding(qreal padding)
{
    if (!qIsFinite(padding)) {
        return;
    }

    padding = qBound(0.0, padding, 200.0);
    if (qFuzzyCompare(d_ptr->scalePadding, padding)) {
        return;
    }

    d_ptr->scalePadding = padding;
    update();
    emit scalePaddingChanged(padding);
}

Qt::PenCapStyle ZzRadialGauge::trackCapStyle() const
{
    return d_ptr->trackCapStyle;
}

void ZzRadialGauge::setTrackCapStyle(Qt::PenCapStyle style)
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

Qt::PenCapStyle ZzRadialGauge::ringCapStyle() const
{
    return d_ptr->ringCapStyle;
}

void ZzRadialGauge::setRingCapStyle(Qt::PenCapStyle style)
{
    if (style != Qt::FlatCap && style != Qt::SquareCap && style != Qt::RoundCap) {
        return;
    }
    if (d_ptr->ringCapStyle == style) {
        return;
    }

    d_ptr->ringCapStyle = style;
    update();
    emit ringCapStyleChanged(style);
}

bool ZzRadialGauge::isProgressGradientEnabled() const
{
    return d_ptr->progressGradientEnabled;
}

void ZzRadialGauge::setProgressGradientEnabled(bool value)
{
    if (d_ptr->progressGradientEnabled == value)
        return;
    d_ptr->progressGradientEnabled = value;
    update();
    emit progressGradientEnabledChanged(value);
}

bool ZzRadialGauge::isSweepAreaVisible() const
{
    return d_ptr->sweepAreaVisible;
}

void ZzRadialGauge::setSweepAreaVisible(bool value)
{
    if (d_ptr->sweepAreaVisible == value)
        return;
    d_ptr->sweepAreaVisible = value;
    update();
    emit sweepAreaVisibleChanged(value);
}

qreal ZzRadialGauge::sweepAreaOpacity() const
{
    return d_ptr->sweepAreaOpacity;
}

void ZzRadialGauge::setSweepAreaOpacity(qreal opacity)
{
    if (!qIsFinite(opacity)) {
        return;
    }

    opacity = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(d_ptr->sweepAreaOpacity + 1.0, opacity + 1.0)) {
        return;
    }

    d_ptr->sweepAreaOpacity = opacity;
    update();
    emit sweepAreaOpacityChanged(opacity);
}

QColor ZzRadialGauge::progressGradientStartColor() const
{
    return d_ptr->progressGradientStartColor;
}

void ZzRadialGauge::setProgressGradientStartColor(QColor value)
{
    if (d_ptr->progressGradientStartColor == value)
        return;
    d_ptr->progressGradientStartColor = value;
    update();
    emit progressGradientStartColorChanged(value);
}

QColor ZzRadialGauge::progressGradientEndColor() const
{
    return d_ptr->progressGradientEndColor;
}

void ZzRadialGauge::setProgressGradientEndColor(QColor value)
{
    if (d_ptr->progressGradientEndColor == value)
        return;
    d_ptr->progressGradientEndColor = value;
    update();
    emit progressGradientEndColorChanged(value);
}

qreal ZzRadialGauge::needleWidth() const
{
    return d_ptr->needleWidth;
}

void ZzRadialGauge::setNeedleWidth(qreal width)
{
    if (!qIsFinite(width)) {
        return;
    }

    width = qBound(0.5, width, 100.0);
    if (qFuzzyCompare(d_ptr->needleWidth, width)) {
        return;
    }

    d_ptr->needleWidth = width;
    update();
    emit needleWidthChanged(width);
}

ZzRadialGauge::NeedleStyle ZzRadialGauge::needleStyle() const
{
    return d_ptr->needleStyle;
}

void ZzRadialGauge::setNeedleStyle(NeedleStyle style)
{
    if (style < NoNeedle || style > TriangleNeedle || d_ptr->needleStyle == style) {
        return;
    }

    d_ptr->needleStyle = style;
    update();
    emit needleStyleChanged(style);
}

qreal ZzRadialGauge::needleLength() const
{
    return d_ptr->needleLength;
}

void ZzRadialGauge::setNeedleLength(qreal length)
{
    if (!qIsFinite(length)) {
        return;
    }

    length = qBound(0.05, length, 1.0);
    if (qFuzzyCompare(d_ptr->needleLength, length)) {
        return;
    }

    d_ptr->needleLength = length;
    update();
    emit needleLengthChanged(length);
}

qreal ZzRadialGauge::tickLength() const
{
    return d_ptr->tickLength;
}

void ZzRadialGauge::setTickLength(qreal length)
{
    if (!qIsFinite(length)) {
        return;
    }

    length = qBound(0.0, length, 100.0);
    if (qFuzzyCompare(d_ptr->tickLength, length)) {
        return;
    }

    d_ptr->tickLength = length;
    update();
    emit tickLengthChanged(length);
}

qreal ZzRadialGauge::tickWidth() const
{
    return d_ptr->tickWidth;
}

void ZzRadialGauge::setTickWidth(qreal width)
{
    if (!qIsFinite(width)) {
        return;
    }

    width = qBound(0.5, width, 100.0);
    if (qFuzzyCompare(d_ptr->tickWidth, width)) {
        return;
    }

    d_ptr->tickWidth = width;
    update();
    emit tickWidthChanged(width);
}

qreal ZzRadialGauge::majorTickLength() const
{
    return d_ptr->majorTickLength;
}

void ZzRadialGauge::setMajorTickLength(qreal length)
{
    if (!qIsFinite(length)) {
        return;
    }

    length = qBound(0.0, length, 100.0);
    if (qFuzzyCompare(d_ptr->majorTickLength, length)) {
        return;
    }

    d_ptr->majorTickLength = length;
    update();
    emit majorTickLengthChanged(length);
}

qreal ZzRadialGauge::majorTickWidth() const
{
    return d_ptr->majorTickWidth;
}

void ZzRadialGauge::setMajorTickWidth(qreal width)
{
    if (!qIsFinite(width)) {
        return;
    }

    width = qBound(0.5, width, 100.0);
    if (qFuzzyCompare(d_ptr->majorTickWidth, width)) {
        return;
    }

    d_ptr->majorTickWidth = width;
    update();
    emit majorTickWidthChanged(width);
}

qreal ZzRadialGauge::tickPadding() const
{
    return d_ptr->tickPadding;
}

void ZzRadialGauge::setTickPadding(qreal padding)
{
    if (!qIsFinite(padding)) {
        return;
    }

    padding = qBound(0.0, padding, 200.0);
    if (qFuzzyCompare(d_ptr->tickPadding, padding)) {
        return;
    }

    d_ptr->tickPadding = padding;
    update();
    emit tickPaddingChanged(padding);
}

bool ZzRadialGauge::areLabelsVisible() const
{
    return d_ptr->labelsVisible;
}

void ZzRadialGauge::setLabelsVisible(bool value)
{
    if (d_ptr->labelsVisible == value)
        return;
    d_ptr->labelsVisible = value;
    update();
    emit labelsVisibleChanged(value);
}

qreal ZzRadialGauge::labelPadding() const
{
    return d_ptr->labelPadding;
}

void ZzRadialGauge::setLabelPadding(qreal padding)
{
    if (!qIsFinite(padding)) {
        return;
    }

    padding = qBound(0.0, padding, 200.0);
    if (qFuzzyCompare(d_ptr->labelPadding, padding)) {
        return;
    }

    d_ptr->labelPadding = padding;
    update();
    emit labelPaddingChanged(padding);
}

int ZzRadialGauge::labelFontPixelSize() const
{
    return d_ptr->labelFontPixelSize;
}

void ZzRadialGauge::setLabelFontPixelSize(int size)
{
    size = qBound(1, size, 200);
    if (d_ptr->labelFontPixelSize == size) {
        return;
    }

    d_ptr->labelFontPixelSize = size;
    update();
    emit labelFontPixelSizeChanged(size);
}

bool ZzRadialGauge::isHubVisible() const
{
    return d_ptr->hubVisible;
}

void ZzRadialGauge::setHubVisible(bool value)
{
    if (d_ptr->hubVisible == value)
        return;
    d_ptr->hubVisible = value;
    update();
    emit hubVisibleChanged(value);
}

qreal ZzRadialGauge::hubRadius() const
{
    return d_ptr->hubRadius;
}

void ZzRadialGauge::setHubRadius(qreal radius)
{
    if (!qIsFinite(radius)) {
        return;
    }

    radius = qBound(0.0, radius, 100.0);
    if (qFuzzyCompare(d_ptr->hubRadius, radius)) {
        return;
    }

    d_ptr->hubRadius = radius;
    update();
    emit hubRadiusChanged(radius);
}

bool ZzRadialGauge::isValueVisible() const
{
    return d_ptr->valueVisible;
}

void ZzRadialGauge::setValueVisible(bool value)
{
    if (d_ptr->valueVisible == value)
        return;
    d_ptr->valueVisible = value;
    update();
    emit valueVisibleChanged(value);
}

ZzRadialGauge::ValuePosition ZzRadialGauge::valuePosition() const
{
    return d_ptr->valuePosition;
}

void ZzRadialGauge::setValuePosition(ValuePosition position)
{
    if (position < CenterValue || position > BottomValue || d_ptr->valuePosition == position) {
        return;
    }

    d_ptr->valuePosition = position;
    update();
    emit valuePositionChanged(position);
}

QString ZzRadialGauge::title() const
{
    return d_ptr->title;
}

void ZzRadialGauge::setTitle(QString value)
{
    if (d_ptr->title == value)
        return;
    d_ptr->title = value;
    update();
    emit titleChanged(value);
}

QString ZzRadialGauge::unit() const
{
    return d_ptr->unit;
}

void ZzRadialGauge::setUnit(QString value)
{
    if (d_ptr->unit == value)
        return;
    d_ptr->unit = value;
    update();
    emit unitChanged(value);
}

int ZzRadialGauge::valueFontPixelSize() const
{
    return d_ptr->valueFontPixelSize;
}

void ZzRadialGauge::setValueFontPixelSize(int size)
{
    size = qBound(0, size, 200);
    if (d_ptr->valueFontPixelSize == size) {
        return;
    }

    d_ptr->valueFontPixelSize = size;
    update();
    emit valueFontPixelSizeChanged(size);
}

QColor ZzRadialGauge::needleColor() const
{
    return d_ptr->needleColor;
}

void ZzRadialGauge::setNeedleColor(QColor value)
{
    if (d_ptr->needleColor == value)
        return;
    d_ptr->needleColor = value;
    update();
    emit needleColorChanged(value);
}

QColor ZzRadialGauge::tickColor() const
{
    return d_ptr->tickColor;
}

void ZzRadialGauge::setTickColor(QColor value)
{
    if (d_ptr->tickColor == value)
        return;
    d_ptr->tickColor = value;
    update();
    emit tickColorChanged(value);
}

QColor ZzRadialGauge::labelColor() const
{
    return d_ptr->labelColor;
}

void ZzRadialGauge::setLabelColor(QColor value)
{
    if (d_ptr->labelColor == value)
        return;
    d_ptr->labelColor = value;
    update();
    emit labelColorChanged(value);
}

QColor ZzRadialGauge::valueColor() const
{
    return d_ptr->valueColor;
}

void ZzRadialGauge::setValueColor(QColor value)
{
    if (d_ptr->valueColor == value)
        return;
    d_ptr->valueColor = value;
    update();
    emit valueColorChanged(value);
}

} // namespace ZzFluentUI
