#include "ZzPushButtonDelegate.hpp"

#include <QPainter>

#include "ZzFluent/Core/ZzTheme.hpp"
#include "ZzPaintPrimitives.hpp"

ZzPushButtonDelegate::ZzPushButtonDelegate(ZzPushButton::ZzButtonStyle style)
    : m_buttonStyle(style)
{
}

void ZzPushButtonDelegate::setButtonStyle(ZzPushButton::ZzButtonStyle style)
{
    m_buttonStyle = style;
}

void ZzPushButtonDelegate::paint(QPainter& painter, const QRectF& rect, const ZzStyleContext& context) const
{
    if (context.theme == nullptr) {
        return;
    }

    if (m_buttonStyle == ZzPushButton::ZzButtonStyle::Accent) {
        paintAccent(painter, rect, context);
        return;
    }

    paintStandard(painter, rect, context);
}

void ZzPushButtonDelegate::paintStandard(QPainter& painter, const QRectF& rect, const ZzStyleContext& context) const
{
    const ZzColorTokens& colors = context.theme->colors();
    const ZzMetricTokens& metrics = context.theme->metrics();

    QColor fill = colors.controlFill;
    QColor stroke = colors.controlStroke;

    if (!context.enabled) {
        fill = colors.controlFillDisabled;
    } else {
        fill = ZzPaintPrimitives::blend(fill, colors.controlFillHover, context.hoverProgress);
        fill = ZzPaintPrimitives::blend(fill, colors.controlFillPressed, context.pressProgress);
        stroke = ZzPaintPrimitives::blend(stroke, colors.controlStrokeHover, context.hoverProgress);
    }

    ZzPaintPrimitives::drawRoundedSurface(painter, rect, colors, metrics, fill, stroke);
    ZzPaintPrimitives::drawCenteredText(
        painter,
        rect,
        context.text,
        context.enabled ? colors.textPrimary : colors.textDisabled);
}

void ZzPushButtonDelegate::paintAccent(QPainter& painter, const QRectF& rect, const ZzStyleContext& context) const
{
    const ZzColorTokens& colors = context.theme->colors();
    const ZzMetricTokens& metrics = context.theme->metrics();

    QColor fill = colors.accent;
    if (!context.enabled) {
        fill = colors.controlFillDisabled;
    } else {
        fill = ZzPaintPrimitives::blend(fill, colors.accentHover, context.hoverProgress);
        fill = ZzPaintPrimitives::blend(fill, colors.accentPressed, context.pressProgress);
    }

    const QColor stroke = context.enabled ? Qt::transparent : colors.controlStroke;
    ZzPaintPrimitives::drawRoundedSurface(painter, rect, colors, metrics, fill, stroke);

    const QColor textColor = context.enabled ? Qt::white : colors.textDisabled;
    ZzPaintPrimitives::drawCenteredText(painter, rect, context.text, textColor);
}
