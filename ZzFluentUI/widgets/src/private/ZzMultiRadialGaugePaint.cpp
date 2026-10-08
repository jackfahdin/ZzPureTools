#include <ZzFluentUI/ZzMultiRadialGauge.h>

#include "ZzMultiRadialGaugePrivate.h"
#include "ZzGaugeSupport_p.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPolygonF>
#include <QStyleOption>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>

namespace ZzFluentUI {

namespace {
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

QColor disabledItemColor(const QColor &color, const QPalette &palette)
{
    if (!color.isValid()) {
        return palette.color(QPalette::Disabled, accentRole());
    }

    const QColor disabledText = palette.color(QPalette::Disabled, QPalette::Text);
    return QColor((color.red() + disabledText.red()) / 2, (color.green() + disabledText.green()) / 2,
                  (color.blue() + disabledText.blue()) / 2, color.alpha());
}
} // namespace

void ZzMultiRadialGauge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QStyleOption option;
    option.initFrom(this);
    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const QPalette::ColorGroup colorGroup =
        enabled ? (option.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive)
                : QPalette::Disabled;
    const QColor accentColor = option.palette.color(colorGroup, accentRole());
    const QColor resolvedTrackColor = enabled && d_ptr->trackColor.isValid()
                                          ? d_ptr->trackColor
                                          : option.palette.color(colorGroup, QPalette::Mid);
    QColor resolvedTickColor = enabled && d_ptr->tickColor.isValid()
                                   ? d_ptr->tickColor
                                   : option.palette.color(colorGroup, QPalette::Text);
    QColor resolvedLabelColor = enabled && d_ptr->labelColor.isValid()
                                    ? d_ptr->labelColor
                                    : option.palette.color(colorGroup, QPalette::Text);
    const QColor resolvedTitleColor = enabled && d_ptr->titleColor.isValid()
                                          ? d_ptr->titleColor
                                          : option.palette.color(colorGroup, QPalette::Text);
    if (!d_ptr->tickColor.isValid() && !zzGaugeHighContrast(this)) {
        resolvedTickColor.setAlpha(enabled ? 150 : 80);
    }
    if (!d_ptr->labelColor.isValid() && !zzGaugeHighContrast(this)) {
        resolvedLabelColor.setAlpha(enabled ? 180 : 90);
    }

    const qreal side = qMin(width(), height());
    const QPointF center = QRectF(rect()).center();
    const qreal maximumLineWidth = qMax(d_ptr->trackWidth, d_ptr->progressWidth);
    const qreal radius = qMax(1.0, side * 0.5 - d_ptr->scalePadding - maximumLineWidth * 0.5 - 1.0);
    const QRectF scaleRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
    const qreal sweep = sweepAngle();
    const qreal painterStartAngle = 90.0 - d_ptr->minimumAngle;

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    if (d_ptr->trackVisible) {
        painter.setPen(QPen(resolvedTrackColor, d_ptr->trackWidth, Qt::SolidLine, d_ptr->trackCapStyle));
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(scaleRect, qRound(painterStartAngle * 16.0), qRound(-sweep * 16.0));
    }

    const auto itemColor = [&](const ZzMultiRadialGaugeItem *item) {
        if (!item) {
            return accentColor;
        }
        if (!enabled) {
            return disabledItemColor(item->color(), option.palette);
        }
        return item->color().isValid() ? item->color() : accentColor;
    };

