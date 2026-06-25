#include "ZzPaintPrimitives.hpp"

#include <QPainter>
#include <QPainterPath>

QColor ZzPaintPrimitives::blend(const QColor& from, const QColor& to, qreal progress)
{
    const qreal t = qBound(0.0, progress, 1.0);
    return QColor(from.red() + static_cast<int>((to.red() - from.red()) * t),
                  from.green() + static_cast<int>((to.green() - from.green()) * t),
                  from.blue() + static_cast<int>((to.blue() - from.blue()) * t),
                  from.alpha() + static_cast<int>((to.alpha() - from.alpha()) * t));
}

void ZzPaintPrimitives::drawRoundedSurface(QPainter& painter, const QRectF& rect,
                                           const ZzColorTokens& colors,
                                           const ZzMetricTokens& metrics, const QColor& fill,
                                           const QColor& stroke)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(rect, metrics.controlRadius, metrics.controlRadius);
    painter.fillPath(path, fill);
    painter.setPen(QPen(stroke, metrics.borderWidth));
    painter.drawPath(path);
    painter.restore();
}

void ZzPaintPrimitives::drawCenteredText(QPainter& painter, const QRectF& rect, const QString& text,
                                         const QColor& color)
{
    painter.save();
    painter.setPen(color);
    painter.drawText(rect, Qt::AlignCenter, text);
    painter.restore();
}

void ZzPaintPrimitives::drawComboArrow(QPainter& painter, const QRectF& rect,
                                       const ZzColorTokens& colors, ZzControlState state)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF arrowRect(rect.right() - 28.0, rect.top(), 24.0, rect.height());
    QPainterPath arrow;
    const qreal centerX = arrowRect.center().x();
    const qreal centerY = arrowRect.center().y();
    arrow.moveTo(centerX - 4.0, centerY - 2.0);
    arrow.lineTo(centerX + 4.0, centerY - 2.0);
    arrow.lineTo(centerX, centerY + 4.0);
    arrow.closeSubpath();

    painter.setPen(Qt::NoPen);
    painter.setBrush(state == ZzControlState::Disabled ? colors.textDisabled
                                                       : colors.textSecondary);
    painter.drawPath(arrow);
    painter.restore();
}
