#include "ZzColorGradientSlider.h"
#include "ZzColorPickerMetrics.h"
#include <QtCore/QPointer>

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzColorGradientSlider::ZzColorGradientSlider(Qt::Orientation orientation, QWidget *parent)
    : QAbstractSlider(parent), theme_(this)
{
    setOrientation(orientation);
    setRange(0, 255);
    setPageStep(16);
    setFocusPolicy(Qt::StrongFocus);
    if (orientation == Qt::Horizontal) {
        setFixedHeight(ZzColorPickerSliderHitExtent);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    } else {
        setFixedWidth(ZzColorPickerSliderHitExtent);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
    connect(this, &QAbstractSlider::valueChanged, this, qOverload<>(&QWidget::update));
}

QSize ZzColorGradientSlider::sizeHint() const
{
    return orientation() == Qt::Horizontal ? QSize(200, 24) : QSize(24, 240);
}

void ZzColorGradientSlider::setColors(QList<QColor> colors)
{
    if (colors_ != colors) {
        colors_ = std::move(colors);
        cache_ = {};
        update();
    }
}

void ZzColorGradientSlider::paintEvent(QPaintEvent *)
{
    const qreal dpr = devicePixelRatioF();
    const QSize pixels(qMax(1, qRound(width() * dpr)), qMax(1, qRound(height() * dpr)));
    if (cache_.size() != pixels || cache_.devicePixelRatio() != dpr) {
        cache_ = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
        cache_.setDevicePixelRatio(dpr);
        cache_.fill(Qt::transparent);
        QPainter imagePainter(&cache_);
        QLinearGradient gradient;
        if (orientation() == Qt::Horizontal) {
            gradient = QLinearGradient(0, 0, width(), 0);
        } else {
            gradient = QLinearGradient(0, height(), 0, 0);
        }
        for (int index = 0; index < colors_.size(); ++index) {
            gradient.setColorAt(index / qreal(qMax(1, colors_.size() - 1)), colors_.at(index));
        }
        imagePainter.fillRect(rect(), gradient);
    }
    theme_.refreshFallback();
    const auto snapshot = theme_.snapshot();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled()) {
        painter.setOpacity(0.45);
    }
    QPainterPath clip;
    const QRectF groove = orientation() == Qt::Horizontal
        ? QRectF(5, (height() - 10) / 2.0, width() - 10, 10)
        : QRectF((width() - 10) / 2.0, 5, 10, height() - 10);
    clip.addRoundedRect(groove, 5, 5);
    painter.setClipPath(clip);
    for (int y = 0; y < height(); y += 4) {
        for (int x = 0; x < width(); x += 4) {
            painter.fillRect(x, y, 4, 4, snapshot->color(((x / 4 + y / 4) % 2)
                ? ZzColorToken::SurfaceSecondary : ZzColorToken::Surface));
        }
    }
    if (orientation() == Qt::Horizontal && isRightToLeft()) {
        painter.translate(width(), 0);
        painter.scale(-1, 1);
    }
    painter.drawImage(QPointF(0, 0), cache_);
    painter.setClipping(false);
    const qreal fraction = (value() - minimum()) / qreal(qMax(1, maximum() - minimum()));
    const QPointF cursor = orientation() == Qt::Horizontal
        ? QPointF(5 + fraction * (width() - 10), height() / 2.0)
        : QPointF(width() / 2.0, 5 + (1 - fraction) * (height() - 10));
    painter.setPen(QPen(snapshot->color(hasFocus() ? ZzColorToken::FocusStroke
                                                  : ZzColorToken::ControlStroke), 1));
    painter.setBrush(Qt::white);
    painter.drawEllipse(cursor, 8, 8);
    painter.setPen(Qt::NoPen);
    painter.setBrush(snapshot->color(ZzColorToken::Accent));
    painter.drawEllipse(cursor, 4, 4);
}

void ZzColorGradientSlider::editAt(QPointF point)
{
    if (!isEnabled()) {
        return;
    }
    qreal fraction = orientation() == Qt::Horizontal
        ? (point.x() - 5) / qMax(1, width() - 10)
        : 1 - (point.y() - 5) / qMax(1, height() - 10);
    if (orientation() == Qt::Horizontal && isRightToLeft()) {
        fraction = 1 - fraction;
    }
    setValue(minimum() + qRound(qBound(0.0, fraction, 1.0) * (maximum() - minimum())));
}

void ZzColorGradientSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isEnabled()) {
        const QPointer<ZzColorGradientSlider> guard(this);
        setFocus(Qt::MouseFocusReason);
        if (!guard) {
            return;
        }
        setSliderDown(true);
        if (!guard) {
            return;
        }
        editAt(event->position());
        event->accept();
    }
}

void ZzColorGradientSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (isSliderDown()) {
        editAt(event->position());
    }
}

void ZzColorGradientSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setSliderDown(false);
    }
}

} // namespace ZzFluentUI
