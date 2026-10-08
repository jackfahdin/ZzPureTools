#include "private/ZzTimelinePrivate.h"
#include <QPointer>
#include <ZzFluentUI/ZzTimeline.h>
namespace ZzFluentUI {
ZzTimelineEvent::ZzTimelineEvent(QObject* parent)
    : QObject(parent)
    , d_ptr(std::make_unique<ZzTimelineEventPrivate>())
{
}
ZzTimelineEvent::ZzTimelineEvent(const QDateTime& timestamp, const QString& title, const QString& description,
    Status status, QObject* parent)
    : ZzTimelineEvent(parent)
{
    d_ptr->timestamp = timestamp;
    d_ptr->title = title;
    d_ptr->description = description;
    d_ptr->status = status >= Normal && status <= Error ? status : Normal;
}
ZzTimelineEvent::~ZzTimelineEvent() = default;
#define ZZ_EVENT_PROPERTY(Type, Name, Setter)                                                                \
    Type ZzTimelineEvent::Name() const { return d_ptr->Name; }                                               \
    void ZzTimelineEvent::Setter(Type value)                                                                 \
    {                                                                                                        \
        if (d_ptr->Name == value)                                                                            \
            return;                                                                                          \
        d_ptr->Name = value;                                                                                 \
        QPointer<ZzTimelineEvent> alive(this);                                                               \
        emit Name##Changed(value);                                                                           \
        if (alive)                                                                                           \
            emit itemChanged();                                                                              \
    }
ZZ_EVENT_PROPERTY(QDateTime, timestamp, setTimestamp)
ZZ_EVENT_PROPERTY(QString, timeText, setTimeText)
ZZ_EVENT_PROPERTY(QString, title, setTitle)
ZZ_EVENT_PROPERTY(QString, description, setDescription)
ZZ_EVENT_PROPERTY(QColor, color, setColor)
ZZ_EVENT_PROPERTY(QString, icon, setIcon)
#undef ZZ_EVENT_PROPERTY
ZzTimelineEvent::Status ZzTimelineEvent::status() const { return d_ptr->status; }
void ZzTimelineEvent::setStatus(Status value)
{
    if (value < Normal || value > Error || d_ptr->status == value)
        return;
    d_ptr->status = value;
    QPointer<ZzTimelineEvent> alive(this);
    emit statusChanged(value);
    if (alive)
        emit itemChanged();
}
ZzTimelineEvent::Placement ZzTimelineEvent::placement() const { return d_ptr->placement; }
void ZzTimelineEvent::setPlacement(Placement value)
{
    if (value < Automatic || value > RightSide || d_ptr->placement == value)
        return;
    d_ptr->placement = value;
    QPointer<ZzTimelineEvent> alive(this);
    emit placementChanged(value);
    if (alive)
        emit itemChanged();
}
void ZzTimelineEvent::setIcon(ZzSegoeIcon value) { setIcon(QString(QChar(static_cast<char16_t>(value)))); }
} // namespace ZzFluentUI
