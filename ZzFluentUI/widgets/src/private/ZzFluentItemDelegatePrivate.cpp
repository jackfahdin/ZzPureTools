#include "ZzFluentItemDelegatePrivate.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "ZzFluentStylePrivate.h"

namespace ZzFluentUI {

ZzFluentItemDelegatePrivate::ZzFluentItemDelegatePrivate(
    ZzFluentItemDelegate *publicObject) noexcept
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzFluentItemDelegatePrivate::paint(
    QPainter *painter,
    const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    Q_ASSERT(painter != nullptr);
    if (painter == nullptr) {
        return;
    }
    QStyleOptionViewItem adjusted = option;
    q_ptr->initStyleOption(&adjusted, index);
    const auto *navigationView = qobject_cast<
        const ZzNavigationView *>(adjusted.widget);
    if (navigationView != nullptr && navigationView->isCompact()) {
        adjusted.text.clear();
        adjusted.decorationPosition = QStyleOptionViewItem::Top;
        adjusted.decorationAlignment = Qt::AlignCenter;
    }

    QStyle *style = adjusted.widget != nullptr
        ? adjusted.widget->style()
        : QApplication::style();
    QStyleOptionViewItem content = adjusted;
    painter->save();
    const bool selected = adjusted.state.testFlag(QStyle::State_Selected);
    const bool hovered = adjusted.state.testFlag(QStyle::State_MouseOver);
    const auto *fluentStyle = qobject_cast<const ZzFluentStyle *>(style);
    if (fluentStyle != nullptr
        && fluentStyle->d_ptr->snapshot != nullptr
        && (selected || hovered)) {
        // 与 ComboBox 弹出项一致：圆角背板 + accent 指示条，文字不反白。
        const auto &snapshot = fluentStyle->d_ptr->snapshot;
        // 树形视图的分支区由 ZzFluentStyle::drawItemViewRow 续画：
        // 此处只保留外侧（右）圆角，指示条也由样式在分支区绘制。
        const auto *treeView = qobject_cast<
            const QTreeView *>(adjusted.widget);
        const bool splitWithBranch = treeView != nullptr
            && treeView->indentation() > 0;
        const bool rtl = adjusted.direction == Qt::RightToLeft;
        const QRectF surface = splitWithBranch
            ? (rtl
               ? QRectF(adjusted.rect).adjusted(-2.0, 2.0, 0.0, -2.0)
               : QRectF(adjusted.rect).adjusted(0.0, 2.0, -2.0, -2.0))
            : QRectF(adjusted.rect).adjusted(2.0, 2.0, -2.0, -2.0);
        const qreal radius = snapshot->metric(ZzMetricToken::CornerRadiusSmall);
        const QRectF extended = splitWithBranch
            ? (rtl
               ? surface.adjusted(0.0, 0.0, radius, 0.0)
               : surface.adjusted(-radius, 0.0, 0.0, 0.0))
            : surface;
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(snapshot->color(
            selected
                ? ZzColorToken::ControlFillPressed
                : ZzColorToken::ControlFillHover));
        painter->drawRoundedRect(extended, radius, radius);
        if (selected && !splitWithBranch) {
            const QRect logicalIndicator(
                adjusted.rect.left() + 4,
                adjusted.rect.center().y() - 8,
                3,
                16);
            const QRect indicator = QStyle::visualRect(
                adjusted.direction,
                adjusted.rect,
                logicalIndicator);
            painter->setBrush(snapshot->color(ZzColorToken::Accent));
            painter->drawRoundedRect(QRectF(indicator), 1.5, 1.5);
        }
        content.state.setFlag(QStyle::State_Selected, false);
        content.state.setFlag(QStyle::State_MouseOver, false);
    } else if (selected) {
        // 非 Fluent 样式回退：保留平台整行高亮。
        painter->fillRect(
            adjusted.rect,
            adjusted.palette.color(QPalette::Highlight));
        content.palette.setColor(
            QPalette::Text,
            adjusted.palette.color(QPalette::HighlightedText));
        content.state.setFlag(QStyle::State_Selected, false);
    } else if (hovered) {
        painter->fillRect(
            adjusted.rect,
            adjusted.palette.color(QPalette::AlternateBase));
        content.state.setFlag(QStyle::State_MouseOver, false);
    }
    const bool hasFocus = content.state.testFlag(QStyle::State_HasFocus);
    content.state.setFlag(QStyle::State_HasFocus, false);
    style->drawControl(
        QStyle::CE_ItemViewItem,
        &content,
        painter,
        adjusted.widget);
    if (hasFocus) {
        QStyleOptionFocusRect focus;
        focus.rect = adjusted.rect.adjusted(1, 1, -1, -1);
        focus.state = adjusted.state;
        focus.direction = adjusted.direction;
        focus.palette = adjusted.palette;
        focus.fontMetrics = adjusted.fontMetrics;
        style->drawPrimitive(
            QStyle::PE_FrameFocusRect,
            &focus,
            painter,
            adjusted.widget);
    }
    painter->restore();
}

} // namespace ZzFluentUI
