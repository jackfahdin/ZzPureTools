#include "ZzItemSelectionAnimation.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QEvent>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QVariantAnimation>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "ZzFluentStylePrivate.h"
#include "ZzTabIndicatorAnimation.h"

namespace ZzFluentUI {

ZzFluentStylePrivate::~ZzFluentStylePrivate()
{
    qDeleteAll(itemAnimations);
    qDeleteAll(tabAnimations);
}

ZzItemSelectionAnimation::ZzItemSelectionAnimation(
    QAbstractItemView *view, ZzFluentStyle *style)
    : QObject(style), view_(view), style_(style), transition_(this)
{
    view->installEventFilter(this);
    connect(transition_.animation(), &QVariantAnimation::valueChanged,
        this, [this] { repaintRows(); });
    connect(transition_.animation(), &QVariantAnimation::finished,
        this, [this] { repaintRows(); });
    if (auto *table = qobject_cast<QTableView *>(view)) {
        connect(table->horizontalHeader(), &QHeaderView::geometriesChanged,
            this, [this] { settle(); });
    }
    if (auto *tree = qobject_cast<QTreeView *>(view)) {
        connect(tree, &QTreeView::collapsed, this, [this] { settle(); });
    }
    synchronize();
}

QModelIndex ZzItemSelectionAnimation::key(const QModelIndex &index) const
{
    return view_ != nullptr && index.isValid()
            && view_->selectionBehavior() == QAbstractItemView::SelectRows
        ? index.siblingAtColumn(0) : index;
}

QModelIndex ZzItemSelectionAnimation::singleSelectedIndex() const
{
    if (selection_ == nullptr) {
        return {};
    }
    // selection() 是共享的范围列表；不展开 selectedIndexes() 的全部单元格。
    const QItemSelection ranges = selection_->selection();
    if (ranges.size() != 1 || ranges.first().height() != 1) {
        return {};
    }
    if (view_->selectionBehavior() != QAbstractItemView::SelectRows
        && ranges.first().width() != 1) {
        return {};
    }
    return key(ranges.first().topLeft());
}

void ZzItemSelectionAnimation::synchronize()
{
    if (view_ == nullptr) {
        return;
    }
    if (selection_ != view_->selectionModel()) {
        for (const auto &connection : connections_) {
            disconnect(connection);
        }
        connections_.clear();
        transition_.transitionTo({}, 0);
        selection_ = view_->selectionModel();
        if (selection_ != nullptr) {
            connections_.append(connect(selection_,
                &QItemSelectionModel::selectionChanged,
                this, [this] { selectionChanged(); }));
            auto *model = selection_->model();
            connections_.append(connect(model, &QAbstractItemModel::modelReset,
                this, [this] { settle(); }));
            connections_.append(connect(model, &QAbstractItemModel::layoutChanged,
                this, [this] { settle(); }));
            connections_.append(connect(model, &QAbstractItemModel::rowsRemoved,
                this, [this] { settle(); }));
            connections_.append(connect(model, &QAbstractItemModel::columnsRemoved,
                this, [this] { settle(); }));
        }
        settle();
    }
    if (style_->themeSnapshot()->reducedMotion()
        && transition_.animation()->state() != QAbstractAnimation::Stopped) {
        settle();
    }
}

void ZzItemSelectionAnimation::selectionChanged()
{
    const QModelIndex target = singleSelectedIndex();
    const bool nextMultiple = selection_ != nullptr
        && selection_->hasSelection() && !target.isValid();
    repaintRows();
    const int duration = !multiple_ && !nextMultiple
            && view_->isVisible() && view_->isEnabled()
        ? style_->themeSnapshot()->duration(ZzMotionToken::Normal) : 0;
    transition_.transitionTo(target, duration);
    multiple_ = nextMultiple;
    repaintRows();
}

void ZzItemSelectionAnimation::settle()
{
    repaintRows();
    const QModelIndex target = singleSelectedIndex();
    multiple_ = selection_ != nullptr
        && selection_->hasSelection() && !target.isValid();
    transition_.transitionTo(target, 0);
    repaintRows();
}

qreal ZzItemSelectionAnimation::scaleFor(
    const QModelIndex &index, bool selected) const
{
    return transition_.scaleFor(key(index), selected);
}

bool ZzItemSelectionAnimation::forcesIndicator(const QModelIndex &index) const
{
    return transition_.forcesIndicator(key(index));
}

void ZzItemSelectionAnimation::repaintRows()
{
    if (view_ == nullptr || view_->viewport() == nullptr) {
        return;
    }
    for (const QModelIndex &index :
         {transition_.outgoingIndex(), transition_.incomingIndex()}) {
        if (!index.isValid()) {
            continue;
        }
        QModelIndex visible = index;
        if (const auto *table = qobject_cast<QTableView *>(view_.data())) {
            const auto *header = table->horizontalHeader();
            const int visual = header->visualIndexAt(0);
            if (visual >= 0) {
                visible = index.siblingAtColumn(header->logicalIndex(visual));
            }
        } else if (const auto *tree = qobject_cast<QTreeView *>(view_.data())) {
            const int column = tree->treePosition() < 0
                ? tree->header()->logicalIndex(0) : tree->treePosition();
            visible = index.siblingAtColumn(column);
        }
        QRect rect = view_->visualRect(visible);
        if (!rect.isEmpty()) {
            rect.setLeft(0);
            rect.setRight(view_->viewport()->width() - 1);
            view_->viewport()->update(rect);
        }
    }
}

bool ZzItemSelectionAnimation::eventFilter(QObject *watched, QEvent *event)
{
    // QWidget destruction can deliver Hide after the item-view destructor.
    // QPointer is not cleared yet, but item-view APIs are no longer safe.
    if (watched == view_ && qobject_cast<QAbstractItemView *>(watched) != nullptr
        && (event->type() == QEvent::Hide
            || event->type() == QEvent::EnabledChange
            || event->type() == QEvent::StyleChange
            || event->type() == QEvent::LayoutDirectionChange)) {
        settle();
    }
    return QObject::eventFilter(watched, event);
}

ZzItemSelectionAnimation *ZzFluentStylePrivate::itemAnimation(
    QAbstractItemView *view)
{
    auto it = itemAnimations.find(view);
    if (it == itemAnimations.end()) {
        auto *animation = new ZzItemSelectionAnimation(view, q_ptr);
        it = itemAnimations.insert(view, animation);
        QObject::connect(view, &QObject::destroyed, animation, [this, view] {
            delete itemAnimations.take(view);
        });
        if (controller != nullptr) {
            QObject::connect(controller, &ZzThemeController::snapshotChanged,
                animation, [animation] { animation->synchronize(); });
        }
    }
    it.value()->synchronize();
    return it.value();
}

} // namespace ZzFluentUI
