#include "ZzNavigationTreeModel.h"

#include <algorithm>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QVariant>
#include <QtGui/QFont>

#include <ZzFluentUI/ZzNavigationItemRole.h>
#include <ZzFluentUI/ZzNavigationPlacement.h>

#include "ZzNavigationPrivateRoles.h"

namespace ZzFluentUI {

namespace {

constexpr int zzNavigationTreeSectionRole = Qt::UserRole + 0x181;

[[nodiscard]] bool zzMetadataChanged(const QList<int> &roles)
{
    if (roles.isEmpty()) {
        return true;
    }
    return roles.contains(static_cast<int>(ZzNavigationItemRole::Section))
        || roles.contains(static_cast<int>(ZzNavigationItemRole::Placement));
}

[[nodiscard]] bool zzIsIncluded(
    const QModelIndex &index,
    ZzNavigationProjection projection)
{
    const QVariant value = index.data(static_cast<int>(
        ZzNavigationItemRole::Placement));
    const auto placement = value.canConvert<ZzNavigationPlacement>()
        ? value.value<ZzNavigationPlacement>()
        : ZzNavigationPlacement::Primary;
    if (projection == ZzNavigationProjection::All) {
        return true;
    }
    return projection == ZzNavigationProjection::Footer
        ? placement == ZzNavigationPlacement::Footer
        : placement == ZzNavigationPlacement::Primary;
}

} // namespace

ZzNavigationTreeModel::ZzNavigationTreeModel(
    ZzNavigationProjection projection,
    QObject *parent)
    : QAbstractProxyModel(parent)
    , projection_(projection)
    , root_(std::make_unique<TreeNode>())
{
}

ZzNavigationTreeModel::~ZzNavigationTreeModel()
{
    disconnectSourceModel();
}

void ZzNavigationTreeModel::setSourceModel(
    QAbstractItemModel *newSourceModel)
{
    if (newSourceModel == sourceModel()) {
        rebuild();
        return;
    }
    disconnectSourceModel();
    QAbstractProxyModel::setSourceModel(newSourceModel);
    connectSourceModel();
    rebuild();
}

QModelIndex ZzNavigationTreeModel::mapToSource(
    const QModelIndex &proxyIndex) const
{
    const TreeNode *const node = nodeForIndex(proxyIndex);
    if (node == nullptr || node->section || !node->sourceIndex.isValid()) {
        return {};
    }
    return QModelIndex(node->sourceIndex);
}

QModelIndex ZzNavigationTreeModel::mapFromSource(
    const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid() || sourceIndex.model() != sourceModel()
        || sourceIndex.parent().isValid() || sourceIndex.column() != 0
        || sourceIndex.row() < 0
        || sourceIndex.row() >= sourceToProxy_.size()) {
        return {};
    }
    return sourceToProxy_.at(sourceIndex.row());
}

QModelIndex ZzNavigationTreeModel::index(
    int row,
    int column,
    const QModelIndex &parent) const
{
    if (column != 0 || row < 0) {
        return {};
    }
    const TreeNode *const parentNode = parent.isValid()
        ? nodeForIndex(parent) : root_.get();
    if (parentNode == nullptr
        || row >= static_cast<int>(parentNode->children.size())) {
        return {};
    }
    TreeNode *const node = parentNode->children.at(row).get();
    return createIndex(row, 0, node);
}

QModelIndex ZzNavigationTreeModel::parent(
    const QModelIndex &child) const
{
    const TreeNode *const node = nodeForIndex(child);
    if (node == nullptr || node->parent == nullptr || node->parent == root_.get()) {
        return {};
    }
    return createIndex(
        node->parent->row,
        0,
        node->parent);
}

int ZzNavigationTreeModel::rowCount(const QModelIndex &parent) const
{
    const TreeNode *const parentNode = parent.isValid()
        ? nodeForIndex(parent) : root_.get();
    return parentNode == nullptr
        ? 0 : static_cast<int>(parentNode->children.size());
}

int ZzNavigationTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return sourceModel() == nullptr ? 0 : 1;
}

QVariant ZzNavigationTreeModel::data(
    const QModelIndex &proxyIndex,
    int role) const
{
    const TreeNode *const node = nodeForIndex(proxyIndex);
    if (node == nullptr) {
        return {};
    }
    if (!node->section) {
        return node->sourceIndex.data(role);
    }
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole
        || role == Qt::AccessibleTextRole) {
        return node->title;
    }
    if (role == zzNavigationSectionHeaderRole
        || role == zzNavigationTreeSectionRole) {
        return true;
    }
    if (role == Qt::FontRole) {
        QFont font;
        font.setWeight(QFont::DemiBold);
        return font;
    }
    return {};
}

Qt::ItemFlags ZzNavigationTreeModel::flags(
    const QModelIndex &proxyIndex) const
{
    const TreeNode *const node = nodeForIndex(proxyIndex);
    if (node == nullptr) {
        return Qt::NoItemFlags;
    }
    return node->section
        ? Qt::ItemIsEnabled : node->sourceIndex.flags();
}

