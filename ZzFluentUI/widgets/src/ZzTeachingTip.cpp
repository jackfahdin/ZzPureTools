#include <ZzFluentUI/ZzTeachingTip.h>

#include <utility>

#include <QtCore/QEvent>
#include <QtGui/QHideEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QPen>
#include <QtGui/QPolygonF>
#include <QtGui/QShowEvent>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "private/ZzTeachingTipPrivate.h"

namespace ZzFluentUI {

ZzTeachingTip::ZzTeachingTip(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
    , d_ptr(std::make_unique<ZzTeachingTipPrivate>(this))
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    d_ptr->refreshPresentation();
}

ZzTeachingTip::~ZzTeachingTip() = default;

QString ZzTeachingTip::title() const
{
    return d_ptr->title;
}

void ZzTeachingTip::setTitle(QString title)
{
    if (d_ptr->title == title) {
        return;
    }
    d_ptr->title = std::move(title);
    d_ptr->refreshPresentation();
    Q_EMIT titleChanged(d_ptr->title);
}

QString ZzTeachingTip::text() const
{
    return d_ptr->text;
}

void ZzTeachingTip::setText(QString text)
{
    if (d_ptr->text == text) {
        return;
    }
    d_ptr->text = std::move(text);
    d_ptr->refreshPresentation();
    Q_EMIT textChanged(d_ptr->text);
}

QWidget *ZzTeachingTip::contentWidget() const noexcept
{
    return d_ptr->contentWidget.data();
}

void ZzTeachingTip::setContentWidget(QWidget *widget)
{
    d_ptr->setContentWidget(widget);
}

QWidget *ZzTeachingTip::takeContentWidget()
{
    return d_ptr->takeContentWidget();
}

QWidget *ZzTeachingTip::targetWidget() const noexcept
{
    return d_ptr->targetWidget.data();
}

void ZzTeachingTip::setTargetWidget(QWidget *target)
{
    d_ptr->setTargetWidget(target);
}

ZzTeachingTipPlacement ZzTeachingTip::preferredPlacement() const noexcept
{
    return d_ptr->preferredPlacement;
}

void ZzTeachingTip::setPreferredPlacement(
    ZzTeachingTipPlacement placement)
{
    if (d_ptr->preferredPlacement == placement) {
        return;
    }
    d_ptr->preferredPlacement = placement;
    if (isVisible() && !d_ptr->dismissing) {
        static_cast<void>(d_ptr->reposition());
    }
    Q_EMIT preferredPlacementChanged(placement);
}

ZzTeachingTipPlacement ZzTeachingTip::effectivePlacement() const noexcept
{
    return d_ptr->effectivePlacement;
}

bool ZzTeachingTip::isLightDismissEnabled() const noexcept
{
    return d_ptr->lightDismissEnabled;
}

void ZzTeachingTip::setLightDismissEnabled(bool enabled)
{
    if (d_ptr->lightDismissEnabled == enabled) {
        return;
    }
    d_ptr->lightDismissEnabled = enabled;
    d_ptr->syncApplicationEventFilter();
    Q_EMIT lightDismissEnabledChanged(enabled);
}

QString ZzTeachingTip::actionText() const
{
    return d_ptr->actionText;
}

void ZzTeachingTip::setActionText(QString text)
{
    if (d_ptr->actionText == text) {
        return;
    }
    d_ptr->actionText = std::move(text);
    d_ptr->refreshPresentation();
    Q_EMIT actionTextChanged(d_ptr->actionText);
}

bool ZzTeachingTip::isActionEnabled() const noexcept
{
    return d_ptr->actionEnabled;
}

void ZzTeachingTip::setActionEnabled(bool enabled)
{
    if (d_ptr->actionEnabled == enabled) {
        return;
    }
    d_ptr->actionEnabled = enabled;
    d_ptr->refreshPresentation();
    Q_EMIT actionEnabledChanged(enabled);
}

bool ZzTeachingTip::isActionVisible() const noexcept
{
    return d_ptr->actionVisible;
}

void ZzTeachingTip::setActionVisible(bool visible)
{
    if (d_ptr->actionVisible == visible) {
        return;
    }
    d_ptr->actionVisible = visible;
    d_ptr->refreshPresentation();
    Q_EMIT actionVisibleChanged(visible);
}

bool ZzTeachingTip::isCloseButtonVisible() const noexcept
{
    return d_ptr->closeButtonVisible;
}

void ZzTeachingTip::setCloseButtonVisible(bool visible)
{
    if (d_ptr->closeButtonVisible == visible) {
        return;
    }
    d_ptr->closeButtonVisible = visible;
    d_ptr->refreshPresentation();
    Q_EMIT closeButtonVisibleChanged(visible);
}

void ZzTeachingTip::showForTarget()
{
    d_ptr->showForTarget();
}

void ZzTeachingTip::dismiss()
{
    d_ptr->dismiss();
}

void ZzTeachingTip::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    const auto snapshot = d_ptr->theme.snapshot();
    const qreal inset = snapshot->metric(
        ZzMetricToken::TeachingTipTargetGap);
    const QRectF bodyRect = QRectF(rect()).adjusted(
        inset, inset, -inset, -inset);
    QPainter painter(this);
    ZzFluentPainter::drawPopupSurface(
        &painter, bodyRect, *snapshot);

