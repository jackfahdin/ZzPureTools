#include "ZzFluentItemDelegatePrivate.h"

#include <QtCore/QAbstractAnimation>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QVariantAnimation>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzAnimationPolicy.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "ZzItemViewVisual.h"
#include "ZzNavigationViewPrivate.h"

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
    auto *treeView = qobject_cast<QTreeView *>(
        const_cast<QWidget *>(adjusted.widget));
    const bool isTreeView = treeView != nullptr;
    if (isTreeView) {
        observeTreeSelection(treeView);
    }
    painter->save();
    const bool selected = adjusted.state.testFlag(QStyle::State_Selected);
    const bool hovered = adjusted.state.testFlag(QStyle::State_MouseOver);
    const auto *fluentStyle = qobject_cast<const ZzFluentStyle *>(style);
    if (fluentStyle != nullptr) {
        const bool ownsIndicator = ZzItemViewVisual::ownsIndicator(
            adjusted.widget,
            index);
        ZzItemViewVisualOptions visualOptions{
            .drawSurface = !isTreeView,
            .ownsIndicator = ownsIndicator};
        if (navigationView != nullptr) {
            visualOptions.forceIndicator =
                navigationView->d_ptr->forcesIndicator(index);
            visualOptions.indicatorScale =
                navigationView->d_ptr->indicatorScale(index, selected);
        } else if (isTreeView) {
            const auto snapshot = fluentStyle->themeSnapshot();
            if (snapshot != nullptr && snapshot->reducedMotion()
                && treeTransition->animation()->state()
                    != QAbstractAnimation::Stopped) {
                treeTransition->finish();
            }
            visualOptions.forceIndicator =
                treeTransition->forcesIndicator(index);
            visualOptions.indicatorScale =
                treeTransition->scaleFor(index, selected);
        }
        const ZzItemViewVisualLayout layout = ZzItemViewVisual::draw(
            *fluentStyle,
            adjusted,
            painter,
            visualOptions);
        content.rect = layout.contentRect;
        if (selected || hovered) {
            content.state.setFlag(QStyle::State_Selected, false);
            content.state.setFlag(QStyle::State_MouseOver, false);
        }
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

void ZzFluentItemDelegatePrivate::observeTreeSelection(
    QTreeView *treeView) const
{
    Q_ASSERT(treeView != nullptr);
    ensureTreeTransition();
    QItemSelectionModel *selectionModel = treeView->selectionModel();
    if (observedTreeView == treeView
        && observedSelectionModel == selectionModel) {
        return;
    }

    QObject::disconnect(selectionChangedConnection);
    observedTreeView = treeView;
    observedSelectionModel = selectionModel;
    selectionChangedConnection = {};
    if (selectionModel == nullptr) {
        return;
    }

    const QPointer<QTreeView> guardedView(treeView);
    selectionChangedConnection = QObject::connect(
        selectionModel,
        &QItemSelectionModel::selectionChanged,
        q_ptr,
        [this, guardedView] {
            if (guardedView != nullptr) {
                transitionTreeSelection();
            }
        });
    treeTransition->transitionTo(selectedTreeIndex(), 0);
    repaintTreeTransitionRows();
}

void ZzFluentItemDelegatePrivate::ensureTreeTransition() const
{
    if (treeTransition != nullptr) {
        return;
    }
    treeTransition = std::make_unique<ZzSelectionIndicatorTransition>(q_ptr);
    QObject::connect(
        treeTransition->animation(),
        &QVariantAnimation::valueChanged,
        q_ptr,
        [this] {
            repaintTreeTransitionRows();
        });
    QObject::connect(
        treeTransition->animation(),
        &QVariantAnimation::finished,
        q_ptr,
        [this] {
            repaintTreeTransitionRows();
        });
}

QModelIndex ZzFluentItemDelegatePrivate::selectedTreeIndex() const
{
    if (observedTreeView == nullptr || observedSelectionModel == nullptr) {
        return {};
    }
    const QModelIndexList selectedRows = observedSelectionModel->selectedRows(
        observedTreeView->treePosition());
    for (const QModelIndex &index : selectedRows) {
        if (index.isValid()) {
            return index;
        }
    }
    return {};
}

void ZzFluentItemDelegatePrivate::transitionTreeSelection() const
{
    repaintTreeTransitionRows();
    treeTransition->transitionTo(
        selectedTreeIndex(),
        treeTransitionDuration());
    repaintTreeTransitionRows();
}

void ZzFluentItemDelegatePrivate::repaintTreeTransitionRows() const
{
    if (observedTreeView == nullptr
        || observedTreeView->viewport() == nullptr) {
        return;
    }
    QWidget *const viewport = observedTreeView->viewport();
    const auto updateRow = [this, viewport](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        QRect rowRect = observedTreeView->visualRect(index);
        if (rowRect.isEmpty()) {
            return;
        }
        rowRect.setLeft(viewport->rect().left());
        rowRect.setRight(viewport->rect().right());
        viewport->update(rowRect);
    };
    if (treeTransition == nullptr) {
        return;
    }
    updateRow(treeTransition->outgoingIndex());
    updateRow(treeTransition->incomingIndex());
}

int ZzFluentItemDelegatePrivate::treeTransitionDuration() const
{
    if (observedTreeView == nullptr) {
        return 0;
    }
    const auto *fluentStyle = qobject_cast<const ZzFluentStyle *>(
        observedTreeView->style());
    if (fluentStyle == nullptr) {
        return 0;
    }
    const auto snapshot = fluentStyle->themeSnapshot();
    if (snapshot == nullptr) {
        return 0;
    }
    return ZzAnimationPolicy::adjustedDuration(
        snapshot->duration(ZzMotionToken::Normal),
        snapshot->reducedMotion(),
        false);
}

} // namespace ZzFluentUI
