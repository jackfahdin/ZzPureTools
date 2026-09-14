#include "ZzTabIndicatorAnimation.h"

#include <QtCore/QEvent>
#include <QtCore/QVariantAnimation>
#include <QtWidgets/QTabBar>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzTabIndicatorAnimation::ZzTabIndicatorAnimation(QTabBar *bar, ZzFluentStyle *style)
    : QObject(style), bar_(bar), style_(style), animation_(new QVariantAnimation(this))
{
    animation_->setEasingCurve(QEasingCurve::InOutSine);
    bar->installEventFilter(this);
    connect(animation_, &QVariantAnimation::valueChanged, this,
        [this](const QVariant &value) {
            const QRectF previous = current_;
            current_ = value.toRectF();
            bar_->update(previous.united(current_).toAlignedRect());
        });
    connect(bar, &QTabBar::currentChanged, this, [this] { transitionToCurrent(); });
    connect(bar, &QTabBar::tabMoved, this, [this] { settle(); });
    current_ = targetRect();
}

QRectF ZzTabIndicatorAnimation::targetRect() const
{
    if (bar_ == nullptr || bar_->currentIndex() < 0
        || !bar_->isTabVisible(bar_->currentIndex())) {
        return {};
    }
    const QRectF tab(bar_->tabRect(bar_->currentIndex()));
    const auto snapshot = style_->themeSnapshot();
    const qreal thickness = snapshot->metric(ZzMetricToken::SelectionIndicatorThickness);
    const qreal extent = snapshot->metric(ZzMetricToken::SelectionIndicatorExtent);
    const qreal leading = snapshot->metric(ZzMetricToken::SelectionIndicatorLeading);
    switch (bar_->shape()) {
    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast: {
        const bool west = bar_->shape() == QTabBar::RoundedWest
            || bar_->shape() == QTabBar::TriangularWest;
        const qreal height = qMin(extent, qMax(0.0, tab.height() - 2 * leading));
        return QRectF(west ? tab.right() - thickness : tab.left(),
            tab.center().y() - height / 2.0, qMin(thickness, tab.width()), height);
    }
    default: {
        const bool south = bar_->shape() == QTabBar::RoundedSouth
            || bar_->shape() == QTabBar::TriangularSouth;
        const qreal width = qMin(extent, qMax(0.0, tab.width() - 2 * leading));
        return QRectF(tab.center().x() - width / 2.0,
            south ? tab.top() : tab.bottom() - thickness,
            width, qMin(thickness, tab.height()));
    }
    }
}

void ZzTabIndicatorAnimation::transitionToCurrent()
{
    const QRectF target = targetRect();
    animation_->stop();
    const int duration = style_->themeSnapshot()->duration(ZzMotionToken::Normal);
    if (target.isEmpty() || current_.isEmpty() || !bar_->isVisible()
        || !bar_->isEnabled() || duration <= 0) {
        settle();
        return;
    }
    animation_->setStartValue(current_);
    animation_->setEndValue(target);
    animation_->setDuration(duration);
    animation_->start();
}

void ZzTabIndicatorAnimation::settle()
{
    animation_->stop();
    const QRectF previous = current_;
    current_ = targetRect();
    if (bar_ != nullptr) {
        bar_->update(previous.united(current_).toAlignedRect());
    }
}

QRectF ZzTabIndicatorAnimation::rect()
{
    if (style_->themeSnapshot()->reducedMotion()) {
        if (animation_->state() != QAbstractAnimation::Stopped) {
            settle();
        }
    }
    if (animation_->state() == QAbstractAnimation::Stopped) {
        current_ = targetRect();
    }
    return current_;
}

bool ZzTabIndicatorAnimation::eventFilter(QObject *watched, QEvent *event)
{
    // 析构期 QPointer 尚未清空，必须先依据事件接收者确认派生类型仍然有效。
    auto *bar = qobject_cast<QTabBar *>(watched);
    if (bar == nullptr || bar != bar_.data()) {
        return QObject::eventFilter(watched, event);
    }
    if (event->type() == QEvent::Hide
        || event->type() == QEvent::Resize
        || event->type() == QEvent::LayoutRequest
        || event->type() == QEvent::LayoutDirectionChange
        || event->type() == QEvent::EnabledChange
        || event->type() == QEvent::StyleChange) {
        settle();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace ZzFluentUI