QHash<int, QByteArray> ZzNavigationTreeModel::roleNames() const
{
    QHash<int, QByteArray> names = sourceModel() != nullptr
        ? sourceModel()->roleNames() : QHash<int, QByteArray>();
    names.insert(
        zzNavigationSectionHeaderRole,
        QByteArrayLiteral("zzNavigationSectionHeader"));
    names.insert(
        zzNavigationTreeSectionRole,
        QByteArrayLiteral("zzNavigationTreeSection"));
    return names;
}

void ZzNavigationTreeModel::rebuild()
{
    beginResetModel();
    root_ = std::make_unique<TreeNode>();
    sourceToProxy_.clear();

    QAbstractItemModel *const source = sourceModel();
    if (source != nullptr) {
        sourceToProxy_.resize(source->rowCount());
        TreeNode *currentSection = nullptr;
        for (int row = 0; row < source->rowCount(); ++row) {
            const QModelIndex sourceIndex = source->index(row, 0);
            if (!sourceIndex.isValid() || !zzIsIncluded(sourceIndex, projection_)) {
                continue;
            }
            const QString section = sourceIndex.data(static_cast<int>(
                ZzNavigationItemRole::Section)).toString().trimmed();
            if (projection_ == ZzNavigationProjection::All
                && sourceIndex.data(static_cast<int>(
                    ZzNavigationItemRole::Placement))
                       .value<ZzNavigationPlacement>()
                    == ZzNavigationPlacement::Footer) {
                currentSection = nullptr;
            }
            if (!section.isEmpty()
                && (currentSection == nullptr
                    || currentSection->title != section)) {
                auto group = std::make_unique<TreeNode>();
                group->title = section;
                group->parent = root_.get();
                group->section = true;
                group->row = static_cast<int>(root_->children.size());
                currentSection = group.get();
                root_->children.push_back(std::move(group));
            }
            TreeNode *const parentNode = currentSection != nullptr
                ? currentSection : root_.get();
            auto leaf = std::make_unique<TreeNode>();
            leaf->sourceIndex = sourceIndex;
            leaf->parent = parentNode;
            leaf->row = static_cast<int>(parentNode->children.size());
            TreeNode *const leafIdentity = leaf.get();
            parentNode->children.push_back(std::move(leaf));
            sourceToProxy_[row] = createIndex(
                leafIdentity->row,
                0,
                leafIdentity);
        }
    }
    endResetModel();
}

void ZzNavigationTreeModel::connectSourceModel()
{
    QAbstractItemModel *const source = sourceModel();
    if (source == nullptr) {
        return;
    }
    const auto rebuildConnection = [this] { rebuild(); };
    sourceConnections_.append(connect(
        source, &QAbstractItemModel::modelReset, this, rebuildConnection));
    sourceConnections_.append(connect(
        source, &QAbstractItemModel::layoutChanged, this, rebuildConnection));
    sourceConnections_.append(connect(
        source, &QAbstractItemModel::rowsInserted, this, rebuildConnection));
    sourceConnections_.append(connect(
        source, &QAbstractItemModel::rowsRemoved, this, rebuildConnection));
    sourceConnections_.append(connect(
        source, &QAbstractItemModel::rowsMoved, this, rebuildConnection));
    sourceConnections_.append(connect(
        source,
        &QAbstractItemModel::dataChanged,
        this,
        [this](
            const QModelIndex &topLeft,
            const QModelIndex &bottomRight,
            const QList<int> &roles) {
            if (zzMetadataChanged(roles)) {
                rebuild();
                return;
            }
            forwardDataChanged(topLeft, bottomRight, roles);
        }));
}

void ZzNavigationTreeModel::disconnectSourceModel()
{
    for (const QMetaObject::Connection &connection : sourceConnections_) {
        disconnect(connection);
    }
    sourceConnections_.clear();
}

void ZzNavigationTreeModel::forwardDataChanged(
    const QModelIndex &topLeft,
    const QModelIndex &bottomRight,
    const QList<int> &roles)
{
    if (!topLeft.isValid() || !bottomRight.isValid()
        || topLeft.parent().isValid() || bottomRight.parent().isValid()) {
        return;
    }
    const int first = std::max(0, topLeft.row());
    const int last = std::min(
        bottomRight.row(), static_cast<int>(sourceToProxy_.size() - 1));
    for (int row = first; row <= last; ++row) {
        const QModelIndex proxyIndex = sourceToProxy_.at(row);
        if (proxyIndex.isValid()) {
            Q_EMIT dataChanged(proxyIndex, proxyIndex, roles);
        }
    }
}

ZzNavigationTreeModel::TreeNode *ZzNavigationTreeModel::nodeForIndex(
    const QModelIndex &index) const noexcept
{
    if (!index.isValid() || index.model() != this || index.column() != 0) {
        return nullptr;
    }
    return static_cast<TreeNode *>(index.internalPointer());
}

} // namespace ZzFluentUI
