#include <ZzFluentUI/ZzRadialGauge.h>

#include "private/ZzRadialGaugePrivate.h"
#include "private/ZzGaugeSupport_p.h"

#include <QEvent>
#include <QEasingCurve>
#include <QPointer>
#include <QSizePolicy>
#include <QVariantAnimation>
#include <QAbstractAnimation>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>
#include <utility>

namespace ZzFluentUI {
namespace {
constexpr qreal FullCircle = 360.0;
qreal circularDistance(qreal first, qreal second)
{
    return std::abs(std::remainder(first - second, FullCircle));
}
} // namespace

ZzRadialGaugeRange::ZzRadialGaugeRange(QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<ZzRadialGaugeRangePrivate>())
{
}

ZzRadialGaugeRange::ZzRadialGaugeRange(int fromValue, int toValue, const QColor &color, QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<ZzRadialGaugeRangePrivate>())
{
    d_ptr->fromValue = fromValue;
    d_ptr->toValue = toValue;
    d_ptr->color = color;
}

ZzRadialGauge::ZzRadialGauge(QWidget *parent) : QDial(parent), d_ptr(std::make_unique<ZzRadialGaugePrivate>())
{

    setWrapping(false);
    setTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    d_ptr->valueAnimation = new QVariantAnimation(this);
    d_ptr->valueAnimation->setDuration(d_ptr->valueAnimationDuration);
    d_ptr->valueAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(d_ptr->valueAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &animatedValue) {
                d_ptr->animationValue = qRound(animatedValue.toReal());
                d_ptr->animationWrite = true;
                QPointer<ZzRadialGauge> guard(this);
                QDial::setValue(d_ptr->animationValue);
                if (guard)
                    d_ptr->animationWrite = false;
            });
    connect(d_ptr->valueAnimation, &QVariantAnimation::finished, this,
            [this] { QDial::setValue(d_ptr->valueAnimation->endValue().toInt()); });
    connect(this, &QDial::rangeChanged, this, [this] { d_ptr->valueAnimation->stop(); });
}

ZzRadialGauge::~ZzRadialGauge()
{
    d_ptr->valueAnimation->stop();
    for (ZzRadialGaugeRange *range : std::as_const(d_ptr->ranges)) {
        disconnect(range, nullptr, this, nullptr);
    }
    d_ptr->ranges.clear();
}

QSize ZzRadialGauge::sizeHint() const
{
    return QSize(240, 240);
}

QSize ZzRadialGauge::minimumSizeHint() const
{
    return QSize(100, 100);
}

QList<ZzRadialGaugeRange *> ZzRadialGauge::ranges() const
{
    return d_ptr->ranges;
}

ZzRadialGaugeRange *ZzRadialGauge::addRange(int fromValue, int toValue, const QColor &color)
{
    auto *range = new ZzRadialGaugeRange(fromValue, toValue, color, this);
    d_ptr->ranges.append(range);

    connect(range, &ZzRadialGaugeRange::rangeChanged, this, [this] {
        update();
        emit rangesChanged();
    });
    connect(range, &QObject::destroyed, this, [this, range] {
        if (d_ptr->ranges.removeOne(range)) {
            update();
            emit rangesChanged();
        }
    });

    update();
    QPointer<ZzRadialGaugeRange> guard(range);
    emit rangesChanged();
    return guard.data();
}

void ZzRadialGauge::removeRange(ZzRadialGaugeRange *range)
{
    if (!range || !d_ptr->ranges.removeOne(range)) {
        return;
    }

    disconnect(range, nullptr, this, nullptr);
    range->deleteLater();
    update();
    emit rangesChanged();
}

void ZzRadialGauge::clearRanges()
{
    if (d_ptr->ranges.isEmpty()) {
        return;
    }

    const auto oldRanges = d_ptr->ranges;
    d_ptr->ranges.clear();
    for (ZzRadialGaugeRange *range : oldRanges) {
        disconnect(range, nullptr, this, nullptr);
        range->deleteLater();
    }

    update();
    emit rangesChanged();
}

