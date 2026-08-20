#include <ZzFluentUI/ZzActivityBar.h>

#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QListView>

#include "private/ZzActivityBarPrivate.h"

namespace ZzFluentUI {

ZzActivityBar::ZzActivityBar(ZzSidePaneEdge edge, QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzActivityBarPrivate>(this, edge))
{
}

ZzActivityBar::~ZzActivityBar() = default;

ZzSidePaneEdge ZzActivityBar::edge() const noexcept
{
    return d_ptr->edge;
}

void ZzActivityBar::setEdge(ZzSidePaneEdge edge)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    d_ptr->setEdge(edge);
}

void ZzActivityBar::setModel(QAbstractItemModel *model)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    d_ptr->setModel(model);
}

QAbstractItemModel *ZzActivityBar::model() const noexcept
{
    return d_ptr->sourceModel.data();
}

void ZzActivityBar::setCurrentSourceIndex(const QModelIndex &index)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    d_ptr->setCurrentSourceIndex(index);
}

QModelIndex ZzActivityBar::currentSourceIndex() const
{
    return d_ptr->currentSourceIndex;
}

bool ZzActivityBar::eventFilter(QObject *watched, QEvent *event)
{
    auto *view = qobject_cast<QListView *>(watched);
    const bool isViewport = view == nullptr;
    if (view == nullptr || event == nullptr) {
        if (watched == d_ptr->primaryView->viewport()) {
            view = d_ptr->primaryView;
        } else if (watched == d_ptr->secondaryView->viewport()) {
            view = d_ptr->secondaryView;
        } else {
            return QWidget::eventFilter(watched, event);
        }
    }
    if (!isViewport && event->type() == QEvent::KeyPress) {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (d_ptr->handleKey(view, keyEvent->key())) {
            event->accept();
            return true;
        }
    }
    if (event->type() == QEvent::DragEnter
        || event->type() == QEvent::DragMove) {
        const auto *dragEvent = static_cast<QDropEvent *>(event);
        if (d_ptr->handleDrop(view, dragEvent->mimeData(), -1)) {
            event->accept();
            return true;
        }
        return true;
    }
    if (event->type() == QEvent::Drop) {
        const auto *dropEvent = static_cast<QDropEvent *>(event);
        if (d_ptr->handleDrop(
                view, dropEvent->mimeData(),
                static_cast<int>(dropEvent->position().y()))) {
            event->accept();
        }
        return true;
    }
    if (event->type() == QEvent::DragLeave) {
        d_ptr->discardDragTokens();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace ZzFluentUI
