#include "ZzActivityBarPrivate.h"

#include <algorithm>
#include <array>
#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QMimeData>
#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconDescriptor.h>

#include "ZzItemViewVisual.h"

namespace ZzFluentUI {

namespace {

constexpr auto zzActivityMoveMimeType =
    "application/x-zzfluentui-activity-move";
constexpr int zzActivityBarWidth = 48;
constexpr int zzActivityItemHeight = 40;
constexpr int zzActivityIconExtent = 20;
constexpr int zzActivityIconExtentWithBadge = 16;
constexpr int zzActivityItemMargin = 4;
constexpr int zzActivityBadgeMaximumWidth = 22;
constexpr int zzActivityBadgeHeight = 14;

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

/** @brief 判断模型返回值是否属于四个受支持的固定区域。 */
[[nodiscard]] bool zzIsKnownArea(ZzActivityArea area) noexcept
{
    switch (area) {
    case ZzActivityArea::LeftPrimary:
    case ZzActivityArea::LeftSecondary:
    case ZzActivityArea::RightPrimary:
    case ZzActivityArea::RightSecondary:
        return true;
    }
    return false;
}

/** @brief 返回上下文菜单使用的可翻译区域名称。 */
[[nodiscard]] QString zzAreaLabel(ZzActivityArea area)
{
    switch (area) {
    case ZzActivityArea::LeftPrimary:
        return QCoreApplication::translate("ZzActivityBar", "左上");
    case ZzActivityArea::LeftSecondary:
        return QCoreApplication::translate("ZzActivityBar", "左下");
    case ZzActivityArea::RightPrimary:
        return QCoreApplication::translate("ZzActivityBar", "右上");
    case ZzActivityArea::RightSecondary:
        return QCoreApplication::translate("ZzActivityBar", "右下");
    }
    return {};
}

[[nodiscard]] bool zzIsEnabled(const QModelIndex &index)
{
    return index.isValid()
        && index.model() != nullptr
        && index.model()->flags(index).testFlag(Qt::ItemIsEnabled);
}

/** @brief 将模型中的徽标数值约束为非负整数。 */
[[nodiscard]] int zzBadgeValue(const QModelIndex &index)
{
    return std::max(
        0,
        index.data(static_cast<int>(ZzActivityItemRole::Badge)).toInt());
}

/** @brief 判断统一图标描述是否包含可交给 Fluent 缓存的来源。 */
[[nodiscard]] bool zzHasIconDescriptor(
    const QVariant &value,
    const ZzIconDescriptor &descriptor)
{
    if (!value.canConvert<ZzIconDescriptor>()) {
        return false;
    }
    switch (descriptor.source) {
    case ZzIconSource::SvgResource:
        return !descriptor.resourceId.trimmed().isEmpty();
    case ZzIconSource::FontGlyph:
        return descriptor.fontIcon != ZzFontIcon::None;
    }
    return false;
}

/** @brief 绘制固定轨道的交互背板、图标后备、badge 和键盘焦点。 */
class ZzActivityItemDelegate final : public QStyledItemDelegate
{
public:
    /** @brief 创建不分配逐行控件的共享 delegate。 */
    explicit ZzActivityItemDelegate(
        ZzActivityBarPrivate *owner,
        QObject *parent)
        : QStyledItemDelegate(parent)
        , owner_(owner)
    {
    }

