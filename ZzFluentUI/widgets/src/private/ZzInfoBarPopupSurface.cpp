#include "ZzInfoBarPopupSurface.h"

#include <utility>

#include <QtCore/QEvent>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QVariantAnimation>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QStyle>

namespace ZzFluentUI {
namespace {
    constexpr int ShadowWidth = 6;
    constexpr int CornerRadius = 4;
    constexpr int EnterDuration = 300;
    constexpr int TopPositionDuration = 400;

    QEasingCurve elementEasing()
    {
        QEasingCurve curve(QEasingCurve::BezierSpline);
        curve.addCubicBezierSegment({ 0.25, 0.1 }, { 0.25, 1.0 }, { 1.0, 1.0 });
        return curve;
    }
} // namespace

ZzInfoBarPopupSurface::ZzInfoBarPopupSurface(QWidget* parent)
    : QWidget(parent)
    , animation_(new QVariantAnimation(this))
    , positionAnimation_(new QPropertyAnimation(this, "pos", this))
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    animation_->setDuration(EnterDuration);
    animation_->setEasingCurve(elementEasing());
    positionAnimation_->setDuration(TopPositionDuration);
    positionAnimation_->setEasingCurve(elementEasing());
    connect(animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        progress_ = value.toReal();
        if (opacity_)
            opacity_->setOpacity(leaving_ ? progress_ : 1.0);
        updateInfoBarGeometry();
        update();
    });
    connect(animation_, &QVariantAnimation::finished, this, [this] {
        if (leaving_ && leaveFinished_) {
            auto callback = std::move(leaveFinished_);
            callback();
        }
    });
}

void ZzInfoBarPopupSurface::setInfoBar(ZzInfoBar* bar)
{
    if (!bar)
        return;
    bar_ = bar;
    bar->setParent(this);
    bar->installEventFilter(this);
    opacity_ = new QGraphicsOpacityEffect(bar);
    opacity_->setOpacity(1.0);
    bar->setGraphicsEffect(opacity_);
    updateGeometry();
    updateInfoBarGeometry();
}

void ZzInfoBarPopupSurface::moveTo(const QPoint& target, bool animate, int duration)
{
    animate = animate && style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this);
    if (positionAnimation_->state() == QAbstractAnimation::Running) {
        if (positionAnimation_->endValue().toPoint() == target)
            return;
        positionAnimation_->stop();
    }
    if (pos() == target)
        return;
    if (!animate || !isVisible()) {
        move(target);
        return;
    }
    positionAnimation_->setStartValue(pos());
    positionAnimation_->setEndValue(target);
    positionAnimation_->setDuration(duration);
    positionAnimation_->start();
}

void ZzInfoBarPopupSurface::startEnter(ZzInfoBarHost::Position position)
{
    animation_->stop();
    leaving_ = false;
    leaveOffset_ = {};
    leaveFinished_ = {};
    if (position == ZzInfoBarHost::TopLeft || position == ZzInfoBarHost::BottomLeft)
        enterDirection_ = { -1, 0 };
    else if (position == ZzInfoBarHost::TopRight || position == ZzInfoBarHost::BottomRight)
        enterDirection_ = { 1, 0 };
    else
        enterDirection_ = { 0, position == ZzInfoBarHost::Top ? -1.0 : 1.0 };
    progress_ = 0;
    if (opacity_)
        opacity_->setOpacity(1.0);
    updateInfoBarGeometry();
    animation_->setStartValue(0.0);
    animation_->setEndValue(1.0);
    if (!style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this)) {
        progress_ = 1.0;
        updateInfoBarGeometry();
        update();
    } else {
        animation_->start();
    }
}

void ZzInfoBarPopupSurface::startLeave(std::function<void()> finished)
{
    if (leaving_)
        return;
    const QRectF panel
        = QRectF(rect()).adjusted(ShadowWidth, ShadowWidth, -ShadowWidth, -ShadowWidth);
    const QPointF offset = currentOffset(panel);
    animation_->stop();
    leaving_ = true;
    enterDirection_ = {};
    leaveOffset_ = offset;
    leaveFinished_ = std::move(finished);
    progress_ = 1;
    updateInfoBarGeometry();
    animation_->setStartValue(1.0);
    animation_->setEndValue(0.0);
    if (!style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this)) {
        progress_ = 0.0;
        if (opacity_)
            opacity_->setOpacity(0.0);
        update();
        auto callback = std::move(leaveFinished_);
        if (callback)
            callback();
    } else {
        animation_->start();
    }
}

