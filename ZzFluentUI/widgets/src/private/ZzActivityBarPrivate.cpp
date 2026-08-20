#include "ZzActivityBarPrivate.h"

#include <algorithm>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QMimeData>
#include <QtCore/QSignalBlocker>
#include <QtCore/QThread>
#include <QtCore/QUuid>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtWidgets/QListView>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>

namespace ZzFluentUI {

namespace {

constexpr auto zzActivityMoveMimeType =
    "application/x-zzfluentui-activity-move";
constexpr int zzActivityItemHeight = 40;

[[nodiscard]] ZzActivityArea zzAreaFor(
    ZzSidePaneEdge edge,
    bool primary)
{
    if (edge == ZzSidePaneEdge::Left) {
        return primary ? ZzActivityArea::LeftPrimary
                       : ZzActivityArea::LeftSecondary;
    }
    return primary ? ZzActivityArea::RightPrimary
                   : ZzActivityArea::RightSecondary;
}

[[nodiscard]] bool zzIsEnabled(const QModelIndex &index)
{
    return index.isValid()
        && index.model() != nullptr
        && index.model()->flags(index).testFlag(Qt::ItemIsEnabled);
}

/** @brief 在标准绘制后用 palette 绘制有界数字徽标。 */
class ZzActivityItemDelegate final : public QStyledItemDelegate
{
public:
    explicit ZzActivityItemDelegate(QObject *parent)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);
        const int badge = std::max(
            0,
            index.data(static_cast<int>(ZzActivityItemRole::Badge)).toInt());
        if (badge <= 0 || painter == nullptr) {
            return;
        }
        const QString text = badge > 99
            ? QStringLiteral("99+") : QString::number(badge);
        const QFontMetrics metrics(option.font);
        const int textWidth = metrics.horizontalAdvance(text);
        const int markerWidth = std::max(16, textWidth + 8);
        const int markerHeight = std::max(
            1, std::min(18, option.rect.height() - 8));
        const QRect marker(
            option.rect.right() - markerWidth - 6,
            option.rect.top() + (option.rect.height() - markerHeight) / 2,
            markerWidth,
            markerHeight);
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(option.palette.highlight());
        painter->drawRoundedRect(marker, markerHeight / 2.0, markerHeight / 2.0);
        painter->setPen(option.palette.highlightedText().color());
        painter->drawText(marker, Qt::AlignCenter, text);
        painter->restore();
    }

    [[nodiscard]] QSize sizeHint(
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override
    {
        QSize result = QStyledItemDelegate::sizeHint(option, index);
        result.setHeight(zzActivityItemHeight);
        return result;
    }
};

} // namespace

/** @brief 将同一外部模型的指定 Area 投影为零复制的行映射。 */
class ZzActivityProjectionModel final : public QAbstractListModel
{
public:
    explicit ZzActivityProjectionModel(
        ZzActivityBarPrivate *owner,
        ZzActivityArea area,
        QObject *parent)
        : QAbstractListModel(parent)
        , owner_(owner)
        , area_(area)
    {
    }

    void setArea(ZzActivityArea area)
    {
        if (area_ == area) {
            return;
        }
        area_ = area;
        refresh();
    }

    void setSourceModel(QAbstractItemModel *model)
    {
        if (sourceModel_.data() == model) {
            return;
        }
        for (const QMetaObject::Connection &connection : connections_) {
            QObject::disconnect(connection);
        }
        connections_.clear();
        sourceModel_ = model;
        if (model != nullptr) {
            const auto refresh = [this] { this->refresh(); };
            connections_.append(QObject::connect(
                model, &QAbstractItemModel::modelReset, this, refresh));
            connections_.append(QObject::connect(
                model, &QAbstractItemModel::rowsInserted, this, refresh));
            connections_.append(QObject::connect(
                model, &QAbstractItemModel::rowsRemoved, this, refresh));
            connections_.append(QObject::connect(
                model, &QAbstractItemModel::rowsMoved, this, refresh));
            connections_.append(QObject::connect(
                model, &QAbstractItemModel::layoutChanged, this, refresh));
            connections_.append(QObject::connect(
                model, &QAbstractItemModel::dataChanged, this, refresh));
            connections_.append(QObject::connect(
                model, &QObject::destroyed, this, [this] {
                sourceModel_.clear();
                this->refresh();
            }));
        }
        refresh();
    }