    const qreal half = qMax<qreal>(2.0, inset / 2.0);
    const qreal center = d_ptr->arrowCenter > 0.0
        ? d_ptr->arrowCenter
        : (d_ptr->effectivePlacement == ZzTeachingTipPlacement::Left
              || d_ptr->effectivePlacement == ZzTeachingTipPlacement::Right
              ? static_cast<qreal>(height()) / 2.0
              : static_cast<qreal>(width()) / 2.0);
    QPolygonF arrow;
    switch (d_ptr->effectivePlacement) {
    case ZzTeachingTipPlacement::Top:
        arrow << QPointF(center - half, bodyRect.bottom() - 1.0)
              << QPointF(center + half, bodyRect.bottom() - 1.0)
              << QPointF(center, static_cast<qreal>(height()));
        break;
    case ZzTeachingTipPlacement::Left:
        arrow << QPointF(bodyRect.right() - 1.0, center - half)
              << QPointF(bodyRect.right() - 1.0, center + half)
              << QPointF(static_cast<qreal>(width()), center);
        break;
    case ZzTeachingTipPlacement::Right:
        arrow << QPointF(bodyRect.left() + 1.0, center - half)
              << QPointF(bodyRect.left() + 1.0, center + half)
              << QPointF(0.0, center);
        break;
    case ZzTeachingTipPlacement::Bottom:
    case ZzTeachingTipPlacement::Auto:
        arrow << QPointF(center - half, bodyRect.top() + 1.0)
              << QPointF(center + half, bodyRect.top() + 1.0)
              << QPointF(center, 0.0);
        break;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(
        snapshot->color(ZzColorToken::ControlStroke),
        snapshot->metric(ZzMetricToken::StrokeThin)));
    painter.setBrush(snapshot->color(ZzColorToken::SurfaceSecondary));
    painter.drawPolygon(arrow);
    painter.restore();
}

void ZzTeachingTip::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    if (event->type() == QEvent::StyleChange
        || event->type() == QEvent::PaletteChange) {
        d_ptr->refreshTheme();
        return;
    }
    if (event->type() == QEvent::LanguageChange
        || event->type() == QEvent::FontChange
        || event->type() == QEvent::DevicePixelRatioChange
        || event->type() == QEvent::LayoutDirectionChange) {
        d_ptr->refreshPresentation();
    }
}

void ZzTeachingTip::keyPressEvent(QKeyEvent *event)
{
    if (event != nullptr && event->key() == Qt::Key_Escape) {
        if (!event->isAutoRepeat()) {
            dismiss();
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ZzTeachingTip::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    d_ptr->syncApplicationEventFilter();
}

void ZzTeachingTip::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    d_ptr->handleHidden();
}

} // namespace ZzFluentUI
