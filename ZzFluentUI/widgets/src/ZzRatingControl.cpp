#include <ZzFluentUI/ZzRatingControl.h>

#include <algorithm>
#include <cmath>

#include <QtCore/QEvent>
#include <QtCore/QtMath>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QSizePolicy>

#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "private/ZzRatingControlPrivate.h"

namespace ZzFluentUI {

namespace {

constexpr int zzMaximumRating = 10;

} // namespace

ZzRatingControl::ZzRatingControl(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzRatingControlPrivate>(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

ZzRatingControl::~ZzRatingControl() = default;

qreal ZzRatingControl::rating() const noexcept
{
    return d_ptr->rating;
}

void ZzRatingControl::setRating(qreal value)
{
    if (!std::isfinite(value)) {
        return;
    }
    static_cast<void>(d_ptr->setRating(value));
}

int ZzRatingControl::maximumRating() const noexcept
{
    return d_ptr->maximumRating;
}

void ZzRatingControl::setMaximumRating(int maximum)
{
    const int bounded = std::clamp(maximum, 1, zzMaximumRating);
    if (d_ptr->maximumRating == bounded) {
        return;
    }
    d_ptr->maximumRating = bounded;
    d_ptr->clearPreview();
    updateGeometry();
    update();
    Q_EMIT maximumRatingChanged(bounded);
    static_cast<void>(d_ptr->setRating(d_ptr->rating));
}

ZzRatingPrecision ZzRatingControl::precision() const noexcept
{
    return d_ptr->precision;
}

void ZzRatingControl::setPrecision(ZzRatingPrecision value)
{
    if (d_ptr->precision == value) {
        return;
    }
    d_ptr->precision = value;
    d_ptr->clearPreview();
    Q_EMIT precisionChanged(value);
    static_cast<void>(d_ptr->setRating(d_ptr->rating));
    update();
}

bool ZzRatingControl::isReadOnly() const noexcept
{
    return d_ptr->readOnly;
}

void ZzRatingControl::setReadOnly(bool value)
{
    if (d_ptr->readOnly == value) {
        return;
    }
    d_ptr->readOnly = value;
    d_ptr->mousePressed = false;
    d_ptr->clearPreview();
    update();
    Q_EMIT readOnlyChanged(value);
}

QSize ZzRatingControl::sizeHint() const
{
    const auto snapshot = d_ptr->theme.snapshot();
    const int extent = qCeil(
        snapshot->metric(ZzMetricToken::RatingGlyphExtent));
    constexpr int spacing = 4;
    const int width = (d_ptr->maximumRating * extent)
        + ((d_ptr->maximumRating - 1) * spacing);
    return QSize(width + 4, extent + 4);
}

QSize ZzRatingControl::minimumSizeHint() const
{
    const int extent = qCeil(
        d_ptr->theme.snapshot()->metric(
            ZzMetricToken::RatingGlyphExtent));
    return QSize(extent + 4, extent + 4);
}

void ZzRatingControl::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    d_ptr->paint(&painter);
}

void ZzRatingControl::mousePressEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }
    if (event->button() == Qt::LeftButton
        && isEnabled()
        && !d_ptr->readOnly
        && rect().contains(event->position().toPoint())) {
        setFocus(Qt::MouseFocusReason);
        d_ptr->mousePressed = true;
        d_ptr->updatePreview(event->position());
        static_cast<void>(d_ptr->setRating(
            d_ptr->ratingAt(event->position())));
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ZzRatingControl::mouseMoveEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }
    if (!isEnabled() || d_ptr->readOnly) {
        d_ptr->clearPreview();
        QWidget::mouseMoveEvent(event);
        return;
    }
    d_ptr->updatePreview(event->position());
    if (d_ptr->mousePressed) {
        static_cast<void>(d_ptr->setRating(
            d_ptr->ratingAt(event->position())));
    }
    event->accept();
}

void ZzRatingControl::mouseReleaseEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }
    if (event->button() == Qt::LeftButton && d_ptr->mousePressed) {
        if (isEnabled() && !d_ptr->readOnly) {
            d_ptr->updatePreview(event->position());
            static_cast<void>(d_ptr->setRating(
                d_ptr->ratingAt(event->position())));
        }
        d_ptr->mousePressed = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ZzRatingControl::leaveEvent(QEvent *event)
{
    d_ptr->clearPreview();
    QWidget::leaveEvent(event);
}

void ZzRatingControl::keyPressEvent(QKeyEvent *event)
{
    if (event == nullptr) {
        return;
    }
    if (!isEnabled() || d_ptr->readOnly
        || event->modifiers() != Qt::NoModifier) {
        QWidget::keyPressEvent(event);
        return;
    }

    qreal next = d_ptr->rating;
    const qreal step = d_ptr->stepSize();
    switch (event->key()) {
    case Qt::Key_Left:
        next += layoutDirection() == Qt::RightToLeft ? step : -step;
        break;
    case Qt::Key_Right:
        next += layoutDirection() == Qt::RightToLeft ? -step : step;
        break;
    case Qt::Key_Up:
        next += step;
        break;
    case Qt::Key_Down:
        next -= step;
        break;
    case Qt::Key_Home:
        next = 0.0;
        break;
    case Qt::Key_End:
        next = static_cast<qreal>(d_ptr->maximumRating);
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    d_ptr->clearPreview();
    static_cast<void>(d_ptr->setRating(next));
    event->accept();
}

void ZzRatingControl::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::EnabledChange:
        if (!isEnabled()) {
            d_ptr->mousePressed = false;
            d_ptr->clearPreview();
        }
        d_ptr->refreshTheme();
        break;
    case QEvent::DevicePixelRatioChange:
    case QEvent::FontChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        d_ptr->refreshTheme();
        break;
    default:
        break;
    }
}

} // namespace ZzFluentUI