    [[nodiscard]] QModelIndex mapToSource(const QModelIndex &index) const
    {
        if (!index.isValid() || index.row() < 0
            || index.row() >= sourceRows_.size() || sourceModel_.isNull()) {
            return {};
        }
        return sourceModel_->index(sourceRows_.at(index.row()), 0);
    }

    [[nodiscard]] QModelIndex mapFromSource(const QModelIndex &index) const
    {
        if (!index.isValid() || index.model() != sourceModel_.data()
            || index.parent().isValid() || index.column() != 0) {
            return {};
        }
        const int row = static_cast<int>(sourceRows_.indexOf(index.row()));
        return row >= 0 ? this->index(row, 0) : QModelIndex();
    }

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(sourceRows_.size());
    }

    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        const QModelIndex sourceIndex = mapToSource(index);
        return sourceIndex.isValid() ? sourceIndex.data(role) : QVariant();
    }

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        const QModelIndex sourceIndex = mapToSource(index);
        return sourceIndex.isValid() ? sourceIndex.model()->flags(sourceIndex)
                                     : Qt::NoItemFlags;
    }

    [[nodiscard]] QStringList mimeTypes() const override
    {
        return {QString::fromLatin1(zzActivityMoveMimeType)};
    }

    [[nodiscard]] QMimeData *mimeData(
        const QModelIndexList &indexes) const override
    {
        if (owner_ == nullptr || indexes.isEmpty()) {
            return nullptr;
        }
        return owner_->createMimeData(mapToSource(indexes.constFirst()));
    }

private:
    void refresh()
    {
        beginResetModel();
        sourceRows_.clear();
        if (sourceModel_ != nullptr) {
            const int count = sourceModel_->rowCount();
            for (int row = 0; row < count; ++row) {
                const QModelIndex index = sourceModel_->index(row, 0);
                const QVariant areaData = index.data(
                    static_cast<int>(ZzActivityItemRole::Area));
                const auto area = areaData.value<ZzActivityArea>();
                if (area == area_) {
                    sourceRows_.append(row);
                }
            }
        }
        endResetModel();
    }

    ZzActivityBarPrivate *owner_ = nullptr;
    ZzActivityArea area_;
    QPointer<QAbstractItemModel> sourceModel_;
    QList<int> sourceRows_;
    QList<QMetaObject::Connection> connections_;
};

namespace {

/** @brief 将源选择同步到一个投影视图，避免转发选择模型信号。 */
void zzSyncSelection(QListView *view, const QModelIndex &index)
{
    QItemSelectionModel *const selection = view->selectionModel();
    if (selection == nullptr) {
        return;
    }
    const QSignalBlocker blocker(selection);
    selection->setCurrentIndex(
        index,
        index.isValid() ? QItemSelectionModel::ClearAndSelect
                        : QItemSelectionModel::Clear);
}

} // namespace

