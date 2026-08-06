#include "ZzNavigationProjectionModel.h"

#include <algorithm>
#include <limits>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QByteArray>
#include <QtCore/QVariant>

#include <ZzFluentUI/ZzNavigationItemRole.h>
#include <ZzFluentUI/ZzNavigationPlacement.h>

#include "ZzNavigationPrivateRoles.h"

namespace ZzFluentUI {

namespace {

[[nodiscard]] bool zzProjectionMetadataChanged(
    const QList<int> &roles)
{
    if (roles.isEmpty()) {
        return true;
    }
    return roles.contains(static_cast<int>(
               ZzNavigationItemRole::Section))
        || roles.contains(static_cast<int>(
            ZzNavigationItemRole::Placement));
}

[[nodiscard]] ZzNavigationPlacement zzNavigationPlacement(
    const QModelIndex &index)
{
    const QVariant value = index.data(static_cast<int>(
        ZzNavigationItemRole::Placement));
    if (!value.canConvert<ZzNavigationPlacement>()) {
        return ZzNavigationPlacement::Primary;
    }
    const auto placement = value.value<ZzNavigationPlacement>();
    return placement == ZzNavigationPlacement::Footer
        ? ZzNavigationPlacement::Footer
        : ZzNavigationPlacement::Primary;
}

} // namespace

ZzNavigationProjectionModel::ZzNavigationProjectionModel(
    ZzNavigationProjection projection,
    QObject *parent)
    : QAbstractProxyModel(parent)
    , projection_(projection)
{
}

ZzNavigationProjectionModel::~ZzNavigationProjectionModel()
{
    disconnectSourceModel();
}

void ZzNavigationProjectionModel::setSourceModel(
    QAbstractItemModel *newSourceModel)
{
    if (newSourceModel == sourceModel()) {
        if (newSourceModel == nullptr && !entries_.isEmpty()) {
            rebuild();
        }
        return;
    }
    disconnectSourceModel();
    QAbstractProxyModel::setSourceModel(newSourceModel);
    connectSourceModel();
    rebuild();
}

QModelIndex ZzNavigationProjectionModel::mapToSource(
    const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || proxyIndex.model() != this
        || proxyIndex.parent().isValid() || proxyIndex.column() != 0
        || proxyIndex.row() < 0
        || static_cast<qsizetype>(proxyIndex.row()) >= entries_.size()) {
        return {};
    }
    const auto &entry = entries_.at(proxyIndex.row());
    return entry.sectionHeader
        ? QModelIndex() : QModelIndex(entry.sourceIndex);
}

QModelIndex ZzNavigationProjectionModel::mapFromSource(
    const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid() || sourceIndex.model() != sourceModel()
        || sourceIndex.parent().isValid() || sourceIndex.column() != 0
        || sourceIndex.row() < 0
        || static_cast<qsizetype>(sourceIndex.row())
            >= sourceToProxyRow_.size()) {
        return {};
    }
    const int proxyRow = sourceToProxyRow_.at(sourceIndex.row());
    return proxyRow >= 0 ? index(proxyRow, 0) : QModelIndex();
}

QModelIndex ZzNavigationProjectionModel::index(
    int row,
    int column,
    const QModelIndex &parent) const
{
    if (parent.isValid() || column != 0 || row < 0
        || static_cast<qsizetype>(row) >= entries_.size()) {
        return {};
    }
    return createIndex(row, column);
}

QModelIndex ZzNavigationProjectionModel::parent(
    const QModelIndex &child) const
{
    Q_UNUSED(child)
    return {};
}

int ZzNavigationProjectionModel::rowCount(
    const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

int ZzNavigationProjectionModel::columnCount(
    const QModelIndex &parent) const
{
    return !parent.isValid() && sourceModel() != nullptr ? 1 : 0;
}

QVariant ZzNavigationProjectionModel::data(
    const QModelIndex &proxyIndex,
    int role) const
{
    if (!proxyIndex.isValid() || proxyIndex.model() != this
        || proxyIndex.parent().isValid() || proxyIndex.column() != 0
        || proxyIndex.row() < 0
        || static_cast<qsizetype>(proxyIndex.row()) >= entries_.size()) {
        return {};
    }
    const auto &entry = entries_.at(proxyIndex.row());
    if (!entry.sectionHeader) {
        return entry.sourceIndex.data(role);
    }
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole
        || role == Qt::AccessibleTextRole) {
        return entry.sectionTitle;
    }
    if (role == zzNavigationSectionHeaderRole) {
        return true;
    }
    return {};
}

