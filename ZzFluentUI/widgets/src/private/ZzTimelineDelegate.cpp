#include "ZzTimelineDelegate.h"
#include "ZzTimelineModel.h"
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionViewItem>
#include <QtMath>
#include <ZzFluentUI/ZzTimeline.h>
#include <algorithm>

namespace ZzFluentUI {
namespace {
    constexpr qreal TextGap = 4.0;
    constexpr qreal AxisGap = 12.0;
    constexpr qreal MinimumEventHeight = 44.0;

    QColor railColor(const QPalette& palette, QPalette::ColorGroup group)
    {
        const QColor background = palette.color(group, QPalette::Base);
        const QColor text = palette.color(group, QPalette::Text);
        const qreal ratio = group == QPalette::Disabled ? 0.12 : 0.22;
        return QColor(qRound(background.red() * (1 - ratio) + text.red() * ratio),
            qRound(background.green() * (1 - ratio) + text.green() * ratio),
            qRound(background.blue() * (1 - ratio) + text.blue() * ratio));
    }
    QPalette::ColorGroup colorGroup(const QStyleOptionViewItem& option)
    {
        if (!(option.state & QStyle::State_Enabled))
            return QPalette::Disabled;
        return (option.state & QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
    }
    void drawHighlight(QPainter* painter, const QStyleOptionViewItem& option, QPalette::ColorGroup group)
    {
        if (!(option.state & (QStyle::State_Selected | QStyle::State_MouseOver)))
            return;
        QColor background = option.palette.color(group, QPalette::Highlight);
        background.setAlpha((option.state & QStyle::State_Selected) ? 34 : 18);
        painter->setPen(Qt::NoPen);
        painter->setBrush(background);
        painter->drawRoundedRect(QRectF(option.rect).adjusted(2, 1, -2, -2), 6, 6);
    }
} // namespace

ZzTimelineDelegate::ZzTimelineDelegate(ZzTimeline* timeline, ZzTimelineModel* model)
    : QStyledItemDelegate(timeline)
    , m_timeline(timeline)
    , m_model(model)
{
}

QSize ZzTimelineDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto* event = m_model->eventAt(index.row());
    if (!event)
        return QStyledItemDelegate::sizeHint(option, index);
    if (m_timeline->orientation() == Qt::Horizontal)
        return QSize(m_timeline->horizontalItemWidth(), qMax(180, m_timeline->viewport()->height()));
    const int width
        = option.rect.width() > 0 ? option.rect.width() : qMax(240, m_timeline->viewport()->width());
    const qreal sideWidth = contentSideWidth(width);
    const QFontMetricsF titleMetrics(titleFont(option));
    const QFontMetricsF descriptionMetrics(descriptionFont(option));
    const QFontMetricsF timeMetrics(timestampFont(option));
    qreal contentHeight = titleMetrics.height();
    if (m_timeline->isDescriptionVisible() && !event->description().isEmpty()) {
        const QRectF bounds = descriptionMetrics.boundingRect(QRectF(0, 0, qMax(40.0, sideWidth), 10000),
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, event->description());
        contentHeight += TextGap + bounds.height();
    }
    const qreal timeHeight
        = m_timeline->isTimestampVisible() && !m_timeline->formattedTimestamp(event).isEmpty()
        ? timeMetrics.height()
        : 0.0;
    return QSize(width,
        qCeil(qMax(MinimumEventHeight, std::max({ contentHeight, timeHeight, qreal(m_timeline->nodeSize()) }))
            + m_timeline->itemSpacing()));
}

void ZzTimelineDelegate::paint(
    QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto* event = m_model->eventAt(index.row());
    if (!painter || !event)
        return;
    if (m_timeline->orientation() == Qt::Horizontal) {
        paintHorizontal(painter, option, index, event);
        return;
    }
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    const auto group = colorGroup(option);
    const QRectF row(option.rect);
    drawHighlight(painter, option, group);
    const bool onRight = isContentOnRight(event, index.row());
    const qreal axisX = axisPosition(row);
    const QFont tf = titleFont(option);
    const QFont df = descriptionFont(option);
    const QFont timeFont = timestampFont(option);
    const QFontMetricsF titleMetrics(tf);
    const qreal top = row.top() + qMax(4.0, m_timeline->itemSpacing() * 0.25);
    const qreal nodeY = top + qMax(titleMetrics.height(), qreal(m_timeline->nodeSize())) * 0.5;
    const qreal gap = m_timeline->nodeSize() * 0.5 + AxisGap;
    const qreal left = row.left() + m_timeline->contentPadding();
    const qreal right = row.right() - m_timeline->contentPadding();
    QRectF content;
    QRectF time;
    if (onRight) {
        content = QRectF(axisX + gap, top, qMax(0.0, right - axisX - gap), row.bottom() - top);
        time = QRectF(left, top, qMax(0.0, axisX - gap - left), titleMetrics.height());
    } else {
        content = QRectF(left, top, qMax(0.0, axisX - gap - left), row.bottom() - top);
        time = QRectF(axisX + gap, top, qMax(0.0, right - axisX - gap), titleMetrics.height());
    }
    if (m_timeline->layoutMode() >= ZzTimeline::Alternating) {
        const qreal width = qMin(time.width(), qreal(m_timeline->timestampWidth()));
        if (onRight)
            time.setLeft(time.right() - width);
        else
            time.setWidth(width);
    }
    const QColor rail
        = m_timeline->lineColor().isValid() ? m_timeline->lineColor() : railColor(option.palette, group);
    painter->setPen(QPen(rail, m_timeline->lineWidth(), Qt::SolidLine, Qt::FlatCap));
    if (index.row() > 0)
        painter->drawLine(QPointF(axisX, row.top()), QPointF(axisX, nodeY));
    if (index.row() + 1 < m_model->rowCount())
        painter->drawLine(QPointF(axisX, nodeY), QPointF(axisX, row.bottom()));
    drawNode(painter, event, QPointF(axisX, nodeY), m_timeline->resolvedEventColor(event, group),
        option.palette, group);
    painter->setFont(tf);
    painter->setPen(option.palette.color(group, QPalette::Text));
    painter->drawText(QRectF(content.left(), top, content.width(), titleMetrics.height()),
        static_cast<int>(onRight ? Qt::AlignLeft : Qt::AlignRight) | static_cast<int>(Qt::AlignVCenter),
        titleMetrics.elidedText(event->title(), Qt::ElideRight, qRound(content.width())));
    if (m_timeline->isDescriptionVisible() && !event->description().isEmpty()) {
        QColor color = option.palette.color(group, QPalette::Text);
        color.setAlpha(group == QPalette::Disabled ? 100 : 170);
        painter->setFont(df);
        painter->setPen(color);
        painter->drawText(QRectF(content.left(), top + titleMetrics.height() + TextGap, content.width(),
                              qMax(0.0, row.bottom() - top - titleMetrics.height() - TextGap)),
            static_cast<int>(onRight ? Qt::AlignLeft : Qt::AlignRight) | static_cast<int>(Qt::AlignTop)
                | static_cast<int>(Qt::TextWordWrap),
            event->description());
    }
    const QString stamp = m_timeline->formattedTimestamp(event);
    if (m_timeline->isTimestampVisible() && !stamp.isEmpty()) {
        QColor color = option.palette.color(group, QPalette::Text);
        color.setAlpha(group == QPalette::Disabled ? 90 : 150);
        painter->setFont(timeFont);
        painter->setPen(color);
        painter->drawText(time, static_cast<int>(onRight ? Qt::AlignRight : Qt::AlignLeft) | static_cast<int>(Qt::AlignVCenter),
            QFontMetricsF(timeFont).elidedText(stamp, Qt::ElideRight, qRound(time.width())));
    }
    painter->restore();
}

void ZzTimelineDelegate::paintHorizontal(QPainter* painter, const QStyleOptionViewItem& option,
    const QModelIndex& index, const ZzTimelineEvent* event) const
{
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    const auto group = colorGroup(option);
    const QRectF item(option.rect);
    drawHighlight(painter, option, group);
    const bool below = isContentOnRight(event, index.row());
    const bool alternating = m_timeline->layoutMode() >= ZzTimeline::Alternating;
    const QFont tf = titleFont(option);
    const QFont df = descriptionFont(option);
    const QFont timeFont = timestampFont(option);
    const QFontMetricsF titleMetrics(tf);
    const QFontMetricsF descriptionMetrics(df);
    const QFontMetricsF timeMetrics(timeFont);
    const qreal radius = m_timeline->nodeSize() * 0.5;
    const qreal gap = radius + AxisGap;
    const qreal padding = qMin(
        qreal(m_timeline->contentPadding()), qMax(0.0, (item.height() - m_timeline->nodeSize()) * 0.5));
    const qreal top = item.top() + padding;
    const qreal bottom = item.bottom() - padding;
    qreal axisY = item.center().y();
    if (!alternating) {
        const qreal timeHeight = m_timeline->isTimestampVisible() ? timeMetrics.height() : 0.0;
        axisY = below ? top + timeHeight + AxisGap + radius : bottom - timeHeight - AxisGap - radius;
    }
    axisY = bottom - top >= radius * 2 ? qBound(top + radius, axisY, bottom - radius) : item.center().y();
    const QPointF center(item.center().x(), axisY);
    const QColor rail
        = m_timeline->lineColor().isValid() ? m_timeline->lineColor() : railColor(option.palette, group);
    painter->setPen(QPen(rail, m_timeline->lineWidth(), Qt::SolidLine, Qt::FlatCap));
    if (index.row() > 0)
        painter->drawLine(QPointF(item.left(), axisY), center);
    if (index.row() + 1 < m_model->rowCount())
        painter->drawLine(center, QPointF(item.right(), axisY));
    drawNode(painter, event, center, m_timeline->resolvedEventColor(event, group), option.palette, group);
    const qreal textLeft = item.left() + m_timeline->contentPadding();
    const qreal textWidth = qMax(0.0, item.width() - m_timeline->contentPadding() * 2.0);
    const QRectF content(textLeft, below ? axisY + gap : top, textWidth,
        below ? qMax(0.0, bottom - axisY - gap) : qMax(0.0, axisY - gap - top));
    qreal descriptionHeight = 0.0;
    if (m_timeline->isDescriptionVisible() && !event->description().isEmpty())
        descriptionHeight = descriptionMetrics
                                .boundingRect(QRectF(0, 0, qMax(40.0, textWidth), 10000),
                                    Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, event->description())
                                .height();
    const qreal contentHeight
        = titleMetrics.height() + (descriptionHeight > 0 ? TextGap + descriptionHeight : 0.0);
    const qreal contentTop = below ? content.top() : qMax(content.top(), content.bottom() - contentHeight);
    painter->setFont(tf);
    painter->setPen(option.palette.color(group, QPalette::Text));
    painter->drawText(QRectF(content.left(), contentTop, content.width(), titleMetrics.height()),
        Qt::AlignHCenter | Qt::AlignVCenter,
        titleMetrics.elidedText(event->title(), Qt::ElideRight, qRound(content.width())));
    if (descriptionHeight > 0) {
        QColor color = option.palette.color(group, QPalette::Text);
        color.setAlpha(group == QPalette::Disabled ? 100 : 170);
        painter->setFont(df);
        painter->setPen(color);
        painter->drawText(
            QRectF(content.left(), contentTop + titleMetrics.height() + TextGap, content.width(),
                qMax(0.0, content.bottom() - contentTop - titleMetrics.height() - TextGap)),
            Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, event->description());
    }
    const QString stamp = m_timeline->formattedTimestamp(event);
    if (m_timeline->isTimestampVisible() && !stamp.isEmpty()) {
        const qreal width = qMin(qMax(0.0, item.width() - m_timeline->contentPadding() * 2.0),
            qreal(m_timeline->timestampWidth()));
        const qreal stampTop = below ? axisY - gap - timeMetrics.height() : axisY + gap;
        const QRectF time(center.x() - width * 0.5, stampTop, width, timeMetrics.height());
        QColor color = option.palette.color(group, QPalette::Text);
        color.setAlpha(group == QPalette::Disabled ? 90 : 150);
        painter->setFont(timeFont);
        painter->setPen(color);
        painter->drawText(
            time, Qt::AlignCenter, timeMetrics.elidedText(stamp, Qt::ElideRight, qRound(width)));
    }
    painter->restore();
}

qreal ZzTimelineDelegate::contentSideWidth(int width) const
{
    const qreal available = qMax(40, width - m_timeline->contentPadding() * 2);
    if (m_timeline->layoutMode() >= ZzTimeline::Alternating)
        return available * 0.5 - m_timeline->nodeSize() * 0.5 - AxisGap;
    return available - (m_timeline->isTimestampVisible() ? m_timeline->timestampWidth() : 0)
        - m_timeline->nodeSize() - AxisGap * 2;
}
bool ZzTimelineDelegate::isContentOnRight(const ZzTimelineEvent* event, int row) const
{
    if (m_timeline->layoutMode() >= ZzTimeline::Alternating) {
        if (event->placement() == ZzTimelineEvent::LeftSide)
            return false;
        if (event->placement() == ZzTimelineEvent::RightSide)
            return true;
    }
    switch (m_timeline->layoutMode()) {
    case ZzTimeline::ContentOnLeft:
        return false;
    case ZzTimeline::Alternating:
        return row % 2 == 0;
    case ZzTimeline::AlternatingReverse:
        return row % 2 != 0;
    default:
        return true;
    }
}
qreal ZzTimelineDelegate::axisPosition(const QRectF& rect) const
{
    const qreal left = rect.left() + m_timeline->contentPadding();
    const qreal right = rect.right() - m_timeline->contentPadding();
    if (m_timeline->layoutMode() >= ZzTimeline::Alternating)
        return (left + right) * 0.5;
    const qreal stamp = m_timeline->isTimestampVisible() ? m_timeline->timestampWidth() : 0;
    return m_timeline->layoutMode() == ZzTimeline::ContentOnRight
        ? left + stamp + AxisGap + m_timeline->nodeSize() * 0.5
        : right - stamp - AxisGap - m_timeline->nodeSize() * 0.5;
}
QFont ZzTimelineDelegate::titleFont(const QStyleOptionViewItem& option) const
{
    QFont result = option.font;
    if (m_timeline->titleFontPixelSize() > 0)
        result.setPixelSize(m_timeline->titleFontPixelSize());
    result.setWeight(QFont::DemiBold);
    return result;
}
QFont ZzTimelineDelegate::descriptionFont(const QStyleOptionViewItem& option) const
{
    QFont result = option.font;
    result.setWeight(QFont::Normal);
    if (m_timeline->descriptionFontPixelSize() > 0)
        result.setPixelSize(m_timeline->descriptionFontPixelSize());
    else if (result.pixelSize() > 0)
        result.setPixelSize(qMax(9, result.pixelSize() - 1));
    return result;
}
QFont ZzTimelineDelegate::timestampFont(const QStyleOptionViewItem& option) const
{
    QFont result = descriptionFont(option);
    if (m_timeline->timestampFontPixelSize() > 0)
        result.setPixelSize(m_timeline->timestampFontPixelSize());
    return result;
}
void ZzTimelineDelegate::drawNode(QPainter* painter, const ZzTimelineEvent* event, const QPointF& center,
    const QColor& color, const QPalette& palette, QPalette::ColorGroup group) const
{
    const qreal radius = m_timeline->nodeSize() * 0.5;
    if (event->status() == ZzTimelineEvent::Current && m_timeline->isAnimationEnabled()) {
        const qreal progress = m_timeline->pulseProgress();
        QColor pulse = color;
        pulse.setAlpha(qRound(105 * (1 - progress)));
        painter->setPen(QPen(pulse, 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(center, radius + 2 + progress * 6, radius + 2 + progress * 6);
    }
    const bool outlined
        = event->status() == ZzTimelineEvent::Normal || event->status() == ZzTimelineEvent::Pending;
    QPen pen(color, 1.0);
    QBrush brush(color);
    if (event->status() == ZzTimelineEvent::Normal) {
        pen.setWidthF(4.0);
        brush = palette.color(group, QPalette::Base);
    } else if (event->status() == ZzTimelineEvent::Pending) {
        pen.setWidthF(2.0);
        brush = palette.color(group, QPalette::Base);
    }
    painter->setPen(pen);
    painter->setBrush(brush);
    const qreal adjustedRadius = qMax(0.0, radius - pen.widthF() * 0.5);
    painter->drawEllipse(center, adjustedRadius, adjustedRadius);
    const QColor symbol = outlined ? color : QColor(Qt::white);
    if (!event->icon().isEmpty()) {
        painter->setFont(ZzSegoeIconFont::font(qMax(8, qRound(m_timeline->nodeSize() * 0.78))));
        painter->setPen(symbol);
        painter->drawText(QRectF(center.x() - radius, center.y() - radius, radius * 2, radius * 2),
            Qt::AlignCenter, event->icon());
        return;
    }
    painter->setPen(
        QPen(symbol, qMax(1.2, m_timeline->nodeSize() * 0.11), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (event->status() == ZzTimelineEvent::Completed) {
        QPainterPath check;
        check.moveTo(center + QPointF(-radius * 0.42, 0));
        check.lineTo(center + QPointF(-radius * 0.10, radius * 0.30));
        check.lineTo(center + QPointF(radius * 0.46, -radius * 0.34));
        painter->drawPath(check);
    } else if (event->status() == ZzTimelineEvent::Error) {
        painter->drawLine(
            center + QPointF(-radius * 0.30, -radius * 0.30), center + QPointF(radius * 0.30, radius * 0.30));
        painter->drawLine(
            center + QPointF(radius * 0.30, -radius * 0.30), center + QPointF(-radius * 0.30, radius * 0.30));
    } else if (event->status() == ZzTimelineEvent::Warning) {
        painter->drawLine(center + QPointF(0, -radius * 0.36), center + QPointF(0, radius * 0.10));
        painter->drawPoint(center + QPointF(0, radius * 0.36));
    }
}
} // namespace ZzFluentUI
