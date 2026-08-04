#include "ZzToggleSwitchPrivate.h"

#include <QtCore/QEasingCurve>
#include <QtCore/QVariantAnimation>
#include <QtWidgets/QStyle>

#include <ZzFluentUI/ZzToggleSwitch.h>

namespace ZzFluentUI {

ZzToggleSwitchPrivate::ZzToggleSwitchPrivate(ZzToggleSwitch *q)
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
            if (q_ptr->style()->styleHint(
                    QStyle::SH_Widget_Animate,
                    nullptr,
                    q_ptr)
                == 0) {
                finishImmediately();
                return;
            }
            progress = value.toReal();
            q_ptr->update();
        });
}

void ZzToggleSwitchPrivate::moveTo(bool checked)
{
    const qreal target = checked ? 1.0 : 0.0;
    const bool animate = q_ptr->isVisible()
        && q_ptr->isEnabled()
        && q_ptr->style()->styleHint(
               QStyle::SH_Widget_Animate,
               nullptr,
               q_ptr)
            != 0;
    animation->stop();
    if (!animate || qFuzzyCompare(progress, target)) {
        progress = target;
        q_ptr->update();
        return;
    }
    animation->setStartValue(progress);
    animation->setEndValue(target);
    animation->start();
}

void ZzToggleSwitchPrivate::finishImmediately() noexcept
{
    animation->stop();
    progress = q_ptr->isChecked() ? 1.0 : 0.0;
    q_ptr->update();
}

} // namespace ZzFluentUI
