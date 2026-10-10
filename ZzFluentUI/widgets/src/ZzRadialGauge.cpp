#include <ZzFluentUI/ZzRadialGauge.h>

#include "private/ZzRadialGaugePrivate.h"
#include "private/ZzGaugeSupportPrivate.h"

#include <QEvent>
#include <QEasingCurve>
#include <QPointer>
#include <QSizePolicy>
#include <QVariantAnimation>
#include <QAbstractAnimation>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>
#include <utility>
#include <QConicalGradient>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStyleOptionSlider>
#include <algorithm>

namespace ZzFluentUI {
namespace {
constexpr qreal FullCircle = 360.0;
qreal circularDistance(qreal first, qreal second)
{
    return std::abs(std::remainder(first - second, FullCircle));
}
constexpr qreal EndpointExtensionAngle = 1.0;
constexpr int MaximumTickCount = 720;
constexpr int MaximumMajorTickCount = 180;
QPointF pointAtAngle(const QPointF &center, qreal radius, qreal angle)
{
    const qreal radians = qDegreesToRadians(angle);
    return QPointF(center.x() + radius * std::sin(radians), center.y() - radius * std::cos(radians));
}
QPalette::ColorRole accentRole()
{
    return QPalette::Accent;
}
} // namespace

 // namespace

ZzRadialGaugeRange::ZzRadialGaugeRange(QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<ZzRadialGaugeRangePrivate>())
{
}

ZzRadialGaugeRange::ZzRadialGaugeRange(int fromValue, int toValue, const QColor &color, QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<ZzRadialGaugeRangePrivate>())
{
    d_ptr->fromValue = fromValue;
    d_ptr->toValue = toValue;
    d_ptr->color = color;
}

ZzRadialGauge::ZzRadialGauge(QWidget *parent) : QDial(parent), d_ptr(std::make_unique<ZzRadialGaugePrivate>())
{

    setWrapping(false);
    setTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    d_ptr->valueAnimation = new QVariantAnimation(this);
    d_ptr->valueAnimation->setDuration(d_ptr->valueAnimationDuration);
    d_ptr->valueAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(d_ptr->valueAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &animatedValue) {
                d_ptr->animationValue = qRound(animatedValue.toReal());
                d_ptr->animationWrite = true;
                QPointer<ZzRadialGauge> guard(this);
                QDial::setValue(d_ptr->animationValue);
                if (guard)
                    d_ptr->animationWrite = false;
            });
    connect(d_ptr->valueAnimation, &QVariantAnimation::finished, this,
            [this] { QDial::setValue(d_ptr->valueAnimation->endValue().toInt()); });
    connect(this, &QDial::rangeChanged, this, [this] { d_ptr->valueAnimation->stop(); });
}

ZzRadialGauge::~ZzRadialGauge()
{
    // 动画可能在自身 valueChanged 发射栈上被同步销毁（业务方在
    // valueChanged 里删除仪表盘），先从父子树摘下并延迟销毁
    d_ptr->valueAnimation->stop();
    d_ptr->valueAnimation->setParent(nullptr);
    d_ptr->valueAnimation->deleteLater();
    for (ZzRadialGaugeRange *range : std::as_const(d_ptr->ranges)) {
        disconnect(range, nullptr, this, nullptr);
    }
    d_ptr->ranges.clear();
}

QSize ZzRadialGauge::sizeHint() const
{
    return QSize(240, 240);
}

QSize ZzRadialGauge::minimumSizeHint() const
{
    return QSize(100, 100);
}

QList<ZzRadialGaugeRange *> ZzRadialGauge::ranges() const
{
    return d_ptr->ranges;
}

ZzRadialGaugeRange *ZzRadialGauge::addRange(int fromValue, int toValue, const QColor &color)
{
    auto *range = new ZzRadialGaugeRange(fromValue, toValue, color, this);
    d_ptr->ranges.append(range);

    connect(range, &ZzRadialGaugeRange::rangeChanged, this, [this] {
        update();
        emit rangesChanged();
    });
    connect(range, &QObject::destroyed, this, [this, range] {
        if (d_ptr->ranges.removeOne(range)) {
            update();
            emit rangesChanged();
        }
    });

    update();
    QPointer<ZzRadialGaugeRange> guard(range);
    emit rangesChanged();
    return guard.data();
}

