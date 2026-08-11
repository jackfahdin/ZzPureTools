#include "ZzPivotPrivate.h"

#include <algorithm>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QEasingCurve>
#include <QtCore/QVariant>
#include <QtCore/QVariantAnimation>
#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionTab>

#include <ZzFluentUI/ZzAnimationPolicy.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzPivot.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzPivotPrivate::ZzPivotPrivate(ZzPivot *q)
    : q_ptr(q)
    , theme(q)
    , indicatorAnimation(new QVariantAnimation(q))
{
    Q_ASSERT(q_ptr != nullptr);
    indicatorAnimation->setEasingCurve(QEasingCurve::InOutSine);
    QObject::connect(
        indicatorAnimation,
        &QVariantAnimation::valueChanged,
        q_ptr,
        [this](const QVariant &value) {
            const QRectF previous = currentIndicatorRect;
            currentIndicatorRect = value.toRectF();
            updateIndicatorRegion(previous);
        });
    QObject::connect(
        indicatorAnimation,
        &QVariantAnimation::finished,
        q_ptr,
        [this] {
            settleIndicator();
        });
    QObject::connect(
        q_ptr,
        &QTabBar::currentChanged,
        q_ptr,
        [this](int index) {
            startIndicatorTransition(index);
        });
}

void ZzPivotPrivate::paint(QPainter *painter)
{
    if (painter == nullptr || !painter->isActive()) {
        return;
    }
    const auto snapshot = theme.snapshot();
    if (snapshot == nullptr) {
        return;
    }

    if (indicatorAnimation->state() == QAbstractAnimation::Stopped) {
        currentIndicatorRect = targetIndicatorRect(q_ptr->currentIndex());
    }
    for (int index = 0; index < q_ptr->count(); ++index) {
        const QRect tab = q_ptr->tabRect(index);
        if (tab.isEmpty() || !tab.intersects(q_ptr->rect())) {
            continue;
        }
        QStyleOptionTab option;
        q_ptr->initStyleOption(&option, index);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const bool pressed = option.state.testFlag(QStyle::State_Sunken);
        if (hovered || pressed) {
            const ZzColorToken fill = pressed
                ? ZzColorToken::ControlFillPressed
                : ZzColorToken::ControlFillHover;
            ZzFluentPainter::drawRoundedSurface(
                painter,
                QRectF(tab),
                *snapshot,
                fill,
                fill,
                snapshot->metric(ZzMetricToken::CornerRadiusSmall),
                0.0);
        }
        q_ptr->style()->drawControl(
            QStyle::CE_TabBarTabLabel,
            &option,
            painter,
            q_ptr);
    }

    if (!currentIndicatorRect.isEmpty()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(snapshot->color(ZzColorToken::Accent));
        const qreal radius = std::min(
            currentIndicatorRect.width(),
            currentIndicatorRect.height()) / 2.0;
        painter->drawRoundedRect(currentIndicatorRect, radius, radius);
        painter->restore();
    }

    if (q_ptr->hasFocus() && q_ptr->currentIndex() >= 0) {
        ZzFluentPainter::drawFocusRing(
            painter,
            QRectF(q_ptr->tabRect(q_ptr->currentIndex())),
            *snapshot,
            q_ptr->devicePixelRatioF());
    }
}

void ZzPivotPrivate::startIndicatorTransition(int index)
{
    const QRectF target = targetIndicatorRect(index);
    if (target.isEmpty() || currentIndicatorRect.isEmpty()) {
        const QRectF previous = currentIndicatorRect;
        indicatorAnimation->stop();
        currentIndicatorRect = target;
        updateIndicatorRegion(previous);
        return;
    }

    const QRectF start = currentIndicatorRect;
    indicatorAnimation->stop();
    const int duration = transitionDuration();
    if (duration <= 0 || start == target) {
        currentIndicatorRect = target;
        updateIndicatorRegion(start);
        return;
    }

    indicatorAnimation->setStartValue(start);
    indicatorAnimation->setEndValue(target);
    indicatorAnimation->setDuration(duration);
    indicatorAnimation->start();
}

void ZzPivotPrivate::settleIndicator()
{
    const QRectF previous = currentIndicatorRect;
    indicatorAnimation->stop();
    currentIndicatorRect = targetIndicatorRect(q_ptr->currentIndex());
    updateIndicatorRegion(previous);
}

void ZzPivotPrivate::refreshTheme()
{
    theme.refreshFallback();
    settleIndicator();
    q_ptr->update();
}

QRectF ZzPivotPrivate::targetIndicatorRect(int index) const
{
    if (index < 0 || index >= q_ptr->count()) {
        return {};
    }
    const QRect tab = q_ptr->tabRect(index);
    if (tab.isEmpty()) {
        return {};
    }
    const auto snapshot = theme.snapshot();
    if (snapshot == nullptr) {
        return {};
    }
    QStyleOptionTab option;
    q_ptr->initStyleOption(&option, index);
    const QRect textBounds = option.fontMetrics.boundingRect(
        option.rect,
        Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextSingleLine,
        option.text);
    const int horizontalPadding = qCeil(
        snapshot->metric(ZzMetricToken::HorizontalPadding));
    const qreal maximumWidth = std::max(
        0,
        tab.width() - (2 * horizontalPadding));
    const qreal indicatorWidth = std::min(
        maximumWidth,
        static_cast<qreal>(textBounds.width()));
    const qreal thickness = snapshot->metric(
        ZzMetricToken::SelectionIndicatorThickness);
    return QRectF(
        static_cast<qreal>(tab.center().x()) - (indicatorWidth / 2.0),
        static_cast<qreal>(tab.bottom() + 1) - thickness,
        indicatorWidth,
        thickness);
}

int ZzPivotPrivate::transitionDuration() const
{
    const auto snapshot = theme.snapshot();
    return ZzAnimationPolicy::adjustedDuration(
        snapshot->duration(ZzMotionToken::Normal),
        snapshot->reducedMotion(),
        false);
}

void ZzPivotPrivate::updateIndicatorRegion(const QRectF &previous)
{
    q_ptr->update(previous.united(currentIndicatorRect).toAlignedRect());
}

} // namespace ZzFluentUI
