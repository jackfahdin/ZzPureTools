#include "private/ZzTimelineDelegate.h"
#include "private/ZzTimelineModel.h"
#include "private/ZzTimelinePrivate.h"
#include <QAbstractAnimation>
#include <QChildEvent>
#include <QEasingCurve>
#include <QEvent>
#include <QHideEvent>
#include <QPointer>
#include <QShowEvent>
#include <QStyle>
#include <QtMath>
#include <ZzFluentUI/ZzTimeline.h>
namespace ZzFluentUI {
ZzTimeline::ZzTimeline(QWidget* parent)
    : QListView(parent)
    , d_ptr(std::make_unique<ZzTimelinePrivate>())
{
    d_ptr->model = new ZzTimelineModel(this);
    d_ptr->delegate = new ZzTimelineDelegate(this, d_ptr->model);
    QListView::setModel(d_ptr->model);
    QListView::setItemDelegate(d_ptr->delegate);
    connect(
        d_ptr->model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int first, int) {
            if (!d_ptr->model->eventAt(first))
                return;
            refreshItemLayout();
            updateAnimationState();
            emit eventsChanged();
        });
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(false);
    viewport()->setAutoFillBackground(false);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setResizeMode(QListView::Adjust);
    setSelectionMode(QAbstractItemView::NoSelection);
    setMouseTracking(true);
    setSpacing(0);
    setUniformItemSizes(false);
    applyViewOrientation();
    d_ptr->pulseAnimation = new QVariantAnimation(this);
    d_ptr->pulseAnimation->setStartValue(0.0);
    d_ptr->pulseAnimation->setEndValue(1.0);
    d_ptr->pulseAnimation->setLoopCount(-1);
    d_ptr->pulseAnimation->setEasingCurve(QEasingCurve::InOutSine);
    d_ptr->pulseAnimation->setDuration(d_ptr->animationDuration);
    connect(d_ptr->pulseAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        d_ptr->pulseProgress = value.toReal();
        viewport()->update();
    });
    connect(this, &QListView::clicked, this, [this](const QModelIndex& index) {
        if (auto* event = eventAt(index.row()))
            emit eventClicked(event);
    });
    connect(this, &QListView::activated, this, [this](const QModelIndex& index) {
        if (auto* event = eventAt(index.row()))
            emit eventActivated(event);
    });
}
ZzTimeline::~ZzTimeline()
{
    d_ptr->pulseAnimation->stop();
    for (auto* event : d_ptr->model->events())
        disconnect(event, nullptr, this, nullptr);
    for (auto* event : findChildren<ZzTimelineEvent*>(QString(), Qt::FindDirectChildrenOnly))
        disconnect(event, nullptr, this, nullptr);
    if (d_ptr->model->isMutating())
        d_ptr->model->deleteLater();
    else
        delete d_ptr->model;
}
#define ZZ_LAYOUT_PROPERTY(Type, Name, Getter, Setter)                                                       \
    Type ZzTimeline::Getter() const { return d_ptr->Name; }                                                  \
    void ZzTimeline::Setter(Type value)                                                                      \
    {                                                                                                        \
        if (d_ptr->Name == value)                                                                            \
            return;                                                                                          \
        d_ptr->Name = value;                                                                                 \
        refreshItemLayout();                                                                                 \
        emit Name##Changed(value);                                                                           \
    }