void ZzRadialGauge::removeRange(ZzRadialGaugeRange *range)
{
    if (!range || !d_ptr->ranges.removeOne(range)) {
        return;
    }

    disconnect(range, nullptr, this, nullptr);
    range->deleteLater();
    update();
    emit rangesChanged();
}

void ZzRadialGauge::clearRanges()
{
    if (d_ptr->ranges.isEmpty()) {
        return;
    }

    const auto oldRanges = d_ptr->ranges;
    d_ptr->ranges.clear();
    for (ZzRadialGaugeRange *range : oldRanges) {
        disconnect(range, nullptr, this, nullptr);
        range->deleteLater();
    }

    update();
    emit rangesChanged();
}

void ZzRadialGauge::mousePressEvent(QMouseEvent *event)
{
    if (!d_ptr->interactive || !isEnabled() || event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    d_ptr->valueAnimation->stop();
    setFocus(Qt::MouseFocusReason);
    QPointer<ZzRadialGauge> guard(this);
    setSliderDown(true);
    if (!guard)
        return;
    updatePositionFromPoint(event->position());
    event->accept();
}

void ZzRadialGauge::mouseMoveEvent(QMouseEvent *event)
{
    if (!d_ptr->interactive || !isEnabled() || !isSliderDown()) {
        event->ignore();
        return;
    }

    updatePositionFromPoint(event->position());
    event->accept();
}

void ZzRadialGauge::mouseReleaseEvent(QMouseEvent *event)
{
    QPointer<ZzRadialGauge> guard(this);
    if (!d_ptr->interactive || !isEnabled() || event->button() != Qt::LeftButton || !isSliderDown()) {
        event->ignore();
        return;
    }

    updatePositionFromPoint(event->position());
    if (!guard)
        return;
    if (!hasTracking()) {
        setValue(sliderPosition());
        if (!guard)
            return;
    }
    setSliderDown(false);
    event->accept();
}

void ZzRadialGauge::wheelEvent(QWheelEvent *event)
{
    if (!d_ptr->interactive) {
        event->ignore();
        return;
    }

    d_ptr->valueAnimation->stop();
    QDial::wheelEvent(event);
}

void ZzRadialGauge::keyPressEvent(QKeyEvent *event)
{
    if (!d_ptr->interactive) {
        event->ignore();
        return;
    }

    d_ptr->valueAnimation->stop();
    QDial::keyPressEvent(event);
}

void ZzRadialGauge::setValue(int targetValue)
{
    targetValue = qBound(minimum(), targetValue, maximum());
    if (!d_ptr->valueAnimation || d_ptr->valueAnimationDuration == 0 || !zzGaugeMotionAllowed(this) ||
        isSliderDown()) {
        if (d_ptr->valueAnimation) {
            d_ptr->valueAnimation->stop();
        }
        QDial::setValue(targetValue);
        return;
    }

    if (value() == targetValue) {
        d_ptr->valueAnimation->stop();
        return;
    }

    const qreal startValue = value();
    auto *animation = d_ptr->valueAnimation;
    {
        // 关键帧配置在 Stopped 状态也会重算值；完整配置前不得发布业务数值。
        const QSignalBlocker blocker(animation);
        animation->stop();
        animation->setDuration(d_ptr->valueAnimationDuration);
        animation->setStartValue(startValue);
        animation->setEndValue(static_cast<qreal>(targetValue));
        animation->setCurrentTime(0);
    }
    animation->start();
}

bool ZzRadialGauge::isValueAnimating() const
{
    return d_ptr->valueAnimation && d_ptr->valueAnimation->state() == QAbstractAnimation::Running;
}

void ZzRadialGauge::settleValueAnimation()
{
    if (!isValueAnimating())
        return;
    const int target = d_ptr->valueAnimation->endValue().toInt();
    d_ptr->valueAnimation->stop();
    QDial::setValue(target);
}

void ZzRadialGauge::sliderChange(SliderChange change)
{
    if (d_ptr && d_ptr->valueAnimation &&
        (change == SliderRangeChange ||
         (change == SliderValueChange && (!d_ptr->animationWrite || value() != d_ptr->animationValue)))) {
        d_ptr->valueAnimation->stop();
    }
    QDial::sliderChange(change);
}

bool ZzRadialGauge::event(QEvent *event)
{
    QPointer<ZzRadialGauge> guard(this);
    const bool result = QDial::event(event);
    if (guard && d_ptr && d_ptr->valueAnimation &&
        (event->type() == QEvent::Hide || event->type() == QEvent::EnabledChange ||
         event->type() == QEvent::StyleChange) &&
        !zzGaugeMotionAllowed(this)) {
        settleValueAnimation();
    }
    if (guard && d_ptr && event->type() == QEvent::ChildRemoved) {
        const auto *removed = static_cast<QChildEvent *>(event);
        for (auto *range : std::as_const(d_ptr->ranges)) {
            if (range == removed->child()) {
                d_ptr->ranges.removeOne(range);
                disconnect(range, nullptr, this, nullptr);
                update();
                // setParent 返回前同步删除 Range 会破坏 QObject 内部父子关系更新。
                QMetaObject::invokeMethod(this, [this] { emit rangesChanged(); }, Qt::QueuedConnection);
                break;
            }
        }
    }
    return result;
}

qreal ZzRadialGauge::sweepAngle() const
{
    qreal sweep = std::fmod(d_ptr->maximumAngle - d_ptr->minimumAngle, FullCircle);
    if (sweep <= 0.0) {
        sweep += FullCircle;
    }
    return sweep;
}

qreal ZzRadialGauge::valueFraction(qreal value) const
{
    const qreal range = static_cast<qreal>(maximum()) - static_cast<qreal>(minimum());
    if (range <= 0.0) {
        return 0.0;
    }

    qreal fraction = (value - static_cast<qreal>(minimum())) / range;
    fraction = qBound(0.0, fraction, 1.0);
    return invertedAppearance() ? 1.0 - fraction : fraction;
}

qreal ZzRadialGauge::positionFraction() const
{
    return valueFraction(sliderPosition());
}

int ZzRadialGauge::positionFromPoint(const QPointF &point) const
{
    const QPointF center = QRectF(rect()).center();
    const qreal deltaX = point.x() - center.x();
    const qreal deltaY = point.y() - center.y();
    if (qFuzzyIsNull(deltaX) && qFuzzyIsNull(deltaY)) {
        return sliderPosition();
    }

    qreal angle = qRadiansToDegrees(std::atan2(deltaX, -deltaY));
    while (angle < d_ptr->minimumAngle) {
        angle += FullCircle;
    }
    while (angle >= d_ptr->minimumAngle + FullCircle) {
        angle -= FullCircle;
    }

    const qreal sweep = sweepAngle();
    const qreal endAngle = d_ptr->minimumAngle + sweep;
    if (qAbs(sweep - FullCircle) < 0.0001 && qAbs(angle - d_ptr->minimumAngle) < 0.0001 &&
        positionFraction() > 0.5) {
        angle = endAngle;
    }

    if (angle > endAngle) {
        const qreal minimumDistance = circularDistance(angle, d_ptr->minimumAngle);
        const qreal maximumDistance = circularDistance(angle, endAngle);
        if (qAbs(minimumDistance - maximumDistance) < 0.0001) {
            angle = positionFraction() < 0.5 ? d_ptr->minimumAngle : endAngle;
        } else {
            angle = minimumDistance < maximumDistance ? d_ptr->minimumAngle : endAngle;
        }
    }

    qreal fraction = (angle - d_ptr->minimumAngle) / sweep;
    if (invertedAppearance()) {
        fraction = 1.0 - fraction;
    }

    const qint64 range = static_cast<qint64>(maximum()) - static_cast<qint64>(minimum());
    const qint64 position = static_cast<qint64>(minimum()) + qRound64(fraction * static_cast<qreal>(range));
    return static_cast<int>(qBound(static_cast<qint64>(minimum()), position, static_cast<qint64>(maximum())));
}

void ZzRadialGauge::updatePositionFromPoint(const QPointF &point)
{
    setSliderPosition(positionFromPoint(point));
}

ZzRadialGaugeRange::~ZzRadialGaugeRange() = default;

 // namespace

void ZzRadialGauge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QStyleOptionSlider option;
    initStyleOption(&option);

    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const QPalette::ColorGroup colorGroup =
        enabled ? (option.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive)
                : QPalette::Disabled;
    const QColor paletteAccentColor = option.palette.color(colorGroup, accentRole());
    const QColor trackColor = option.palette.color(colorGroup, QPalette::Mid);
    const auto resolveColor = [enabled, &option, colorGroup](const QColor &color,
                                                             QPalette::ColorRole fallbackRole) {
        return enabled && color.isValid() ? color : option.palette.color(colorGroup, fallbackRole);
    };

    const QColor needlePaintColor = resolveColor(d_ptr->needleColor, accentRole());
    QColor tickPaintColor = resolveColor(d_ptr->tickColor, QPalette::Text);
    QColor labelPaintColor = resolveColor(d_ptr->labelColor, QPalette::Text);
    const QColor valuePaintColor = resolveColor(d_ptr->valueColor, QPalette::Text);
    if (!d_ptr->tickColor.isValid() && !zzGaugeHighContrast(this)) {
        tickPaintColor.setAlpha(enabled ? 150 : 80);
    }
    if (!d_ptr->labelColor.isValid() && !zzGaugeHighContrast(this)) {
        labelPaintColor.setAlpha(enabled ? 180 : 90);
    }

    const qreal side = qMin(width(), height());
    const QPointF center = QRectF(rect()).center();
    const qreal radius = qMax(1.0, side * 0.5 - d_ptr->scalePadding - d_ptr->scaleWidth * 0.5 - 1.0);
    const QRectF scaleRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
    const qreal sweep = sweepAngle();
    const qreal endpointExtension = sweep < FullCircle - 0.001 ? EndpointExtensionAngle : 0.0;
    const qreal fraction = positionFraction();
    const qreal trackStartAngle = 90.0 - d_ptr->minimumAngle + endpointExtension;
    const qreal trackSpan = -(sweep + endpointExtension * 2.0);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    const QColor progressStartColor =
        enabled ? (d_ptr->progressGradientStartColor.isValid() ? d_ptr->progressGradientStartColor
                                                               : paletteAccentColor.lighter(135))
                : paletteAccentColor;
    const QColor progressEndColor = enabled && d_ptr->progressGradientEndColor.isValid()
                                        ? d_ptr->progressGradientEndColor
                                        : paletteAccentColor;
    const auto makeAngularGradient = [&](qreal sectionStartAngle, qreal sectionSpan, const QColor &startColor,
                                         const QColor &endColor) {
        qreal gradientAngle = std::fmod(sectionStartAngle + sectionSpan, FullCircle);
        if (gradientAngle < 0.0) {
            gradientAngle += FullCircle;
        }

        const qreal activeStop = qBound(0.0, -sectionSpan / FullCircle, 1.0);
        QConicalGradient gradient(center, gradientAngle);
        gradient.setColorAt(0.0, endColor);
        gradient.setColorAt(activeStop, startColor);
        if (activeStop < 1.0) {
            gradient.setColorAt(1.0, endColor);
        }
        return QBrush(gradient);
    };

    if (d_ptr->scaleMode == ProgressScale && d_ptr->sweepAreaVisible && fraction > 0.0 &&
        d_ptr->sweepAreaOpacity > 0.0) {
        QColor areaStartColor = progressStartColor;
        QColor areaEndColor = progressEndColor;
        areaStartColor.setAlphaF(static_cast<float>(areaStartColor.alphaF() * d_ptr->sweepAreaOpacity));
        areaEndColor.setAlphaF(static_cast<float>(areaEndColor.alphaF() * d_ptr->sweepAreaOpacity));

        const qreal areaStartAngle = 90.0 - d_ptr->minimumAngle;
        const qreal areaSpan = -sweep * fraction;
        QPainterPath sweepArea;
        sweepArea.moveTo(center);
        sweepArea.lineTo(pointAtAngle(center, radius, d_ptr->minimumAngle));
        sweepArea.arcTo(scaleRect, areaStartAngle, areaSpan);
        sweepArea.closeSubpath();
        painter.fillPath(sweepArea,
                         makeAngularGradient(areaStartAngle, areaSpan, areaStartColor, areaEndColor));
    }

    QPen trackPen(trackColor, d_ptr->scaleWidth, Qt::SolidLine, d_ptr->trackCapStyle);
    painter.setPen(trackPen);
    painter.drawArc(scaleRect, qRound(trackStartAngle * 16.0), qRound(trackSpan * 16.0));

    const auto drawScaleSection = [&](qreal firstFraction, qreal secondFraction, const QBrush &brush,
                                      Qt::PenCapStyle capStyle) {
        firstFraction = qBound(0.0, firstFraction, 1.0);
        secondFraction = qBound(0.0, secondFraction, 1.0);
        if (firstFraction > secondFraction) {
            qSwap(firstFraction, secondFraction);
        }
        if (qFuzzyCompare(firstFraction, secondFraction)) {
            return;
        }

        qreal sectionStartAngle = 90.0 - (d_ptr->minimumAngle + sweep * firstFraction);
        qreal sectionSpan = -sweep * (secondFraction - firstFraction);
        if (endpointExtension > 0.0 && qFuzzyIsNull(firstFraction)) {
            sectionStartAngle += endpointExtension;
            sectionSpan -= endpointExtension;
        }
        if (endpointExtension > 0.0 && qFuzzyCompare(secondFraction, 1.0)) {
            sectionSpan -= endpointExtension;
        }
        painter.setPen(QPen(brush, d_ptr->scaleWidth, Qt::SolidLine, capStyle));
        painter.drawArc(scaleRect, qRound(sectionStartAngle * 16.0), qRound(sectionSpan * 16.0));
    };

    if (d_ptr->scaleMode == ProgressScale && fraction > 0.0) {
        QBrush progressBrush(paletteAccentColor);
        if (enabled && d_ptr->progressGradientEnabled) {
            const qreal sectionStartAngle = 90.0 - d_ptr->minimumAngle + endpointExtension;
            qreal sectionSpan = -(sweep * fraction + endpointExtension);
            if (qFuzzyCompare(fraction, 1.0)) {
                sectionSpan -= endpointExtension;
            }

            progressBrush =
                makeAngularGradient(sectionStartAngle, sectionSpan, progressStartColor, progressEndColor);
        }
        drawScaleSection(0.0, fraction, progressBrush, d_ptr->ringCapStyle);
    } else if (d_ptr->scaleMode == RangeScale) {
        for (const ZzRadialGaugeRange *rangeItem : d_ptr->ranges) {
            if (!rangeItem) {
                continue;
            }

            QColor rangeColor = rangeItem->color();
            if (!enabled || !rangeColor.isValid()) {
                rangeColor = paletteAccentColor;
            }
            drawScaleSection(valueFraction(rangeItem->fromValue()), valueFraction(rangeItem->toValue()),
                             QBrush(rangeColor), d_ptr->ringCapStyle);
        }
    }

    const qint64 range = static_cast<qint64>(maximum()) - static_cast<qint64>(minimum());
    const int majorTickCount =
        range > 0
            ? qMin(d_ptr->majorTickCount, static_cast<int>(qMin<qint64>(range + 1, MaximumMajorTickCount)))
            : 0;
    const int majorIntervalCount = qMax(1, majorTickCount - 1);
    const int maximumMinorTickCount = qMax(0, MaximumTickCount / majorIntervalCount - 1);
    const int minorTickCount = qMin(d_ptr->minorTickCount, maximumMinorTickCount);
    const auto visualFraction = [this](qreal logicalFraction) {
        return invertedAppearance() ? 1.0 - logicalFraction : logicalFraction;
    };
    const auto forEachMajorTick = [&](const auto &callback) {
        for (int index = 0; index < majorTickCount; ++index) {
            const qreal logicalFraction = static_cast<qreal>(index) / majorIntervalCount;
            callback(index, logicalFraction, visualFraction(logicalFraction));
        }
    };

    if (majorTickCount >= 2 && (d_ptr->tickLength > 0.0 || d_ptr->majorTickLength > 0.0)) {
        const qreal tickOuterRadius = qMax(0.0, radius - d_ptr->tickPadding);

        const auto drawTick = [&](qreal tickFraction, qreal tickLength, qreal tickWidth) {
            const qreal angle = d_ptr->minimumAngle + sweep * tickFraction;
            const qreal tickInnerRadius = qMax(0.0, tickOuterRadius - tickLength);
            painter.setPen(QPen(tickPaintColor, tickWidth, Qt::SolidLine, Qt::FlatCap));
            painter.drawLine(pointAtAngle(center, tickInnerRadius, angle),
                             pointAtAngle(center, tickOuterRadius, angle));
        };

        if (d_ptr->tickLength > 0.0 && minorTickCount > 0) {
            for (int interval = 0; interval < majorIntervalCount; ++interval) {
                for (int minorIndex = 1; minorIndex <= minorTickCount; ++minorIndex) {
                    const qreal positionInInterval = static_cast<qreal>(minorIndex) / (minorTickCount + 1);
                    const qreal logicalFraction = (interval + positionInInterval) / majorIntervalCount;
                    drawTick(visualFraction(logicalFraction), d_ptr->tickLength, d_ptr->tickWidth);
                }
            }
        }
        if (d_ptr->majorTickLength > 0.0) {
            forEachMajorTick([&](int, qreal, qreal tickFraction) {
                drawTick(tickFraction, d_ptr->majorTickLength, d_ptr->majorTickWidth);
            });
        }
    }

    if (d_ptr->labelsVisible && majorTickCount >= 2) {
        QFont labelFont = font();
        labelFont.setPixelSize(qMin(d_ptr->labelFontPixelSize, qMax(1, qRound(side * 0.12))));
        painter.setFont(labelFont);
        painter.setPen(labelPaintColor);
        const QFontMetricsF metrics(labelFont);
        const qreal labelRadius = qMax(0.0, radius - d_ptr->scaleWidth * 0.5 - d_ptr->labelPadding);

        const auto drawLabel = [&](qint64 labelValue, qreal labelFraction) {
            const qreal angle = d_ptr->minimumAngle + sweep * labelFraction;
            const QPointF labelCenter = pointAtAngle(center, labelRadius, angle);
            const QString text = QString::number(labelValue);
            const QRectF textBounds = metrics.boundingRect(text);
            const QRectF textRect(labelCenter.x() - textBounds.width() * 0.5 - 2.0,
                                  labelCenter.y() - metrics.height() * 0.5, textBounds.width() + 4.0,
                                  metrics.height());
            painter.drawText(textRect, Qt::AlignCenter, text);
        };

        forEachMajorTick([&](int index, qreal logicalFraction, qreal labelFraction) {
            qint64 labelValue =
                static_cast<qint64>(minimum()) + qRound64(static_cast<qreal>(range) * logicalFraction);
            if (index == majorTickCount - 1) {
                labelValue = maximum();
            }
            drawLabel(labelValue, labelFraction);
        });
    }

    const qreal needleAngle = d_ptr->minimumAngle + sweep * fraction;
    if (d_ptr->needleStyle == LineNeedle) {
        painter.setPen(QPen(needlePaintColor, d_ptr->needleWidth, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(center, pointAtAngle(center, radius * d_ptr->needleLength, needleAngle));
    } else if (d_ptr->needleStyle == TriangleNeedle) {
        const qreal radians = qDegreesToRadians(needleAngle);
        const QPointF direction(std::sin(radians), -std::cos(radians));
        const QPointF normal(-direction.y(), direction.x());
        const QPointF tip = center + direction * (radius * d_ptr->needleLength);
        const QPointF tail = center - direction * qMax(2.0, d_ptr->needleWidth);
        const qreal halfWidth = d_ptr->needleWidth * 0.5;

        QPolygonF needle;
        needle << tip << center + normal * halfWidth << tail << center - normal * halfWidth;
        painter.setPen(Qt::NoPen);
        painter.setBrush(needlePaintColor);
        painter.drawPolygon(needle);
    }

    if (d_ptr->needleStyle != NoNeedle && d_ptr->hubVisible && d_ptr->hubRadius > 0.0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(needlePaintColor);
        painter.drawEllipse(center, d_ptr->hubRadius, d_ptr->hubRadius);
        painter.setBrush(option.palette.color(colorGroup, QPalette::Base));
        painter.drawEllipse(center, d_ptr->hubRadius * 0.38, d_ptr->hubRadius * 0.38);
    }

    if (d_ptr->valueVisible || !d_ptr->title.isEmpty()) {
        QFont valueFont = font();
        valueFont.setWeight(QFont::DemiBold);
        const int automaticValueSize = qBound(14, qRound(side * 0.12), 38);
        valueFont.setPixelSize(d_ptr->valueFontPixelSize > 0 ? d_ptr->valueFontPixelSize
                                                             : automaticValueSize);
        painter.setFont(valueFont);
        painter.setPen(valuePaintColor);

        const QFontMetricsF metrics(valueFont);
        const qreal titleOffset = d_ptr->title.isEmpty() ? 0.0 : qBound(2.0, metrics.height() * 0.1, 5.0);
        const qreal valueCenterY =
            (d_ptr->valuePosition == CenterValue ? center.y() + radius * 0.38 : center.y() + radius * 0.72) +
            titleOffset;
        const QRectF valueRect(center.x() - radius * 0.62, valueCenterY - metrics.height() * 0.5,
                               radius * 1.24, metrics.height());
        if (d_ptr->valueVisible) {
            QString valueText = QString::number(sliderPosition());
            if (!d_ptr->unit.isEmpty()) {
                valueText += QStringLiteral(" ") + d_ptr->unit;
            }
            painter.drawText(valueRect, Qt::AlignCenter, valueText);
        }

        if (!d_ptr->title.isEmpty()) {
            QFont titleFont = font();
            titleFont.setPixelSize(qMax(8, qRound(valueFont.pixelSize() * 0.48)));
            titleFont.setWeight(QFont::DemiBold);
            painter.setFont(titleFont);
            const QFontMetricsF titleMetrics(titleFont);
            const QRectF titleRect(center.x() - radius * 0.62, valueRect.top() - titleMetrics.height() * 0.92,
                                   radius * 1.24, titleMetrics.height());
            painter.drawText(titleRect, Qt::AlignCenter, d_ptr->title);
        }
    }
}

 // namespace

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

ZzRadialGauge::ZzGaugeScaleMode ZzRadialGauge::scaleMode() const
{
    return d_ptr->scaleMode;
}

void ZzRadialGauge::setScaleMode(ZzGaugeScaleMode mode)
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

ZzRadialGauge::ZzNeedleStyle ZzRadialGauge::needleStyle() const
{
    return d_ptr->needleStyle;
}

void ZzRadialGauge::setNeedleStyle(ZzNeedleStyle style)
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

ZzRadialGauge::ZzGaugeValuePosition ZzRadialGauge::valuePosition() const
{
    return d_ptr->valuePosition;
}

void ZzRadialGauge::setValuePosition(ZzGaugeValuePosition position)
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
