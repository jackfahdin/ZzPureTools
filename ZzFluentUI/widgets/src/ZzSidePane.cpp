#include <ZzFluentUI/ZzSidePane.h>

#include <algorithm>

#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QStackedWidget>

#include "private/ZzSidePanePrivate.h"

namespace ZzFluentUI {

ZzSidePane::ZzSidePane(ZzSidePaneEdge edge, QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzSidePanePrivate>(this, edge))
{
}

ZzSidePane::~ZzSidePane() = default;

ZzSidePaneEdge ZzSidePane::edge() const noexcept
{
    return d_ptr->edge;
}

void ZzSidePane::setEdge(ZzSidePaneEdge edge)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    if (d_ptr->edge == edge) {
        return;
    }
    d_ptr->setEdge(edge);
    Q_EMIT edgeChanged(edge);
}

int ZzSidePane::pageCount() const noexcept
{
    return d_ptr->stack->count();
}

QWidget *ZzSidePane::currentWidget() const noexcept
{
    return d_ptr->stack->currentWidget();
}

bool ZzSidePane::addWidget(QWidget *widget, const QString &title)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return false;
    }
    return d_ptr->addWidget(widget, title);
}

QWidget *ZzSidePane::takeWidget(QWidget *widget)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return nullptr;
    }
    return d_ptr->takeWidget(widget);
}

bool ZzSidePane::setCurrentWidget(QWidget *widget)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return false;
    }
    return d_ptr->setCurrentWidget(widget);
}

bool ZzSidePane::isCollapsed() const noexcept
{
    return d_ptr->collapsed;
}

void ZzSidePane::setCollapsed(bool collapsed)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    if (d_ptr->collapsed == collapsed) {
        return;
    }
    d_ptr->collapsed = collapsed;
    if (collapsed) {
        hide();
        setFixedWidth(0);
    } else {
        d_ptr->applyExpandedWidth();
        show();
    }
    Q_EMIT collapsedChanged(collapsed);
}

int ZzSidePane::minimumPaneWidth() const noexcept
{
    return d_ptr->minimumWidth;
}

void ZzSidePane::setMinimumPaneWidth(int width)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    const int normalized = std::max(4, width);
    if (d_ptr->minimumWidth == normalized) {
        return;
    }
    d_ptr->minimumWidth = normalized;
    d_ptr->maximumWidth = std::max(d_ptr->maximumWidth, normalized);
    setPaneWidth(d_ptr->expandedWidth);
    Q_EMIT minimumPaneWidthChanged(normalized);
}

int ZzSidePane::maximumPaneWidth() const noexcept
{
    return d_ptr->maximumWidth;
}

void ZzSidePane::setMaximumPaneWidth(int width)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    const int normalized = std::max(d_ptr->minimumWidth, width);
    if (d_ptr->maximumWidth == normalized) {
        return;
    }
    d_ptr->maximumWidth = normalized;
    setPaneWidth(d_ptr->expandedWidth);
    Q_EMIT maximumPaneWidthChanged(normalized);
}

int ZzSidePane::paneWidth() const noexcept
{
    return d_ptr->expandedWidth;
}

void ZzSidePane::setPaneWidth(int width)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    const int normalized = d_ptr->clampWidth(width);
    if (d_ptr->expandedWidth == normalized) {
        return;
    }
    d_ptr->expandedWidth = normalized;
    if (!d_ptr->collapsed) {
        d_ptr->applyExpandedWidth();
    }
    Q_EMIT paneWidthChanged(normalized);
}

int ZzSidePane::lastExpandedWidth() const noexcept
{
    return d_ptr->expandedWidth;
}

bool ZzSidePane::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != d_ptr->resizeHandle || event == nullptr) {
        return QWidget::eventFilter(watched, event);
    }
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            return d_ptr->handleResizeDrag(
                static_cast<int>(mouseEvent->globalPosition().x()), true, false);
        }
        break;
    }
    case QEvent::MouseMove: {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (d_ptr->resizing) {
            return d_ptr->handleResizeDrag(
                static_cast<int>(mouseEvent->globalPosition().x()), false, false);
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && d_ptr->resizing) {
            return d_ptr->handleResizeDrag(
                static_cast<int>(mouseEvent->globalPosition().x()), false, true);
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace ZzFluentUI