Qt::Orientation ZzTimeline::orientation() const { return d_ptr->orientation; }
void ZzTimeline::setOrientation(Qt::Orientation value)
{
    if ((value != Qt::Vertical && value != Qt::Horizontal) || d_ptr->orientation == value)
        return;
    d_ptr->orientation = value;
    applyViewOrientation();
    refreshItemLayout();
    emit orientationChanged(value);
}
ZzTimeline::LayoutMode ZzTimeline::layoutMode() const { return d_ptr->layoutMode; }
void ZzTimeline::setLayoutMode(LayoutMode value)
{
    if (value < ContentOnRight || value > AlternatingReverse || d_ptr->layoutMode == value)
        return;
    d_ptr->layoutMode = value;
    refreshItemLayout();
    emit layoutModeChanged(value);
}
bool ZzTimeline::isReverse() const { return d_ptr->reverse; }
void ZzTimeline::setReverse(bool value)
{
    if (d_ptr->reverse == value)
        return;
    d_ptr->reverse = value;
    QPointer<ZzTimeline> self(this);
    d_ptr->model->setReverse(value);
    if (!self)
        return;
    refreshItemLayout();
    emit reverseChanged(value);
}
ZZ_LAYOUT_PROPERTY(bool, timestampVisible, isTimestampVisible, setTimestampVisible)
ZZ_LAYOUT_PROPERTY(bool, descriptionVisible, isDescriptionVisible, setDescriptionVisible)
ZZ_LAYOUT_PROPERTY(QString, timestampFormat, timestampFormat, setTimestampFormat)
ZZ_LAYOUT_PROPERTY(QColor, lineColor, lineColor, setLineColor)
#undef ZZ_LAYOUT_PROPERTY
#define ZZ_CLAMPED_PROPERTY(Name, Getter, Setter, Minimum, Maximum)                                          \
    int ZzTimeline::Getter() const { return d_ptr->Name; }                                                   \
    void ZzTimeline::Setter(int value)                                                                       \
    {                                                                                                        \
        value = qBound(Minimum, value, Maximum);                                                             \
        if (d_ptr->Name == value)                                                                            \
            return;                                                                                          \
        d_ptr->Name = value;                                                                                 \
        refreshItemLayout();                                                                                 \
        emit Name##Changed(value);                                                                           \
    }
