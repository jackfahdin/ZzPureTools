#include <ZzFluentUI/ZzBottomPane.h>

#include <algorithm>

#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QStackedWidget>

#include "private/ZzBottomPanePrivate.h"

namespace ZzFluentUI {

ZzBottomPane::ZzBottomPane(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzBottomPanePrivate>(this))
{
}

ZzBottomPane::~ZzBottomPane() = default;

int ZzBottomPane::widgetCount() const noexcept
{
    return static_cast<int>(d_ptr->widgets.size());
}

QWidget *ZzBottomPane::currentWidget() const noexcept
{
    return d_ptr->stackedWidget->currentWidget();
}

bool ZzBottomPane::addWidget(
    QWidget *widget,
    const QString &title,
    const ZzIconDescriptor &icon)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return false;
    }
    return d_ptr->addWidget(widget, title, icon);
}

QWidget *ZzBottomPane::takeWidget(QWidget *widget)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return nullptr;
    }
    return d_ptr->takeWidget(widget);
}

bool ZzBottomPane::setCurrentWidget(QWidget *widget)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return false;
    }
    return d_ptr->setCurrentWidget(widget);
}

bool ZzBottomPane::isCollapsed() const noexcept
{
    return d_ptr->collapsed;
}

void ZzBottomPane::setCollapsed(bool collapsed)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread() || d_ptr->collapsed == collapsed) {
        return;
    }
    d_ptr->collapsed = collapsed;
    if (collapsed) {
        d_ptr->header->hide();
        d_ptr->stackedWidget->hide();
        setFixedHeight(0);
    } else {
        d_ptr->applyExpandedHeight();
        d_ptr->header->show();
        d_ptr->stackedWidget->show();
    }
    Q_EMIT collapsedChanged(collapsed);
}

int ZzBottomPane::minimumPaneHeight() const noexcept
{
    return d_ptr->minimumHeight;
}

void ZzBottomPane::setMinimumPaneHeight(int height)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    const int normalized = std::max(4, height);
    if (d_ptr->minimumHeight == normalized) {
        return;
    }
    const int previousMaximum = d_ptr->maximumHeight;
    d_ptr->minimumHeight = normalized;
    d_ptr->maximumHeight = std::max(d_ptr->maximumHeight, normalized);
    setPaneHeight(d_ptr->expandedHeight);
    Q_EMIT minimumPaneHeightChanged(normalized);
    if (d_ptr->maximumHeight != previousMaximum) {
        Q_EMIT maximumPaneHeightChanged(d_ptr->maximumHeight);
    }
}

int ZzBottomPane::maximumPaneHeight() const noexcept
{
    return d_ptr->maximumHeight;
}

void ZzBottomPane::setMaximumPaneHeight(int height)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    const int normalized = std::max(d_ptr->minimumHeight, height);
    if (d_ptr->maximumHeight == normalized) {
        return;
    }
    d_ptr->maximumHeight = normalized;
    setPaneHeight(d_ptr->expandedHeight);
    Q_EMIT maximumPaneHeightChanged(normalized);
}

int ZzBottomPane::paneHeight() const noexcept
{
    return d_ptr->expandedHeight;
}

void ZzBottomPane::setPaneHeight(int height)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    const int normalized = d_ptr->clampHeight(height);
    if (d_ptr->expandedHeight == normalized) {
        return;
    }
    d_ptr->expandedHeight = normalized;
    if (!d_ptr->collapsed) {
        d_ptr->applyExpandedHeight();
    }
    Q_EMIT paneHeightChanged(normalized);
}

int ZzBottomPane::lastExpandedHeight() const noexcept
{
    return d_ptr->expandedHeight;
}

bool ZzBottomPane::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != d_ptr->resizeHandle || event == nullptr || d_ptr->collapsed) {
        return QWidget::eventFilter(watched, event);
    }
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            return d_ptr->handleResizeDrag(
                static_cast<int>(mouseEvent->globalPosition().y()), true, false);
        }
        break;
    }
    case QEvent::MouseMove: {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (d_ptr->resizing) {
            return d_ptr->handleResizeDrag(
                static_cast<int>(mouseEvent->globalPosition().y()), false, false);
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && d_ptr->resizing) {
            return d_ptr->handleResizeDrag(
                static_cast<int>(mouseEvent->globalPosition().y()), false, true);
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace ZzFluentUI
