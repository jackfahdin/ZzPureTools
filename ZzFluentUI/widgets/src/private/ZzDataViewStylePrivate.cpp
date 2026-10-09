#include "ZzDataViewStylePrivate.h"

#include <QtCore/QEvent>
#include <QtGui/QHoverEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCalendarWidget>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QListView>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "ZzItemViewVisual.h"

namespace ZzFluentUI {
namespace {
constexpr char zzHoverPosition[] = "_zzDataViewHoverPosition";
constexpr char zzOriginalTracking[] = "_zzDataViewOriginalTracking";
constexpr char zzOriginalHover[] = "_zzDataViewOriginalHover";

/** @brief 由表头总长度计算行边界，隐藏、移动列不需要逐列扫描。 */
QRect zzRowRect(const QHeaderView *header, const QWidget *viewport, const QRect &cell)
{
    const int width = header->length();
    const int left = header->layoutDirection() == Qt::RightToLeft
        ? viewport->width() - width + header->offset() : -header->offset();
    return QRect(left, cell.top(), width, cell.height());
}

QColor zzSubtleFill(const ZzThemeSnapshot &theme, bool selected)
{
    const bool dark = theme.mode() == ZzThemeMode::Dark;
    QColor color = dark ? QColorConstants::White : QColorConstants::Black;
    color.setAlphaF(dark ? (selected ? 0.0419F : 0.0605F) : (selected ? 0.055F : 0.04F));
    return color;
}

/** @brief 行选中与悬停背板的圆角半径，单位逻辑像素。 */
constexpr qreal zzRowSurfaceRadius = 4.0;

/** @brief 内容前景与背板共享同一整行悬停状态。 */
bool zzHovered(const QStyleOptionViewItem &option)
{
    if (!option.state.testFlag(QStyle::State_Enabled)) {
        return false;
    }
    const auto *table = qobject_cast<const QTableView *>(option.widget);
    if (table != nullptr && table->selectionBehavior() == QAbstractItemView::SelectRows
        && option.index.isValid()) {
        const QVariant position = table->viewport()->property(zzHoverPosition);
        if (position.isValid()) {
            const QModelIndex hover = table->indexAt(position.toPoint());
            return hover.isValid() && hover.row() == option.index.row();
        }
    }
    return option.state.testFlag(QStyle::State_MouseOver);
}

/** @brief 在调用者裁剪范围内绘制完整行几何，避免列间出现圆角断缝。 */
void zzDrawSurface(const ZzFluentStyle &style, const QStyleOptionViewItem &option,
    QPainter *painter, const QRect &surface, bool indicator)
{
    const auto theme = style.themeSnapshot();
    const auto *view = qobject_cast<const QAbstractItemView *>(option.widget);
    const auto *table = qobject_cast<const QTableView *>(view);
    const bool rowTable = table != nullptr
        && table->selectionBehavior() == QAbstractItemView::SelectRows;
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = zzHovered(option);
    const bool highContrast = theme->mode() == ZzThemeMode::HighContrast;
    const auto group = !option.state.testFlag(QStyle::State_Enabled) ? QPalette::Disabled
        : option.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
    const auto backgroundRole = view != nullptr ? view->viewport()->backgroundRole() : QPalette::Base;
    painter->save();
    painter->setClipRect(option.rect, Qt::IntersectClip);
    // 每块只合成一次状态色；自定义模型背景不能在随后绘制内容时盖住选中态。
    const bool alternate = option.features.testFlag(QStyleOptionViewItem::Alternate);
    QBrush background = option.palette.brush(group, alternate ? QPalette::AlternateBase : backgroundRole);
    if (alternate && !option.palette.isBrushSet(group, QPalette::AlternateBase)) {
        background = option.palette.brush(group, QPalette::Window);
    }
    painter->fillRect(option.rect, background);
    if (!selected && !hovered && option.backgroundBrush.style() != Qt::NoBrush) {
        painter->fillRect(option.rect, option.backgroundBrush);
    }
    painter->setPen(Qt::NoPen);
    if (selected || hovered) {
        if (rowTable && !highContrast) {
            const bool dark = theme->mode() == ZzThemeMode::Dark;
            QColor color = option.palette.color(group, QPalette::Highlight);
            color.setAlpha(selected ? (dark ? 56 : 40) : (dark ? 40 : 28));
            const QRect row = surface.adjusted(0, 1, 0, -1);
            painter->fillRect(row, color);
            const QWidget *focus = QApplication::focusWidget();
            const bool editing = focus != nullptr && focus != view && view->isAncestorOf(focus)
                && view->currentIndex().row() == option.index.row();
            if (selected && !editing) {
                color.setAlpha(dark ? 210 : 190);
                painter->setRenderHint(QPainter::Antialiasing, false);
                painter->setPen(QPen(color, 1));
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(row.adjusted(0, 0, -1, -1));
            }
        } else {
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(highContrast ? option.palette.color(group, QPalette::Highlight)
                                          : zzSubtleFill(*theme, selected));
            painter->drawRoundedRect(QRectF(surface).adjusted(2, 2, -2, -2), zzRowSurfaceRadius, zzRowSurfaceRadius);
        }
    }
    if (indicator && !highContrast) {
        QStyleOptionViewItem marker = option;
        marker.rect = surface;
        (void)ZzItemViewVisual::draw(style, marker, painter, {.drawSurface = false});
    }
    painter->restore();
}
} // namespace

bool ZzDataViewStylePrivate::applies(const QWidget *widget)
{
    const auto *view = qobject_cast<const QAbstractItemView *>(widget);
    if (view == nullptr || view->windowType() == Qt::Popup
        || qobject_cast<const ZzNavigationView *>(view) != nullptr) {
        return false;
    }
    for (const QWidget *parent = view->parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
        if (qobject_cast<const QComboBox *>(parent) != nullptr
            || qobject_cast<const QCalendarWidget *>(parent) != nullptr
            || qobject_cast<const ZzNavigationPane *>(parent) != nullptr) {
            return false;
        }
    }
    if (const auto *list = qobject_cast<const QListView *>(view)) {
        return list->viewMode() == QListView::ListMode;
    }
    return qobject_cast<const QTreeView *>(view) != nullptr || qobject_cast<const QTableView *>(view) != nullptr;
}

QRect ZzDataViewStylePrivate::contentRect(const QStyleOptionViewItem &option)
{
    // 参考样式在列表与树内容 leading 侧增加 6px，表格使用原生单元格留白。
    if (qobject_cast<const QTableView *>(option.widget) != nullptr) {
        return option.rect;
    }
    const int inset = qMin(6, option.rect.width());
    return QStyle::visualRect(option.direction, option.rect, option.rect.adjusted(inset, 0, 0, 0));
}

void ZzDataViewStylePrivate::drawItem(const ZzFluentStyle &style,
    const QStyleOptionViewItem &option, QPainter *painter)
{
    const auto *tree = qobject_cast<const QTreeView *>(option.widget);
    const auto *table = qobject_cast<const QTableView *>(option.widget);
    QRect surface = option.rect;
    if (table != nullptr && table->selectionBehavior() == QAbstractItemView::SelectRows) {
        surface = zzRowRect(table->horizontalHeader(), table->viewport(), option.rect);
    } else if (tree != nullptr && tree->selectionBehavior() == QAbstractItemView::SelectRows) {
        surface = zzRowRect(tree->header(), tree->viewport(), option.rect);
    } else if (tree != nullptr && ZzItemViewVisual::ownsIndicator(tree, option.index)) {
        surface.setLeft(tree->header()->sectionViewportPosition(option.index.column()));
        surface.setWidth(tree->header()->sectionSize(option.index.column()));
    }
    const bool indicator = table == nullptr && (tree == nullptr
        || tree->selectionBehavior() == QAbstractItemView::SelectRows
        || ZzItemViewVisual::ownsIndicator(tree, option.index));
    zzDrawSurface(style, option, painter, surface, indicator);
    QStyleOptionViewItem content = option;
    content.rect = contentRect(option);
    content.version = zzItemContentOptionVersion;
    content.backgroundBrush = Qt::NoBrush;
    content.features.setFlag(QStyleOptionViewItem::Alternate, false);
    content.state.setFlag(QStyle::State_Selected, false);
    content.state.setFlag(QStyle::State_MouseOver, false);
    const bool focus = content.state.testFlag(QStyle::State_HasFocus);
    content.state.setFlag(QStyle::State_HasFocus, false);
    if (style.themeSnapshot()->mode() == ZzThemeMode::HighContrast
        && (option.state.testFlag(QStyle::State_Selected) || zzHovered(option))) {
        content.palette.setBrush(QPalette::Text, option.palette.brush(QPalette::HighlightedText));
    }
    style.drawControl(QStyle::CE_ItemViewItem, &content, painter, option.widget);
    if (focus) {
        QStyleOptionFocusRect frame;
        frame.rect = option.rect.adjusted(2, 2, -2, -2);
        frame.state = option.state;
        frame.palette = option.palette;
        style.drawPrimitive(QStyle::PE_FrameFocusRect, &frame, painter, option.widget);
    }
}

void ZzDataViewStylePrivate::drawTreeRow(const ZzFluentStyle &style,
    const QStyleOptionViewItem &option, QPainter *painter, const QWidget *widget)
{
    const auto *tree = qobject_cast<const QTreeView *>(widget);
    if (tree == nullptr && widget != nullptr) {
        tree = qobject_cast<const QTreeView *>(widget->parentWidget());
    }
    if (tree == nullptr) {
        return;
    }
    if (option.features.testFlag(QStyleOptionViewItem::Alternate)) {
        const auto group = !option.state.testFlag(QStyle::State_Enabled) ? QPalette::Disabled
            : option.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
        painter->fillRect(option.rect, option.palette.brush(group,
            option.palette.isBrushSet(group, QPalette::AlternateBase) ? QPalette::AlternateBase : QPalette::Window));
    }
    // 这里只绘制树缩进；正文由 CE_ItemViewItem 绘制，兼容 6.8 的几何判定。
    const int treeColumn = tree->treePosition() < 0 ? tree->header()->logicalIndex(0) : tree->treePosition();
    const int sectionLeft = tree->header()->sectionViewportPosition(treeColumn);
    const int sectionWidth = tree->header()->sectionSize(treeColumn);
    const bool branch =
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        option.features.testFlag(QStyleOptionViewItem::IsDecorationForRootColumn);
#else
        tree->indentation() > 0
        && (option.direction == Qt::RightToLeft
            ? option.rect.right() + 1 == sectionLeft + sectionWidth : option.rect.left() == sectionLeft);
#endif
    if (!branch) {
        return;
    }
    QStyleOptionViewItem adjusted = option;
    adjusted.widget = tree;
    if (!adjusted.index.isValid()) {
        adjusted.index = tree->indexAt(QPoint(sectionLeft + sectionWidth / 2, option.rect.center().y()));
    }
    if (adjusted.index.isValid() && tree->selectionModel() != nullptr
        && tree->selectionModel()->isSelected(adjusted.index)) {
        adjusted.state.setFlag(QStyle::State_Selected);
    }
    QRect surface = zzRowRect(tree->header(), tree->viewport(), option.rect);
    if (tree->selectionBehavior() != QAbstractItemView::SelectRows) {
        surface = QRect(sectionLeft, option.rect.top(), sectionWidth, option.rect.height());
    }
    zzDrawSurface(style, adjusted, painter, surface, true);
}

void ZzDataViewStylePrivate::drawHeader(const ZzFluentStyle &style,
    const QStyleOptionHeader &option, QPainter *painter)
{
    painter->save();
    painter->fillRect(option.rect, option.palette.brush(QPalette::Button));
    const auto mode = style.themeSnapshot()->mode();
    const QColor separator = mode == ZzThemeMode::HighContrast ? option.palette.color(QPalette::ButtonText)
        : mode == ZzThemeMode::Dark ? QColor::fromRgb(255, 255, 255, 18) : QColor::fromRgb(0, 0, 0, 15);
    painter->setPen(QPen(separator, 1));
    if (option.orientation == Qt::Horizontal) {
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        if (option.position != QStyleOptionHeader::OnlyOneSection) {
            const int x = option.direction == Qt::RightToLeft ? option.rect.left() : option.rect.right();
            painter->drawLine(x, option.rect.top(), x, option.rect.bottom());
        }
    } else {
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        painter->drawLine(option.rect.topRight(), option.rect.bottomRight());
    }
    painter->restore();
}

void ZzDataViewStylePrivate::polish(QWidget *widget, bool enabled)
{
    const auto *table = qobject_cast<QTableView *>(widget);
    if (table == nullptr || !applies(table)) {
        return;
    }
    QWidget *viewport = table->viewport();
    if (enabled && !viewport->property(zzOriginalTracking).isValid()) {
        viewport->setProperty(zzOriginalTracking, viewport->hasMouseTracking());
        viewport->setProperty(zzOriginalHover, viewport->testAttribute(Qt::WA_Hover));
        viewport->setMouseTracking(true);
        viewport->setAttribute(Qt::WA_Hover);
    } else if (!enabled && viewport->property(zzOriginalTracking).isValid()) {
        viewport->setMouseTracking(viewport->property(zzOriginalTracking).toBool());
        viewport->setAttribute(Qt::WA_Hover, viewport->property(zzOriginalHover).toBool());
        viewport->setProperty(zzOriginalTracking, QVariant());
        viewport->setProperty(zzOriginalHover, QVariant());
        viewport->setProperty(zzHoverPosition, QVariant());
    }
}

void ZzDataViewStylePrivate::handleEvent(QObject *watched, QEvent *event)
{
    auto *viewport = qobject_cast<QWidget *>(watched);
    auto *table = viewport != nullptr ? qobject_cast<QTableView *>(viewport->parentWidget()) : nullptr;
    if (table == nullptr || table->viewport() != viewport || !applies(table)) {
        return;
    }
    QPoint position(-1, -1);
    switch (event->type()) {
    case QEvent::MouseMove:
        position = static_cast<QMouseEvent *>(event)->position().toPoint();
        break;
    case QEvent::HoverMove:
        position = static_cast<QHoverEvent *>(event)->position().toPoint();
        break;
    case QEvent::Leave:
    case QEvent::HoverLeave:
        break;
    default:
        return;
    }
    if (viewport->property(zzHoverPosition) != QVariant(position)) {
        const QVariant previous = viewport->property(zzHoverPosition);
        const QModelIndex oldIndex = previous.isValid() ? table->indexAt(previous.toPoint()) : QModelIndex();
        const QModelIndex newIndex = table->indexAt(position);
        viewport->setProperty(zzHoverPosition, position);
        if (oldIndex.row() != newIndex.row()) {
            for (const QModelIndex &index : {oldIndex, newIndex}) {
                if (index.isValid()) {
                    viewport->update(QRect(0, table->rowViewportPosition(index.row()),
                        viewport->width(), table->rowHeight(index.row())));
                }
            }
        }
    }
}

} // namespace ZzFluentUI