ZZ_CLAMPED_PROPERTY(timestampWidth, timestampWidth, setTimestampWidth, 32, 400)
ZZ_CLAMPED_PROPERTY(nodeSize, nodeSize, setNodeSize, 6, 64)
ZZ_CLAMPED_PROPERTY(itemSpacing, itemSpacing, setItemSpacing, 0, 160)
ZZ_CLAMPED_PROPERTY(horizontalItemWidth, horizontalItemWidth, setHorizontalItemWidth, 120, 640)
ZZ_CLAMPED_PROPERTY(contentPadding, contentPadding, setContentPadding, 0, 160)
ZZ_CLAMPED_PROPERTY(titleFontPixelSize, titleFontPixelSize, setTitleFontPixelSize, 0, 96)
ZZ_CLAMPED_PROPERTY(descriptionFontPixelSize, descriptionFontPixelSize, setDescriptionFontPixelSize, 0, 96)
ZZ_CLAMPED_PROPERTY(timestampFontPixelSize, timestampFontPixelSize, setTimestampFontPixelSize, 0, 96)
#undef ZZ_CLAMPED_PROPERTY
qreal ZzTimeline::lineWidth() const { return d_ptr->lineWidth; }
void ZzTimeline::setLineWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound(0.5, value, 24.0);
    if (qFuzzyCompare(d_ptr->lineWidth + 1.0, value + 1.0))
        return;
    d_ptr->lineWidth = value;
    viewport()->update();
    emit lineWidthChanged(value);
}
bool ZzTimeline::isAnimationEnabled() const { return d_ptr->animationEnabled; }
void ZzTimeline::setAnimationEnabled(bool value)
{
    if (d_ptr->animationEnabled == value)
        return;
    d_ptr->animationEnabled = value;
    updateAnimationState();
    emit animationEnabledChanged(value);
}
int ZzTimeline::animationDuration() const { return d_ptr->animationDuration; }
void ZzTimeline::setAnimationDuration(int value)
{
    value = qBound(200, value, 10000);
    if (d_ptr->animationDuration == value)
        return;
    d_ptr->animationDuration = value;
    d_ptr->pulseAnimation->setDuration(value);
    emit animationDurationChanged(value);
}
QList<ZzTimelineEvent*> ZzTimeline::events() const { return d_ptr->model->events(); }
ZzTimelineEvent* ZzTimeline::eventAt(int visualIndex) const { return d_ptr->model->eventAt(visualIndex); }
ZzTimelineEvent* ZzTimeline::addEvent(const QDateTime& timestamp, const QString& title,
    const QString& description, ZzTimelineEvent::Status status)
{
    auto* event = new ZzTimelineEvent(timestamp, title, description, status, this);
    QPointer<ZzTimeline> self(this);
    QPointer<ZzTimelineEvent> alive(event);
    addEvent(event);
    return self && alive && d_ptr->model->contains(event) && event->parent() == this ? event : nullptr;
}
void ZzTimeline::addEvent(ZzTimelineEvent* event)
{
    if (!event)
        return;
    if (d_ptr->model->contains(event)) {
        if (!d_ptr->model->isInvalid(event))
            return;
        QPointer<ZzTimeline> self(this);
        QPointer<ZzTimelineEvent> alive(event);
        event->setParent(this);
        if (!self || !alive || event->parent() != this)
            return;
        d_ptr->model->restoreEvent(event);
        if (!self || !alive || !d_ptr->model->contains(event) || d_ptr->model->isInvalid(event)
            || event->parent() != this)
            return;
        refreshItemLayout();
        updateAnimationState();
        emit eventsChanged();
        return;
    }
    QPointer<ZzTimeline> self(this);
    QPointer<ZzTimelineEvent> alive(event);
    if (auto* previous = qobject_cast<ZzTimeline*>(event->parent())) {
        if (previous != this)
            previous->takeEvent(event);
    }
    if (!self || !alive)
        return;
    event->setParent(this);
    if (!self || !alive)
        return;
    event->installEventFilter(this);
    connectEvent(event);
    d_ptr->model->appendEvent(event);
    if (!self || !alive)
        return;
}
ZzTimelineEvent* ZzTimeline::takeEvent(ZzTimelineEvent* event)
{
    if (!event || !d_ptr->model->contains(event))
        return nullptr;
    QPointer<ZzTimeline> self(this);
    QPointer<ZzTimelineEvent> alive(event);
    d_ptr->model->takeEvent(event);
    if (!self)
        return nullptr;
    if (!alive) {
        refreshItemLayout();
        updateAnimationState();
        QMetaObject::invokeMethod(this, [this] { emit eventsChanged(); }, Qt::QueuedConnection);
        return nullptr;
    }
    disconnect(event, nullptr, this, nullptr);
    event->removeEventFilter(this);
    if (event->parent() == this)
        event->setParent(nullptr);
    refreshItemLayout();
    updateAnimationState();
    emit eventsChanged();
    return alive && !alive->parent() ? alive.data() : nullptr;
}
void ZzTimeline::removeEvent(ZzTimelineEvent* event)
{
    QPointer<ZzTimelineEvent> alive(takeEvent(event));
    if (alive && !alive->parent())
        alive->deleteLater();
}
void ZzTimeline::clearEvents()
{
    if (d_ptr->model->rowCount() == 0)
        return;
    QPointer<ZzTimeline> self(this);
    QList<QPointer<ZzTimelineEvent>> old;
    for (auto* event : d_ptr->model->events())
        old.append(event);
    d_ptr->model->takeAllEvents();
    if (!self)
        return;
    for (auto event : old) {
        if (!event)
            continue;
        disconnect(event, nullptr, this, nullptr);
        event->removeEventFilter(this);
        if (event->parent() == this)
            event->setParent(nullptr);
    }
    refreshItemLayout();
    updateAnimationState();
    emit eventsChanged();
    for (auto event : old)
        if (event && !event->parent())
            event->deleteLater();
}
QSize ZzTimeline::sizeHint() const
{
    return d_ptr->orientation == Qt::Horizontal ? QSize(720, 280) : QSize(640, 420);
}
QSize ZzTimeline::minimumSizeHint() const
{
    return d_ptr->orientation == Qt::Horizontal ? QSize(320, 180) : QSize(280, 160);
}
void ZzTimeline::showEvent(QShowEvent* event)
{
    QListView::showEvent(event);
    updateAnimationState();
}
void ZzTimeline::hideEvent(QHideEvent* event)
{
    QListView::hideEvent(event);
    updateAnimationState();
}
void ZzTimeline::changeEvent(QEvent* event)
{
    QListView::changeEvent(event);
    if (event
        && (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange
            || event->type() == QEvent::StyleChange))
        refreshItemLayout();
    if (event && (event->type() == QEvent::EnabledChange || event->type() == QEvent::StyleChange))
        updateAnimationState();
}
bool ZzTimeline::eventFilter(QObject* watched, QEvent* event)
{
    if (event && event->type() == QEvent::ParentChange) {
        auto* item = qobject_cast<ZzTimelineEvent*>(watched);
        if (item && d_ptr->model->contains(item) && item->parent() != this) {
            markExternalDetach(item);
        }
    }
    return QListView::eventFilter(watched, event);
}
void ZzTimeline::childEvent(QChildEvent* event)
{
    if (d_ptr && d_ptr->model && event && event->removed())
        markExternalDetach(reinterpret_cast<ZzTimelineEvent*>(event->child()));
    QListView::childEvent(event);
}
QString ZzTimeline::formattedTimestamp(const ZzTimelineEvent* event) const
{
    if (!event)
        return {};
    if (!event->timeText().isEmpty())
        return event->timeText();
    return event->timestamp().isValid() ? event->timestamp().toString(d_ptr->timestampFormat) : QString();
}
QColor ZzTimeline::resolvedEventColor(const ZzTimelineEvent* event, QPalette::ColorGroup group) const
{
    if (event && event->color().isValid() && group != QPalette::Disabled)
        return event->color();
    if (group == QPalette::Disabled)
        return palette().color(QPalette::Disabled, QPalette::Accent);
    if (!event)
        return palette().color(group, QPalette::Accent);
    switch (event->status()) {
    case ZzTimelineEvent::Completed:
        return QColor(QStringLiteral("#107C10"));
    case ZzTimelineEvent::Pending: {
        const QColor base = palette().color(group, QPalette::Base);
        const QColor text = palette().color(group, QPalette::Text);
        return QColor(qRound(base.red() * 0.78 + text.red() * 0.22),
            qRound(base.green() * 0.78 + text.green() * 0.22),
            qRound(base.blue() * 0.78 + text.blue() * 0.22));
    }
    case ZzTimelineEvent::Warning:
        return QColor(QStringLiteral("#F2A900"));
    case ZzTimelineEvent::Error:
        return QColor(QStringLiteral("#D13438"));
    default:
        return palette().color(group, QPalette::Accent);
    }
}
qreal ZzTimeline::pulseProgress() const { return d_ptr->pulseProgress; }
void ZzTimeline::applyViewOrientation()
{
    const bool horizontal = d_ptr->orientation == Qt::Horizontal;
    setFlow(horizontal ? QListView::LeftToRight : QListView::TopToBottom);
    setWrapping(false);
    setHorizontalScrollBarPolicy(horizontal ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(horizontal ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}
void ZzTimeline::connectEvent(ZzTimelineEvent* event)
{
    QPointer<ZzTimelineEvent> live(event);
    connect(event, &ZzTimelineEvent::itemChanged, this, [this, live] {
        if (!live || live->parent() != this || !d_ptr->model->contains(live) || d_ptr->model->isInvalid(live))
            return;
        QPointer<ZzTimeline> alive(this);
        d_ptr->model->notifyEventChanged(live);
        if (!alive)
            return;
        refreshItemLayout();
        updateAnimationState();
        emit eventsChanged();
    });
    connect(event, &QObject::destroyed, this, [this, event] { markExternalDetach(event); });
}
void ZzTimeline::markExternalDetach(ZzTimelineEvent* event)
{
    if (!d_ptr->model->invalidateEvent(event))
        return;
    QMetaObject::invokeMethod(
        this,
        [this, event] {
            if (!d_ptr->model->isInvalid(event))
                return;
            QPointer<ZzTimeline> self(this);
            if (!d_ptr->model->takeEvent(event) || !self)
                return;
            refreshItemLayout();
            updateAnimationState();
            emit eventsChanged();
        },
        Qt::QueuedConnection);
}
void ZzTimeline::refreshItemLayout()
{
    scheduleDelayedItemsLayout();
    updateGeometry();
    viewport()->update();
}
void ZzTimeline::updateAnimationState()
{
    bool hasCurrent = false;
    for (auto* event : d_ptr->model->events())
        if (event && event->status() == ZzTimelineEvent::Current) {
            hasCurrent = true;
            break;
        }
    if (d_ptr->animationEnabled && isVisible() && isEnabled() && hasCurrent
        && style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this) != 0) {
        if (d_ptr->pulseAnimation->state() != QAbstractAnimation::Running)
            d_ptr->pulseAnimation->start();
    } else {
        d_ptr->pulseAnimation->stop();
        d_ptr->pulseProgress = 0.0;
        viewport()->update();
    }
}
} // namespace ZzFluentUI
