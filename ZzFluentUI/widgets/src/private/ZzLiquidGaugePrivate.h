#pragma once

#include <ZzFluentUI/ZzLiquidGauge.h>
#include <QElapsedTimer>
#include <QPainterPath>
#include <QTimer>

namespace ZzFluentUI {

/** @brief 波形配置、相位计时与绘制辅助；不发布外部通知。 */
class ZzLiquidGaugePrivate final
{
public:
    explicit ZzLiquidGaugePrivate(ZzLiquidGauge *widget);
    ZzLiquidGauge *const q;
    ZzLiquidGauge::ZzLiquidShape shape = ZzLiquidGauge::CircleShape;
    qreal waveAmplitude = 6.0;
    int waveCount = 3;
    int waveAnimationDuration = 2400;
    bool animationEnabled = true;
    qreal secondaryWaveOpacity = 0.45;
    qreal outlineWidth = 2.0;
    qreal outlineDistance = 3.0;
    QColor waveColor, backgroundColor, outlineColor, textColor, submergedTextColor;
    int contentFontPixelSize = 0;
    QTimer timer;
    QElapsedTimer elapsed;
    qreal phase = 0.0;

    void syncAnimation();
    [[nodiscard]] qreal valueFraction() const;
    [[nodiscard]] QPainterPath shapePath(const QRectF &bounds) const;
    [[nodiscard]] QPainterPath wavePath(const QRectF &bounds, qreal baseline,
                                       qreal amplitude, qreal angle) const;
    [[nodiscard]] QColor resolvedColor(const QColor &color, QPalette::ColorRole role) const;
    void paint();
};

} // namespace ZzFluentUI
