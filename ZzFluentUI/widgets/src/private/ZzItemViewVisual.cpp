#include "ZzItemViewVisual.h"

#include <algorithm>

#include <QtCore/QModelIndex>
#include <QtGui/QPainter>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "ZzFluentStylePrivate.h"

namespace ZzFluentUI {

namespace {

constexpr int zzItemSurfaceInset = 2;
constexpr int zzItemIndicatorLeading = 4;
constexpr int zzItemContentLeading = 10;
constexpr int zzItemHoverAccentAlpha = 32;

/** @brief 返回表格按当前视觉顺序排列的首个可见逻辑列。 */
[[nodiscard]] int zzFirstVisibleTableColumn(
    const QTableView *tableView) noexcept
{
    if (tableView == nullptr || tableView->horizontalHeader() == nullptr) {
        return -1;
    }
    const QHeaderView *header = tableView->horizontalHeader();
    for (int visual = 0; visual < header->count(); ++visual) {
        const int logical = header->logicalIndex(visual);
        if (logical >= 0 && !header->isSectionHidden(logical)) {
            return logical;
        }
    }
    return -1;
}

/** @brief 在逻辑 leading 侧预留固定强调条槽位。 */
[[nodiscard]] QRect zzContentRect(
    const QStyleOptionViewItem &option,
    bool reserveIndicator,
    ZzItemIndicatorPlacement placement) noexcept
{
    if (!reserveIndicator || option.rect.isEmpty()) {
        return option.rect;
    }
    if (placement == ZzItemIndicatorPlacement::PhysicalLeft) {
        return option.rect.adjusted(zzItemContentLeading, 0, 0, 0);
    }
    if (placement == ZzItemIndicatorPlacement::PhysicalRight) {
        return option.rect.adjusted(0, 0, -zzItemContentLeading, 0);
    }
    QRect logical = option.rect;
    logical.setLeft(std::min(
        logical.right() + 1,
        logical.left() + zzItemContentLeading));
    return QStyle::visualRect(option.direction, option.rect, logical);
}

} // namespace

bool ZzItemViewVisual::ownsIndicator(
    const QWidget *widget,
    const QModelIndex &index) noexcept
{
    if (const auto *treeView = qobject_cast<const QTreeView *>(widget)) {
        return index.isValid() && index.column() == treeView->treePosition();
    }
    const auto *tableView = qobject_cast<const QTableView *>(widget);
    if (tableView != nullptr
        && tableView->selectionBehavior() == QAbstractItemView::SelectRows) {
        return index.isValid()
            && index.column() == zzFirstVisibleTableColumn(tableView);
    }
    return true;
}

ZzItemViewVisualLayout ZzItemViewVisual::draw(
    const ZzFluentStyle &style,
    const QStyleOptionViewItem &option,
    QPainter *painter,
    ZzItemViewVisualOptions options)
{
    Q_ASSERT(painter != nullptr);
    const auto &snapshot = style.d_ptr->snapshot;
    const int indicatorWidth = snapshot == nullptr
        ? 0
        : qRound(snapshot->metric(
              ZzMetricToken::SelectionIndicatorThickness));
    const int indicatorHeight = snapshot == nullptr
        ? 0
        : qRound(snapshot->metric(
              ZzMetricToken::SelectionIndicatorExtent));
    const QRect logicalIndicator(
        option.rect.left() + zzItemIndicatorLeading,
        option.rect.center().y() - indicatorHeight / 2,
        indicatorWidth,
        indicatorHeight);
    QRect placedIndicatorRect;
    switch (options.indicatorPlacement) {
    case ZzItemIndicatorPlacement::LogicalLeading:
        placedIndicatorRect = QStyle::visualRect(
            option.direction,
            option.rect,
            logicalIndicator);
        break;
    case ZzItemIndicatorPlacement::PhysicalLeft:
        placedIndicatorRect = logicalIndicator;
        break;
    case ZzItemIndicatorPlacement::PhysicalRight:
        placedIndicatorRect = QRect(
            option.rect.right() - zzItemIndicatorLeading
                - indicatorWidth + 1,
            logicalIndicator.top(),
            indicatorWidth,
            indicatorHeight);
        break;
    }
    ZzItemViewVisualLayout result{
        QRectF(option.rect).adjusted(
            zzItemSurfaceInset,
            zzItemSurfaceInset,
            -zzItemSurfaceInset,
            -zzItemSurfaceInset),
        placedIndicatorRect,
        zzContentRect(
            option,
            options.ownsIndicator,
            options.indicatorPlacement)};

    if (painter == nullptr || snapshot == nullptr) {
        return result;
    }
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const qreal indicatorScale = std::clamp(
        options.indicatorScale,
        0.0,
        1.0);
    const bool drawsIndicator = options.ownsIndicator
        && indicatorScale > 0.0
        && (selected || options.forceIndicator);
    if (!selected && !hovered && !drawsIndicator) {
        return result;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    if (options.drawSurface && (selected || hovered)) {
        painter->fillRect(option.rect, option.palette.brush(QPalette::Base));
        QColor surfaceColor = snapshot->color(
            selected
                ? ZzColorToken::ControlFillPressed
                : ZzColorToken::ControlFillHover);
        // 高对比主题的 hover 与 Base 都是黑色；此时使用低透明度强调色，
        // 保持文字对比度的同时确保鼠标反馈可见。
        if (hovered
            && surfaceColor == option.palette.color(QPalette::Base)) {
            surfaceColor = snapshot->color(ZzColorToken::Accent);
            surfaceColor.setAlpha(zzItemHoverAccentAlpha);
        }
        painter->setBrush(surfaceColor);
        const qreal radius = snapshot->metric(
            ZzMetricToken::CornerRadiusSmall);
        painter->drawRoundedRect(result.surfaceRect, radius, radius);
    }
    if (drawsIndicator) {
        QRectF indicatorRect(result.indicatorRect);
        const qreal scaledHeight = indicatorRect.height() * indicatorScale;
        indicatorRect.setTop(
            indicatorRect.center().y() - (scaledHeight / 2.0));
        indicatorRect.setHeight(scaledHeight);
        painter->setBrush(snapshot->color(ZzColorToken::Accent));
        const qreal radius = qMin(
            indicatorRect.width(), indicatorRect.height()) / 2.0;
        painter->drawRoundedRect(indicatorRect, radius, radius);
    }
    painter->restore();
    return result;
}

} // namespace ZzFluentUI
