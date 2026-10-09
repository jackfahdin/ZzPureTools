#include <ZzFluentUI/ZzLiquidGauge.h>

#include "private/ZzGaugeSupportPrivate.h"
#include "private/ZzLiquidGaugePrivate.h"

#include <QEvent>
#include <QPainter>
#include <QPointer>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace ZzFluentUI {
namespace {
constexpr qreal Pi = std::numbers::pi_v<qreal>;
}

ZzLiquidGaugePrivate::ZzLiquidGaugePrivate(ZzLiquidGauge *widget) : q(widget)
{
    timer.setInterval(16);
    timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&timer, &QTimer::timeout, q, [this] {
        syncAnimation();
        if (!timer.isActive()) return;
        const qreal step = static_cast<qreal>(elapsed.restart()) / waveAnimationDuration;
        phase = std::fmod(phase + step, 1.0);
        q->update();
    });
}

void ZzLiquidGaugePrivate::syncAnimation()
{
    const bool run = animationEnabled && waveAmplitude > 0.0 && zzGaugeMotionAllowed(q);
    if (run && !timer.isActive()) {
        elapsed.start();
        timer.start();
    } else if (!run) {
        timer.stop();
    }
}

ZzLiquidGauge::ZzLiquidGauge(QWidget *parent)
    : QProgressBar(parent), d_ptr(std::make_unique<ZzLiquidGaugePrivate>(this))
{
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setAutoFillBackground(false);
}

ZzLiquidGauge::~ZzLiquidGauge() = default;

ZzLiquidGauge::ZzLiquidShape ZzLiquidGauge::shape() const { return d_ptr->shape; }
bool ZzLiquidGauge::isAnimationEnabled() const { return d_ptr->animationEnabled; }
bool ZzLiquidGauge::isRunning() const { return d_ptr->timer.isActive(); }

void ZzLiquidGauge::setShape(ZzLiquidShape value)
{
    if (value < CircleShape || value > TriangleShape || d_ptr->shape == value) return;
    d_ptr->shape = value;
    update();
    Q_EMIT shapeChanged(value);
}

void ZzLiquidGauge::setAnimationEnabled(bool value)
{
    if (d_ptr->animationEnabled == value) return;
    d_ptr->animationEnabled = value;
    d_ptr->syncAnimation();
    update();
    Q_EMIT animationEnabledChanged(value);
}

// Finish all state changes before the single public signal; slots may delete or reenter.
#define ZZ_LIQUID_REAL_PROPERTY(Name, Setter, Maximum, Geometry, Motion) \
    qreal ZzLiquidGauge::Name() const { return d_ptr->Name; } \
    void ZzLiquidGauge::Setter(qreal value) \
    { \
        if (!std::isfinite(value)) return; \
        value = std::clamp(value, 0.0, Maximum); \
        if (qFuzzyCompare(d_ptr->Name, value)) return; \
        d_ptr->Name = value; \
        if (Geometry) updateGeometry(); \
        if (Motion) d_ptr->syncAnimation(); \
        update(); \
        Q_EMIT Name##Changed(value); \
    }
ZZ_LIQUID_REAL_PROPERTY(waveAmplitude, setWaveAmplitude, 100.0, false, true)
ZZ_LIQUID_REAL_PROPERTY(secondaryWaveOpacity, setSecondaryWaveOpacity, 1.0, false, false)
ZZ_LIQUID_REAL_PROPERTY(outlineWidth, setOutlineWidth, 100.0, true, false)
ZZ_LIQUID_REAL_PROPERTY(outlineDistance, setOutlineDistance, 100.0, true, false)
#undef ZZ_LIQUID_REAL_PROPERTY

#define ZZ_LIQUID_INT_PROPERTY(Name, Setter, Minimum, Maximum) \
    int ZzLiquidGauge::Name() const { return d_ptr->Name; } \
    void ZzLiquidGauge::Setter(int value) \
    { \
        value = std::clamp(value, Minimum, Maximum); \
        if (d_ptr->Name == value) return; \
        d_ptr->Name = value; \
        update(); \
        Q_EMIT Name##Changed(value); \
    }