ZzActivityBarPrivate::ZzActivityBarPrivate(
    ZzActivityBar *publicObject,
    ZzSidePaneEdge initialEdge)
    : q_ptr(publicObject)
    , edge(initialEdge)
{
    Q_ASSERT(q_ptr != nullptr);
    primaryProjection = new ZzActivityProjectionModel(
        this, zzAreaFor(edge, true), q_ptr);
    secondaryProjection = new ZzActivityProjectionModel(
        this, zzAreaFor(edge, false), q_ptr);
    delegate = new ZzActivityItemDelegate(q_ptr);
    primaryView = new QListView(q_ptr);
    secondaryView = new QListView(q_ptr);
    primaryView->setObjectName(QStringLiteral("zzActivityPrimaryView"));
    secondaryView->setObjectName(QStringLiteral("zzActivitySecondaryView"));
    for (QListView *view : {primaryView, secondaryView}) {
        view->setModel(view == primaryView
            ? static_cast<QAbstractItemModel *>(primaryProjection)
            : static_cast<QAbstractItemModel *>(secondaryProjection));
        view->setItemDelegate(delegate);
        view->setUniformItemSizes(true);
        view->setLayoutMode(QListView::Batched);
        view->setBatchSize(64);
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setDragEnabled(true);
        view->setAcceptDrops(true);
        view->setDragDropMode(QAbstractItemView::DragDrop);
        view->viewport()->setAcceptDrops(true);
        view->installEventFilter(q_ptr);
        view->viewport()->installEventFilter(q_ptr);
        QObject::connect(
            view,
            &QListView::clicked,
            q_ptr,
            [this, view](const QModelIndex &index) {
                const auto *projection = static_cast<ZzActivityProjectionModel *>(
                    view->model());
                activateSourceIndex(projection->mapToSource(index));
            });
    }
    auto *layout = new QVBoxLayout(q_ptr);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(primaryView, 1);
    layout->addWidget(secondaryView);
}

ZzActivityBarPrivate::~ZzActivityBarPrivate()
{
    QObject::disconnect(modelDestroyedConnection);
}

void ZzActivityBarPrivate::setEdge(ZzSidePaneEdge newEdge)
{
    if (edge == newEdge) {
        return;
    }
    edge = newEdge;
    primaryProjection->setArea(zzAreaFor(edge, true));
    secondaryProjection->setArea(zzAreaFor(edge, false));
    setCurrentSourceIndex(currentSourceIndex);
    Q_EMIT q_ptr->edgeChanged(edge);
}

void ZzActivityBarPrivate::setModel(QAbstractItemModel *model)
{
    if (sourceModel.data() == model) {
        return;
    }
    QObject::disconnect(modelDestroyedConnection);
    sourceModel = model;
    primaryProjection->setSourceModel(model);
    secondaryProjection->setSourceModel(model);
    currentSourceIndex = QPersistentModelIndex();
    dragTokens.clear();
    if (model != nullptr) {
        modelDestroyedConnection = QObject::connect(
            model, &QObject::destroyed, q_ptr, [this] {
                handleModelDestroyed();
            });
    } else {
        modelDestroyedConnection = {};
    }
    setCurrentSourceIndex({});
    Q_EMIT q_ptr->modelChanged(model);
}

void ZzActivityBarPrivate::handleModelDestroyed()
{
    sourceModel.clear();
    primaryProjection->setSourceModel(nullptr);
    secondaryProjection->setSourceModel(nullptr);
    currentSourceIndex = QPersistentModelIndex();
    dragTokens.clear();
    modelDestroyedConnection = {};
    setCurrentSourceIndex({});
    Q_EMIT q_ptr->modelChanged(nullptr);
}

void ZzActivityBarPrivate::setCurrentSourceIndex(const QModelIndex &index)
{
    const bool accepted = index.isValid()
        && index.model() == sourceModel.data()
        && !index.parent().isValid() && index.column() == 0;
    const QModelIndex sourceIndex = accepted ? index : QModelIndex();
    const QModelIndex primaryIndex = accepted
        ? primaryProjection->mapFromSource(sourceIndex) : QModelIndex();
    const QModelIndex secondaryIndex = accepted
        ? secondaryProjection->mapFromSource(sourceIndex) : QModelIndex();
    zzSyncSelection(primaryView, primaryIndex);
    zzSyncSelection(secondaryView, secondaryIndex);
    const QPersistentModelIndex previous = currentSourceIndex;
    currentSourceIndex = primaryIndex.isValid() || secondaryIndex.isValid()
        ? QPersistentModelIndex(sourceIndex) : QPersistentModelIndex();
    if (previous != currentSourceIndex) {
        Q_EMIT q_ptr->currentSourceIndexChanged(currentSourceIndex);
    }
}