    if (d_ptr->progressVisible) {
        const auto drawProgress = [&](const ZzMultiRadialGaugeItem *item, qreal itemRadius) {
            const qreal fraction = valueFraction(displayedValue(item));
            if (fraction <= 0.0 || itemRadius <= d_ptr->progressWidth * 0.5) {
                return;
            }
            const QRectF itemRect(center.x() - itemRadius, center.y() - itemRadius, itemRadius * 2.0,
                                  itemRadius * 2.0);
            painter.setPen(
                QPen(itemColor(item), d_ptr->progressWidth, Qt::SolidLine, d_ptr->progressCapStyle));
            painter.drawArc(itemRect, qRound(painterStartAngle * 16.0), qRound(-sweep * fraction * 16.0));
        };

        if (d_ptr->progressOverlap) {
            QList<const ZzMultiRadialGaugeItem *> sortedItems;
            sortedItems.reserve(d_ptr->items.size());
            for (const ZzMultiRadialGaugeItem *item : std::as_const(d_ptr->items)) {
                if (item && item->isVisible()) {
                    sortedItems.append(item);
                }
            }
            std::stable_sort(sortedItems.begin(), sortedItems.end(),
                             [this](const ZzMultiRadialGaugeItem *left, const ZzMultiRadialGaugeItem *right) {
                                 return valueFraction(displayedValue(left)) >
                                        valueFraction(displayedValue(right));
                             });
            for (const ZzMultiRadialGaugeItem *item : std::as_const(sortedItems)) {
                drawProgress(item, radius);
            }
        } else {
            for (qsizetype index = 0; index < d_ptr->items.size(); ++index) {
                const ZzMultiRadialGaugeItem *item = d_ptr->items.at(index);
                const qreal itemRadius =
                    radius - static_cast<qreal>(index) * (d_ptr->progressWidth + d_ptr->progressSpacing);
                if (!item || !item->isVisible() || itemRadius <= d_ptr->progressWidth * 0.5) {
                    continue;
                }
                drawProgress(item, itemRadius);
            }
        }
    }

    const int majorTickCount = qBound(2, d_ptr->majorTickCount, MaximumMajorTickCount);
    const int majorIntervalCount = majorTickCount - 1;
    const int maximumMinorTickCount = qMax(0, MaximumTickCount / majorIntervalCount - 1);
    const int minorTickCount = qMin(d_ptr->minorTickCount, maximumMinorTickCount);
    const qreal tickOuterRadius = qMax(0.0, radius - maximumLineWidth * 0.5 - d_ptr->tickPadding);

    const auto drawTick = [&](qreal fraction, qreal length, qreal width) {
        const qreal angle = d_ptr->minimumAngle + sweep * fraction;
        const qreal innerRadius = qMax(0.0, tickOuterRadius - length);
        painter.setPen(QPen(resolvedTickColor, width, Qt::SolidLine, Qt::FlatCap));
        painter.drawLine(pointAtAngle(center, innerRadius, angle),
                         pointAtAngle(center, tickOuterRadius, angle));
    };

    if (d_ptr->tickLength > 0.0 && minorTickCount > 0) {
        for (int interval = 0; interval < majorIntervalCount; ++interval) {
            for (int minorIndex = 1; minorIndex <= minorTickCount; ++minorIndex) {
                const qreal position = static_cast<qreal>(minorIndex) / (minorTickCount + 1);
                drawTick((interval + position) / majorIntervalCount, d_ptr->tickLength, d_ptr->tickWidth);
            }
        }
    }
    if (d_ptr->majorTickLength > 0.0) {
        for (int index = 0; index < majorTickCount; ++index) {
            drawTick(static_cast<qreal>(index) / majorIntervalCount, d_ptr->majorTickLength,
                     d_ptr->majorTickWidth);
        }
    }

    if (d_ptr->labelsVisible) {
        QFont labelFont = font();
        labelFont.setPixelSize(qMin(d_ptr->labelFontPixelSize, qMax(1, qRound(side * 0.12))));
        painter.setFont(labelFont);
        painter.setPen(resolvedLabelColor);
        const QFontMetricsF metrics(labelFont);
        const qreal labelRadius = qMax(0.0, tickOuterRadius - d_ptr->majorTickLength - d_ptr->labelPadding);
        for (int index = 0; index < majorTickCount; ++index) {
            const qreal fraction = static_cast<qreal>(index) / majorIntervalCount;
            const qreal value = std::lerp(d_ptr->minimum, d_ptr->maximum, fraction);
            const QPointF labelCenter =
                pointAtAngle(center, labelRadius, d_ptr->minimumAngle + sweep * fraction);
            const QString text = QString::number(value, 'g', 4);
            const QRectF bounds = metrics.boundingRect(text);
            const QRectF textRect(labelCenter.x() - bounds.width() * 0.5 - 2.0,
                                  labelCenter.y() - metrics.height() * 0.5, bounds.width() + 4.0,
                                  metrics.height());
            painter.drawText(textRect, Qt::AlignCenter, text);
        }
    }