ZZ_LIQUID_INT_PROPERTY(waveCount, setWaveCount, 1, 20)
ZZ_LIQUID_INT_PROPERTY(waveAnimationDuration, setWaveAnimationDuration, 100, 60000)
ZZ_LIQUID_INT_PROPERTY(contentFontPixelSize, setContentFontPixelSize, 0, 200)
#undef ZZ_LIQUID_INT_PROPERTY

#define ZZ_LIQUID_COLOR_PROPERTY(Name, Setter) \
    QColor ZzLiquidGauge::Name() const { return d_ptr->Name; } \
    void ZzLiquidGauge::Setter(QColor value) \
    { \
        if (d_ptr->Name == value) return; \
        d_ptr->Name = value; \
        update(); \
        Q_EMIT Name##Changed(value); \
    }
ZZ_LIQUID_COLOR_PROPERTY(waveColor, setWaveColor)
ZZ_LIQUID_COLOR_PROPERTY(backgroundColor, setBackgroundColor)
ZZ_LIQUID_COLOR_PROPERTY(outlineColor, setOutlineColor)
ZZ_LIQUID_COLOR_PROPERTY(textColor, setTextColor)
ZZ_LIQUID_COLOR_PROPERTY(submergedTextColor, setSubmergedTextColor)
#undef ZZ_LIQUID_COLOR_PROPERTY

QSize ZzLiquidGauge::sizeHint() const { return {180, 180}; }
QSize ZzLiquidGauge::minimumSizeHint() const { return {56, 56}; }

bool ZzLiquidGauge::event(QEvent *event)
{
    const QPointer<ZzLiquidGauge> guard(this);
    const bool result = QProgressBar::event(event);
    if (!guard || !d_ptr) return result;
    switch (event->type()) {
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::EnabledChange:
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        d_ptr->syncAnimation();
        update();
        break;
    case QEvent::FontChange:
        updateGeometry();
        update();
        break;
    default:
        break;
    }
    return result;
}

void ZzLiquidGauge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    d_ptr->syncAnimation();
    d_ptr->paint();
}

qreal ZzLiquidGaugePrivate::valueFraction() const
{
    const qint64 minimum = q->minimum();
    const qint64 maximum = q->maximum();
    const qint64 value = q->value();
    if (maximum <= minimum || value < minimum) return 0.0;
    return std::clamp(static_cast<qreal>(value - minimum) /
                          static_cast<qreal>(maximum - minimum), 0.0, 1.0);
}

QPainterPath ZzLiquidGaugePrivate::shapePath(const QRectF &bounds) const
{
    QPainterPath path;
    switch (shape) {
    case ZzLiquidGauge::RectShape:
        path.addRect(bounds);
        break;
    case ZzLiquidGauge::PinShape: {
        const qreal cx = bounds.center().x();
        const qreal top = bounds.top(), bottom = bounds.bottom();
        const qreal w = bounds.width(), h = bounds.height();
        const qreal left = bounds.left() + w * 0.08;
        const qreal right = bounds.right() - w * 0.08;
        path.moveTo(cx, bottom);
        path.cubicTo(cx - w * 0.08, bottom - h * 0.16, left, top + h * 0.58, left, top + h * 0.37);
        path.cubicTo(left, top + h * 0.16, cx - w * 0.2, top, cx, top);
        path.cubicTo(cx + w * 0.2, top, right, top + h * 0.16, right, top + h * 0.37);
        path.cubicTo(right, top + h * 0.58, cx + w * 0.08, bottom - h * 0.16, cx, bottom);
        path.closeSubpath();
        break;
    }
    case ZzLiquidGauge::TriangleShape:
        path.addPolygon(QPolygonF{QPointF(bounds.center().x(), bounds.top()),
                                  bounds.bottomRight(), bounds.bottomLeft()});
        path.closeSubpath();
        break;
    case ZzLiquidGauge::CircleShape:
        path.addEllipse(bounds);
        break;
    }
    return path;
}

QPainterPath ZzLiquidGaugePrivate::wavePath(const QRectF &bounds, qreal baseline,
                                          qreal amplitude, qreal angle) const
{
    const qreal wavelength = bounds.width() / waveCount;
    const qreal left = bounds.left() - wavelength;
    const qreal right = bounds.right() + wavelength;
    const int samples = std::max(64, static_cast<int>(std::ceil((right - left) / 2.0)));
    QPainterPath path;
    path.moveTo(left, bounds.bottom() + amplitude + 2.0);
    for (int i = 0; i <= samples; ++i) {
        const qreal x = left + static_cast<qreal>(i) / samples * (right - left);
        const qreal y = baseline + amplitude * std::sin((x - bounds.left()) / wavelength * 2.0 * Pi + angle);
        path.lineTo(x, y);
    }
    path.lineTo(right, bounds.bottom() + amplitude + 2.0);
    path.closeSubpath();
    return path;
}

