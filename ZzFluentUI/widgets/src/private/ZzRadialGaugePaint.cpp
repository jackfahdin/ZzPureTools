#include <ZzFluentUI/ZzRadialGauge.h>

#include "ZzRadialGaugePrivate.h"
#include "ZzGaugeSupport_p.h"

#include <QConicalGradient>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStyleOptionSlider>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>

namespace ZzFluentUI {

namespace {
constexpr qreal FullCircle = 360.0;
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

} // namespace ZzFluentUI