void ZzInfoBarPopupSurface::cancelLeave()
{
    if (!leaving_)
        return;
    animation_->stop();
    leaving_ = false;
    leaveOffset_ = {};
    leaveFinished_ = {};
    progress_ = 1;
    if (opacity_)
        opacity_->setOpacity(1.0);
    updateInfoBarGeometry();
    update();
}

void ZzInfoBarPopupSurface::deactivateForQueue()
{
    animation_->stop();
    positionAnimation_->stop();
    leaving_ = false;
    leaveFinished_ = {};
    leaveOffset_ = {};
    hide();
}

QSize ZzInfoBarPopupSurface::sizeHint() const
{
    return bar_ ? bar_->sizeHint() + QSize(ShadowWidth * 2, ShadowWidth * 2) : QWidget::sizeHint();
}
QSize ZzInfoBarPopupSurface::minimumSizeHint() const
{
    return bar_ ? bar_->minimumSizeHint() + QSize(ShadowWidth * 2, ShadowWidth * 2)
                : QWidget::minimumSizeHint();
}
bool ZzInfoBarPopupSurface::hasHeightForWidth() const { return true; }
int ZzInfoBarPopupSurface::heightForWidth(int width) const
{
    if (!bar_)
        return sizeHint().height();
    const int contentWidth = qMax(0, width - ShadowWidth * 2);
    return (bar_->hasHeightForWidth() ? bar_->heightForWidth(contentWidth)
                                      : bar_->sizeHint().height())
        + ShadowWidth * 2;
}

bool ZzInfoBarPopupSurface::event(QEvent* event)
{
    const bool result = QWidget::event(event);
    if ((event->type() == QEvent::StyleChange || event->type() == QEvent::Hide
            || event->type() == QEvent::EnabledChange
            || event->type() == QEvent::DynamicPropertyChange)
        && animation_ && positionAnimation_
        && (!isVisible() || !isEnabled()
            || !style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this))) {
        finishAnimations();
    }
    return result;
}

void ZzInfoBarPopupSurface::finishAnimations()
{
    if (positionAnimation_->state() == QAbstractAnimation::Running) {
        const QPoint destination = positionAnimation_->endValue().toPoint();
        positionAnimation_->stop();
        move(destination);
    }
    if (animation_->state() != QAbstractAnimation::Running)
        return;
    auto callback = leaving_ ? std::move(leaveFinished_) : std::function<void()> {};
    animation_->stop();
    progress_ = leaving_ ? 0.0 : 1.0;
    if (opacity_)
        opacity_->setOpacity(leaving_ ? 0.0 : 1.0);
    updateInfoBarGeometry();
    update();
    if (callback)
        callback();
}

bool ZzInfoBarPopupSurface::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == bar_
        && (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Show
            || event->type() == QEvent::Hide)) {
        updateGeometry();
        updateInfoBarGeometry();
    }
    return QWidget::eventFilter(watched, event);
}

void ZzInfoBarPopupSurface::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateInfoBarGeometry();
}

QPointF ZzInfoBarPopupSurface::currentOffset(const QRectF& panel) const
{
    if (leaving_)
        return leaveOffset_;
    return { enterDirection_.x() * panel.width() * (1.0 - progress_),
        enterDirection_.y() * panel.height() * (1.0 - progress_) };
}

void ZzInfoBarPopupSurface::updateInfoBarGeometry()
{
    if (!bar_)
        return;
    const QRectF panel
        = QRectF(rect()).adjusted(ShadowWidth, ShadowWidth, -ShadowWidth, -ShadowWidth);
    bar_->setGeometry(panel.toRect().translated(currentOffset(panel).toPoint()));
}

void ZzInfoBarPopupSurface::paintEvent(QPaintEvent*)
{
    const QRectF panel
        = QRectF(rect()).adjusted(ShadowWidth, ShadowWidth, -ShadowWidth, -ShadowWidth);
    if (!panel.isValid())
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.translate(currentOffset(panel));
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    const QColor base = dark ? QColor { 0, 0, 0 } : QColor { 0x99, 0x99, 0x99 };
    constexpr int elevation = 5;
    constexpr int levels = elevation - 1;
    for (int level = 1; level <= levels; ++level) {
        const qreal extent = qreal(ShadowWidth * level) / levels;
        QPainterPath ring;
        ring.setFillRule(Qt::OddEvenFill);
        ring.addRoundedRect(panel.adjusted(-extent, -extent, extent, extent), CornerRadius + extent,
            CornerRadius + extent);
        ring.addRoundedRect(panel, CornerRadius, CornerRadius);
        painter.setOpacity((leaving_ ? progress_ : 1.0) * 0.01 * (elevation - level + 1));
        painter.setBrush(base);
        painter.drawPath(ring);
    }
}

} // namespace ZzFluentUI
