#include <ZzFluentUI/ZzRangeSlider.h>
#include "private/ZzRangeSliderPrivate.h"
#include "private/ZzRangeSliderAccessible.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QHoverEvent>
#include <QtGui/QPainter>
#include <QtGui/QFocusEvent>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtWidgets/QStyle>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace ZzFluentUI {
namespace {
using Handle = ZzRangeSlider::Handle;
int movedValue(int original, std::int64_t delta, int minimum, int maximum)
{
    return static_cast<int>(std::clamp(
        static_cast<std::int64_t>(original) + delta,
        static_cast<std::int64_t>(minimum),
        static_cast<std::int64_t>(maximum)));
}
bool highContrast(const QWidget *widget)
{
    auto *style = qobject_cast<const ZzFluentStyle *>(widget->style());
    return style && style->themeSnapshot()->mode() == ZzThemeMode::HighContrast;
}
}
ZzRangeSlider::ZzRangeSlider(QWidget *parent)
    : ZzRangeSlider(Qt::Horizontal, parent) {}
ZzRangeSlider::ZzRangeSlider(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<ZzRangeSliderPrivate>(this))
{
    d_ptr->orientation = orientation;
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setSizePolicy(orientation == Qt::Horizontal
        ? QSizePolicy::Expanding : QSizePolicy::Fixed,
        orientation == Qt::Horizontal
        ? QSizePolicy::Fixed : QSizePolicy::Expanding);
    zzInstallRangeSliderAccessibility();
}
ZzRangeSlider::~ZzRangeSlider() = default;
int ZzRangeSlider::minimum() const noexcept { return d_ptr->minimum; }
int ZzRangeSlider::maximum() const noexcept { return d_ptr->maximum; }
void ZzRangeSlider::setMinimum(int value)
{ setRange(value, qMax(value, d_ptr->maximum)); }
void ZzRangeSlider::setMaximum(int value)
{ setRange(qMin(d_ptr->minimum, value), value); }
void ZzRangeSlider::setRange(int low, int high)
{
    if (low > high) std::swap(low, high);
    if (low == d_ptr->minimum && high == d_ptr->maximum) return;
    QPointer<ZzRangeSlider> guard(this);
    d_ptr->cancelDrag();
    if (!guard) return;
    d_ptr->minimum = low;
    d_ptr->maximum = high;
    d_ptr->lower = qBound(low, d_ptr->lower, high);
    d_ptr->upper = qBound(low, d_ptr->upper, high);
    d_ptr->lowerPosition = d_ptr->lower;
    d_ptr->upperPosition = d_ptr->upper;
    Q_EMIT rangeChanged(low, high);
    if (!guard) return;
    d_ptr->notifyValues();
    if (!guard) return;
    update();
}
int ZzRangeSlider::lowerValue() const noexcept { return d_ptr->lower; }
int ZzRangeSlider::upperValue() const noexcept { return d_ptr->upper; }
void ZzRangeSlider::setLowerValue(int value)
{ d_ptr->commit(qMin(value, d_ptr->upper), d_ptr->upper, false); }
void ZzRangeSlider::setUpperValue(int value)
{ d_ptr->commit(d_ptr->lower, qMax(value, d_ptr->lower), false); }
void ZzRangeSlider::setValues(int lower, int upper)
{ d_ptr->commit(lower, upper, false); }
int ZzRangeSlider::lowerPosition() const noexcept { return d_ptr->lowerPosition; }
int ZzRangeSlider::upperPosition() const noexcept { return d_ptr->upperPosition; }
int ZzRangeSlider::singleStep() const noexcept { return d_ptr->singleStep; }
int ZzRangeSlider::pageStep() const noexcept { return d_ptr->pageStep; }
void ZzRangeSlider::setSingleStep(int value) { d_ptr->singleStep = qMax(1, value); }
void ZzRangeSlider::setPageStep(int value) { d_ptr->pageStep = qMax(1, value); }
Qt::Orientation ZzRangeSlider::orientation() const noexcept { return d_ptr->orientation; }
void ZzRangeSlider::setOrientation(Qt::Orientation value)
{
    if (value != Qt::Horizontal && value != Qt::Vertical) return;
    if (d_ptr->orientation == value) return;
    QPointer<ZzRangeSlider> guard(this);
    d_ptr->cancelDrag();
    if (!guard) return;
    d_ptr->orientation = value;
    setSizePolicy(value == Qt::Horizontal
        ? QSizePolicy::Expanding : QSizePolicy::Fixed,
        value == Qt::Horizontal
        ? QSizePolicy::Fixed : QSizePolicy::Expanding);
    updateGeometry();
    update();
}
ZzRangeSlider::SnapMode ZzRangeSlider::snapMode() const noexcept { return d_ptr->snapMode; }
void ZzRangeSlider::setSnapMode(SnapMode value)
{
    if (value != SnapMode::NoSnap && value != SnapMode::SnapAlways
        && value != SnapMode::SnapOnRelease) return;
    d_ptr->snapMode = value;
}
bool ZzRangeSlider::hasTickPosition() const noexcept { return d_ptr->ticks; }
void ZzRangeSlider::setTickPosition(bool value) { d_ptr->ticks = value; update(); }
int ZzRangeSlider::tickInterval() const noexcept { return d_ptr->tickInterval; }
void ZzRangeSlider::setTickInterval(int value)
{ d_ptr->tickInterval = qMax(0, value); update(); }
bool ZzRangeSlider::hasTracking() const noexcept { return d_ptr->tracking; }
void ZzRangeSlider::setTracking(bool value) { d_ptr->tracking = value; }
bool ZzRangeSlider::valueTipEnabled() const noexcept { return d_ptr->valueTip; }
void ZzRangeSlider::setValueTipEnabled(bool value)
{ d_ptr->valueTip = value; if (!value) d_ptr->hideTip(); }
bool ZzRangeSlider::handleFocusRingEnabled() const noexcept { return d_ptr->focusRing; }
void ZzRangeSlider::setHandleFocusRingEnabled(bool value)
{ d_ptr->focusRing = value; update(); }
ZzRangeSlider::Handle ZzRangeSlider::activeHandle() const noexcept { return d_ptr->active; }
QSize ZzRangeSlider::sizeHint() const
{ return d_ptr->orientation == Qt::Horizontal ? QSize(160, 32) : QSize(32, 160); }
QSize ZzRangeSlider::minimumSizeHint() const
{ return d_ptr->orientation == Qt::Horizontal ? QSize(44, 32) : QSize(32, 44); }
void ZzRangeSlider::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    const bool contrast = highContrast(this);
    const QColor accent = contrast ? palette().color(QPalette::Highlight)
        : palette().color(QPalette::Accent);
    const QColor baseGrooveColor = contrast ? palette().color(QPalette::WindowText)
        : dark ? QColor(255, 255, 255, 0x8b) : QColor(0, 0, 0, 0x72);
    QColor grooveColor = baseGrooveColor;
    if (!isEnabled() && !contrast) grooveColor.setAlphaF(grooveColor.alphaF() * 0.6f);
    const QRectF groove = d_ptr->orientation == Qt::Horizontal
        ? QRectF(2, height() / 2.0 - 2, qMax(0, width() - 4), 4)
        : QRectF(width() / 2.0 - 2, 2, 4, qMax(0, height() - 4));
    painter.setPen(Qt::NoPen);
    painter.setBrush(grooveColor);
    painter.drawRoundedRect(groove, 2, 2);

    const QPointF low = d_ptr->center(d_ptr->lowerPosition);
    const QPointF high = d_ptr->center(d_ptr->upperPosition);
    QRectF activeTrack = d_ptr->orientation == Qt::Horizontal
        ? QRectF(qMin(low.x(), high.x()), groove.top(),
            qAbs(low.x() - high.x()), 4)
        : QRectF(groove.left(), qMin(low.y(), high.y()),
            4, qAbs(low.y() - high.y()));
    if (activeTrack.width() > 0 && activeTrack.height() > 0) {
        painter.setBrush(isEnabled() ? accent : baseGrooveColor);
        painter.drawRoundedRect(activeTrack, 2, 2);
    }

    if (d_ptr->ticks && d_ptr->maximum > d_ptr->minimum) {
        const std::int64_t interval = d_ptr->tickInterval > 0
            ? d_ptr->tickInterval : d_ptr->singleStep;
        const std::int64_t span = static_cast<std::int64_t>(d_ptr->maximum)
            - d_ptr->minimum;
        const std::int64_t count = span / interval + 1;
        const int pixels = qMax(1, d_ptr->orientation == Qt::Horizontal
            ? width() : height());
        const int samples = static_cast<int>(qMin<std::int64_t>(count, pixels + 1));
        painter.setBrush(palette().color(QPalette::Text));
        for (int i = 0; i < samples; ++i) {
            const std::int64_t tickIndex = samples == 1 ? 0
                : (static_cast<std::int64_t>(i) * (count - 1)) / (samples - 1);
            const int value = static_cast<int>(qMin<std::int64_t>(
                static_cast<std::int64_t>(d_ptr->minimum) + tickIndex * interval,
                d_ptr->maximum));
            const QPointF point = d_ptr->center(value);
            if (d_ptr->orientation == Qt::Horizontal) {
                painter.drawRect(QRectF(point.x() - 0.5, groove.top() - 10, 1, 4));
                painter.drawRect(QRectF(point.x() - 0.5, groove.bottom() + 6, 1, 4));
            } else {
                painter.drawRect(QRectF(groove.left() - 10, point.y() - 0.5, 4, 1));
                painter.drawRect(QRectF(groove.right() + 6, point.y() - 0.5, 4, 1));
            }
        }
    }

    const auto drawHandle = [&](Handle handle, const QPointF &center) {
        const QColor outerFill = contrast ? palette().color(QPalette::Base)
            : dark ? QColor("#454545") : Qt::white;
        const QColor outerStroke = contrast ? palette().color(QPalette::ButtonText)
            : dark ? QColor(255, 255, 255, 0x18) : QColor(0, 0, 0, 0x29);
        if (!contrast) {
            for (int layer = 5; layer >= 1; --layer) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0,
                    qRound((40.0 / layer) * (dark ? 1.0 : 0.7))));
                painter.drawEllipse(center, 9 + layer * 0.8, 9 + layer * 0.8);
            }
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(outerFill);
        painter.drawEllipse(center, 9, 9);
        QColor inner = isEnabled() ? accent
            : contrast ? palette().color(QPalette::ButtonText)
            : dark ? QColor(255, 255, 255, 40) : QColor(0, 0, 0, 55);
        if (isEnabled() && d_ptr->pressed == handle)
            inner.setAlphaF(inner.alphaF() * 0.8f);
        else if (isEnabled() && d_ptr->hovered == handle)
            inner.setAlphaF(inner.alphaF() * 0.902f);
        painter.setBrush(inner);
        const qreal radius = d_ptr->innerRadius(handle);
        painter.drawEllipse(center, radius, radius);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(outerStroke, 1));
        painter.drawEllipse(center, 9.5, 9.5);
        if (d_ptr->focusRing && hasFocus() && d_ptr->active == handle) {
            QColor ring = accent;
            if (!contrast) ring.setAlpha(dark ? 200 : 130);
            painter.setPen(QPen(ring, 2));
            painter.drawEllipse(center, 11, 11);
        }
    };
    if (d_ptr->active == Handle::LowerHandle) {
        drawHandle(Handle::UpperHandle, high);
        drawHandle(Handle::LowerHandle, low);
    } else {
        drawHandle(Handle::LowerHandle, low);
        drawHandle(Handle::UpperHandle, high);
    }
}

