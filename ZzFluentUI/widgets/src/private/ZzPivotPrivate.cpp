#include "ZzPivotPrivate.h"

#include <algorithm>
#include <cmath>

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

namespace {

/** @brief 把逻辑矩形四边内缩到实际绘制设备的物理像素边界。 */
QRectF zzAlignedIndicatorRect(
    const QRectF &rect,
    qreal devicePixelRatio)
{
    if (rect.isEmpty()) {
        return {};
    }
    const qreal ratio = std::isfinite(devicePixelRatio)
            && devicePixelRatio > 0.0
        ? devicePixelRatio
        : 1.0;
    const qreal left = std::ceil(rect.left() * ratio) / ratio;
    const qreal top = std::ceil(rect.top() * ratio) / ratio;
    const qreal right = std::floor(rect.right() * ratio) / ratio;
    const qreal bottom = std::floor(rect.bottom() * ratio) / ratio;
    if (right <= left || bottom <= top) {
        return {};
    }
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

} // namespace

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
        initLabelStyleOption(&option, index);
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

    const qreal devicePixelRatio = painter->device() != nullptr
        ? painter->device()->devicePixelRatioF()
        : 1.0;
    const QRectF paintIndicatorRect = zzAlignedIndicatorRect(
        currentIndicatorRect,
        devicePixelRatio);
    if (!paintIndicatorRect.isEmpty()) {
        painter->save();
        painter->setClipRect(
            paintIndicatorRect,
            Qt::IntersectClip);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(snapshot->color(ZzColorToken::Accent));
        const qreal radius = std::min(
            paintIndicatorRect.width(),
            paintIndicatorRect.height()) / 2.0;
        painter->drawRoundedRect(paintIndicatorRect, radius, radius);
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

void ZzPivotPrivate::initLabelStyleOption(
    QStyleOptionTab *option,
    int index) const
{
    Q_ASSERT(option != nullptr);
    q_ptr->initStyleOption(option, index);
    const int visibleTop = std::max(
        option->rect.top(),
        q_ptr->rect().top());
    const int visibleBottom = std::min(
        option->rect.bottom(),
        q_ptr->rect().bottom());
    option->rect.setY(visibleTop);
    option->rect.setHeight(std::max(
        0,
        visibleBottom - visibleTop + 1));
    const auto snapshot = theme.snapshot();
    if (snapshot == nullptr) {
        return;
    }
    const int gutterHeight = std::max(
        0,
        qCeil(snapshot->metric(
            ZzMetricToken::SelectionIndicatorThickness)));
    option->rect.setHeight(std::max(
        0,
        option->rect.height() - gutterHeight));
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
    initLabelStyleOption(&option, index);
    QRect contentBounds = q_ptr->style()->subElementRect(
        QStyle::SE_TabBarTabText,
        &option,
        q_ptr).intersected(option.rect);
    if (contentBounds.isEmpty() && !option.icon.isNull()) {
        const int preferredIconExtent = option.iconSize.isValid()
            ? option.iconSize.width()
            : q_ptr->style()->pixelMetric(
                  QStyle::PM_SmallIconSize,
                  &option,
                  q_ptr);
        const int iconExtent = std::max(0, preferredIconExtent);
        contentBounds = QRect(
            option.rect.center().x() - (iconExtent / 2),
            option.rect.top(),
            std::min(iconExtent, option.rect.width()),
            option.rect.height());
    }
    if (contentBounds.isEmpty()) {
        return {};
    }
    const int horizontalPadding = qCeil(
        snapshot->metric(ZzMetricToken::HorizontalPadding));
    const qreal maximumWidth = std::max(
        0,
        tab.width() - (2 * horizontalPadding));
    const qreal indicatorWidth = std::min(
        maximumWidth,
        static_cast<qreal>(contentBounds.width()));
    const qreal thickness = snapshot->metric(
        ZzMetricToken::SelectionIndicatorThickness);
    const qreal bottom = static_cast<qreal>(tab.bottom() + 1);
    const qreal top = std::max(
        static_cast<qreal>(option.rect.bottom() + 1),
        bottom - thickness);
    const qreal indicatorHeight = std::max(0.0, bottom - top);
    return QRectF(
        static_cast<qreal>(contentBounds.center().x())
            - (indicatorWidth / 2.0),
        top,
        indicatorWidth,
        indicatorHeight);
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
