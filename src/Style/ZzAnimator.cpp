#include "ZzPureTools/Style/ZzAnimator.hpp"

#include <QVariantAnimation>

ZzAnimator::ZzAnimator(QObject* parent)
    : QObject(parent),
      m_hoverAnimation(new QVariantAnimation(this)),
      m_pressAnimation(new QVariantAnimation(this))
{
    configureAnimation(m_hoverAnimation, m_hoverProgress);
    configureAnimation(m_pressAnimation, m_pressProgress);
    setDuration(zzDefaultMetricTokens().animationDurationMs);
}

void ZzAnimator::setHovered(bool hovered)
{
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(hovered ? 1.0 : 0.0);
    m_hoverAnimation->start();
}

void ZzAnimator::setPressed(bool pressed)
{
    m_pressAnimation->stop();
    m_pressAnimation->setStartValue(m_pressProgress);
    m_pressAnimation->setEndValue(pressed ? 1.0 : 0.0);
    m_pressAnimation->start();
}

void ZzAnimator::reset()
{
    m_hoverAnimation->stop();
    m_pressAnimation->stop();
    m_hoverProgress = 0.0;
    m_pressProgress = 0.0;
    emit updated();
}

qreal ZzAnimator::hoverProgress() const noexcept
{
    return m_hoverProgress;
}

qreal ZzAnimator::pressProgress() const noexcept
{
    return m_pressProgress;
}

ZzControlState ZzAnimator::resolvedState(bool enabled) const
{
    if (!enabled) {
        return ZzControlState::Disabled;
    }
    if (m_pressProgress > 0.5) {
        return ZzControlState::Pressed;
    }
    if (m_hoverProgress > 0.01) {
        return ZzControlState::Hovered;
    }
    return ZzControlState::Normal;
}

void ZzAnimator::setDuration(int durationMs)
{
    m_hoverAnimation->setDuration(durationMs);
    m_pressAnimation->setDuration(durationMs);
}

void ZzAnimator::configureAnimation(QVariantAnimation* animation, qreal& progress)
{
    animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QVariantAnimation::valueChanged, this,
            [this, &progress](const QVariant& value) {
                progress = value.toReal();
                emit updated();
            });
}
