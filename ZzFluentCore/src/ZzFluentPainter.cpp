#include "ZzFluentPainter.h"

#include <QPainter>
#include <QPainterPath>

void ZzFluentPainter::drawControlSurface(
    QPainter& painter,
    const QRectF& rect,
    const ZzColorTokens& colors,
    const ZzMetricTokens& metrics,
    ZzControlState state)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(rect, metrics.controlRadius, metrics.controlRadius);

    QColor fill = colors.controlFill;
    QColor stroke = colors.controlStroke;
    switch (state) {
    case ZzControlState::Disabled:
        fill = colors.controlFillDisabled;
        break;
    case ZzControlState::Pressed:
        fill = colors.controlFillPressed;
        stroke = colors.controlStrokeHover;
        break;
    case ZzControlState::Hovered:
        fill = colors.controlFillHover;
        stroke = colors.controlStrokeHover;
        break;
    case ZzControlState::Normal:
    default:
        break;
    }

    painter.fillPath(path, fill);
    painter.setPen(QPen(stroke, metrics.borderWidth));
    painter.drawPath(path);
    painter.restore();
}

void ZzFluentPainter::drawAccentButton(
    QPainter& painter,
    const QRectF& rect,
    const ZzColorTokens& colors,
    const ZzMetricTokens& metrics,
    ZzControlState state)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(rect, metrics.controlRadius, metrics.controlRadius);

    QColor fill = colors.accent;
    switch (state) {
    case ZzControlState::Disabled:
        fill = colors.controlFillDisabled;
        break;
    case ZzControlState::Pressed:
        fill = colors.accentPressed;
        break;
    case ZzControlState::Hovered:
        fill = colors.accentHover;
        break;
    case ZzControlState::Normal:
    default:
        break;
    }

    painter.fillPath(path, fill);
    if (state != ZzControlState::Disabled) {
        painter.setPen(Qt::NoPen);
    } else {
        painter.setPen(QPen(colors.controlStroke, metrics.borderWidth));
        painter.drawPath(path);
    }
    painter.restore();
}

void ZzFluentPainter::drawButtonText(
    QPainter& painter,
    const QRectF& rect,
    const QString& text,
    const QColor& color)
{
    painter.save();
    painter.setPen(color);
    painter.drawText(rect, Qt::AlignCenter, text);
    painter.restore();
}

void ZzFluentPainter::drawComboSurface(
    QPainter& painter,
    const QRectF& rect,
    const ZzColorTokens& colors,
    const ZzMetricTokens& metrics,
    ZzControlState state,
    bool popupVisible)
{
    drawControlSurface(painter, rect, colors, metrics, popupVisible ? ZzControlState::Pressed : state);

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
    painter.setBrush(state == ZzControlState::Disabled ? colors.textDisabled : colors.textSecondary);
    painter.drawPath(arrow);
    painter.restore();
}

void ZzFluentPainter::drawPopupItem(
    QPainter& painter,
    const QRectF& rect,
    const ZzColorTokens& colors,
    const ZzMetricTokens& metrics,
    ZzControlState state,
    bool selected)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor fill = Qt::transparent;
    if (selected) {
        fill = colors.popupItemSelected;
    } else if (state == ZzControlState::Hovered) {
        fill = colors.popupItemHover;
    }

    if (fill.alpha() > 0) {
        QPainterPath path;
        path.addRoundedRect(rect.adjusted(4, 1, -4, -1), metrics.controlRadius - 1, metrics.controlRadius - 1);
        painter.fillPath(path, fill);
    }

    painter.restore();
}
