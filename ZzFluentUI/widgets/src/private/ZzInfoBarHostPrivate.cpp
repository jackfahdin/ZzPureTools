#include "ZzInfoBarHostPrivate.h"

#include "ZzInfoBarPopupSurface.h"

#include <QtCore/QTimer>

namespace ZzFluentUI {
namespace {
    bool topPosition(ZzInfoBarHost::Position position)
    {
        return position == ZzInfoBarHost::TopLeft || position == ZzInfoBarHost::Top
            || position == ZzInfoBarHost::TopRight;
    }
} // namespace

ZzInfoBarHostPrivate::ZzInfoBarHostPrivate(ZzInfoBarHost* host, QWidget* targetWidget)
    : q(host)
    , target(targetWidget)
{
}

ZzInfoBarHostPrivate::Entry* ZzInfoBarHostPrivate::findEntry(ZzInfoBar* bar) const
{
    for (Entry* entry : entries)
        if (entry->bar == bar)
            return entry;
    return nullptr;
}

void ZzInfoBarHostPrivate::startTimer(Entry* entry)
{
    if (!entry || !entry->bar || entry->timeout <= 0)
        return;
    if (entry->timer) {
        resumeTimer(entry);
        return;
    }
    entry->remaining = entry->timeout;
    entry->timer = new QTimer(q);
    entry->timer->setSingleShot(true);
    QObject::connect(entry->timer, &QTimer::timeout, q, [entry] {
        if (entry->bar)
            entry->bar->dismiss();
    });
    resumeTimer(entry);
}

void ZzInfoBarHostPrivate::pauseTimer(Entry* entry)
{
    if (!entry || !entry->timer || !entry->timer->isActive())
        return;
    entry->remaining = qMax(0, entry->remaining - static_cast<int>(entry->elapsed.elapsed()));
    entry->timer->stop();
}

void ZzInfoBarHostPrivate::resumeTimer(Entry* entry)
{
    if (!entry || !entry->timer || !entry->bar || !entry->active || entry->closing || entry->hovered
        || !target || !target->isVisible())
        return;
    if (entry->remaining <= 0) {
        entry->bar->dismiss();
        return;
    }
    entry->elapsed.restart();
    entry->timer->start(entry->remaining);
}

void ZzInfoBarHostPrivate::removeEntry(Entry* entry, bool preserveBar)
{
    if (!entry || !entries.contains(entry))
        return;
    ZzInfoBar* bar = entry->bar.data();
    const auto position = entry->position;
    entries.removeOne(entry);
    if (entry->timer) {
        entry->timer->stop();
        entry->timer->deleteLater();
    }
    if (bar) {
        bar->removeEventFilter(q);
        QObject::disconnect(bar, nullptr, q, nullptr);
        if (preserveBar) {
            bar->setParent(nullptr);
            bar->setProperty("_zzInfoBarPopupSurface", false);
            bar->hide();
        }
    }
    if (entry->surface)
        entry->surface->deleteLater();
    else if (bar && !preserveBar)
        bar->deleteLater();
    scheduleReposition(true);
    delete entry;
    emit q->infoBarClosed(bar, position);
}

int ZzInfoBarHostPrivate::surfaceWidth() const
{
    if (!target)
        return 0;
    const qint64 available = qMax<qint64>(0, qint64(target->width()) - qint64(margin) * 2);
    return static_cast<int>(qMin<qint64>(maximumWidth, available));
}

int ZzInfoBarHostPrivate::surfaceHeight(const Entry* entry, int width) const
{
    if (!entry || !entry->surface || width <= 0)
        return 0;
    const int height = entry->surface->heightForWidth(width);
    return qMax(entry->surface->minimumSizeHint().height(), height);
}

QVector<ZzInfoBarHostPrivate::Entry*> ZzInfoBarHostPrivate::activeEntries(
    ZzInfoBarHost::Position position) const
{
    QVector<Entry*> result;
    for (Entry* entry : entries) {
        if (entry->position == position && entry->active && !entry->closing && entry->bar
            && entry->surface)
            result.append(entry);
    }
    return result;
}

int ZzInfoBarHostPrivate::horizontalPosition(ZzInfoBarHost::Position position, int width) const
{
    if (position == ZzInfoBarHost::Top || position == ZzInfoBarHost::Bottom)
        return (target->width() - width) / 2;
    if (position == ZzInfoBarHost::TopRight || position == ZzInfoBarHost::BottomRight)
        return target->width() - margin - width;
    return margin;
}

void ZzInfoBarHostPrivate::fitActiveEntries(ZzInfoBarHost::Position position)
{
    if (!target)
        return;
    const int width = surfaceWidth();
    const qint64 available = qMax<qint64>(0, qint64(target->height()) - qint64(margin) * 2);
    qint64 used = 0;
    int count = 0;
    bool blocked = width <= 0 || available <= 0;
    for (Entry* entry : entries) {
        if (entry->position != position || !entry->active || entry->closing || !entry->bar
            || !entry->surface)
            continue;
        const qint64 required = qint64(count ? spacing : 0) + surfaceHeight(entry, width);
        if (blocked || used + required > available) {
            blocked = true;
            entry->active = false;
            entry->hovered = false;
            pauseTimer(entry);
            entry->surface->deactivateForQueue();
            continue;
        }
        used += required;
        ++count;
    }
}

void ZzInfoBarHostPrivate::repositionPosition(ZzInfoBarHost::Position position, bool animate)
{
    if (!target)
        return;
    const int width = surfaceWidth();
    const auto active = activeEntries(position);
    if (width <= 0) {
        for (Entry* entry : active)
            entry->surface->hide();
        return;
    }
    const int x = qMax(0, horizontalPosition(position, width));
    qint64 y = topPosition(position) ? margin : qint64(target->height()) - margin;
    for (Entry* entry : active) {
        const int height = surfaceHeight(entry, width);
        entry->surface->resize(width, height);
        if (topPosition(position)) {
            entry->surface->moveTo({ x, static_cast<int>(y) }, animate, 400);
            y += qint64(height) + spacing;
        } else {
            y -= height;
            entry->surface->moveTo({ x, static_cast<int>(y) }, animate, 300);
            y -= spacing;
        }
    }
}

bool ZzInfoBarHostPrivate::activatePending(ZzInfoBarHost::Position position, bool animateExisting)
{
    if (!target || !target->isVisible())
        return false;
    const int width = surfaceWidth();
    const qint64 available = qMax<qint64>(0, qint64(target->height()) - qint64(margin) * 2);
    if (width <= 0 || available <= 0)
        return false;
    for (Entry* entry : entries) {
        if (entry->position == position && entry->closing && entry->bar && entry->surface)
            return false;
    }
    int count = 0;
    qint64 used = 0;
    for (Entry* entry : entries) {
        if (entry->position != position || !entry->active || entry->closing || !entry->bar
            || !entry->surface)
            continue;
        used += qint64(count ? spacing : 0) + surfaceHeight(entry, width);
        ++count;
    }
    QVector<Entry*> activated;
    for (Entry* entry : entries) {
        if (entry->position != position || entry->active || entry->closing || !entry->bar
            || !entry->surface)
            continue;
        const qint64 required = qint64(count ? spacing : 0) + surfaceHeight(entry, width);
        if (used + required > available)
            break;
        entry->active = true;
        activated.append(entry);
        used += required;
        ++count;
    }
    if (activated.isEmpty())
        return false;
    repositionPosition(position, animateExisting);
    QPointer<ZzInfoBarHost> hostGuard(q);
    for (Entry* entry : activated) {
        if (!hostGuard)
            return true;
        if (!entries.contains(entry) || !entry->bar || !entry->surface)
            continue;
        entry->bar->setOpen(true);
        if (!hostGuard)
            return true;
        if (!entries.contains(entry) || !entry->bar || !entry->surface || !entry->bar->isOpen())
            continue;
        entry->surface->startEnter(position);
        entry->surface->show();
        entry->surface->raise();
        startTimer(entry);
        if (!hostGuard)
            return true;
        if (!entries.contains(entry) || !entry->bar || !entry->surface || !entry->active
            || entry->closing || !entry->bar->isOpen())
            continue;
        emit q->infoBarShown(entry->bar, position);
    }
    return true;
}

void ZzInfoBarHostPrivate::scheduleReposition(bool animate)
{
    animateReposition = animateReposition || animate;
    if (repositionScheduled)
        return;
    repositionScheduled = true;
    QTimer::singleShot(0, q, [this] {
        repositionScheduled = false;
        const bool shouldAnimate = animateReposition;
        animateReposition = false;
        repositionAll(shouldAnimate);
    });
}

void ZzInfoBarHostPrivate::repositionAll(bool animate)
{
    if (!target)
        return;
    if (repositioning) {
        scheduleReposition(animate);
        return;
    }
    repositioning = true;
    QPointer<ZzInfoBarHost> hostGuard(q);
    for (int index = 0; index < 6; ++index) {
        if (!hostGuard)
            return;
        const auto position = static_cast<ZzInfoBarHost::Position>(index);
        fitActiveEntries(position);
        if (!hostGuard)
            return;
        const bool activated = activatePending(position, animate);
        if (!hostGuard)
            return;
        if (!activated)
            repositionPosition(position, animate);
    }
    repositioning = false;
}

} // namespace ZzFluentUI