void ZzActivityBarPrivate::activateSourceIndex(const QModelIndex &index)
{
    if (!zzIsEnabled(index)) {
        return;
    }
    if (currentSourceIndex == index) {
        Q_EMIT q_ptr->collapseRequested(index);
        return;
    }
    setCurrentSourceIndex(index);
    Q_EMIT q_ptr->activationRequested(index);
}

bool ZzActivityBarPrivate::handleKey(QListView *view, int key)
{
    if (view != primaryView && view != secondaryView) {
        return false;
    }
    if (key == Qt::Key_Enter || key == Qt::Key_Return || key == Qt::Key_Space) {
        activateSourceIndex(currentSourceIndex);
        return true;
    }
    if (key != Qt::Key_Home && key != Qt::Key_End
        && key != Qt::Key_Up && key != Qt::Key_Down) {
        return false;
    }
    QList<QModelIndex> indexes;
    for (ZzActivityProjectionModel *projection :
         {primaryProjection, secondaryProjection}) {
        for (int row = 0; row < projection->rowCount(); ++row) {
            const QModelIndex sourceIndex = projection->mapToSource(
                projection->index(row, 0));
            if (zzIsEnabled(sourceIndex)) {
                indexes.append(sourceIndex);
            }
        }
    }
    if (indexes.isEmpty()) {
        return true;
    }
    int target = static_cast<int>(indexes.indexOf(currentSourceIndex));
    if (key == Qt::Key_Home) {
        target = 0;
    } else if (key == Qt::Key_End) {
        target = static_cast<int>(indexes.size()) - 1;
    } else if (key == Qt::Key_Up) {
        target = target > 0 ? target - 1 : 0;
    } else {
        target = target >= 0 && target + 1 < static_cast<int>(indexes.size())
            ? target + 1 : static_cast<int>(indexes.size()) - 1;
    }
    setCurrentSourceIndex(indexes.at(target));
    return true;
}

QMimeData *ZzActivityBarPrivate::createMimeData(const QModelIndex &sourceIndex)
{
    if (!zzIsEnabled(sourceIndex)
        || !sourceIndex.model()->flags(sourceIndex).testFlag(Qt::ItemIsDragEnabled)) {
        return nullptr;
    }
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    dragTokens.insert(token, QPersistentModelIndex(sourceIndex));
    auto *mimeData = new QMimeData;
    mimeData->setData(
        QString::fromLatin1(zzActivityMoveMimeType), token.toUtf8());
    return mimeData;
}

void ZzActivityBarPrivate::discardDragTokens()
{
    dragTokens.clear();
}

bool ZzActivityBarPrivate::handleDrop(
    QListView *targetView,
    const QMimeData *mimeData,
    int y)
{
    if (targetView == nullptr || mimeData == nullptr) {
        return false;
    }
    const QString format = QString::fromLatin1(zzActivityMoveMimeType);
    if (!mimeData->hasFormat(format)) {
        return false;
    }
    const QString token = QString::fromUtf8(mimeData->data(format));
    const auto tokenIt = dragTokens.constFind(token);
    if (tokenIt == dragTokens.cend() || !tokenIt.value().isValid()
        || tokenIt.value().model() != sourceModel.data()) {
        return false;
    }
    if (y < 0) {
        return true;
    }
    const QModelIndex target = targetView->indexAt(QPoint(0, y));
    const int targetRow = target.isValid() ? target.row()
        : targetView->model()->rowCount();
    const QModelIndex sourceIndex = tokenIt.value();
    dragTokens.remove(token);
    Q_EMIT q_ptr->moveRequested(
        sourceIndex, areaForView(targetView), targetRow);
    return true;
}

ZzActivityArea ZzActivityBarPrivate::areaForView(const QListView *view) const
{
    return zzAreaFor(edge, view == primaryView);
}

} // namespace ZzFluentUI