QColor ZzLiquidGaugePrivate::resolvedColor(const QColor &color, QPalette::ColorRole role) const
{
    const auto group = q->isEnabled() ? QPalette::Active : QPalette::Disabled;
    QColor result = color.isValid() ? color : q->palette().color(group, role);
    if (!q->isEnabled() && color.isValid()) result.setAlphaF(result.alphaF() * 0.46F);
    return result;
}

void ZzLiquidGaugePrivate::paint()
{
    const qreal side = std::min(q->width(), q->height());
    if (side <= 2.0) return;
    const QPointF center = QRectF(q->rect()).center();
    const qreal outerInset = std::max(1.0, outlineWidth * 0.5 + 1.0);
    const QRectF outerBounds(center.x() - side * 0.5 + outerInset,
                             center.y() - side * 0.5 + outerInset,
                             side - outerInset * 2.0, side - outerInset * 2.0);
    const qreal innerInset = outlineWidth * 0.5 + outlineDistance;
    const QRectF innerBounds = outerBounds.adjusted(innerInset, innerInset, -innerInset, -innerInset);
    if (outerBounds.isEmpty() || innerBounds.width() <= 1.0 || innerBounds.height() <= 1.0) return;

    QPainter painter(q);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    const QPainterPath outer = shapePath(outerBounds), inner = shapePath(innerBounds);
    const QColor wave = resolvedColor(waveColor, QPalette::Accent);
    painter.fillPath(inner, resolvedColor(backgroundColor, QPalette::Base));

    const qreal fraction = valueFraction();
    const qreal baseline = innerBounds.bottom() - fraction * innerBounds.height();
    const qreal amplitude = std::min(waveAmplitude, innerBounds.height() * 0.14) * std::sin(fraction * Pi);
    const qreal angle = phase * 2.0 * Pi;
    const QPainterPath rear = wavePath(innerBounds, baseline + amplitude * 0.18,
                                       amplitude * 0.82, -angle * 0.78 + Pi * 0.55);
    const QPainterPath front = wavePath(innerBounds, baseline, amplitude, angle);
    painter.save();
    painter.setClipPath(inner);
    QColor rearColor = wave;
    rearColor.setAlphaF(rearColor.alphaF() * static_cast<float>(secondaryWaveOpacity));
    painter.fillPath(rear, rearColor);
    painter.fillPath(front, wave);
    painter.restore();

    if (outlineWidth > 0.0) {
        const QColor outline = outlineColor.isValid() ? resolvedColor(outlineColor, QPalette::Mid) : wave;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(outline, outlineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(outer);
    }
    const QString text = q->text();
    if (!q->isTextVisible() || text.isEmpty()) return;
    QFont contentFont = q->font();
    contentFont.setPixelSize(contentFontPixelSize > 0 ? contentFontPixelSize
        : std::max(10, qRound(side * (shape == ZzLiquidGauge::PinShape ? 0.14 : 0.18))));
    contentFont.setWeight(QFont::DemiBold);
    painter.setFont(contentFont);
    const QRectF textBounds = innerBounds.adjusted(4.0, 4.0, -4.0, -4.0);
    if (textBounds.isEmpty()) return;
    const int flags = static_cast<int>(q->alignment()) | Qt::TextSingleLine;
    painter.setClipPath(inner);
    painter.setPen(resolvedColor(textColor, QPalette::Text));
    painter.drawText(textBounds, flags, text);
    const auto drawWetText = [&](const QPainterPath &clip) {
        painter.save();
        painter.setClipPath(clip, Qt::IntersectClip);
        painter.setPen(resolvedColor(submergedTextColor, QPalette::HighlightedText));
        painter.drawText(textBounds, flags, text);
        painter.restore();
    };
    if (secondaryWaveOpacity > 0.0) drawWetText(rear);
    drawWetText(front);
}

} // namespace ZzFluentUI