void ZzRadialGauge::mousePressEvent(QMouseEvent *event)
{
    if (!d_ptr->interactive || !isEnabled() || event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    d_ptr->valueAnimation->stop();
    setFocus(Qt::MouseFocusReason);
    QPointer<ZzRadialGauge> guard(this);
    setSliderDown(true);
    if (!guard)
        return;
    updatePositionFromPoint(event->position());
    event->accept();
}

void ZzRadialGauge::mouseMoveEvent(QMouseEvent *event)
{
    if (!d_ptr->interactive || !isEnabled() || !isSliderDown()) {
        event->ignore();
        return;
    }

    updatePositionFromPoint(event->position());
    event->accept();
}

void ZzRadialGauge::mouseReleaseEvent(QMouseEvent *event)
{
    QPointer<ZzRadialGauge> guard(this);
    if (!d_ptr->interactive || !isEnabled() || event->button() != Qt::LeftButton || !isSliderDown()) {
        event->ignore();
        return;
    }

    updatePositionFromPoint(event->position());
    if (!guard)
        return;
    if (!hasTracking()) {
        setValue(sliderPosition());
        if (!guard)
            return;
    }
    setSliderDown(false);
    event->accept();
}

void ZzRadialGauge::wheelEvent(QWheelEvent *event)
{
    if (!d_ptr->interactive) {
        event->ignore();
        return;
    }

    d_ptr->valueAnimation->stop();
    QDial::wheelEvent(event);
}

void ZzRadialGauge::keyPressEvent(QKeyEvent *event)
{
    if (!d_ptr->interactive) {
        event->ignore();
        return;
    }

    d_ptr->valueAnimation->stop();
    QDial::keyPressEvent(event);
}

void ZzRadialGauge::setValue(int targetValue)
{
    targetValue = qBound(minimum(), targetValue, maximum());
    if (!d_ptr->valueAnimation || d_ptr->valueAnimationDuration == 0 || !zzGaugeMotionAllowed(this) ||
        isSliderDown()) {
        if (d_ptr->valueAnimation) {
            d_ptr->valueAnimation->stop();
        }
        QDial::setValue(targetValue);
        return;
    }

    if (value() == targetValue) {
        d_ptr->valueAnimation->stop();
        return;
    }

    const qreal startValue = value();
    auto *animation = d_ptr->valueAnimation;
    {
        // 关键帧配置在 Stopped 状态也会重算值；完整配置前不得发布业务数值。
        const QSignalBlocker blocker(animation);
        animation->stop();
        animation->setDuration(d_ptr->valueAnimationDuration);
        animation->setStartValue(startValue);
        animation->setEndValue(static_cast<qreal>(targetValue));
        animation->setCurrentTime(0);
    }
    animation->start();
}

bool ZzRadialGauge::isValueAnimating() const
{
    return d_ptr->valueAnimation && d_ptr->valueAnimation->state() == QAbstractAnimation::Running;
}

void ZzRadialGauge::settleValueAnimation()
{
    if (!isValueAnimating())
        return;
    const int target = d_ptr->valueAnimation->endValue().toInt();
    d_ptr->valueAnimation->stop();
    QDial::setValue(target);
}

void ZzRadialGauge::sliderChange(SliderChange change)
{
    if (d_ptr && d_ptr->valueAnimation &&
        (change == SliderRangeChange ||
         (change == SliderValueChange && (!d_ptr->animationWrite || value() != d_ptr->animationValue)))) {
        d_ptr->valueAnimation->stop();
    }
    QDial::sliderChange(change);
}

bool ZzRadialGauge::event(QEvent *event)
{
    QPointer<ZzRadialGauge> guard(this);
    const bool result = QDial::event(event);
    if (guard && d_ptr && d_ptr->valueAnimation &&
        (event->type() == QEvent::Hide || event->type() == QEvent::EnabledChange ||
         event->type() == QEvent::StyleChange) &&
        !zzGaugeMotionAllowed(this)) {
        settleValueAnimation();
    }
    if (guard && d_ptr && event->type() == QEvent::ChildRemoved) {
        const auto *removed = static_cast<QChildEvent *>(event);
        for (auto *range : std::as_const(d_ptr->ranges)) {
            if (range == removed->child()) {
                d_ptr->ranges.removeOne(range);
                disconnect(range, nullptr, this, nullptr);
                update();
                // setParent 返回前同步删除 Range 会破坏 QObject 内部父子关系更新。
                QMetaObject::invokeMethod(this, [this] { emit rangesChanged(); }, Qt::QueuedConnection);
                break;
            }
        }
    }
    return result;
}

qreal ZzRadialGauge::sweepAngle() const
{
    qreal sweep = std::fmod(d_ptr->maximumAngle - d_ptr->minimumAngle, FullCircle);
    if (sweep <= 0.0) {
        sweep += FullCircle;
    }
    return sweep;
}

qreal ZzRadialGauge::valueFraction(qreal value) const
{
    const qreal range = static_cast<qreal>(maximum()) - static_cast<qreal>(minimum());
    if (range <= 0.0) {
        return 0.0;
    }

    qreal fraction = (value - static_cast<qreal>(minimum())) / range;
    fraction = qBound(0.0, fraction, 1.0);
    return invertedAppearance() ? 1.0 - fraction : fraction;
}

qreal ZzRadialGauge::positionFraction() const
{
    return valueFraction(sliderPosition());
}

int ZzRadialGauge::positionFromPoint(const QPointF &point) const
{
    const QPointF center = QRectF(rect()).center();
    const qreal deltaX = point.x() - center.x();
    const qreal deltaY = point.y() - center.y();
    if (qFuzzyIsNull(deltaX) && qFuzzyIsNull(deltaY)) {
        return sliderPosition();
    }

    qreal angle = qRadiansToDegrees(std::atan2(deltaX, -deltaY));
    while (angle < d_ptr->minimumAngle) {
        angle += FullCircle;
    }
    while (angle >= d_ptr->minimumAngle + FullCircle) {
        angle -= FullCircle;
    }

    const qreal sweep = sweepAngle();
    const qreal endAngle = d_ptr->minimumAngle + sweep;
    if (qAbs(sweep - FullCircle) < 0.0001 && qAbs(angle - d_ptr->minimumAngle) < 0.0001 &&
        positionFraction() > 0.5) {
        angle = endAngle;
    }

    if (angle > endAngle) {
        const qreal minimumDistance = circularDistance(angle, d_ptr->minimumAngle);
        const qreal maximumDistance = circularDistance(angle, endAngle);
        if (qAbs(minimumDistance - maximumDistance) < 0.0001) {
            angle = positionFraction() < 0.5 ? d_ptr->minimumAngle : endAngle;
        } else {
            angle = minimumDistance < maximumDistance ? d_ptr->minimumAngle : endAngle;
        }
    }

    qreal fraction = (angle - d_ptr->minimumAngle) / sweep;
    if (invertedAppearance()) {
        fraction = 1.0 - fraction;
    }

    const qint64 range = static_cast<qint64>(maximum()) - static_cast<qint64>(minimum());
    const qint64 position = static_cast<qint64>(minimum()) + qRound64(fraction * static_cast<qreal>(range));
    return static_cast<int>(qBound(static_cast<qint64>(minimum()), position, static_cast<qint64>(maximum())));
}

void ZzRadialGauge::updatePositionFromPoint(const QPointF &point)
{
    setSliderPosition(positionFromPoint(point));
}

ZzRadialGaugeRange::~ZzRadialGaugeRange() = default;

} // namespace ZzFluentUI
