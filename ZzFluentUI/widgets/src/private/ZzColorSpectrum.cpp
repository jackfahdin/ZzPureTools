#include "ZzColorSpectrum.h"
#include "ZzColorPickerMath.h"
#include "ZzColorPickerMetrics.h"
#include <QtCore/QPointer>

#include <cmath>
#include <numbers>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzColorSpectrum::ZzColorSpectrum(QWidget *parent)
    : QWidget(parent), theme_(this)
{
    setObjectName(QStringLiteral("zzColorSpectrum"));
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(ZzColorPickerSpectrumMinimum, ZzColorPickerSpectrumMinimum);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize ZzColorSpectrum::sizeHint() const
{
    return {280, 240};
}

void ZzColorSpectrum::setState(qreal hue, qreal saturation, bool ring)
{
    hue_ = hue;
    saturation_ = saturation;
    ring_ = ring;
    update();
}

void ZzColorSpectrum::paintEvent(QPaintEvent *)
{
    const qreal dpr = devicePixelRatioF();
    const QSize pixels(qMax(1, qRound(width() * dpr)),
                       qMax(1, qRound(height() * dpr)));
    if (cache_.size() != pixels || cache_.devicePixelRatio() != dpr
        || cachedRing_ != ring_) {
        cache_ = QImage(pixels, QImage::Format_ARGB32);
        cache_.setDevicePixelRatio(dpr);
        cache_.fill(Qt::transparent);
        const QPointF center((pixels.width() - 1) / 2.0,
                             (pixels.height() - 1) / 2.0);
        const qreal radius = qMax(1.0, qMin(center.x(), center.y()));
        for (int y = 0; y < pixels.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(cache_.scanLine(y));
            for (int x = 0; x < pixels.width(); ++x) {
                qreal h = x / qreal(qMax(1, pixels.width() - 1));
                qreal s = 1 - y / qreal(qMax(1, pixels.height() - 1));
                if (ring_) {
                    const qreal dx = x - center.x();
                    const qreal dy = y - center.y();
                    s = std::hypot(dx, dy) / radius;
                    if (s > 1) {
                        continue;
                    }
                    h = std::atan2(dy, dx) / (2 * std::numbers::pi);
                    if (h < 0) {
                        h += 1;
                    }
                }
                line[x] = zzColorFromHsv(h, s, 1).rgb();
            }
        }
        cachedRing_ = ring_;
    }
    theme_.refreshFallback();
    const auto snapshot = theme_.snapshot();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled()) {
        painter.setOpacity(0.45);
    }
    if (isRightToLeft()) {
        painter.translate(width(), 0);
        painter.scale(-1, 1);
    }
    painter.drawImage(QPointF(0, 0), cache_);
    QPointF cursor(hue_ * (width() - 1), (1 - saturation_) * (height() - 1));
    if (ring_) {
        const qreal radius = qMin(width() - 1, height() - 1) / 2.0;
        cursor = QPointF((width() - 1) / 2.0, (height() - 1) / 2.0)
            + QPointF(std::cos(hue_ * 2 * std::numbers::pi),
                      std::sin(hue_ * 2 * std::numbers::pi)) * radius * saturation_;
    }
    painter.setPen(QPen(Qt::black, 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(cursor, 6, 6);
    painter.setPen(QPen(Qt::white, 2));
    painter.drawEllipse(cursor, 6, 6);
    if (hasFocus()) {
        painter.setPen(QPen(snapshot->color(ZzColorToken::FocusStroke), 2));
        painter.drawRoundedRect(rect().adjusted(1, 1, -2, -2), ZzColorPickerCornerRadius, ZzColorPickerCornerRadius);
    }
}

void ZzColorSpectrum::editAt(QPointF position)
{
    if (!isEnabled()) {
        return;
    }
    if (isRightToLeft()) {
        position.setX(width() - 1 - position.x());
    }
    qreal h = qBound(0.0, position.x() / qMax(1, width() - 1), 1.0);
    qreal s = qBound(0.0, 1 - position.y() / qMax(1, height() - 1), 1.0);
    if (ring_) {
        const QPointF delta = position - QPointF((width() - 1) / 2.0, (height() - 1) / 2.0);
        const qreal radius = qMax(1.0, qMin(width() - 1, height() - 1) / 2.0);
        s = qBound(0.0, std::hypot(delta.x(), delta.y()) / radius, 1.0);
        h = std::atan2(delta.y(), delta.x()) / (2 * std::numbers::pi);
        if (h < 0) {
            h += 1;
        }
    }
    if (edited) {
        edited(h, s);
    }
}

void ZzColorSpectrum::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isEnabled()) {
        const QPointer<ZzColorSpectrum> guard(this);
        setFocus(Qt::MouseFocusReason);
        if (!guard) {
            return;
        }
        editAt(event->position());
        event->accept();
    }
}

void ZzColorSpectrum::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons().testFlag(Qt::LeftButton)) {
        editAt(event->position());
    }
}

void ZzColorSpectrum::keyPressEvent(QKeyEvent *event)
{
    if (!isEnabled()) {
        return;
    }
    const qreal step = event->modifiers().testFlag(Qt::ShiftModifier) ? 0.05 : 0.01;
    qreal h = hue_;
    qreal s = saturation_;
    switch (event->key()) {
    case Qt::Key_Left: h -= isRightToLeft() ? -step : step; break;
    case Qt::Key_Right: h += isRightToLeft() ? -step : step; break;
    case Qt::Key_Up: s += step; break;
    case Qt::Key_Down: s -= step; break;
    case Qt::Key_Home: h = 0; break;
    case Qt::Key_End: h = 1; break;
    default: QWidget::keyPressEvent(event); return;
    }
    if (edited) {
        edited(qBound(0.0, h, 1.0), qBound(0.0, s, 1.0));
    }
    event->accept();
}

} // namespace ZzFluentUI
