#include "ZzTimelineModel.h"
#include <QPointer>
#include <ZzFluentUI/ZzTimeline.h>
#include <utility>
namespace ZzFluentUI {
ZzTimelineModel::ZzTimelineModel(ZzTimeline* owner)
    : QAbstractListModel(nullptr)
    , m_owner(owner)
{
}
int ZzTimelineModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_events.size());
}
QVariant ZzTimelineModel::data(const QModelIndex& index, int role) const
{
    auto* event = index.isValid() && index.column() == 0 ? eventAt(index.row()) : nullptr;
    if (!event || m_invalid.contains(event))
        return {};
    if (role == Qt::DisplayRole)
        return event->title();
    if (role == Qt::ToolTipRole)
        return event->description();
    if (role == Qt::AccessibleTextRole)
        return event->description().isEmpty() ? event->title()
                                              : event->title() + QStringLiteral(", ") + event->description();
    return {};
}
Qt::ItemFlags ZzTimelineModel::flags(const QModelIndex& index) const
{
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}
QList<ZzTimelineEvent*> ZzTimelineModel::events() const
{
    QList<ZzTimelineEvent*> result;
    for (auto* event : m_events)
        if (!m_invalid.contains(event) && m_live.value(event))
            result.append(event);
    return result;
}
ZzTimelineEvent* ZzTimelineModel::eventAt(int visualIndex) const
{
    if (visualIndex < 0 || visualIndex >= m_events.size())
        return nullptr;
    auto* event = m_events.at(m_reverse ? m_events.size() - 1 - visualIndex : visualIndex);
    return m_invalid.contains(event) || !m_live.value(event) ? nullptr : event;
}
bool ZzTimelineModel::contains(const ZzTimelineEvent* event) const
{
    auto* item = const_cast<ZzTimelineEvent*>(event);
    return m_events.contains(item) || m_pendingAdds.contains(item) || m_inserting == item;
}
bool ZzTimelineModel::invalidateEvent(ZzTimelineEvent* event)
{
    if (!contains(event))
        return false;
    if (cancelPendingAppend(event))
        return true;
    if (m_invalid.contains(event))
        return false;
    m_invalid.insert(event);
    return true;
}
bool ZzTimelineModel::isInvalid(const ZzTimelineEvent* event) const
{
    return m_invalid.contains(const_cast<ZzTimelineEvent*>(event));
}
void ZzTimelineModel::restoreEvent(ZzTimelineEvent* event)
{
    if (m_owner && event && event->parent() == m_owner && m_invalid.remove(event))
        notifyEventChanged(event);
}
void ZzTimelineModel::appendEvent(ZzTimelineEvent* event)
{
    if (!event || contains(event))
        return;
    if (m_mutating) {
        m_pendingAdds.insert(event);
        m_pending.append({ ZzTimelineOperation::Append, event, event, false });
        return;
    }
    if (!m_owner || event->parent() != m_owner)
        return;
    const int row = m_reverse ? 0 : int(m_events.size());
    m_mutating = true;
    m_inserting = event;
    QPointer<QObject> alive(this);
    QPointer<ZzTimelineEvent> eventAlive(event);
    beginInsertRows({}, row, row);
    if (!alive)
        return;
    if (!eventAlive || !m_owner || eventAlive->parent() != m_owner)
        m_invalid.insert(event);
    m_events.append(event);
    m_live.insert(event, eventAlive);
    endInsertRows();
    if (!alive)
        return;
    m_inserting = nullptr;
    m_mutating = false;
    if (m_invalid.contains(event))
        takeEvent(event);
    drainPending();
}
bool ZzTimelineModel::takeEvent(ZzTimelineEvent* event)
{
    if (m_removing.contains(event))
        return false;
    const qsizetype source = m_events.indexOf(event);
    if (source < 0) {
        if (cancelPendingAppend(event))
            return true;
        if (m_inserting == event) {
            m_invalid.insert(event);
            return true;
        }
        return false;
    }
    if (m_mutating) {
        m_invalid.insert(event);
        m_pending.append({ ZzTimelineOperation::Take, event, {}, false });
        return true;
    }
    const int row = int(m_reverse ? m_events.size() - 1 - source : source);
    m_removing.insert(event);
    m_mutating = true;
    QPointer<QObject> alive(this);
    beginRemoveRows({}, row, row);
    if (!alive)
        return false;
    m_events.removeAt(source);
    m_live.remove(event);
    endRemoveRows();
    if (!alive)
        return false;
    m_removing.remove(event);
    m_invalid.remove(event);
    m_mutating = false;
    drainPending();
    return true;
}
bool ZzTimelineModel::cancelPendingAppend(ZzTimelineEvent* event)
{
    if (!m_pendingAdds.remove(event))
        return false;
    for (qsizetype index = m_pending.size(); index > 0; --index)
        if (m_pending.at(index - 1).type == ZzTimelineOperation::Append && m_pending.at(index - 1).event == event)
            m_pending.removeAt(index - 1);
    return true;
}
QList<ZzTimelineEvent*> ZzTimelineModel::takeAllEvents()
{
    if (m_events.isEmpty())
        return {};
    if (m_mutating) {
        m_pending.append({ ZzTimelineOperation::Reset, nullptr, {}, false });
        return m_events;
    }
    m_mutating = true;
    QPointer<QObject> alive(this);
    beginResetModel();
    if (!alive)
        return {};
    auto old = std::exchange(m_events, {});
    m_live.clear();
    endResetModel();
    if (!alive)
        return {};
    m_invalid.clear();
    m_mutating = false;
    drainPending();
    return old;
}
void ZzTimelineModel::notifyEventChanged(ZzTimelineEvent* event)
{
    const qsizetype source = m_events.indexOf(event);
    if (source < 0 || m_invalid.contains(event) || !m_live.value(event))
        return;
    const int row = int(m_reverse ? m_events.size() - 1 - source : source);
    const bool wasMutating = m_mutating;
    m_mutating = true;
    QPointer<QObject> alive(this);
    emit dataChanged(
        index(row, 0), index(row, 0), { Qt::DisplayRole, Qt::ToolTipRole, Qt::AccessibleTextRole });
    if (!alive)
        return;
    m_mutating = wasMutating;
    if (!wasMutating)
        drainPending();
}
void ZzTimelineModel::setReverse(bool reverse)
{
    if (m_mutating) {
        for (qsizetype index = m_pending.size(); index > 0; --index)
            if (m_pending.at(index - 1).type == ZzTimelineOperation::Reverse)
                m_pending.removeAt(index - 1);
        m_pending.append({ ZzTimelineOperation::Reverse, nullptr, {}, reverse });
        return;
    }
    if (m_reverse == reverse)
        return;
    m_mutating = true;
    QPointer<QObject> alive(this);
    beginResetModel();
    if (!alive)
        return;
    m_reverse = reverse;
    endResetModel();
    if (!alive)
        return;
    m_mutating = false;
    drainPending();
}
void ZzTimelineModel::drainPending()
{
    while (!m_mutating && !m_pending.isEmpty()) {
        QPointer<QObject> alive(this);
        const ZzPendingTimelineOperation operation = m_pending.takeFirst();
        switch (operation.type) {
        case ZzTimelineOperation::Append:
            if (m_pendingAdds.remove(operation.event) && operation.liveEvent && m_owner
                && operation.liveEvent->parent() == m_owner)
                appendEvent(operation.liveEvent);
            break;
        case ZzTimelineOperation::Take:
            takeEvent(operation.event);
            break;
        case ZzTimelineOperation::Reset:
            takeAllEvents();
            break;
        case ZzTimelineOperation::Reverse:
            setReverse(operation.reverse);
            break;
        }
        if (!alive)
            return;
    }
}
bool ZzTimelineModel::isMutating() const { return m_mutating; }
} // namespace ZzFluentUI