void ZzRangeSlider::mousePressEvent(QMouseEvent *event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPointF point = event->position();
    QPointer<ZzRangeSlider> guard(this);
    const Handle direct = d_ptr->handleAt(point);
    const Handle handle = direct == Handle::NoHandle
        ? d_ptr->nearestHandle(point) : direct;
    d_ptr->active = handle;
    d_ptr->pressed = handle;
    d_ptr->coincidentPress = direct != Handle::NoHandle
        && d_ptr->lowerPosition == d_ptr->upperPosition;
    d_ptr->pressOffset = direct == Handle::NoHandle ? 0.0
        : (d_ptr->orientation == Qt::Horizontal ? point.x() : point.y())
            - d_ptr->axisPosition(handle == Handle::LowerHandle
                ? d_ptr->lowerPosition : d_ptr->upperPosition);
    setFocus(Qt::MouseFocusReason);
    if (!guard) return;
    d_ptr->accessibleFocusChanged();
    if (!guard) return;
    d_ptr->animateHandle(handle);
    Q_EMIT sliderPressed(handle);
    if (!guard || d_ptr->pressed != handle) { event->accept(); return; }
    if (direct == Handle::NoHandle) {
        d_ptr->preview(handle, d_ptr->valueAt(
            d_ptr->orientation == Qt::Horizontal ? point.x() : point.y()));
        if (!guard) { event->accept(); return; }
    }
    d_ptr->showTip();
    event->accept();
}

