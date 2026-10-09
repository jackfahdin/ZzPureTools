#pragma once

#include <QElapsedTimer>
#include <QPainterPath>
#include <QTimer>
#include <ZzFluentUI/ZzBorderBeam.h>

class QPainter;

namespace ZzFluentUI {

/** @brief 容器与按钮共用的路径缓存、配色和动画时钟；不拥有宿主。 */
class ZzBorderBeamPrivate final
{
public:
    explicit ZzBorderBeamPrivate(QWidget *widget);
    void advance();
    void synchronize();
    void restart();
    void drawSurface(QPainter &painter, bool pressed = false, bool hovered = false) const;
    void drawBeam(QPainter &painter);
    [[nodiscard]] ZzBorderBeam::ZzBeamThemeConfig activeTheme() const;
    [[nodiscard]] ZzBorderBeam::ZzBeamThemeConfig resolvedTheme() const;

    QWidget *const widget;
    QTimer timer;
    QElapsedTimer elapsed;
    qreal beamLength = 60.0;
    qreal beamWidth = 2.0;
    qreal cornerRadius = 8.0;
    QColor backgroundColor, borderColor, startColor, endColor;
    int animationDuration = 6000;
    qreal initialProgress = 0.0;
    qreal progress = 0.0;
    ZzBorderBeam::ZzBeamDirection direction = ZzBorderBeam::Clockwise;
    int beamCount = 1;
    bool animationEnabled = true;
    ZzBorderBeam::ZzBeamThemeMode themeMode = ZzBorderBeam::AutoTheme;
    ZzBorderBeam::ZzBeamThemeConfig lightTheme = ZzBorderBeam::defaultLightTheme();
    ZzBorderBeam::ZzBeamThemeConfig darkTheme = ZzBorderBeam::defaultDarkTheme();
    bool pathDirty = true;
    QSize pathSize;
    QPainterPath path;
    qreal pathLength = 0.0;
};

} // namespace ZzFluentUI
