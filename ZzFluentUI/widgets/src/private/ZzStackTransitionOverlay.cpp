#include "ZzStackTransitionOverlay.h"
#include <cmath>
#include <limits>
#include <utility>
#include <QtCore/QEvent>
#include <QtGui/QPainter>

namespace ZzFluentUI {

ZzStackTransitionOverlay::ZzStackTransitionOverlay(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("zzAnimatedStackOverlay"));
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_AcceptTouchEvents);
    hide();
}

QSize ZzStackTransitionOverlay::boundedPixelSize(QSize logical, qreal dpr)
{
    if (logical.isEmpty() || !std::isfinite(dpr) || dpr <= 0) return {};
    const qreal width = std::ceil(logical.width() * dpr);
    const qreal height = std::ceil(logical.height() * dpr);
    constexpr qreal maximumBytes = 64.0 * 1024.0 * 1024.0;
    if (width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max()
        || width * height * 8.0 > maximumBytes) return {};
    return QSize(static_cast<int>(width), static_cast<int>(height));
}

void ZzStackTransitionOverlay::setFrames(QImage outgoing, QImage incoming, Mode mode, bool reverse)
{
    outgoing_ = std::move(outgoing);
    incoming_ = std::move(incoming);
    mode_ = mode;
    reverse_ = reverse;
    progress_ = 0;
    update();
}
void ZzStackTransitionOverlay::setProgress(qreal progress) { progress_ = progress; update(); }
void ZzStackTransitionOverlay::clear() { outgoing_ = {}; incoming_ = {}; }

void ZzStackTransitionOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    if (mode_ == Mode::Fade) {
        painter.drawImage(QPointF(), outgoing_);
        painter.setOpacity(progress_);
        painter.drawImage(QPointF(), incoming_);
        return;
    }
    const bool horizontal = mode_ == Mode::SlideHorizontal;
    const qreal extent = horizontal ? width() : height();
    const qreal sign = reverse_ ? -1 : 1;
    const qreal outgoing = -sign * extent * progress_;
    const qreal incoming = sign * extent * (1 - progress_);
    painter.drawImage(horizontal ? QPointF(outgoing, 0) : QPointF(0, outgoing), outgoing_);
    painter.drawImage(horizontal ? QPointF(incoming, 0) : QPointF(0, incoming), incoming_);
}

bool ZzStackTransitionOverlay::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
    case QEvent::ContextMenu:
        event->accept();
        return true;
    default: return QWidget::event(event);
    }
}

} // namespace ZzFluentUI