    /** @brief 通过 Fluent 图标缓存绘制当前可见活动入口。 */
    void paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override
    {
        if (painter == nullptr) {
            return;
        }
        QStyleOptionViewItem adjusted = option;
        initStyleOption(&adjusted, index);
        auto *fluentStyle = adjusted.widget != nullptr
            ? qobject_cast<ZzFluentStyle *>(adjusted.widget->style())
            : nullptr;
        if (fluentStyle == nullptr) {
            QStyledItemDelegate::paint(painter, adjusted, index);
            drawBadge(painter, adjusted, zzBadgeValue(index));
            return;
        }

        painter->save();
        ZzItemViewVisualOptions visualOptions;
        if (owner_ != nullptr) {
            visualOptions.forceIndicator =
                owner_->isProjectionIndexActive(index);
            visualOptions.indicatorPlacement =
                owner_->edge == ZzSidePaneEdge::Left
                ? ZzItemIndicatorPlacement::PhysicalLeft
                : ZzItemIndicatorPlacement::PhysicalRight;
        }
        [[maybe_unused]] const ZzItemViewVisualLayout visual =
            ZzItemViewVisual::draw(
                *fluentStyle,
                adjusted,
                painter,
                visualOptions);
        const bool enabled = adjusted.state.testFlag(QStyle::State_Enabled);
        const QPalette::ColorGroup colorGroup = enabled
            ? QPalette::Normal : QPalette::Disabled;
        const QColor foreground = adjusted.palette.color(
            colorGroup, QPalette::Text);
        const QVariant descriptorValue = index.data(Qt::DecorationRole);
        const ZzIconDescriptor descriptor =
            descriptorValue.value<ZzIconDescriptor>();
        const bool hasDescriptor = zzHasIconDescriptor(
            descriptorValue, descriptor);
        const int badge = zzBadgeValue(index);
        const int iconExtent = badge > 0
            ? zzActivityIconExtentWithBadge : zzActivityIconExtent;
        const QRect logicalIconRect = badge > 0
            ? QRect(
                  adjusted.rect.left() + zzActivityItemMargin,
                  adjusted.rect.center().y() - iconExtent / 2,
                  iconExtent,
                  iconExtent)
            : QRect(
                  adjusted.rect.center().x() - iconExtent / 2,
                  adjusted.rect.center().y() - iconExtent / 2,
                  iconExtent,
                  iconExtent);
        const QRect iconRect = QStyle::visualRect(
            adjusted.direction, adjusted.rect, logicalIconRect);
        QPixmap icon;
        if (hasDescriptor && adjusted.widget != nullptr) {
            icon = fluentStyle->iconPixmap(
                descriptor,
                QSize(iconExtent, iconExtent),
                adjusted.widget->devicePixelRatioF(),
                foreground,
                adjusted.direction);
        }
        if (!icon.isNull()) {
            painter->drawPixmap(iconRect, icon);
        } else {
            drawTextFallback(painter, adjusted, iconRect, foreground);
        }
        drawBadge(painter, adjusted, badge);
        drawFocus(painter, adjusted);
        painter->restore();
    }