void ZzRangeSlider::mouseMoveEvent(QMouseEvent *event)
{
    QPointer<ZzRangeSlider> guard(this);
    if (d_ptr->pressed == Handle::NoHandle) {
        const Handle hovered = d_ptr->handleAt(event->position());
        if (hovered != d_ptr->hovered) {
            const Handle previous = d_ptr->hovered;
            d_ptr->hovered = hovered;
            d_ptr->animateHandle(previous);
            d_ptr->animateHandle(hovered);
        }
        QWidget::mouseMoveEvent(event);
        return;
    }
    const qreal axis = (d_ptr->orientation == Qt::Horizontal
        ? event->position().x() : event->position().y()) - d_ptr->pressOffset;
    if (d_ptr->coincidentPress) {
        const int requested = d_ptr->valueAt(axis);
        const int shared = d_ptr->lowerPosition;
        if (requested != shared) {
            const Handle next = requested > shared
                ? Handle::UpperHandle : Handle::LowerHandle;
            d_ptr->coincidentPress = false;
            if (next != d_ptr->pressed) {
                const Handle previous = d_ptr->pressed;
                const auto previousOrientation = d_ptr->orientation;
                const auto previousDirection = layoutDirection();
                const auto cancellation = d_ptr->dragCancellation;
                // 先结束旧端点，再启动新端点；释放回调取消时不能产生幽灵按下。
                d_ptr->pressed = Handle::NoHandle;
                Q_EMIT sliderReleased(previous);
                if (!guard) return;
                if (d_ptr->dragCancellation != cancellation || !isVisible() || !isEnabled()
                    || d_ptr->lowerPosition != shared || d_ptr->upperPosition != shared
                    || d_ptr->orientation != previousOrientation
                    || layoutDirection() != previousDirection) {
                    d_ptr->hideTip();
                    d_ptr->animateHandle(previous);
                    event->accept();
                    return;
                }
                d_ptr->pressed = next;
                d_ptr->active = next;
                d_ptr->accessibleFocusChanged();
                if (!guard) return;
                d_ptr->animateHandle(previous);
                d_ptr->animateHandle(next);
                Q_EMIT sliderPressed(next);
                if (!guard || d_ptr->pressed != next) return;
            }
        }
    }
    d_ptr->preview(d_ptr->pressed, d_ptr->valueAt(axis));
    event->accept();
}

void ZzRangeSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || d_ptr->pressed == Handle::NoHandle) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    const Handle released = d_ptr->pressed;
    QPointer<ZzRangeSlider> guard(this);
    const qreal axis = (d_ptr->orientation == Qt::Horizontal
        ? event->position().x() : event->position().y()) - d_ptr->pressOffset;
    d_ptr->preview(released, d_ptr->valueAt(axis));
    if (!guard) { event->accept(); return; }
    d_ptr->coincidentPress = false;
    if (d_ptr->pressed != released) { event->accept(); return; }
    if (d_ptr->snapMode == SnapMode::SnapOnRelease) {
        const int snapped = d_ptr->snapped(released == Handle::LowerHandle
            ? d_ptr->lowerPosition : d_ptr->upperPosition);
        d_ptr->preview(released, snapped);
        if (!guard) { event->accept(); return; }
        if (d_ptr->pressed != released) { event->accept(); return; }
    }
    d_ptr->pressed = Handle::NoHandle;
    d_ptr->commit(d_ptr->lowerPosition, d_ptr->upperPosition, true);
    if (!guard) { event->accept(); return; }
    d_ptr->lowerPosition = d_ptr->lower;
    d_ptr->upperPosition = d_ptr->upper;
    d_ptr->hideTip();
    d_ptr->animateHandle(released);
    Q_EMIT sliderReleased(released);
    event->accept();
}

void ZzRangeSlider::wheelEvent(QWheelEvent *event)
{
    if (!isEnabled() || event->angleDelta().y() == 0) {
        event->ignore();
        return;
    }
    const std::int64_t delta = event->angleDelta().y() > 0
        ? d_ptr->singleStep : -static_cast<std::int64_t>(d_ptr->singleStep);
    QPointer<ZzRangeSlider> guard(this);
    if (d_ptr->active == Handle::UpperHandle)
        setUpperValue(movedValue(d_ptr->upper, delta, d_ptr->lower, d_ptr->maximum));
    else
        setLowerValue(movedValue(d_ptr->lower, delta, d_ptr->minimum, d_ptr->upper));
    if (!guard) { event->accept(); return; }
    Q_EMIT sliderMoved(d_ptr->lowerPosition, d_ptr->upperPosition);
    event->accept();
}

