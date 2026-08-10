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
constexpr int zzItemIndicatorWidth = 3;
constexpr int zzItemIndicatorHeight = 16;
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
    bool reserveIndicator) noexcept
{
    if (!reserveIndicator || option.rect.isEmpty()) {
        return option.rect;
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
    const QRect logicalIndicator(
        option.rect.left() + zzItemIndicatorLeading,
        option.rect.center().y() - zzItemIndicatorHeight / 2,
        zzItemIndicatorWidth,
        zzItemIndicatorHeight);
    ZzItemViewVisualLayout result{
        QRectF(option.rect).adjusted(
            zzItemSurfaceInset,
            zzItemSurfaceInset,
            -zzItemSurfaceInset,
            -zzItemSurfaceInset),
        QStyle::visualRect(
            option.direction,
            option.rect,
            logicalIndicator),
        zzContentRect(option, options.ownsIndicator)};

    if (painter == nullptr || style.d_ptr->snapshot == nullptr) {
        return result;
    }
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    if ((!selected && !hovered)
        || (!options.drawSurface && (!selected || !options.ownsIndicator))) {
        return result;
    }

    const auto &snapshot = style.d_ptr->snapshot;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    if (options.drawSurface) {
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
    if (selected && options.ownsIndicator) {
        painter->setBrush(snapshot->color(ZzColorToken::Accent));
        painter->drawRoundedRect(QRectF(result.indicatorRect), 1.5, 1.5);
    }
    painter->restore();
    return result;
}

} // namespace ZzFluentUI
