#include <ZzFluentUI/ZzFluentPainter.h>

#include <QtGui/QPainter>
#include <QtGui/QPen>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzDpiScale.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

namespace {

qreal zzAlignedStrokeWidth(QPainter *painter, qreal logicalWidth)
{
    if (logicalWidth <= 0.0) {
        return 0.0;
    }
    const qreal devicePixelRatio = painter->device() != nullptr
        ? painter->device()->devicePixelRatioF()
        : 1.0;
    const qreal ratio = static_cast<qreal>(
        ZzDpiScale::bucket(devicePixelRatio)) / 100.0;
    return static_cast<qreal>(ZzDpiScale::physicalPixels(
        logicalWidth,
        devicePixelRatio)) / ratio;
}

} // namespace

void ZzFluentPainter::drawControlBackground(
    QPainter *painter,
    const QRectF &rect,
    const ZzThemeSnapshot &snapshot,
    bool hovered,
    bool pressed,
    bool enabled)
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    ZzColorToken token = ZzColorToken::ControlFill;
    if (!enabled) {
        token = ZzColorToken::ControlFillDisabled;
    } else if (pressed) {
        token = ZzColorToken::ControlFillPressed;
    } else if (hovered) {
        token = ZzColorToken::ControlFillHover;
    }

    const qreal radius = snapshot.metric(
        ZzMetricToken::CornerRadiusMedium);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(
        snapshot.color(ZzColorToken::ControlStroke),
        snapshot.metric(ZzMetricToken::StrokeThin)));
    painter->setBrush(snapshot.color(token));
    painter->drawRoundedRect(rect, radius, radius);
    painter->restore();
}

void ZzFluentPainter::drawFocusRing(
    QPainter *painter,
    const QRectF &rect,
    const ZzThemeSnapshot &snapshot,
    qreal devicePixelRatio)
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    const int physicalWidth = ZzDpiScale::physicalPixels(
        snapshot.metric(ZzMetricToken::FocusStrokeWidth),
        devicePixelRatio);
    const qreal ratio = static_cast<qreal>(
        ZzDpiScale::bucket(devicePixelRatio)) / 100.0;
    const qreal logicalWidth = physicalWidth / ratio;
    const qreal radius = snapshot.metric(
        ZzMetricToken::CornerRadiusMedium);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(
        snapshot.color(ZzColorToken::FocusStroke),
        logicalWidth));
    painter->drawRoundedRect(
        rect.adjusted(
            logicalWidth / 2.0,
            logicalWidth / 2.0,
            -logicalWidth / 2.0,
            -logicalWidth / 2.0),
        radius,
        radius);
    painter->restore();
}

void ZzFluentPainter::drawRoundedSurface(
    QPainter *painter,
    const QRectF &rect,
    const ZzThemeSnapshot &snapshot,
    ZzColorToken fill,
    ZzColorToken stroke,
    qreal radius,
    qreal strokeWidth)
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    const qreal alignedStroke = zzAlignedStrokeWidth(painter, strokeWidth);
    const QRectF paintRect = alignedStroke > 0.0
        ? rect.adjusted(
              alignedStroke / 2.0,
              alignedStroke / 2.0,
              -alignedStroke / 2.0,
              -alignedStroke / 2.0)
        : rect;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(alignedStroke > 0.0
        ? QPen(snapshot.color(stroke), alignedStroke)
        : QPen(Qt::NoPen));
    painter->setBrush(snapshot.color(fill));
    const qreal validRadius = qMax<qreal>(0.0, radius);
    painter->drawRoundedRect(paintRect, validRadius, validRadius);
    painter->restore();
}

void ZzFluentPainter::drawOverlayScrim(
    QPainter *painter,
    const QRectF &rect,
    const ZzThemeSnapshot &snapshot)
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(snapshot.color(ZzColorToken::OverlayScrim));
    painter->drawRect(rect);
    painter->restore();
}

void ZzFluentPainter::drawPopupSurface(
    QPainter *painter,
    const QRectF &rect,
    const ZzThemeSnapshot &snapshot)
{
    drawRoundedSurface(
        painter,
        rect,
        snapshot,
        ZzColorToken::SurfaceSecondary,
        ZzColorToken::ControlStroke,
        snapshot.metric(ZzMetricToken::CornerRadiusMedium),
        snapshot.metric(ZzMetricToken::StrokeThin));
}

void ZzFluentPainter::drawBadgeSurface(
    QPainter *painter,
    const QRectF &rect,
    const ZzThemeSnapshot &snapshot,
    ZzColorToken fill)
{
    drawRoundedSurface(
        painter,
        rect,
        snapshot,
        fill,
        fill,
        qMax<qreal>(0.0, rect.height() / 2.0),
        0.0);
}

} // namespace ZzFluentUI
