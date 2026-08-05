#include "ZzScrollBarPrivate.h"

#include <algorithm>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QEasingCurve>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QVariantAnimation>
#include <QtWidgets/QStyle>

#include <ZzFluentUI/ZzScrollBar.h>

namespace ZzFluentUI {

ZzScrollBarPrivate::ZzScrollBarPrivate(ZzScrollBar *q)
    : q_ptr(q)
    , animation(new QVariantAnimation(q))
{
    Q_ASSERT(q_ptr != nullptr);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setDuration(167);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(
        animation,
        &QVariantAnimation::valueChanged,
        q_ptr,
        [this](const QVariant &value) {
            if (!q_ptr->isEnabled()
                || q_ptr->style() == nullptr
                || q_ptr->style()->styleHint(
                       QStyle::SH_Widget_Animate,
                       nullptr,
                       q_ptr)
                    == 0) {
                finishImmediately(q_ptr->isEnabled() && q_ptr->underMouse());
                return;
            }
            expansion = std::clamp(value.toReal(), 0.0, 1.0);
            q_ptr->update();
        });
}

ZzScrollBarPrivate::~ZzScrollBarPrivate()
{
    animation->stop();
    QObject::disconnect(animation, nullptr, q_ptr, nullptr);
}

void ZzScrollBarPrivate::setExpanded(bool expanded)
{
    const qreal target = expanded ? 1.0 : 0.0;
    const bool animate = q_ptr->isVisible()
        && q_ptr->isEnabled()
        && q_ptr->style() != nullptr
        && q_ptr->style()->styleHint(
               QStyle::SH_Widget_Animate,
               nullptr,
               q_ptr)
            != 0;
    animation->stop();
    if (!animate || qFuzzyCompare(expansion, target)) {
        finishImmediately(expanded);
        return;
    }
    animation->setStartValue(expansion);
    animation->setEndValue(target);
    animation->start();
}

void ZzScrollBarPrivate::finishImmediately(bool expanded) noexcept
{
    animation->stop();
    expansion = expanded ? 1.0 : 0.0;
    q_ptr->update();
}

} // namespace ZzFluentUI
