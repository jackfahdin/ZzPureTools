#include <ZzFluentUI/ZzFluentPainter.h>

#include <QtGui/QPainter>
#include <QtGui/QPen>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzDpiScale.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

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

} // namespace ZzFluentUI
