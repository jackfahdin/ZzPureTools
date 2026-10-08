#include <ZzFluentUI/ZzInfoBarHost.h>

#include "private/ZzInfoBarHostPrivate.h"
#include "private/ZzInfoBarPopupSurface.h"

#include <QtCore/QEvent>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

namespace ZzFluentUI {
namespace {
    QPointer<QWidget> defaultInfoBarTarget;
    QPointer<ZzInfoBarHost> defaultInfoBarHost;
    QHash<ZzInfoBar*, QPointer<ZzInfoBarHost>> owners;

    bool validPosition(ZzInfoBarHost::Position position)
    {
        return position == ZzInfoBarHost::TopLeft || position == ZzInfoBarHost::Top
            || position == ZzInfoBarHost::TopRight || position == ZzInfoBarHost::BottomLeft
            || position == ZzInfoBarHost::Bottom || position == ZzInfoBarHost::BottomRight;
    }

    bool validSeverity(ZzInfoBar::Severity severity)
    {
        return severity == ZzInfoBar::Informational || severity == ZzInfoBar::Success
            || severity == ZzInfoBar::Warning || severity == ZzInfoBar::Error;
    }
} // namespace

ZzInfoBarHost::ZzInfoBarHost(QWidget* target, QObject* parent)
    : QObject(parent ? parent : target)
    , d_ptr(std::make_unique<ZzInfoBarHostPrivate>(this, target))
{
    if (target)
        target->installEventFilter(this);
    connect(this, &ZzInfoBarHost::infoBarClosed, this, [this](ZzInfoBar* bar, Position) {
        if (bar && owners.value(bar) == this)
            owners.remove(bar);
    });
}

ZzInfoBarHost::~ZzInfoBarHost()
{
    if (d_ptr->target)
        d_ptr->target->removeEventFilter(this);
    const auto entries = d_ptr->entries;
    d_ptr->entries.clear();
    for (auto* entry : entries) {
        if (entry->bar) {
            owners.remove(entry->bar.data());
            entry->bar->removeEventFilter(this);
            disconnect(entry->bar, nullptr, this, nullptr);
        }
        if (entry->timer)
            entry->timer->stop();
    }
    for (auto* entry : entries) {
        delete entry->surface;
        delete entry;
    }
}

void ZzInfoBarHost::setDefaultTarget(QWidget* target)
{
    if (defaultInfoBarTarget == target)
        return;
    ZzInfoBarHost* old = defaultInfoBarHost.data();
    defaultInfoBarHost = nullptr;
    defaultInfoBarTarget = target;
    delete old;
}
QWidget* ZzInfoBarHost::defaultTarget() { return defaultInfoBarTarget.data(); }
ZzInfoBarHost* ZzInfoBarHost::defaultHost()
{
    QWidget* target = defaultInfoBarTarget.data();
    if (!target)
        return nullptr;
    if (!defaultInfoBarHost || defaultInfoBarHost->target() != target)
        defaultInfoBarHost = new ZzInfoBarHost(target, target);
    return defaultInfoBarHost.data();
}
QWidget* ZzInfoBarHost::target() const { return d_ptr->target.data(); }
int ZzInfoBarHost::margin() const { return d_ptr->margin; }
void ZzInfoBarHost::setMargin(int margin)
{
    margin = qMax(0, margin);
    if (d_ptr->margin == margin)
        return;
    d_ptr->margin = margin;
    d_ptr->scheduleReposition();
    emit marginChanged(margin);
}
int ZzInfoBarHost::spacing() const { return d_ptr->spacing; }
void ZzInfoBarHost::setSpacing(int spacing)
{
    spacing = qMax(0, spacing);
    if (d_ptr->spacing == spacing)
        return;
    d_ptr->spacing = spacing;
    d_ptr->scheduleReposition();
    emit spacingChanged(spacing);
}
int ZzInfoBarHost::maximumWidth() const { return d_ptr->maximumWidth; }
void ZzInfoBarHost::setMaximumWidth(int width)
{
    width = qMax(160, width);
    if (d_ptr->maximumWidth == width)
        return;
    d_ptr->maximumWidth = width;
    d_ptr->scheduleReposition();
    emit maximumWidthChanged(width);
}
int ZzInfoBarHost::defaultTimeout() const { return d_ptr->defaultTimeout; }
void ZzInfoBarHost::setDefaultTimeout(int milliseconds)
{
    milliseconds = qMax(0, milliseconds);
    if (d_ptr->defaultTimeout == milliseconds)
        return;
    d_ptr->defaultTimeout = milliseconds;
    emit defaultTimeoutChanged(milliseconds);
}

ZzInfoBar* ZzInfoBarHost::showInfoBar(ZzInfoBar::Severity severity, const QString& title,
    const QString& message, Position position, int timeout)
{
    if (!d_ptr->target || !validSeverity(severity) || !validPosition(position))
        return nullptr;
    auto* bar = new ZzInfoBar;
    bar->setSeverity(severity);
    bar->setTitle(title);
    bar->setMessage(message);
    QPointer<ZzInfoBar> guard(bar);
    addInfoBar(bar, position, timeout);
    return guard.data();
}

void ZzInfoBarHost::addInfoBar(ZzInfoBar* bar, Position position, int timeout)
{
    if (!bar || !d_ptr->target || !validPosition(position) || bar == d_ptr->target
        || bar->isAncestorOf(d_ptr->target) || d_ptr->findEntry(bar))
        return;
    QPointer<ZzInfoBar> guarded(bar);
    QPointer<ZzInfoBarHost> self(this);
    if (ZzInfoBarHost* owner = owners.value(bar)) {
        if (owner == this)
            return;
        owner->d_ptr->removeEntry(owner->d_ptr->findEntry(bar), true);
        if (!guarded || !self || owners.contains(bar))
            return;
    }
    auto* entry = new ZzInfoBarHostPrivate::Entry;
    entry->bar = bar;
    entry->position = position;
    entry->timeout = timeout < 0 ? d_ptr->defaultTimeout : qMax(0, timeout);
    d_ptr->entries.append(entry);
    owners.insert(bar, this);

    auto* surface = new ZzInfoBarPopupSurface(d_ptr->target);
    entry->surface = surface;
    bar->setParent(surface);
    bar->setProperty("_zzInfoBarPopupSurface", true);
    bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    bar->installEventFilter(this);
    surface->setInfoBar(bar);
    surface->hide();

    connect(bar, &ZzInfoBar::openChanged, this, [this, entry](bool open) {
        QPointer<ZzInfoBarHost> hostGuard(this);
        if (!d_ptr->entries.contains(entry))
            return;
        if (open) {
            if (entry->closing) {
                entry->closing = false;
                if (entry->surface)
                    entry->surface->cancelLeave();
                QPointer<ZzInfoBar> barGuard(entry->bar);
                d_ptr->resumeTimer(entry);
                if (!hostGuard || !barGuard || d_ptr->findEntry(barGuard) != entry || entry->closing
                    || !barGuard->isOpen())
                    return;
                d_ptr->repositionPosition(entry->position, true);
                d_ptr->scheduleReposition(true);
            }
            return;
        }
        entry->closing = true;
        d_ptr->pauseTimer(entry);
        const Position closingPosition = entry->position;
        const QPointer<ZzInfoBar> guard = entry->bar;
        if (entry->surface && entry->surface->isVisible()) {
            entry->surface->startLeave([guard] {
                if (guard)
                    guard->finishPopupClose();
            });
        } else if (guard) {
            guard->finishPopupClose();
        }
        if (!hostGuard)
            return;
        d_ptr->repositionPosition(closingPosition, true);
    });
    connect(bar, &ZzInfoBar::closed, this, [this, entry] { d_ptr->removeEntry(entry); });
    connect(bar, &QObject::destroyed, this, [this, entry, bar] {
        if (owners.value(bar) == this)
            owners.remove(bar);
        if (d_ptr->entries.contains(entry)) {
            entry->bar = nullptr;
            d_ptr->removeEntry(entry);
        }
    });
    d_ptr->repositionAll();
}

void ZzInfoBarHost::dismissAll()
{
    const auto entries = d_ptr->entries;
    QPointer<ZzInfoBarHost> guard(this);
    for (auto* entry : entries) {
        if (!guard)
            return;
        if (!d_ptr->entries.contains(entry) || !entry->bar || entry->closing)
            continue;
        entry->closing = true;
        if (entry->bar->isOpen())
            entry->bar->dismiss();
        else
            entry->bar->finishPopupClose();
    }
}
void ZzInfoBarHost::dismissAll(Position position)
{
    const auto entries = d_ptr->entries;
    QPointer<ZzInfoBarHost> guard(this);
    for (auto* entry : entries) {
        if (!guard)
            return;
        if (!d_ptr->entries.contains(entry) || entry->position != position || !entry->bar
            || entry->closing)
            continue;
        entry->closing = true;
        if (entry->bar->isOpen())
            entry->bar->dismiss();
        else
            entry->bar->finishPopupClose();
    }
}

bool ZzInfoBarHost::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == d_ptr->target) {
        if (event->type() == QEvent::Hide) {
            for (auto* entry : d_ptr->entries)
                d_ptr->pauseTimer(entry);
        } else if (event->type() == QEvent::Show) {
            const auto entries = d_ptr->entries;
            QPointer<ZzInfoBarHost> hostGuard(this);
            QPointer<QObject> watchedGuard(watched);
            for (auto* entry : entries) {
                if (!hostGuard)
                    return false;
                if (!d_ptr->entries.contains(entry))
                    continue;
                d_ptr->resumeTimer(entry);
                if (!watchedGuard)
                    return true;
                if (!hostGuard)
                    return false;
            }
            d_ptr->scheduleReposition();
        } else if (event->type() == QEvent::Resize || event->type() == QEvent::Move
            || event->type() == QEvent::LayoutRequest
            || event->type() == QEvent::WindowStateChange) {
            d_ptr->scheduleReposition();
        }
    } else if (auto* bar = qobject_cast<ZzInfoBar*>(watched)) {
        auto* entry = d_ptr->findEntry(bar);
        if (entry) {
            if (event->type() == QEvent::Enter) {
                entry->hovered = true;
                d_ptr->pauseTimer(entry);
            } else if (event->type() == QEvent::Leave) {
                entry->hovered = false;
                QPointer<ZzInfoBarHost> hostGuard(this);
                QPointer<QObject> watchedGuard(watched);
                d_ptr->resumeTimer(entry);
                if (!watchedGuard)
                    return true;
                if (!hostGuard)
                    return false;
            } else if ((event->type() == QEvent::Resize || event->type() == QEvent::Show
                           || event->type() == QEvent::Hide
                           || event->type() == QEvent::LayoutRequest)
                && !d_ptr->repositioning) {
                d_ptr->scheduleReposition();
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

} // namespace ZzFluentUI
