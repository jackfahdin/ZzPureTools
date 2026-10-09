#pragma once
#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QSet>
namespace ZzFluentUI {
class ZzTimelineEvent;
class ZzTimeline;
/** @brief 保持插入顺序存储，只在视图索引层应用反序。 */
class ZzTimelineModel final : public QAbstractListModel {
public:
    explicit ZzTimelineModel(ZzTimeline* owner);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QList<ZzTimelineEvent*> events() const;
    [[nodiscard]] ZzTimelineEvent* eventAt(int visualIndex) const;
    [[nodiscard]] bool contains(const ZzTimelineEvent* event) const;
    [[nodiscard]] bool invalidateEvent(ZzTimelineEvent* event);
    [[nodiscard]] bool isInvalid(const ZzTimelineEvent* event) const;
    void restoreEvent(ZzTimelineEvent* event);
    void appendEvent(ZzTimelineEvent* event);
    bool takeEvent(ZzTimelineEvent* event);
    QList<ZzTimelineEvent*> takeAllEvents();
    void notifyEventChanged(ZzTimelineEvent* event);
    void setReverse(bool reverse);
    [[nodiscard]] bool isMutating() const;

private:
    enum class ZzTimelineOperation { Append, Take, Reset, Reverse };
    struct ZzPendingTimelineOperation {
        ZzTimelineOperation type;
        ZzTimelineEvent* event = nullptr;
        QPointer<ZzTimelineEvent> liveEvent;
        bool reverse = false;
    };
    bool cancelPendingAppend(ZzTimelineEvent* event);
    void drainPending();
    QList<ZzTimelineEvent*> m_events;
    QPointer<ZzTimeline> m_owner;
    QHash<ZzTimelineEvent*, QPointer<ZzTimelineEvent>> m_live;
    bool m_reverse = false;
    QSet<ZzTimelineEvent*> m_removing;
    QSet<ZzTimelineEvent*> m_invalid;
    QSet<ZzTimelineEvent*> m_pendingAdds;
    ZzTimelineEvent* m_inserting = nullptr;
    QList<ZzPendingTimelineOperation> m_pending;
    bool m_mutating = false;
};
} // namespace ZzFluentUI
