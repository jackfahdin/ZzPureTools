#include "ZzRangeSliderPrivate.h"
#include "ZzRangeSliderAccessible.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QPainter>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

namespace ZzFluentUI {
namespace {
using Handle = ZzRangeSlider::Handle;
constexpr qreal kHalf = 10.0;
constexpr qreal kNormal = 9.0 * 0.55;
class ZzRangeSliderTipLabel final : public QLabel
{
public:
    explicit ZzRangeSliderTipLabel(QWidget *parent)
        : QLabel(parent, Qt::ToolTip | Qt::FramelessWindowHint
            | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus)
    {
        setObjectName(QStringLiteral("zzRangeSliderValueTip"));
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        setAlignment(Qt::AlignCenter);
        setFrameShape(QFrame::NoFrame);
        setMargin(1 + style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth));
    }
protected:
    void paintEvent(QPaintEvent *event) override
    {
        {
            QPainter painter(this);
            QStyleOption option;
            option.initFrom(this);
            style()->drawPrimitive(QStyle::PE_PanelTipLabel, &option, &painter, this);
        }
        QLabel::paintEvent(event);
    }
};
int bound(std::int64_t v, int low, int high)
{
    return static_cast<int>(std::clamp(v,
        static_cast<std::int64_t>(low), static_cast<std::int64_t>(high)));
}
}

qreal ZzRangeSliderPrivate::axisPosition(int value) const
{
    const qreal length = qMax<qreal>(0, (orientation == Qt::Horizontal
        ? q->width() : q->height()) - 2 * kHalf);
    const std::int64_t span = static_cast<std::int64_t>(maximum) - minimum;
    if (length <= 0 || span == 0) return kHalf;
    qreal fraction = static_cast<qreal>(
        static_cast<std::int64_t>(value) - minimum) / static_cast<qreal>(span);
    if (orientation == Qt::Vertical || q->layoutDirection() == Qt::RightToLeft)
        fraction = 1.0 - fraction;
    return kHalf + fraction * length;
}

int ZzRangeSliderPrivate::valueAt(qreal position) const
{
    const qreal length = qMax<qreal>(0, (orientation == Qt::Horizontal
        ? q->width() : q->height()) - 2 * kHalf);
    if (length <= 0 || minimum == maximum) return minimum;
    qreal fraction = std::clamp((position - kHalf) / length, 0.0, 1.0);
    if (orientation == Qt::Vertical || q->layoutDirection() == Qt::RightToLeft)
        fraction = 1.0 - fraction;
    const std::int64_t span = static_cast<std::int64_t>(maximum) - minimum;
    return bound(static_cast<std::int64_t>(minimum)
        + std::llround(fraction * static_cast<qreal>(span)), minimum, maximum);
}

int ZzRangeSliderPrivate::snapped(int value) const
{
    const std::int64_t step = qMax(1, singleStep);
    const std::int64_t offset = static_cast<std::int64_t>(value) - minimum;
    return bound(static_cast<std::int64_t>(minimum)
        + ((offset + step / 2) / step) * step, minimum, maximum);
}

QPointF ZzRangeSliderPrivate::center(int value) const
{
    const qreal axis = axisPosition(value);
    return orientation == Qt::Horizontal
        ? QPointF(axis, q->height() / 2.0)
        : QPointF(q->width() / 2.0, axis);
}

Handle ZzRangeSliderPrivate::nearestHandle(const QPointF &point) const
{
    const qreal axis = orientation == Qt::Horizontal ? point.x() : point.y();
    const qreal lowDistance = std::abs(axis - axisPosition(lowerPosition));
    const qreal highDistance = std::abs(axis - axisPosition(upperPosition));
    if (!qFuzzyCompare(lowDistance + 1.0, highDistance + 1.0))
        return lowDistance < highDistance ? Handle::LowerHandle : Handle::UpperHandle;
    const int requested = valueAt(axis);
    if (requested < lowerPosition) return Handle::LowerHandle;
    if (requested > upperPosition) return Handle::UpperHandle;
    return active == Handle::NoHandle ? Handle::LowerHandle : active;
}

Handle ZzRangeSliderPrivate::handleAt(const QPointF &point) const
{
    const QPointF a = point - center(lowerPosition);
    const QPointF b = point - center(upperPosition);
    const qreal da = QPointF::dotProduct(a, a);
    const qreal db = QPointF::dotProduct(b, b);
    const bool first = da <= kHalf * kHalf;
    const bool second = db <= kHalf * kHalf;
    if (first && second)
        return qFuzzyCompare(da + 1.0, db + 1.0)
            ? nearestHandle(point) : da < db ? Handle::LowerHandle : Handle::UpperHandle;
    if (first) return Handle::LowerHandle;
    if (second) return Handle::UpperHandle;
    return Handle::NoHandle;
}

void ZzRangeSliderPrivate::commit(int requestedLower, int requestedUpper, bool user)
{
    int nextLower = bound(requestedLower, minimum, maximum);
    int nextUpper = bound(requestedUpper, minimum, maximum);
    if (nextLower > nextUpper) std::swap(nextLower, nextUpper);
    const bool changedLower = lower != nextLower;
    const bool changedUpper = upper != nextUpper;
    if (!changedLower && !changedUpper) return;
    lower = nextLower;
    upper = nextUpper;
    if (!user || pressed == Handle::NoHandle) {
        lowerPosition = lower;
        upperPosition = upper;
    }
    QPointer<ZzRangeSlider> guard(q);
    notifyValues();
    if (!guard) return;
    q->update();
}

