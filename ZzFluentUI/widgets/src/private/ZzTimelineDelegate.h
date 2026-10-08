#pragma once
#include <QPalette>
#include <QStyledItemDelegate>
namespace ZzFluentUI {
class ZzTimeline;
class ZzTimelineModel;
class ZzTimelineEvent;
class ZzTimelineDelegate final : public QStyledItemDelegate {
public:
    ZzTimelineDelegate(ZzTimeline* timeline, ZzTimelineModel* model);
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(
        QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    void paintHorizontal(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
        const ZzTimelineEvent* event) const;
    void drawNode(QPainter* painter, const ZzTimelineEvent* event, const QPointF& center, const QColor& color,
        const QPalette& palette, QPalette::ColorGroup group) const;
    qreal contentSideWidth(int totalWidth) const;
    bool isContentOnRight(const ZzTimelineEvent* event, int row) const;
    qreal axisPosition(const QRectF& rect) const;
    QFont titleFont(const QStyleOptionViewItem& option) const;
    QFont descriptionFont(const QStyleOptionViewItem& option) const;
    QFont timestampFont(const QStyleOptionViewItem& option) const;
    ZzTimeline* m_timeline;
    ZzTimelineModel* m_model;
};
} // namespace ZzFluentUI
