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
    animation->setDuration(indeterminateDuration);
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
    QObject::disconnect(centerDestroyedConnection);
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

void ZzProgressRingPrivate::setIndeterminateDuration(int milliseconds)
{
    const qreal previousPhase = phase;
    indeterminateDuration = milliseconds;
    animation->setDuration(milliseconds);
    if (animation->state() != QAbstractAnimation::Stopped) {
        // 归一化相位映射到新周期；不重启或在循环边界跳回零度。
        animation->setCurrentTime(std::min(milliseconds - 1, qRound(previousPhase * milliseconds)));
    }
}

QFont ZzProgressRingPrivate::valueFont() const
{
    if (customValueFont != QFont()) {
        return customValueFont;
    }
    QFont result = q_ptr->font();
    if (!q_ptr->testAttribute(Qt::WA_SetFont)) {
        constexpr qreal valueExtentRatio = 0.18;
        constexpr int minimumValuePixels = 12;
        constexpr int maximumValuePixels = 32;
        const QRect content = q_ptr->contentsRect();
        result.setPixelSize(std::clamp(
            qRound(std::min(content.width(), content.height()) * valueExtentRatio),
            minimumValuePixels, maximumValuePixels));
        result.setWeight(QFont::DemiBold);
    }
    return result;
}

QFont ZzProgressRingPrivate::titleFont() const
{
    if (customTitleFont != QFont()) {
        return customTitleFont;
    }
    QFont result = q_ptr->font();
    if (!q_ptr->testAttribute(Qt::WA_SetFont)) {
        result.setPixelSize(std::clamp(qRound(
            std::min(q_ptr->width(), q_ptr->height()) * 0.09), 9, 14));
    }
    return result;
}

} // namespace ZzFluentUI