void ZzRangeSlider::keyPressEvent(QKeyEvent *event)
{
    const bool horizontal = d_ptr->orientation == Qt::Horizontal;
    std::int64_t delta = 0;
    QPointer<ZzRangeSlider> guard(this);
    Handle select = Handle::NoHandle;
    switch (event->key()) {
    case Qt::Key_Left:
        if (horizontal) delta = layoutDirection() == Qt::RightToLeft
            ? d_ptr->singleStep : -static_cast<std::int64_t>(d_ptr->singleStep);
        else select = Handle::LowerHandle;
        break;
    case Qt::Key_Right:
        if (horizontal) delta = layoutDirection() == Qt::RightToLeft
            ? -static_cast<std::int64_t>(d_ptr->singleStep) : d_ptr->singleStep;
        else select = Handle::UpperHandle;
        break;
    case Qt::Key_Up:
        if (horizontal) select = Handle::UpperHandle;
        else delta = d_ptr->singleStep;
        break;
    case Qt::Key_Down:
        if (horizontal) select = Handle::LowerHandle;
        else delta = -static_cast<std::int64_t>(d_ptr->singleStep);
        break;
    case Qt::Key_PageUp: delta = d_ptr->pageStep; break;
    case Qt::Key_PageDown: delta = -static_cast<std::int64_t>(d_ptr->pageStep); break;
    case Qt::Key_Home:
        if (d_ptr->active == Handle::UpperHandle) setUpperValue(d_ptr->lower);
        else setLowerValue(d_ptr->minimum);
        if (!guard) { event->accept(); return; }
        Q_EMIT sliderMoved(d_ptr->lowerPosition, d_ptr->upperPosition);
        event->accept(); return;
    case Qt::Key_End:
        if (d_ptr->active == Handle::UpperHandle) setUpperValue(d_ptr->maximum);
        else setLowerValue(d_ptr->upper);
        if (!guard) { event->accept(); return; }
        Q_EMIT sliderMoved(d_ptr->lowerPosition, d_ptr->upperPosition);
        event->accept(); return;
    default: QWidget::keyPressEvent(event); return;
    }
    if (select != Handle::NoHandle) {
        d_ptr->active = select;
        d_ptr->accessibleFocusChanged();
        if (!guard) return;
        update();
    } else if (delta != 0) {
        if (d_ptr->active == Handle::UpperHandle)
            setUpperValue(movedValue(d_ptr->upper, delta, d_ptr->lower, d_ptr->maximum));
        else
            setLowerValue(movedValue(d_ptr->lower, delta, d_ptr->minimum, d_ptr->upper));
        if (!guard) { event->accept(); return; }
        Q_EMIT sliderMoved(d_ptr->lowerPosition, d_ptr->upperPosition);
    }
    event->accept();
}

void ZzRangeSlider::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    update();
    d_ptr->accessibleFocusChanged();
}
void ZzRangeSlider::focusOutEvent(QFocusEvent *event)
{
    QPointer<ZzRangeSlider> guard(this);
    d_ptr->cancelDrag();
    if (!guard) return;
    QWidget::focusOutEvent(event);
    update();
}
void ZzRangeSlider::hideEvent(QHideEvent *event)
{
    QPointer<ZzRangeSlider> guard(this);
    d_ptr->hovered = Handle::NoHandle;
    d_ptr->cancelDrag();
    if (!guard) return;
    QWidget::hideEvent(event);
}
void ZzRangeSlider::changeEvent(QEvent *event)
{
    QPointer<ZzRangeSlider> guard(this);
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange && !isEnabled()) {
        d_ptr->hovered = Handle::NoHandle;
        d_ptr->cancelDrag();
        if (!guard) return;
    }
    if (event->type() == QEvent::LayoutDirectionChange) {
        d_ptr->cancelDrag();
        if (!guard) return;
    }
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::StyleChange
        || event->type() == QEvent::LayoutDirectionChange
        || event->type() == QEvent::ThemeChange) {
        // 动效偏好经 StyleChange 通知，立即停下已有动画并落到目标状态。
        d_ptr->animateHandle(Handle::LowerHandle);
        d_ptr->animateHandle(Handle::UpperHandle);
        if (d_ptr->pressed != Handle::NoHandle) d_ptr->showTip();
        update();
    }
}
bool ZzRangeSlider::event(QEvent *event)
{
    if (event->type() == QEvent::HoverMove) {
        const auto *hover = static_cast<QHoverEvent *>(event);
        const Handle next = d_ptr->handleAt(hover->position());
        if (next != d_ptr->hovered) {
            const Handle previous = d_ptr->hovered;
            d_ptr->hovered = next;
            d_ptr->animateHandle(previous);
            d_ptr->animateHandle(next);
        }
    } else if (event->type() == QEvent::HoverLeave) {
        const Handle previous = d_ptr->hovered;
        d_ptr->hovered = Handle::NoHandle;
        d_ptr->animateHandle(previous);
    }
    return QWidget::event(event);
}
} // namespace ZzFluentUI