Qt::ItemFlags ZzNavigationProjectionModel::flags(
    const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || proxyIndex.model() != this
        || proxyIndex.row() < 0
        || static_cast<qsizetype>(proxyIndex.row()) >= entries_.size()) {
        return Qt::NoItemFlags;
    }
    const auto &entry = entries_.at(proxyIndex.row());
    return entry.sectionHeader
        ? Qt::NoItemFlags : entry.sourceIndex.flags();
}

QHash<int, QByteArray> ZzNavigationProjectionModel::roleNames() const
{
    QHash<int, QByteArray> names = sourceModel() != nullptr
        ? sourceModel()->roleNames() : QHash<int, QByteArray>();
    names.insert(
        zzNavigationSectionHeaderRole,
        QByteArrayLiteral("zzNavigationSectionHeader"));
    return names;
}

void ZzNavigationProjectionModel::rebuild()
{
    beginResetModel();
    entries_.clear();
    sourceToProxyRow_.clear();

    QAbstractItemModel *const source = sourceModel();
    if (source != nullptr) {
        const int rows = source->rowCount();
        sourceToProxyRow_.fill(-1, rows);
        entries_.reserve(rows);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex sourceIndex = source->index(row, 0);
            if (!sourceIndex.isValid()) {
                continue;
            }
            const auto placement = zzNavigationPlacement(sourceIndex);
            const bool include = projection_ == ZzNavigationProjection::Footer
                ? placement == ZzNavigationPlacement::Footer
                : placement == ZzNavigationPlacement::Primary;
            if (!include) {
                continue;
            }

            if (projection_ == ZzNavigationProjection::Primary) {
                const QString section = sourceIndex
                    .data(static_cast<int>(ZzNavigationItemRole::Section))
                    .toString()
                    .trimmed();
                if (!section.isEmpty()) {
                    if (entries_.size() >= std::numeric_limits<int>::max()) {
                        break;
                    }
                    entries_.append({{}, section, true});
                }
            }
            if (entries_.size() >= std::numeric_limits<int>::max()) {
                break;
            }
            const int proxyRow = static_cast<int>(entries_.size());
            entries_.append({sourceIndex, {}, false});
            sourceToProxyRow_[row] = proxyRow;
        }
    }
    endResetModel();
}

void ZzNavigationProjectionModel::connectSourceModel()
{
    QAbstractItemModel *const source = sourceModel();
    if (source == nullptr) {
        return;
    }
    const auto rebuildConnection = [this] {
        rebuild();
    };
    sourceConnections_.append(connect(
        source,
        &QAbstractItemModel::modelReset,
        this,
        rebuildConnection));
    sourceConnections_.append(connect(
        source,
        &QAbstractItemModel::layoutChanged,
        this,
        rebuildConnection));
    sourceConnections_.append(connect(
        source,
        &QAbstractItemModel::rowsInserted,
        this,
        rebuildConnection));
    sourceConnections_.append(connect(
        source,
        &QAbstractItemModel::rowsRemoved,
        this,
        rebuildConnection));
    sourceConnections_.append(connect(
        source,
        &QAbstractItemModel::rowsMoved,
        this,
        rebuildConnection));
    sourceConnections_.append(connect(
        source,
        &QAbstractItemModel::dataChanged,
        this,
        [this](
            const QModelIndex &topLeft,
            const QModelIndex &bottomRight,
            const QList<int> &roles) {
            if (zzProjectionMetadataChanged(roles)) {
                rebuild();
                return;
            }
            forwardDataChanged(topLeft, bottomRight, roles);
        }));
}

void ZzNavigationProjectionModel::disconnectSourceModel()
{
    for (const auto &connection : sourceConnections_) {
        disconnect(connection);
    }
    sourceConnections_.clear();
}

void ZzNavigationProjectionModel::forwardDataChanged(
    const QModelIndex &topLeft,
    const QModelIndex &bottomRight,
    const QList<int> &roles)
{
    if (!topLeft.isValid() || !bottomRight.isValid()
        || topLeft.parent().isValid() || bottomRight.parent().isValid()
        || sourceToProxyRow_.isEmpty()) {
        return;
    }
    const int first = std::max(0, topLeft.row());
    const int last = std::min(
        bottomRight.row(),
        static_cast<int>(sourceToProxyRow_.size() - 1));
    for (int sourceRow = first; sourceRow <= last; ++sourceRow) {
        const int proxyRow = sourceToProxyRow_.at(sourceRow);
        if (proxyRow >= 0) {
            const QModelIndex changed = index(proxyRow, 0);
            Q_EMIT dataChanged(changed, changed, roles);
        }
    }
}

} // namespace ZzFluentUI
