#ifndef ZZFLUENTPAINTER_H
#define ZZFLUENTPAINTER_H

#include <QRectF>

#include "ZzDesignTokens.h"
#include "ZzFluentCoreGlobal.h"

class QPainter;
class QString;

class ZZ_CORE_EXPORT ZzFluentPainter
{
public:
    static void drawControlSurface(
        QPainter& painter,
        const QRectF& rect,
        const ZzColorTokens& colors,
        const ZzMetricTokens& metrics,
        ZzControlState state);

    static void drawAccentButton(
        QPainter& painter,
        const QRectF& rect,
        const ZzColorTokens& colors,
        const ZzMetricTokens& metrics,
        ZzControlState state);

    static void drawButtonText(
        QPainter& painter,
        const QRectF& rect,
        const QString& text,
        const QColor& color);

    static void drawComboSurface(
        QPainter& painter,
        const QRectF& rect,
        const ZzColorTokens& colors,
        const ZzMetricTokens& metrics,
        ZzControlState state,
        bool popupVisible);

    static void drawPopupItem(
        QPainter& painter,
        const QRectF& rect,
        const ZzColorTokens& colors,
        const ZzMetricTokens& metrics,
        ZzControlState state,
        bool selected);
};

#endif // ZZFLUENTPAINTER_H
