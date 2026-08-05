#include "ZzProgressRingPrivate.h"

#include <algorithm>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QEasingCurve>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QVariantAnimation>
#include <QtWidgets/QStyle>

#include <ZzFluentUI/ZzProgressRing.h>

namespace ZzFluentUI {

ZzProgressRingPrivate::ZzProgressRingPrivate(ZzProgressRing *q)
    : q_ptr(q)
    , animation(new QVariantAnimation(q))
{
    Q_ASSERT(q_ptr != nullptr);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setDuration(1200);
    animation->setLoopCount(-1);
    animation->setEasingCurve(QEasingCurve::Linear);
    QObject::connect(
        animation,
        &QVariantAnimation::valueChanged,
        q_ptr,
        [this](const QVariant &value) {
            phase = std::clamp(value.toReal(), 0.0, 1.0);
            q_ptr->update();
        });
}

ZzProgressRingPrivate::~ZzProgressRingPrivate()
{
    animation->stop();
    QObject::disconnect(animation, nullptr, q_ptr, nullptr);
}

void ZzProgressRingPrivate::syncAnimation()
{
    const bool shouldAnimate = q_ptr->isVisible()
        && q_ptr->isEnabled()
        && isIndeterminate()
        && q_ptr->style() != nullptr
        && q_ptr->style()->styleHint(
               QStyle::SH_Widget_Animate,
               nullptr,
               q_ptr)
            != 0;
    if (!shouldAnimate) {
        stopAnimation();
        return;
    }
    if (animation->state() != QAbstractAnimation::Running) {
        animation->start();
    }
}

void ZzProgressRingPrivate::stopAnimation() noexcept
{
    animation->stop();
    if (!qFuzzyIsNull(phase)) {
        phase = 0.0;
        q_ptr->update();
    }
}

bool ZzProgressRingPrivate::isIndeterminate() const noexcept
{
    return q_ptr->minimum() == 0 && q_ptr->maximum() == 0;
}

} // namespace ZzFluentUI