    const QPointF needleCenter =
        center + QPointF(d_ptr->needleOffset.x() * radius, d_ptr->needleOffset.y() * radius);
    if (d_ptr->needleStyle != NoNeedle) {
        for (const ZzMultiRadialGaugeItem *item : std::as_const(d_ptr->items)) {
            if (!item || !item->isVisible()) {
                continue;
            }
            const qreal angle = d_ptr->minimumAngle + sweep * valueFraction(displayedValue(item));
            const qreal radians = qDegreesToRadians(angle);
            const QPointF direction(std::sin(radians), -std::cos(radians));
            const QPointF normal(-direction.y(), direction.x());
            const QPointF tip = needleCenter + direction * (radius * d_ptr->needleLength);
            const QColor color = itemColor(item);
            if (d_ptr->needleStyle == LineNeedle) {
                const QPointF tail = needleCenter - direction * qMax(2.0, d_ptr->needleWidth * 1.8);
                painter.setPen(QPen(color, d_ptr->needleWidth, Qt::SolidLine, Qt::RoundCap));
                painter.setBrush(Qt::NoBrush);
                painter.drawLine(tail, tip);
            } else {
                const QPointF tail = needleCenter - direction * qMax(2.0, d_ptr->needleWidth);
                const qreal halfWidth = d_ptr->needleWidth * 0.5;
                QPolygonF needle;
                needle << tip << needleCenter + normal * halfWidth << tail
                       << needleCenter - normal * halfWidth;
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawPolygon(needle);
            }
        }
    }

    if (d_ptr->hubVisible && d_ptr->hubRadius > 0.0) {
        const QColor color = enabled && d_ptr->hubColor.isValid() ? d_ptr->hubColor : accentColor;
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(needleCenter, d_ptr->hubRadius, d_ptr->hubRadius);
    }

    QFont titleFont = font();
    titleFont.setPixelSize(qMin(d_ptr->titleFontPixelSize, qMax(1, qRound(side * 0.12))));
    QFont detailFont = font();
    detailFont.setPixelSize(qMin(d_ptr->detailFontPixelSize, qMax(1, qRound(side * 0.12))));
    detailFont.setWeight(QFont::DemiBold);
    const QFontMetricsF titleMetrics(titleFont);
    const QFontMetricsF detailMetrics(detailFont);

    for (const ZzMultiRadialGaugeItem *item : std::as_const(d_ptr->items)) {
        if (!item || !item->isVisible()) {
            continue;
        }
        const QColor color = itemColor(item);
        if (d_ptr->titleVisible && !item->label().isEmpty()) {
            const QPointF titleCenter =
                center + QPointF(item->titleOffset().x() * radius, item->titleOffset().y() * radius);
            const qreal width = qMax(40.0, titleMetrics.horizontalAdvance(item->label()) + 8.0);
            const QRectF titleRect(titleCenter.x() - width * 0.5,
                                   titleCenter.y() - titleMetrics.height() * 0.5, width,
                                   titleMetrics.height());
            painter.setFont(titleFont);
            painter.setPen(resolvedTitleColor);
            painter.drawText(titleRect, Qt::AlignCenter, item->label());
        }

        if (d_ptr->detailVisible) {
            const QString valueText =
                QString::number(displayedValue(item), 'f', d_ptr->valueDecimals) + d_ptr->valueSuffix;
            const QPointF detailCenter =
                center + QPointF(item->detailOffset().x() * radius, item->detailOffset().y() * radius);
            const qreal textWidth = detailMetrics.horizontalAdvance(valueText);
            const qreal detailWidth = textWidth + d_ptr->detailBadgePadding * 2.0;
            const qreal detailHeight = detailMetrics.height() + 2.0;
            const QRectF detailRect(detailCenter.x() - detailWidth * 0.5,
                                    detailCenter.y() - detailHeight * 0.5, detailWidth, detailHeight);
            painter.setFont(detailFont);
            if (d_ptr->detailBadgeVisible) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawRoundedRect(detailRect, 3.0, 3.0);
                painter.setPen(d_ptr->detailTextColor.isValid() ? d_ptr->detailTextColor
                                                                : zzGaugeContrastingText(color));
            } else {
                painter.setPen(d_ptr->detailTextColor.isValid() ? d_ptr->detailTextColor : color);
            }
            painter.drawText(detailRect, Qt::AlignCenter, valueText);
        }
    }
}

} // namespace ZzFluentUI