void ZzRangeSliderPrivate::notifyValues()
{
    if (notifying) return;
    notifying = true;
    QPointer<ZzRangeSlider> guard(q);
    while (guard) {
        if (notifiedLower != lower) {
            const int value = lower;
            notifiedLower = value;
            Q_EMIT q->lowerValueChanged(value);
            if (!guard) return;
            accessibleValueChanged(Handle::LowerHandle, value);
            if (!guard) return;
            continue;
        }
        if (notifiedUpper != upper) {
            const int value = upper;
            notifiedUpper = value;
            Q_EMIT q->upperValueChanged(value);
            if (!guard) return;
            accessibleValueChanged(Handle::UpperHandle, value);
            if (!guard) return;
            continue;
        }
        if (notifiedPairLower != lower || notifiedPairUpper != upper) {
            notifiedPairLower = lower;
            notifiedPairUpper = upper;
            Q_EMIT q->valuesChanged(lower, upper);
            if (!guard) return;
            continue;
        }
        break;
    }
    notifying = false;
}

void ZzRangeSliderPrivate::preview(Handle handle, int value)
{
    if (handle == Handle::NoHandle) return;
    if (snapMode == ZzRangeSlider::SnapMode::SnapAlways) value = snapped(value);
    value = handle == Handle::LowerHandle
        ? bound(value, minimum, upperPosition)
        : bound(value, lowerPosition, maximum);
    int &position = handle == Handle::LowerHandle ? lowerPosition : upperPosition;
    if (position == value) return;
    position = value;
    QPointer<ZzRangeSlider> guard(q);
    if (tracking) {
        if (handle == Handle::LowerHandle) commit(value, upper, true);
        else commit(lower, value, true);
    }
    if (!guard) return;
    if (pressed != handle || position != value) return;
    Q_EMIT q->sliderMoved(lowerPosition, upperPosition);
    if (!guard) return;
    showTip();
    q->update();
}

void ZzRangeSliderPrivate::cancelDrag()
{
    ++dragCancellation;
    QPointer<ZzRangeSlider> guard(q);
    if (pressed != Handle::NoHandle) {
        const Handle released = pressed;
        pressed = Handle::NoHandle;
        coincidentPress = false;
        lowerPosition = lower;
        upperPosition = upper;
        Q_EMIT q->sliderReleased(released);
        if (!guard) return;
    }
    hideTip();
    animateHandle(Handle::LowerHandle);
    animateHandle(Handle::UpperHandle);
    q->update();
}

void ZzRangeSliderPrivate::showTip()
{
    if (!valueTip || pressed == Handle::NoHandle
        || !q->isVisible() || !q->isEnabled()) return;
    if (!tip) {
        tip = new ZzRangeSliderTipLabel(q);
    }
    const int value = pressed == Handle::LowerHandle ? lowerPosition : upperPosition;
    tip->setText(QString::number(value));
    tip->setPalette(q->palette());
    tip->setFont(q->font());
    tip->setMargin(1 + q->style()->pixelMetric(
        QStyle::PM_ToolTipLabelFrameWidth, nullptr, q));
    tip->adjustSize();
    const QPoint point = q->mapToGlobal(center(value).toPoint());
    QPoint anchor = orientation == Qt::Horizontal
        ? QPoint(point.x() - tip->width() / 2, point.y() - 13 - tip->height())
        : QPoint(point.x() - 13 - tip->width(), point.y() - tip->height() / 2);
    QScreen *screen = QGuiApplication::screenAt(point);
    if (!screen) screen = q->screen();
    if (screen) {
        const QRect bounds = screen->availableGeometry();
        if (orientation == Qt::Horizontal && anchor.y() < bounds.top())
            anchor.setY(point.y() + 13);
        if (orientation == Qt::Vertical && anchor.x() < bounds.left())
            anchor.setX(point.x() + 13);
        anchor.setX(std::clamp(anchor.x(), bounds.left(),
            qMax(bounds.left(), bounds.right() - tip->width() + 1)));
        anchor.setY(std::clamp(anchor.y(), bounds.top(),
            qMax(bounds.top(), bounds.bottom() - tip->height() + 1)));
    }
    tip->move(anchor);
    tip->show();
}

void ZzRangeSliderPrivate::hideTip()
{
    if (tip) tip->hide();
}

void ZzRangeSliderPrivate::animateHandle(Handle handle)
{
    if (handle == Handle::NoHandle) return;
    QVariantAnimation *&animation = handle == Handle::LowerHandle
        ? lowerAnimation : upperAnimation;
    if (!animation) {
        animation = new QVariantAnimation(q);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(animation, &QVariantAnimation::valueChanged,
            q, [this] { q->update(); });
    }
    const qreal current = animation->currentValue().isValid()
        ? animation->currentValue().toReal() : kNormal;
    const qreal target = !q->isEnabled() ? kNormal
        : pressed == handle ? 9.0 * 0.40
        : hovered == handle ? 9.0 * 0.65 : kNormal;
    animation->stop();
    animation->setStartValue(current);
    animation->setEndValue(target);
    const bool animate = !qFuzzyCompare(current, target) && q->isVisible() && q->isEnabled()
        && q->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, q);
    animation->setDuration(animate ? 300 : 0);
    animation->start();
    q->update();
}

qreal ZzRangeSliderPrivate::innerRadius(Handle handle) const
{
    const QVariantAnimation *animation = handle == Handle::LowerHandle
        ? lowerAnimation : upperAnimation;
    return animation && animation->currentValue().isValid()
        ? animation->currentValue().toReal() : kNormal;
}

void ZzRangeSliderPrivate::accessibleValueChanged(Handle handle, int value)
{ zzRangeSliderAccessibleValueChanged(q, handle, value); }
void ZzRangeSliderPrivate::accessibleFocusChanged()
{ zzRangeSliderAccessibleFocusChanged(q); }
} // namespace ZzFluentUI
