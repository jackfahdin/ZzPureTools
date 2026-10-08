#include "ZzBorderBeamPrivate.h"

#include <QPainter>
#include <QStyle>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <algorithm>
#include <cmath>

namespace ZzFluentUI {
namespace {
qreal zzWrapped(qreal distance, qreal length)
{
    distance = std::fmod(distance, length);
    return distance < 0 ? distance + length : distance;
}

QColor zzMixed(const QColor &from, const QColor &to, qreal fraction)
{
    return QColor::fromRgbF(float(from.redF() + (to.redF() - from.redF()) * fraction),
        float(from.greenF() + (to.greenF() - from.greenF()) * fraction),
        float(from.blueF() + (to.blueF() - from.blueF()) * fraction),
        float(from.alphaF() + (to.alphaF() - from.alphaF()) * fraction));
}
} // namespace

ZzBorderBeamPrivate::ZzBorderBeamPrivate(QWidget *owner)
    : widget(owner)
{
    timer.setInterval(16);
    timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&timer, &QTimer::timeout, widget, [this] {
        advance();
        widget->update();
    });
}

void ZzBorderBeamPrivate::advance()
{
    if (!timer.isActive())
        return;
    const qreal sign = direction == ZzBorderBeam::Clockwise ? 1.0 : -1.0;
    progress = zzWrapped(progress + sign * qreal(elapsed.restart()) / animationDuration, 1.0);
}

void ZzBorderBeamPrivate::synchronize()
{
    const bool run = animationEnabled && beamLength > 0 && widget->isVisible() && widget->isEnabled()
        && widget->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, widget);
    if (run == timer.isActive())
        return;
    if (run) {
        elapsed.start();
        timer.start();
    } else {
        advance();
        timer.stop();
    }
}

void ZzBorderBeamPrivate::restart()
{
    progress = zzWrapped(initialProgress, 1.0);
    if (timer.isActive())
        elapsed.restart();
    widget->update();
}

ZzBorderBeam::ThemeConfig ZzBorderBeamPrivate::activeTheme() const
{
    const bool dark = themeMode == ZzBorderBeam::DarkTheme
        || (themeMode == ZzBorderBeam::AutoTheme
            && widget->palette().color(QPalette::Window).lightness() < 128);
    return dark ? darkTheme : lightTheme;
}

ZzBorderBeam::ThemeConfig ZzBorderBeamPrivate::resolvedTheme() const
{
    const auto palette = widget->palette();
    const auto group = widget->isEnabled() ? QPalette::Active : QPalette::Disabled;
    auto theme = activeTheme();
    if (const auto *style = qobject_cast<const ZzFluentStyle *>(widget->style());
        style && style->themeSnapshot()->mode() == ZzThemeMode::HighContrast) {
        theme = { palette.color(group, QPalette::Window), palette.color(group, QPalette::WindowText),
            palette.color(group, QPalette::Highlight), palette.color(group, QPalette::WindowText) };
    }
    const auto resolve = [](const QColor &overrideColor, const QColor &config, const QColor &fallback) {
        return overrideColor.isValid() ? overrideColor : (config.isValid() ? config : fallback);
    };
    theme.backgroundColor
        = resolve(backgroundColor, theme.backgroundColor, palette.color(group, QPalette::Window));
    theme.borderColor = resolve(borderColor, theme.borderColor, palette.color(group, QPalette::Mid));
    theme.startColor = resolve(startColor, theme.startColor, palette.color(group, QPalette::Accent));
    theme.endColor = resolve(endColor, theme.endColor, theme.startColor.lighter(145));
    return theme;
}

void ZzBorderBeamPrivate::drawSurface(QPainter &painter, bool pressed, bool hovered) const
{
    const auto theme = resolvedTheme();
    QColor background = theme.backgroundColor;
    if (pressed)
        background = background.darker(108);
    else if (hovered)
        background = background.lighter(106);
    const QRectF bounds = QRectF(widget->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    if (bounds.isEmpty())
        return;
    const qreal radius = std::min(cornerRadius, std::min(bounds.width(), bounds.height()) * 0.5);
    painter.setPen(QPen(theme.borderColor, 1.0));
    painter.setBrush(background);
    painter.drawRoundedRect(bounds, radius, radius);
}

void ZzBorderBeamPrivate::drawBeam(QPainter &painter)
{
    if (beamLength <= 0)
        return;
    if (pathDirty || pathSize != widget->size()) {
        pathDirty = false;
        pathSize = widget->size();
        path = QPainterPath();
        const qreal inset = beamWidth * 0.5 + 0.5;
        const QRectF bounds = QRectF(widget->rect()).adjusted(inset, inset, -inset, -inset);
        if (!bounds.isEmpty()) {
            const qreal radius
                = std::clamp(cornerRadius - inset, 0.0, std::min(bounds.width(), bounds.height()) * 0.5);
            path.addRoundedRect(bounds, radius, radius);
        }
        pathLength = path.length();
    }
    if (pathLength <= 0 || path.isEmpty())
        return;
    auto theme = resolvedTheme();
    if (!widget->isEnabled()) {
        theme.startColor.setAlphaF(theme.startColor.alphaF() * 0.46f);
        theme.endColor.setAlphaF(theme.endColor.alphaF() * 0.46f);
    }
    const qreal length = std::min(beamLength, pathLength * 0.95);
    const int segments = std::clamp(int(std::ceil(length / 2.5)), 12, 160);
    const qreal sign = direction == ZzBorderBeam::Clockwise ? 1.0 : -1.0;
    const auto point = [this](qreal distance) {
        return path.pointAtPercent(path.percentAtLength(zzWrapped(distance, pathLength)));
    };
    for (int index = 0; index < beamCount; ++index) {
        const qreal head = zzWrapped(progress + qreal(index) / beamCount, 1.0) * pathLength;
        QPointF previous = point(head - sign * length);
        for (int segment = 1; segment <= segments; ++segment) {
            const qreal fraction = qreal(segment) / segments;
            const QPointF next = point(head - sign * length * (1.0 - fraction));
            QColor color = zzMixed(theme.startColor, theme.endColor, fraction);
            color.setAlphaF(float(color.alphaF() * std::min(1.0, fraction * 4.0)));
            painter.setPen(QPen(color, beamWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawLine(previous, next);
            previous = next;
        }
    }
}

} // namespace ZzFluentUI
