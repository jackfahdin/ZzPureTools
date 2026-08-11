#include "ZzSelectionIndicatorTransition.h"

#include <algorithm>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QEasingCurve>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QVariantAnimation>

namespace ZzFluentUI {

namespace {

constexpr qreal zzHalfProgress = 0.5;

} // namespace

ZzSelectionIndicatorTransition::ZzSelectionIndicatorTransition(
    QObject *owner)
    : animation_(new QVariantAnimation(owner))
{
    Q_ASSERT(owner != nullptr);
    animation_->setStartValue(0.0);
    animation_->setEndValue(1.0);
    animation_->setEasingCurve(QEasingCurve::InOutSine);
    QObject::connect(
        animation_,
        &QVariantAnimation::valueChanged,
        owner,
        [this](const QVariant &value) {
            updateScales(value.toReal());
        });
    QObject::connect(
        animation_,
        &QVariantAnimation::finished,
        owner,
        [this] {
            outgoingIndex_ = QPersistentModelIndex();
            outgoingStartScale_ = 0.0;
            outgoingScale_ = 0.0;
            incomingStartScale_ = incomingIndex_.isValid() ? 1.0 : 0.0;
            incomingScale_ = incomingStartScale_;
        });
}

void ZzSelectionIndicatorTransition::transitionTo(
    const QModelIndex &target,
    int durationMilliseconds)
{
    if (animation_->state() == QAbstractAnimation::Stopped
        && QPersistentModelIndex(target) == incomingIndex_) {
        return;
    }

    const QPersistentModelIndex previousOutgoing = outgoingIndex_;
    const QPersistentModelIndex previousIncoming = incomingIndex_;
    const qreal previousOutgoingScale = outgoingScale_;
    const qreal previousIncomingScale = incomingScale_;
    const qreal targetScale = trackedScale(target);
    animation_->stop();

    outgoingIndex_ = QPersistentModelIndex();
    outgoingStartScale_ = 0.0;
    const auto considerOutgoing = [this, &target](
                                      const QPersistentModelIndex &candidate,
                                      qreal scale) {
        if (!candidate.isValid() || candidate == target || scale <= 0.0
            || scale <= outgoingStartScale_) {
            return;
        }
        outgoingIndex_ = candidate;
        outgoingStartScale_ = scale;
    };
    considerOutgoing(previousOutgoing, previousOutgoingScale);
    considerOutgoing(previousIncoming, previousIncomingScale);

    incomingIndex_ = target;
    incomingStartScale_ = std::clamp(targetScale, 0.0, 1.0);
    outgoingScale_ = outgoingStartScale_;
    incomingScale_ = incomingStartScale_;

    if (durationMilliseconds <= 0
        || (!outgoingIndex_.isValid() && incomingStartScale_ <= 0.0)) {
        outgoingIndex_ = QPersistentModelIndex();
        outgoingStartScale_ = 0.0;
        outgoingScale_ = 0.0;
        incomingStartScale_ = incomingIndex_.isValid() ? 1.0 : 0.0;
        incomingScale_ = incomingStartScale_;
        return;
    }

    animation_->setDuration(durationMilliseconds);
    animation_->start();
}

void ZzSelectionIndicatorTransition::finish()
{
    if (animation_->state() != QAbstractAnimation::Stopped) {
        animation_->stop();
    }
    outgoingIndex_ = QPersistentModelIndex();
    outgoingStartScale_ = 0.0;
    outgoingScale_ = 0.0;
    incomingStartScale_ = incomingIndex_.isValid() ? 1.0 : 0.0;
    incomingScale_ = incomingStartScale_;
}

qreal ZzSelectionIndicatorTransition::scaleFor(
    const QModelIndex &index,
    bool staticallySelected) const noexcept
{
    if (index == outgoingIndex_) {
        return outgoingScale_;
    }
    if (index == incomingIndex_) {
        return incomingScale_;
    }
    return staticallySelected ? 1.0 : 0.0;
}

bool ZzSelectionIndicatorTransition::forcesIndicator(
    const QModelIndex &index) const noexcept
{
    return index.isValid() && index == outgoingIndex_
        && outgoingScale_ > 0.0;
}

QModelIndex ZzSelectionIndicatorTransition::outgoingIndex() const
{
    return outgoingIndex_;
}

QModelIndex ZzSelectionIndicatorTransition::incomingIndex() const
{
    return incomingIndex_;
}

QVariantAnimation *ZzSelectionIndicatorTransition::animation() const noexcept
{
    return animation_;
}

void ZzSelectionIndicatorTransition::updateScales(qreal progress) noexcept
{
    const qreal bounded = std::clamp(progress, 0.0, 1.0);
    if (bounded <= zzHalfProgress) {
        outgoingScale_ = outgoingStartScale_
            * (1.0 - (bounded / zzHalfProgress));
        incomingScale_ = incomingStartScale_;
        return;
    }
    outgoingScale_ = 0.0;
    const qreal incomingProgress = (bounded - zzHalfProgress)
        / zzHalfProgress;
    incomingScale_ = incomingStartScale_
        + ((1.0 - incomingStartScale_) * incomingProgress);
}

qreal ZzSelectionIndicatorTransition::trackedScale(
    const QModelIndex &index) const noexcept
{
    if (!index.isValid()) {
        return 0.0;
    }
    if (index == outgoingIndex_) {
        return outgoingScale_;
    }
    if (index == incomingIndex_) {
        return incomingScale_;
    }
    return 0.0;
}

} // namespace ZzFluentUI
