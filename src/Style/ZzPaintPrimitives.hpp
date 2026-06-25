#ifndef ZZPAINTPRIMITIVES_HPP
#define ZZPAINTPRIMITIVES_HPP

#include <QColor>
#include <QRectF>

#include "ZzFluent/Core/ZzToken.hpp"

class QPainter;
class QPainterPath;

class ZzPaintPrimitives
{
public:
    [[nodiscard]] static QColor blend(const QColor& from, const QColor& to, qreal progress);

    static void drawRoundedSurface(
        QPainter& painter,
        const QRectF& rect,
        const ZzColorTokens& colors,
        const ZzMetricTokens& metrics,
        const QColor& fill,
        const QColor& stroke);

    static void drawCenteredText(QPainter& painter, const QRectF& rect, const QString& text, const QColor& color);

    static void drawComboArrow(
        QPainter& painter,
        const QRectF& rect,
        const ZzColorTokens& colors,
        ZzControlState state);
};

#endif // ZZPAINTPRIMITIVES_HPP
