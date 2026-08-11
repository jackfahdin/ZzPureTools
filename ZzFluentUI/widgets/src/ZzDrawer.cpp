#include <ZzFluentUI/ZzDrawer.h>

#include <algorithm>

#include <QtCore/QEvent>
#include <QtGui/QHideEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>

#include "private/ZzDrawerPrivate.h"

#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

namespace {

constexpr int zzMaximumDrawerWidthHint = 4096;

} // namespace

ZzDrawer::ZzDrawer(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzDrawerPrivate>(this))
{
    setObjectName(QStringLiteral("zzDrawer"));
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    hide();
    d_ptr->refreshPresentation();
}

ZzDrawer::~ZzDrawer() = default;

ZzDrawerEdge ZzDrawer::edge() const noexcept
{
    return d_ptr->edge;
}

void ZzDrawer::setEdge(ZzDrawerEdge edge)
{
    if (d_ptr->edge == edge) {
        return;
    }
    d_ptr->edge = edge;
    d_ptr->refreshPresentation();
    Q_EMIT edgeChanged(edge);
}

bool ZzDrawer::isModal() const noexcept
{
    return d_ptr->modal;
}

void ZzDrawer::setModal(bool modal)
{
    if (d_ptr->modal == modal) {
        return;
    }
    d_ptr->modal = modal;
    d_ptr->refreshPresentation();
    Q_EMIT modalChanged(modal);
}

int ZzDrawer::widthHint() const noexcept
{
    return d_ptr->widthHint;
}

void ZzDrawer::setWidthHint(int logicalWidth)
{
    const int normalized = logicalWidth <= 0
        ? 0
        : std::min(logicalWidth, zzMaximumDrawerWidthHint);
    if (d_ptr->widthHint == normalized) {
        return;
    }
    d_ptr->widthHint = normalized;
    d_ptr->refreshPresentation();
    Q_EMIT widthHintChanged(normalized);
}

bool ZzDrawer::isOpen() const noexcept
{
    return d_ptr->open;
}

QWidget *ZzDrawer::contentWidget() const noexcept
{
    return d_ptr->contentWidget.data();
}

void ZzDrawer::setContentWidget(QWidget *widget)
{
    d_ptr->setContentWidget(widget);
}

QWidget *ZzDrawer::takeContentWidget()
{
    return d_ptr->takeContentWidget();
}

void ZzDrawer::openDrawer()
{
    d_ptr->openDrawer();
}

void ZzDrawer::closeDrawer()
{
    d_ptr->closeDrawer();
}

bool ZzDrawer::event(QEvent *event)
{
    const bool handled = QWidget::event(event);
    if (event != nullptr && event->type() == QEvent::ParentChange) {
        d_ptr->updateHostBinding();
    }
    return handled;
}

void ZzDrawer::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    if (event != nullptr) {
        painter.setClipRegion(event->region());
    }
    const auto snapshot = d_ptr->theme.snapshot();
    if (snapshot == nullptr) {
        return;
    }
    if (d_ptr->modal && d_ptr->progress > 0.0) {
        painter.save();
        painter.setOpacity(d_ptr->progress);
        ZzFluentPainter::drawOverlayScrim(
            &painter,
            QRectF(rect()),
            *snapshot);
        painter.restore();
    }
    const QRectF panel = d_ptr->panelRect();
    if (!panel.isEmpty() && panel.intersects(QRectF(rect()))) {
        ZzFluentPainter::drawPopupSurface(&painter, panel, *snapshot);
    }
}

void ZzDrawer::changeEvent(QEvent *event)
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
        || event->type() == QEvent::LayoutDirectionChange
        || event->type() == QEvent::DevicePixelRatioChange) {
        d_ptr->refreshPresentation();
    }
}

void ZzDrawer::keyPressEvent(QKeyEvent *event)
{
    if (event != nullptr && event->key() == Qt::Key_Escape
        && (d_ptr->open || d_ptr->progress > 0.0)) {
        closeDrawer();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ZzDrawer::mousePressEvent(QMouseEvent *event)
{
    if (event != nullptr && d_ptr->modal
        && event->button() == Qt::LeftButton
        && d_ptr->progress > 0.0
        && !d_ptr->panelRect().contains(event->position())) {
        closeDrawer();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ZzDrawer::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    d_ptr->handleExternalHide();
}

} // namespace ZzFluentUI