    /** @brief 返回与 Activity Bar 视觉合同一致的固定逻辑行高。 */
    [[nodiscard]] QSize sizeHint(
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override
    {
        QSize result = QStyledItemDelegate::sizeHint(option, index);
        result.setWidth(zzActivityBarWidth);
        result.setHeight(zzActivityItemHeight);
        return result;
    }

private:
    /** @brief 在没有有效 descriptor 时居中绘制标题首字符。 */
    static void drawTextFallback(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QRect &visualRect,
        const QColor &foreground)
    {
        const QString fallback = option.text.trimmed().left(1).toUpper();
        if (fallback.isEmpty()) {
            return;
        }
        QFont font = option.font;
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        painter->setPen(foreground);
        painter->drawText(visualRect, Qt::AlignCenter, fallback);
    }

    /** @brief 用 palette 绘制有界数字徽标，不创建额外 QObject。 */
    static void drawBadge(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        int badge)
    {
        if (badge <= 0) {
            return;
        }
        const QString text = badge > 99
            ? QStringLiteral("99+") : QString::number(badge);
        QFont badgeFont = option.font;
        badgeFont.setPixelSize(10);
        badgeFont.setWeight(QFont::DemiBold);
        const QFontMetrics metrics(badgeFont);
        const int textWidth = metrics.horizontalAdvance(text);
        const int markerWidth = std::min(
            zzActivityBadgeMaximumWidth,
            std::max(14, textWidth + 4));
        const int markerHeight = std::min(
            zzActivityBadgeHeight,
            std::max(1, option.rect.height() - 2 * zzActivityItemMargin));
        const QRect logicalMarker(
            option.rect.right() - markerWidth - zzActivityItemMargin + 1,
            option.rect.center().y() - markerHeight / 2,
            markerWidth,
            markerHeight);
        const QRect marker = QStyle::visualRect(
            option.direction, option.rect, logicalMarker);
        painter->save();
        painter->setFont(badgeFont);
        painter->setPen(Qt::NoPen);
        painter->setBrush(option.palette.highlight());
        painter->drawRoundedRect(marker, markerHeight / 2.0, markerHeight / 2.0);
        painter->setPen(option.palette.highlightedText().color());
        painter->drawText(marker, Qt::AlignCenter, text);
        painter->restore();
    }

    /** @brief 使用当前样式绘制键盘导航焦点矩形。 */
    static void drawFocus(
        QPainter *painter,
        const QStyleOptionViewItem &option)
    {
        if (!option.state.testFlag(QStyle::State_HasFocus)) {
            return;
        }
        QStyleOptionFocusRect focus;
        focus.rect = option.rect.adjusted(1, 1, -1, -1);
        focus.state = option.state;
        focus.direction = option.direction;
        focus.palette = option.palette;
        focus.fontMetrics = option.fontMetrics;
        QStyle *const style = option.widget != nullptr
            ? option.widget->style() : QApplication::style();
        style->drawPrimitive(
            QStyle::PE_FrameFocusRect,
            &focus,
            painter,
            option.widget);
    }

    ZzActivityBarPrivate *owner_ = nullptr;
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
        const auto position = std::lower_bound(
            sourceRows_.cbegin(), sourceRows_.cend(), index.row());
        if (position == sourceRows_.cend() || *position != index.row()) {
            return {};
        }
        return this->index(
            static_cast<int>(position - sourceRows_.cbegin()), 0);
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
                    // 源 row 按递增顺序追加，保证二分反向映射成立。
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
    q_ptr->setFixedWidth(zzActivityBarWidth);
    dragTokenExpiryTimer = new QTimer(q_ptr);
    dragTokenExpiryTimer->setSingleShot(true);
    dragTokenExpiryTimer->setInterval(5000);
    QObject::connect(
        dragTokenExpiryTimer,
        &QTimer::timeout,
        q_ptr,
        [this] { discardDragTokens(); });
    primaryProjection = new ZzActivityProjectionModel(
        this, zzAreaFor(edge, true), q_ptr);
    secondaryProjection = new ZzActivityProjectionModel(
        this, zzAreaFor(edge, false), q_ptr);
    delegate = new ZzActivityItemDelegate(this, q_ptr);
    primaryView = new QListView(q_ptr);
    secondaryView = new QListView(q_ptr);
    primaryView->setObjectName(QStringLiteral("zzActivityPrimaryView"));
    secondaryView->setObjectName(QStringLiteral("zzActivitySecondaryView"));
    for (QListView *view : {primaryView, secondaryView}) {
        view->setFixedWidth(zzActivityBarWidth);
        view->setFrameShape(QFrame::NoFrame);
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
        view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
                scheduleSourceActivation(projection->mapToSource(index));
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
    for (const QMetaObject::Connection &connection : activeModelConnections) {
        QObject::disconnect(connection);
    }
}

void ZzActivityBarPrivate::setEdge(ZzSidePaneEdge newEdge)
{
    if (edge == newEdge) {
        return;
    }
    edge = newEdge;
    primaryProjection->setArea(zzAreaFor(edge, true));
    secondaryProjection->setArea(zzAreaFor(edge, false));
    QPointer<ZzActivityBar> barGuard(q_ptr);
    setCurrentSourceIndex(currentSourceIndex);
    if (barGuard.isNull()) {
        return;
    }
    sanitizeActiveIndexes();
    if (barGuard.isNull()) {
        return;
    }
    Q_EMIT q_ptr->edgeChanged(edge);
}

void ZzActivityBarPrivate::setModel(QAbstractItemModel *model)
{
    if (sourceModel.data() == model) {
        return;
    }
    QObject::disconnect(modelDestroyedConnection);
    for (const QMetaObject::Connection &connection : activeModelConnections) {
        QObject::disconnect(connection);
    }
    activeModelConnections.clear();
    sourceModel = model;
    primaryProjection->setSourceModel(model);
    secondaryProjection->setSourceModel(model);
    dragTokens.clear();
    dragTokenExpiryTimer->stop();
    if (model != nullptr) {
        modelDestroyedConnection = QObject::connect(
            model, &QObject::destroyed, q_ptr, [this] {
                handleModelDestroyed();
            });
        const auto sanitize = [this] { sanitizeActiveIndexes(); };
        activeModelConnections.append(QObject::connect(
            model, &QAbstractItemModel::modelReset, q_ptr, sanitize));
        activeModelConnections.append(QObject::connect(
            model, &QAbstractItemModel::rowsRemoved, q_ptr,
            [sanitize](const QModelIndex &, int, int) { sanitize(); }));
        activeModelConnections.append(QObject::connect(
            model, &QAbstractItemModel::layoutChanged, q_ptr,
            [sanitize](const QList<QPersistentModelIndex> &,
                       QAbstractItemModel::LayoutChangeHint) { sanitize(); }));
        activeModelConnections.append(QObject::connect(
            model, &QAbstractItemModel::dataChanged, q_ptr,
            [sanitize](const QModelIndex &, const QModelIndex &,
                       const QList<int> &) { sanitize(); }));
    } else {
        modelDestroyedConnection = {};
    }
    QPointer<ZzActivityBar> barGuard(q_ptr);
    setCurrentSourceIndex({});
    if (barGuard.isNull()) {
        return;
    }
    setActiveSourceIndexes({});
    if (barGuard.isNull()) {
        return;
    }
    Q_EMIT q_ptr->modelChanged(model);
}

void ZzActivityBarPrivate::handleModelDestroyed()
{
    sourceModel.clear();
    primaryProjection->setSourceModel(nullptr);
    secondaryProjection->setSourceModel(nullptr);
    dragTokens.clear();
    dragTokenExpiryTimer->stop();
    modelDestroyedConnection = {};
    QPointer<ZzActivityBar> barGuard(q_ptr);
    setCurrentSourceIndex({});
    if (barGuard.isNull()) {
        return;
    }
    setActiveSourceIndexes({});
    if (barGuard.isNull()) {
        return;
    }
    Q_EMIT q_ptr->modelChanged(nullptr);
}

void ZzActivityBarPrivate::setCurrentSourceIndex(const QModelIndex &index)
{
    const bool accepted = acceptsSourceIndex(index);
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
    if (!multiActiveEnabled) {
        QPointer<ZzActivityBar> barGuard(q_ptr);
        setActiveSourceIndexes({currentSourceIndex});
        if (barGuard.isNull()) {
            return;
        }
    }
    if (previous != currentSourceIndex) {
        Q_EMIT q_ptr->currentSourceIndexChanged(currentSourceIndex);
    }
}

void ZzActivityBarPrivate::setActiveSourceIndexes(
    const QList<QModelIndex> &indexes)
{
    QList<QPersistentModelIndex> next;
    if (!multiActiveEnabled) {
        if (acceptsSourceIndex(currentSourceIndex)) {
            next.append(currentSourceIndex);
        }
    } else {
        next.reserve(indexes.size());
        QSet<int> seenRows;
        seenRows.reserve(indexes.size());
        for (const QModelIndex &index : indexes) {
            if (!acceptsSourceIndex(index)) {
                continue;
            }
            if (seenRows.contains(index.row())) {
                continue;
            }
            seenRows.insert(index.row());
            next.append(QPersistentModelIndex(index));
        }
    }
    if (activeSourceIndexes == next) {
        return;
    }
    activeSourceIndexes = next;
    primaryView->viewport()->update();
    secondaryView->viewport()->update();
    QList<QModelIndex> publicIndexes;
    publicIndexes.reserve(activeSourceIndexes.size());
    for (const QPersistentModelIndex &index : activeSourceIndexes) {
        publicIndexes.append(index);
    }
    Q_EMIT q_ptr->activeSourceIndexesChanged(publicIndexes);
}

void ZzActivityBarPrivate::sanitizeActiveIndexes()
{
    QList<QModelIndex> indexes;
    indexes.reserve(activeSourceIndexes.size());
    for (const QPersistentModelIndex &index : activeSourceIndexes) {
        indexes.append(index);
    }
    setActiveSourceIndexes(indexes);
}

bool ZzActivityBarPrivate::isSourceIndexActive(const QModelIndex &index) const
{
    return activeSourceIndexes.contains(index);
}

bool ZzActivityBarPrivate::isProjectionIndexActive(
    const QModelIndex &index) const
{
    const auto *projection = index.model() == primaryProjection
        ? primaryProjection
        : index.model() == secondaryProjection ? secondaryProjection : nullptr;
    return projection != nullptr
        && isSourceIndexActive(projection->mapToSource(index));
}

bool ZzActivityBarPrivate::acceptsSourceIndex(const QModelIndex &index) const
{
    if (!index.isValid() || index.model() != sourceModel.data()
        || index.parent().isValid() || index.column() != 0) {
        return false;
    }
    return primaryProjection->mapFromSource(index).isValid()
        || secondaryProjection->mapFromSource(index).isValid();
}

void ZzActivityBarPrivate::activateSourceIndex(const QModelIndex &index)
{
    if (!zzIsEnabled(index)) {
        return;
    }
    if (!index.flags().testFlag(Qt::ItemIsSelectable)) {
        Q_EMIT q_ptr->activationRequested(index);
        return;
    }
    if (currentSourceIndex == index) {
        Q_EMIT q_ptr->collapseRequested(index);
        return;
    }
    QPointer<ZzActivityBar> barGuard(q_ptr);
    QPointer<QAbstractItemModel> modelGuard(sourceModel);
    const QPersistentModelIndex indexGuard(index);
    setCurrentSourceIndex(index);
    if (barGuard.isNull() || modelGuard.isNull()) {
        return;
    }
    if (sourceModel.data() != modelGuard.data()
        || !indexGuard.isValid()
        || indexGuard.model() != modelGuard.data()
        || currentSourceIndex != indexGuard) {
        return;
    }
    const QModelIndex emittedIndex = indexGuard;
    Q_EMIT q_ptr->activationRequested(emittedIndex);
}

void ZzActivityBarPrivate::scheduleSourceActivation(const QModelIndex &index)
{
    if (!acceptsSourceIndex(index)) {
        return;
    }
    const QVariant areaValue = index.data(
        static_cast<int>(ZzActivityItemRole::Area));
    if (!areaValue.isValid() || !areaValue.canConvert<ZzActivityArea>()) {
        return;
    }
    const ZzActivityArea expectedArea = areaValue.value<ZzActivityArea>();
    const Qt::ItemFlags expectedFlags = index.flags();
    const QPersistentModelIndex expectedCurrent = currentSourceIndex;
    QPointer<ZzActivityBar> barGuard(q_ptr);
    QPointer<QAbstractItemModel> modelGuard(sourceModel);
    const QPersistentModelIndex indexGuard(index);
    QTimer::singleShot(
        0,
        q_ptr,
        [this, barGuard, modelGuard, indexGuard, expectedArea, expectedFlags,
         expectedCurrent] {
            if (barGuard.isNull() || modelGuard.isNull()
                || sourceModel.data() != modelGuard.data()
                || !indexGuard.isValid()
                || indexGuard.model() != modelGuard.data()) {
                return;
            }
            const QVariant currentAreaValue = indexGuard.data(
                static_cast<int>(ZzActivityItemRole::Area));
            if (!acceptsSourceIndex(indexGuard)
                || currentSourceIndex != expectedCurrent
                || indexGuard.flags() != expectedFlags
                || !currentAreaValue.isValid()
                || !currentAreaValue.canConvert<ZzActivityArea>()
                || currentAreaValue.value<ZzActivityArea>() != expectedArea) {
                return;
            }
            activateSourceIndex(indexGuard);
        });
}

bool ZzActivityBarPrivate::handleKey(QListView *view, int key)
{
    if (view != primaryView && view != secondaryView) {
        return false;
    }
    if (key == Qt::Key_Enter || key == Qt::Key_Return || key == Qt::Key_Space) {
        const auto *projection =
            static_cast<ZzActivityProjectionModel *>(view->model());
        const QModelIndex sourceIndex = projection->mapToSource(
            view->currentIndex());
        activateSourceIndex(sourceIndex);
        return true;
    }
    if (key != Qt::Key_Home && key != Qt::Key_End
        && key != Qt::Key_Up && key != Qt::Key_Down) {
        return false;
    }
    /** @brief 绑定键盘遍历中的视图、投影索引与稳定源索引。 */
    struct ZzKeyboardEntry final
    {
        QListView *view = nullptr;
        QModelIndex projectionIndex;
        QModelIndex sourceIndex;
    };
    QList<ZzKeyboardEntry> entries;
    for (const auto [candidateView, projection] :
         {std::pair{primaryView, primaryProjection},
          std::pair{secondaryView, secondaryProjection}}) {
        for (int row = 0; row < projection->rowCount(); ++row) {
            const QModelIndex projectionIndex = projection->index(row, 0);
            const QModelIndex sourceIndex =
                projection->mapToSource(projectionIndex);
            if (zzIsEnabled(sourceIndex)) {
                entries.append(
                    {candidateView, projectionIndex, sourceIndex});
            }
        }
    }
    if (entries.isEmpty()) {
        return true;
    }
    const auto *currentProjection =
        static_cast<ZzActivityProjectionModel *>(view->model());
    const QModelIndex focusedSource = currentProjection->mapToSource(
        view->currentIndex());
    int target = -1;
    for (qsizetype index = 0; index < entries.size(); ++index) {
        if (entries.at(index).sourceIndex == focusedSource) {
            target = static_cast<int>(index);
            break;
        }
    }
    if (target < 0) {
        for (qsizetype index = 0; index < entries.size(); ++index) {
            if (entries.at(index).sourceIndex == currentSourceIndex) {
                target = static_cast<int>(index);
                break;
            }
        }
    }
    if (key == Qt::Key_Home) {
        target = 0;
    } else if (key == Qt::Key_End) {
        target = static_cast<int>(entries.size()) - 1;
    } else if (key == Qt::Key_Up) {
        target = target > 0 ? target - 1 : 0;
    } else {
        target = target >= 0 && target + 1 < static_cast<int>(entries.size())
            ? target + 1 : static_cast<int>(entries.size()) - 1;
    }
    const ZzKeyboardEntry &entry = entries.at(target);
    entry.view->setFocus(Qt::TabFocusReason);
    if (entry.sourceIndex.flags().testFlag(Qt::ItemIsSelectable)) {
        setCurrentSourceIndex(entry.sourceIndex);
    } else if (entry.view->selectionModel() != nullptr) {
        entry.view->selectionModel()->setCurrentIndex(
            entry.projectionIndex,
            QItemSelectionModel::NoUpdate);
    }
    return true;
}

bool ZzActivityBarPrivate::showMoveContextMenu(
    QListView *view,
    const QPoint &viewportPosition)
{
    if (view != primaryView && view != secondaryView) {
        return false;
    }
    const auto *projection =
        static_cast<ZzActivityProjectionModel *>(view->model());
    const QModelIndex sourceIndex = projection->mapToSource(
        view->indexAt(viewportPosition));
    if (!zzIsEnabled(sourceIndex)
        || !sourceIndex.flags().testFlag(Qt::ItemIsDragEnabled)) {
        return false;
    }
    const QVariant areaValue = sourceIndex.data(
        static_cast<int>(ZzActivityItemRole::Area));
    if (!areaValue.isValid()
        || !areaValue.canConvert<ZzActivityArea>()) {
        return false;
    }
    const ZzActivityArea currentArea = areaValue.value<ZzActivityArea>();
    if (!zzIsKnownArea(currentArea)) {
        return false;
    }

    auto *root = new QMenu(q_ptr);
    root->setAttribute(Qt::WA_DeleteOnClose);
    QMenu *const moveMenu = root->addMenu(
        QCoreApplication::translate("ZzActivityBar", "移动到"));
    const QPersistentModelIndex guardedSource(sourceIndex);
    constexpr std::array areas = {
        ZzActivityArea::LeftPrimary,
        ZzActivityArea::LeftSecondary,
        ZzActivityArea::RightPrimary,
        ZzActivityArea::RightSecondary,
    };
    for (const ZzActivityArea targetArea : areas) {
        if (targetArea == currentArea) {
            continue;
        }
        QAction *const action = moveMenu->addAction(
            zzAreaLabel(targetArea));
        action->setData(QVariant::fromValue(targetArea));
        QObject::connect(
            action,
            &QAction::triggered,
            root,
            [this, guardedSource, currentArea, targetArea] {
                if (!guardedSource.isValid()
                    || guardedSource.model() != sourceModel.data()
                    || !zzIsEnabled(guardedSource)
                    || !guardedSource.flags().testFlag(
                        Qt::ItemIsDragEnabled)) {
                    return;
                }
                const QVariant currentAreaValue = guardedSource.data(
                    static_cast<int>(ZzActivityItemRole::Area));
                if (!currentAreaValue.isValid()
                    || !currentAreaValue.canConvert<ZzActivityArea>()
                    || currentAreaValue.value<ZzActivityArea>()
                        != currentArea) {
                    return;
                }
                const QModelIndex emittedSource = guardedSource;
                const int targetRow = rowCountForArea(targetArea);
                Q_EMIT q_ptr->moveRequested(
                    emittedSource,
                    targetArea,
                    targetRow);
            });
    }
    QObject::connect(
        root,
        &QMenu::aboutToHide,
        root,
        &QObject::deleteLater);
    root->popup(view->viewport()->mapToGlobal(viewportPosition));
    return true;
}

int ZzActivityBarPrivate::rowCountForArea(ZzActivityArea area) const
{
    if (sourceModel.isNull() || !zzIsKnownArea(area)) {
        return 0;
    }
    int count = 0;
    for (int row = 0; row < sourceModel->rowCount(); ++row) {
        const QModelIndex index = sourceModel->index(row, 0);
        const QVariant areaValue = index.data(
            static_cast<int>(ZzActivityItemRole::Area));
        if (areaValue.isValid()
            && areaValue.canConvert<ZzActivityArea>()
            && areaValue.value<ZzActivityArea>() == area) {
            ++count;
        }
    }
    return count;
}

QMimeData *ZzActivityBarPrivate::createMimeData(const QModelIndex &sourceIndex)
{
    if (!zzIsEnabled(sourceIndex)
        || !sourceIndex.model()->flags(sourceIndex).testFlag(Qt::ItemIsDragEnabled)) {
        return nullptr;
    }
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    dragTokens.clear();
    dragTokens.insert(token, QPersistentModelIndex(sourceIndex));
    dragTokenExpiryTimer->start();
    auto *mimeData = new QMimeData;
    mimeData->setData(
        QString::fromLatin1(zzActivityMoveMimeType), token.toUtf8());
    return mimeData;
}

void ZzActivityBarPrivate::discardDragTokens()
{
    dragTokens.clear();
    dragTokenExpiryTimer->stop();
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
    if (dragTokens.isEmpty()) {
        dragTokenExpiryTimer->stop();
    }
    Q_EMIT q_ptr->moveRequested(
        sourceIndex, areaForView(targetView), targetRow);
    return true;
}

ZzActivityArea ZzActivityBarPrivate::areaForView(const QListView *view) const
{
    return zzAreaFor(edge, view == primaryView);
}

} // namespace ZzFluentUI
