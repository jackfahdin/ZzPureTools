#include <ZzPureTools/ZzNavigationModel.h>

#include <utility>

#include "private/ZzNavigationModelPrivate.h"

namespace ZzPureTools {

ZzNavigationModel::ZzNavigationModel(QObject *parent)
    : QAbstractListModel(parent)
    , d_ptr(std::make_unique<ZzNavigationModelPrivate>(this))
{
}

ZzNavigationModel::~ZzNavigationModel() = default;

int ZzNavigationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(d_ptr->nodes.size());
}

QVariant ZzNavigationModel::data(
    const QModelIndex &index,
    int role) const
{
    if (!index.isValid() || index.parent().isValid()
        || index.column() != 0 || index.row() < 0
        || index.row() >= d_ptr->nodes.size()) {
        return {};
    }

    const auto row = static_cast<qsizetype>(index.row());
    const auto &node = d_ptr->nodes.at(row);
    if (role == Qt::DisplayRole) {
        return d_ptr->translatedTitles.at(row);
    }
    if (role == static_cast<int>(ZzNavigationRole::Route)) {
        return QVariant::fromValue(node.routeId);
    }
    if (role == static_cast<int>(ZzNavigationRole::Icon)) {
        return QVariant::fromValue(node.icon);
    }
    return {};
}

QHash<int, QByteArray> ZzNavigationModel::roleNames() const
{
    return {
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {static_cast<int>(ZzNavigationRole::Route),
         QByteArrayLiteral("route")},
        {static_cast<int>(ZzNavigationRole::Icon),
         QByteArrayLiteral("icon")}};
}

ZzCore::ZzResult<void> ZzNavigationModel::setNodes(
    QList<ZzNavigationNode> nodes)
{
    return d_ptr->setNodes(std::move(nodes));
}

ZzCore::ZzResult<ZzNavigationNode> ZzNavigationModel::nodeAt(
    qsizetype row) const
{
    return d_ptr->nodeAt(row);
}

void ZzNavigationModel::refreshTranslations()
{
    d_ptr->refreshTranslations();
}

} // namespace ZzPureTools
