#include <ZzFluentUI/ZzMultiProgressRing.h>

#include "ZzMultiProgressRingPrivate.h"
#include "ZzGaugeSupport_p.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QStyleOption>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>

namespace ZzFluentUI {

namespace {

QPalette::ColorRole accentRole()
{
    return QPalette::Accent;
}
} // namespace

void ZzMultiProgressRing::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QStyleOption option;
    option.initFrom(this);
    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const QPalette::ColorGroup colorGroup =
        enabled ? (option.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive)
                : QPalette::Disabled;
    const QColor fallbackRingColor = option.palette.color(colorGroup, accentRole());
    const QColor resolvedTrackColor = enabled && d_ptr->trackColor.isValid()
                                          ? d_ptr->trackColor
                                          : option.palette.color(colorGroup, QPalette::Mid);
    const QColor resolvedLabelColor = enabled && d_ptr->labelColor.isValid()
                                          ? d_ptr->labelColor
                                          : option.palette.color(colorGroup, QPalette::Text);

    const qreal side = qMin(width(), height());
    const QPointF center = QRectF(rect()).center();
    const qreal outerRadius = side * 0.5 - d_ptr->ringPadding - d_ptr->ringWidth * 0.5 - 1.0;
    const qreal painterStartAngle = 90.0 - d_ptr->startAngle;

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    qreal innermostRadius = outerRadius;
    for (qsizetype index = 0; index < d_ptr->items.size(); ++index) {
        const ZzMultiProgressRingItem *item = d_ptr->items.at(index);
        const qreal radius =
            outerRadius - static_cast<qreal>(index) * (d_ptr->ringWidth + d_ptr->ringSpacing);
        if (!item || radius <= d_ptr->ringWidth * 0.5) {
            break;
        }
        innermostRadius = radius;

        const QRectF ringRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
        if (d_ptr->trackVisible && d_ptr->sweepAngle > 0.0) {
            painter.setPen(QPen(resolvedTrackColor, d_ptr->ringWidth, Qt::SolidLine, d_ptr->capStyle));
            painter.drawArc(ringRect, qRound(painterStartAngle * 16.0), qRound(-d_ptr->sweepAngle * 16.0));
        }

        const qreal fraction = itemFraction(displayedValue(item));
        if (fraction <= 0.0 || d_ptr->sweepAngle <= 0.0) {
            continue;
        }

        const QColor ringColor = enabled && item->color().isValid() ? item->color() : fallbackRingColor;
        painter.setPen(QPen(ringColor, d_ptr->ringWidth, Qt::SolidLine, d_ptr->capStyle));
        painter.drawArc(ringRect, qRound(painterStartAngle * 16.0),
                        qRound(-d_ptr->sweepAngle * fraction * 16.0));
    }

    if (!d_ptr->detailsVisible || d_ptr->items.isEmpty()) {
        return;
    }

    QFont labelFont = font();
    labelFont.setPixelSize(d_ptr->labelFontPixelSize > 0 ? d_ptr->labelFontPixelSize
                                                         : qBound(9, qRound(side * 0.045), 13));
    QFont valueFont = font();
    valueFont.setPixelSize(d_ptr->valueFontPixelSize > 0 ? d_ptr->valueFontPixelSize
                                                         : qBound(9, qRound(side * 0.045), 13));
    valueFont.setWeight(QFont::DemiBold);

    const QFontMetricsF labelMetrics(labelFont);
    const QFontMetricsF valueMetrics(valueFont);
    const qreal badgeHeight = valueMetrics.height() + 2.0;
    const qreal rowSpacing = qMax(3.0, side * 0.012);
    const qreal rowHeight = labelMetrics.height() + 2.0 + badgeHeight;
    const qreal totalHeight = static_cast<qreal>(d_ptr->items.size()) * rowHeight +
                              static_cast<qreal>(qMax<qsizetype>(0, d_ptr->items.size() - 1)) * rowSpacing;
    qreal rowTop = center.y() - totalHeight * 0.5;
    const qreal contentHalfWidth = qMax(16.0, innermostRadius - d_ptr->ringWidth * 0.5 - 8.0);

    for (const ZzMultiProgressRingItem *item : std::as_const(d_ptr->items)) {
        if (!item) {
            continue;
        }

        const QColor itemColor = enabled && item->color().isValid() ? item->color() : fallbackRingColor;
        painter.setFont(labelFont);
        painter.setPen(resolvedLabelColor);
        painter.drawText(
            QRectF(center.x() - contentHalfWidth, rowTop, contentHalfWidth * 2.0, labelMetrics.height()),
            Qt::AlignCenter, item->label());

        const QString valueText =
            QString::number(displayedValue(item), 'f', d_ptr->valueDecimals) + d_ptr->valueSuffix;
        const qreal valueTop = rowTop + labelMetrics.height() + 2.0;
        painter.setFont(valueFont);
        if (d_ptr->valueBadgeVisible) {
            const qreal badgeWidth =
                qMin(contentHalfWidth * 2.0, valueMetrics.horizontalAdvance(valueText) + 16.0);
            const QRectF badgeRect(center.x() - badgeWidth * 0.5, valueTop, badgeWidth, badgeHeight);
            painter.setPen(QPen(itemColor, 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(badgeRect, badgeRect.height() * 0.5, badgeRect.height() * 0.5);
            painter.setPen(itemColor);
            painter.drawText(badgeRect, Qt::AlignCenter, valueText);
        } else {
            painter.setPen(itemColor);
            painter.drawText(
                QRectF(center.x() - contentHalfWidth, valueTop, contentHalfWidth * 2.0, badgeHeight),
                Qt::AlignCenter, valueText);
        }

        rowTop += rowHeight + rowSpacing;
    }
}

} // namespace ZzFluentUI
