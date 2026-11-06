#include "ZzAnimatedStackedWidgetPrivate.h"
#include <algorithm>
#include <QtCore/QEvent>
#include <QtCore/QThread>

namespace ZzFluentUI {

ZzAnimatedStackedWidgetPrivate::ZzAnimatedStackedWidgetPrivate(ZzAnimatedStackedWidget *owner)
    : q(owner), theme(owner), host(new QWidget(owner))
{
    host->setGeometry(q->contentsRect());
    host->installEventFilter(this);
}

ZzAnimatedStackedWidgetPrivate::~ZzAnimatedStackedWidgetPrivate()
{
    for (const auto &page : pages) {
        QObject::disconnect(page.destroyed);
        if (page.view) page.view->removeEventFilter(this);
    }
}

int ZzAnimatedStackedWidgetPrivate::indexOf(QWidget *page) const noexcept
{
    if (!page) return -1;
    for (qsizetype i = 0; i < pages.size(); ++i) {
        if (pages.at(i).identity == page) return static_cast<int>(i);
    }
    return -1;
}

void ZzAnimatedStackedWidgetPrivate::notifyCurrent()
{
    const int index = indexOf(current);
    if (notifiedCurrent == current && notifiedIndex == index) return;
    notifiedCurrent = current;
    notifiedIndex = index;
    Q_EMIT q->currentChanged(index);
}

void ZzAnimatedStackedWidgetPrivate::endMutation()
{
    --mutationDepth;
    if (mutationDepth || draining || running) return;
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    draining = true;
    while (pending) {
        const QPointer<QWidget> next = pending;
        const auto direction = pendingDirection;
        pending.clear();
        request(next, direction);
        if (!guard) return;
        if (running) break;
    }
    draining = false;
}

void ZzAnimatedStackedWidgetPrivate::synchronizePages()
{
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    ++mutationDepth;
    host->setGeometry(q->contentsRect());
    if (!guard) return;
    // 显隐事件可能移除任意页面，遍历独立观察快照而不是活动容器。
    const auto snapshot = pages;
    for (const auto &entry : snapshot) {
        if (entry.view && entry.identity != current && indexOf(entry.identity) >= 0) entry.view->hide();
        if (!guard) return;
    }
    const QPointer<QWidget> target = q->currentWidget();
    if (target) {
        target->setGeometry(host->rect());
        if (!guard) return;
        if (target && target == q->currentWidget()) target->show();
        if (!guard) return;
    }
    q->updateGeometry();
    endMutation();
}

int ZzAnimatedStackedWidgetPrivate::insert(int index, QWidget *page)
{
    if (!page || QThread::currentThread() != q->thread() || page->thread() != q->thread()
        || page == q || page->isAncestorOf(q) || index < 0 || index > pages.size()) return -1;
    const int existing = indexOf(page);
    if (existing >= 0) return existing;
    if (page->parent() && page->parent() != q) return -1;
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    const QPointer<QWidget> view(page);
    ++mutationDepth;
    ++generation;
    page->setParent(host);
    if (!guard) return -1;
    if (!view || view->parentWidget() != host) { endMutation(); return -1; }
    ZzAnimatedStackPage entry;
    entry.view = page;
    entry.identity = page;
    entry.destroyed = QObject::connect(page, &QObject::destroyed, this, [this, page] { remove(page, false); });
    pages.insert(std::min(index, static_cast<int>(pages.size())), entry);
    if (!current) current = page;
    page->installEventFilter(this);
    synchronizePages();
    if (!guard) return -1;
    Q_EMIT q->countChanged(static_cast<int>(pages.size()));
    if (!guard) return -1;
    notifyCurrent();
    if (!guard) return -1;
    endMutation();
    return guard && view ? indexOf(view) : -1;
}

QWidget *ZzAnimatedStackedWidgetPrivate::remove(QWidget *page, bool detach)
{
    const int index = indexOf(page);
    if (index < 0) return nullptr;
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    const auto entry = pages.at(index);
    const QPointer<QWidget> view = entry.view;
    const bool removedCurrent = current == page;
    ++mutationDepth;
    ++generation;
    pages.removeAt(index);
    QObject::disconnect(entry.destroyed);
    if (view) view->removeEventFilter(this);
    if (pending == page) pending.clear();
    if (removedCurrent) current = pages.isEmpty() ? nullptr : pages.at(std::min(index, static_cast<int>(pages.size()) - 1)).identity;
    if (detach && view) {
        view->hide();
        if (!guard) return nullptr;
        if (view) view->setParent(nullptr);
        if (!guard) return view;
    }
    synchronizePages();
    if (!guard) return view;
    Q_EMIT q->countChanged(static_cast<int>(pages.size()));
    if (!guard) return view;
    notifyCurrent();
    if (!guard) return view;
    endMutation();
    return view;
}

bool ZzAnimatedStackedWidgetPrivate::request(QWidget *page, Direction direction)
{
    if (indexOf(page) < 0 || direction < Direction::Automatic || direction > Direction::Backward) return false;
    if (mutationDepth || running) {
        pending = current == page ? nullptr : page;
        pendingDirection = direction;
        return true;
    }
    if (current == page) return true;
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    ++mutationDepth;
    ++generation;
    current = page;
    synchronizePages();
    if (!guard) return true;
    notifyCurrent();
    if (guard) endMutation();
    return true;
}

void ZzAnimatedStackedWidgetPrivate::settle() {}

bool ZzAnimatedStackedWidgetPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == host && event->type() == QEvent::LayoutRequest) q->updateGeometry();
    if (event->type() == QEvent::ParentChange) {
        auto *page = qobject_cast<QWidget *>(watched);
        if (page && indexOf(page) >= 0 && page->parentWidget() != host) {
            const QPointer<QObject> receiver(watched);
            remove(page, false);
            return receiver.isNull();
        }
    }
    return false;
}

} // namespace ZzFluentUI
